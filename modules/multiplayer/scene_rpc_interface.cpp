/**************************************************************************/
/*  scene_rpc_interface.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "scene_rpc_interface.h"

#include "scene_multiplayer.h"

#include "core/debugger/engine_debugger.h"
#include "core/io/marshalls.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"
#include "scene/main/window.h"

// The RPC meta is composed by a single byte that contains (starting from the least significant bit):
// - `NetworkCommands` in the first four bits.
// - `NetworkNodeIdCompression` in the next 2 bits.
// - `NetworkNameIdCompression` in the next 1 bit.
// - `byte_only_or_no_args` in the next 1 bit.
#define NODE_ID_COMPRESSION_SHIFT SceneMultiplayer::CMD_FLAG_0_SHIFT
#define NAME_ID_COMPRESSION_SHIFT SceneMultiplayer::CMD_FLAG_2_SHIFT
#define BYTE_ONLY_OR_NO_ARGS_SHIFT SceneMultiplayer::CMD_FLAG_3_SHIFT

#define NODE_ID_COMPRESSION_FLAG ((1 << NODE_ID_COMPRESSION_SHIFT) | (1 << (NODE_ID_COMPRESSION_SHIFT + 1)))
#define NAME_ID_COMPRESSION_FLAG (1 << NAME_ID_COMPRESSION_SHIFT)
#define BYTE_ONLY_OR_NO_ARGS_FLAG (1 << BYTE_ONLY_OR_NO_ARGS_SHIFT)

#ifdef DEBUG_ENABLED
_FORCE_INLINE_ void SceneRPCInterface::_profile_node_data(const String &p_what, ObjectID p_id, int p_size) {
	if (EngineDebugger::is_profiling("multiplayer:rpc")) {
		Array values;
		values.push_back(p_what);
		values.push_back(p_id);
		values.push_back(p_size);
		EngineDebugger::profiler_add_frame_data("multiplayer:rpc", values);
	}
}
#endif

// Returns the packet size stripping the node path added when the node is not yet cached.
int get_packet_len(uint32_t p_node_target, int p_packet_len) {
	if (p_node_target & 0x80000000) {
		int ofs = p_node_target & 0x7FFFFFFF;
		return p_packet_len - (p_packet_len - ofs);
	} else {
		return p_packet_len;
	}
}

void SceneRPCInterface::_parse_rpc_config(const Variant &p_config, bool p_for_node, RPCConfigCache &r_cache) {
	if (p_config.get_type() == Variant::NIL) {
		return;
	}
	ERR_FAIL_COND(p_config.get_type() != Variant::DICTIONARY);
	const Dictionary config = p_config;
	Array names = config.keys();
	names.sort_custom(callable_mp_static(&StringLikeVariantOrder::compare)); // Ensure ID order
	for (int i = 0; i < names.size(); i++) {
		ERR_CONTINUE(!names[i].is_string());
		String name = names[i].operator String();
		ERR_CONTINUE(config[name].get_type() != Variant::DICTIONARY);
		ERR_CONTINUE(!config[name].operator Dictionary().has("rpc_mode"));
		Dictionary dict = config[name];
		RPCConfig cfg;
		cfg.name = name;
		cfg.rpc_mode = ((MultiplayerAPI::RPCMode)dict.get("rpc_mode", MultiplayerAPI::RPC_MODE_AUTHORITY).operator int());
		cfg.transfer_mode = ((MultiplayerPeer::TransferMode)dict.get("transfer_mode", MultiplayerPeer::TRANSFER_MODE_RELIABLE).operator int());
		cfg.call_local = dict.get("call_local", false).operator bool();
		cfg.channel = dict.get("channel", 0).operator int();
		uint16_t id = ((uint16_t)i);
		if (p_for_node) {
			id |= (1 << 15);
		}
		r_cache.configs[id] = cfg;
		r_cache.ids[name] = id;
	}
}

const SceneRPCInterface::RPCConfigCache &SceneRPCInterface::_get_node_config(const Node *p_node) {
	const ObjectID oid = p_node->get_instance_id();
	if (rpc_cache.has(oid)) {
		return rpc_cache[oid];
	}
	RPCConfigCache cache;
	_parse_rpc_config(p_node->get_rpc_config(), true, cache);
	if (p_node->get_script_instance()) {
		_parse_rpc_config(p_node->get_script_instance()->get_rpc_config(), false, cache);
	}
	rpc_cache[oid] = cache;
	return rpc_cache[oid];
}

String SceneRPCInterface::get_rpc_md5(const Object *p_obj) {
	const Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_NULL_V(node, "");
	const RPCConfigCache cache = _get_node_config(node);
	String rpc_list;
	for (const KeyValue<uint16_t, RPCConfig> &config : cache.configs) {
		rpc_list += String(config.value.name);
	}
	return rpc_list.md5_text();
}

Node *SceneRPCInterface::_process_get_node(int p_from, const uint8_t *p_packet, uint32_t p_node_target, int p_packet_len) {
	Node *root_node = SceneTree::get_singleton()->get_root()->get_node(multiplayer->get_root_path());
	ERR_FAIL_NULL_V(root_node, nullptr);
	Node *node = nullptr;

	if (p_node_target & 0x80000000) {
		// Use full path (not cached yet).
		int ofs = p_node_target & 0x7FFFFFFF;

		ERR_FAIL_COND_V_MSG(ofs >= p_packet_len, nullptr, "Invalid packet received. Size smaller than declared.");

		String paths;
		paths.parse_utf8((const char *)&p_packet[ofs], p_packet_len - ofs);

		NodePath np = paths;

		node = root_node->get_node(np);

		if (!node) {
			ERR_PRINT("Failed to get path from RPC: " + String(np) + ".");
		}
		return node;
	} else {
		// Use cached path.
		return Object::cast_to<Node>(multiplayer_cache->get_cached_object(p_from, p_node_target));
	}
}

void SceneRPCInterface::process_rpc(int p_from, const uint8_t *p_packet, int p_packet_len) {
	// Extract packet meta
	int packet_min_size = 1;
	int name_id_offset = 1;
	ERR_FAIL_COND_MSG(p_packet_len < packet_min_size, "Invalid packet received. Size too small.");
	// Compute the meta size, which depends on the compression level.
	int node_id_compression = (p_packet[0] & NODE_ID_COMPRESSION_FLAG) >> NODE_ID_COMPRESSION_SHIFT;
	int name_id_compression = (p_packet[0] & NAME_ID_COMPRESSION_FLAG) >> NAME_ID_COMPRESSION_SHIFT;

	switch (node_id_compression) {
		case NETWORK_NODE_ID_COMPRESSION_8:
			packet_min_size += 1;
			name_id_offset += 1;
			break;
		case NETWORK_NODE_ID_COMPRESSION_16:
			packet_min_size += 2;
			name_id_offset += 2;
			break;
		case NETWORK_NODE_ID_COMPRESSION_32:
			packet_min_size += 4;
			name_id_offset += 4;
			break;
		default:
			ERR_FAIL_MSG("Was not possible to extract the node id compression mode.");
	}
	switch (name_id_compression) {
		case NETWORK_NAME_ID_COMPRESSION_8:
			packet_min_size += 1;
			break;
		case NETWORK_NAME_ID_COMPRESSION_16:
			packet_min_size += 2;
			break;
		default:
			ERR_FAIL_MSG("Was not possible to extract the name id compression mode.");
	}
	ERR_FAIL_COND_MSG(p_packet_len < packet_min_size, "Invalid packet received. Size too small.");

	uint32_t node_target = 0;
	switch (node_id_compression) {
		case NETWORK_NODE_ID_COMPRESSION_8:
			node_target = p_packet[1];
			break;
		case NETWORK_NODE_ID_COMPRESSION_16:
			node_target = decode_uint16(p_packet + 1);
			break;
		case NETWORK_NODE_ID_COMPRESSION_32:
			node_target = decode_uint32(p_packet + 1);
			break;
		default:
			// Unreachable, checked before.
			CRASH_NOW();
	}

	Node *node = _process_get_node(p_from, p_packet, node_target, p_packet_len);
	ERR_FAIL_NULL_MSG(node, "Invalid packet received. Requested node was not found.");

	uint16_t name_id = 0;
	switch (name_id_compression) {
		case NETWORK_NAME_ID_COMPRESSION_8:
			name_id = p_packet[name_id_offset];
			break;
		case NETWORK_NAME_ID_COMPRESSION_16:
			name_id = decode_uint16(p_packet + name_id_offset);
			break;
		default:
			// Unreachable, checked before.
			CRASH_NOW();
	}

	const int packet_len = get_packet_len(node_target, p_packet_len);
	_process_rpc(node, name_id, p_from, p_packet, packet_len, packet_min_size);
}

void SceneRPCInterface::_process_rpc(Node *p_node, const uint16_t p_rpc_method_id, int p_from, const uint8_t *p_packet, int p_packet_len, int p_offset) {
	ERR_FAIL_COND_MSG(p_offset > p_packet_len, "Invalid packet received. Size too small.");

	// Check that remote can call the RPC on this node.
	const RPCConfigCache &cache_config = _get_node_config(p_node);
	ERR_FAIL_COND(!cache_config.configs.has(p_rpc_method_id));
	const RPCConfig &config = cache_config.configs[p_rpc_method_id];

	bool can_call = false;
	switch (config.rpc_mode) {
		case MultiplayerAPI::RPC_MODE_DISABLED: {
			can_call = false;
		} break;
		case MultiplayerAPI::RPC_MODE_ANY_PEER: {
			can_call = true;
		} break;
		case MultiplayerAPI::RPC_MODE_AUTHORITY: {
			can_call = p_from == p_node->get_multiplayer_authority();
		} break;
	}

	ERR_FAIL_COND_MSG(!can_call, "RPC '" + String(config.name) + "' is not allowed on node " + p_node->get_path() + " from: " + itos(p_from) + ". Mode is " + itos((int)config.rpc_mode) + ", authority is " + itos(p_node->get_multiplayer_authority()) + ".");

	int argc = 0;

	const bool byte_only_or_no_args = p_packet[0] & BYTE_ONLY_OR_NO_ARGS_FLAG;
	if (byte_only_or_no_args) {
		if (p_offset < p_packet_len) {
			// This packet contains only bytes.
			argc = 1;
		}
	} else {
		// Normal variant, takes the argument count from the packet.
		ERR_FAIL_COND_MSG(p_offset >= p_packet_len, "Invalid packet received. Size too small.");
		argc = p_packet[p_offset];
		p_offset += 1;
	}

	Vector<Variant> args;
	Vector<const Variant *> argp;
	args.resize(argc);
	argp.resize(argc);

#ifdef DEBUG_ENABLED
	_profile_node_data("rpc_in", p_node->get_instance_id(), p_packet_len);
#endif

	int out;
	MultiplayerAPI::decode_and_decompress_variants(args, &p_packet[p_offset], p_packet_len - p_offset, out, byte_only_or_no_args, multiplayer->is_object_decoding_allowed());
	for (int i = 0; i < argc; i++) {
		argp.write[i] = &args[i];
	}

	Callable::CallError ce;

	p_node->callp(config.name, (const Variant **)argp.ptr(), argc, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		String error = Variant::get_call_error_text(p_node, config.name, (const Variant **)argp.ptr(), argc, ce);
		error = "RPC - " + error;
		ERR_PRINT(error);
	}
}

void SceneRPCInterface::_send_rpc(Node *p_node, int p_to, uint16_t p_rpc_id, const RPCConfig &p_config, const StringName &p_name, const Variant **p_arg, int p_argcount) {
	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	ERR_FAIL_COND_MSG(peer.is_null(), "Attempt to call RPC without active multiplayer peer.");

	ERR_FAIL_COND_MSG(peer->get_connection_status() == MultiplayerPeer::CONNECTION_CONNECTING, "Attempt to call RPC while multiplayer peer is not connected yet.");

	ERR_FAIL_COND_MSG(peer->get_connection_status() == MultiplayerPeer::CONNECTION_DISCONNECTED, "Attempt to call RPC while multiplayer peer is disconnected.");

	ERR_FAIL_COND_MSG(p_argcount > 255, "Too many arguments (>255).");

	if (p_to != 0 && !multiplayer->get_connected_peers().has(ABS(p_to))) {
		ERR_FAIL_COND_MSG(p_to == multiplayer->get_unique_id(), "Attempt to call RPC on yourself! Peer unique ID: " + itos(multiplayer->get_unique_id()) + ".");

		ERR_FAIL_MSG("Attempt to call RPC with unknown peer ID: " + itos(p_to) + ".");
	}

	// See if all peers have cached path (if so, call can be fast) while building the RPC target list.
	HashSet<int> targets;
	int psc_id = -1;
	bool has_all_peers = true;
	const ObjectID oid = p_node->get_instance_id();
	if (p_to > 0) {
		ERR_FAIL_COND_MSG(!multiplayer_replicator->is_rpc_visible(oid, p_to), "Attempt to call an RPC to a peer that cannot see this node. Peer ID: " + itos(p_to));
		targets.insert(p_to);
		has_all_peers = multiplayer_cache->send_object_cache(p_node, p_to, psc_id);
	} else {
		bool restricted = !multiplayer_replicator->is_rpc_visible(oid, 0);
		for (const int &P : multiplayer->get_connected_peers()) {
			if (p_to < 0 && P == -p_to) {
				continue; // Excluded peer.
			}
			if (restricted && !multiplayer_replicator->is_rpc_visible(oid, P)) {
				continue; // Not visible to this peer.
			}
			targets.insert(P);
			bool has_peer = multiplayer_cache->send_object_cache(p_node, P, psc_id);
			has_all_peers = has_all_peers && has_peer;
		}
	}
	if (targets.is_empty()) {
		return; // No one in sight.
	}

	// Create base packet, lots of hardcode because it must be tight.
	int ofs = 0;

#define MAKE_ROOM(m_amount)             \
	if (packet_cache.size() < m_amount) \
		packet_cache.resize(m_amount);

	// Encode meta.
	uint8_t command_type = SceneMultiplayer::NETWORK_COMMAND_REMOTE_CALL;
	uint8_t node_id_compression = UINT8_MAX;
	uint8_t name_id_compression = UINT8_MAX;
	bool byte_only_or_no_args = false;

	MAKE_ROOM(1);
	// The meta is composed along the way, so just set 0 for now.
	packet_cache.write[0] = 0;
	ofs += 1;

	// Encode Node ID.
	if (has_all_peers) {
		// Compress the node ID only if all the target peers already know it.
		if (psc_id >= 0 && psc_id <= 255) {
			// We can encode the id in 1 byte
			node_id_compression = NETWORK_NODE_ID_COMPRESSION_8;
			MAKE_ROOM(ofs + 1);
			packet_cache.write[ofs] = static_cast<uint8_t>(psc_id);
			ofs += 1;
		} else if (psc_id >= 0 && psc_id <= 65535) {
			// We can encode the id in 2 bytes
			node_id_compression = NETWORK_NODE_ID_COMPRESSION_16;
			MAKE_ROOM(ofs + 2);
			encode_uint16(static_cast<uint16_t>(psc_id), &(packet_cache.write[ofs]));
			ofs += 2;
		} else {
			// Too big, let's use 4 bytes.
			node_id_compression = NETWORK_NODE_ID_COMPRESSION_32;
			MAKE_ROOM(ofs + 4);
			encode_uint32(psc_id, &(packet_cache.write[ofs]));
			ofs += 4;
		}
	} else {
		// The targets don't know the node yet, so we need to use 32 bits int.
		node_id_compression = NETWORK_NODE_ID_COMPRESSION_32;
		MAKE_ROOM(ofs + 4);
		encode_uint32(psc_id, &(packet_cache.write[ofs]));
		ofs += 4;
	}

	// Encode method ID
	if (p_rpc_id <= UINT8_MAX) {
		// The ID fits in 1 byte
		name_id_compression = NETWORK_NAME_ID_COMPRESSION_8;
		MAKE_ROOM(ofs + 1);
		packet_cache.write[ofs] = static_cast<uint8_t>(p_rpc_id);
		ofs += 1;
	} else {
		// The ID is larger, let's use 2 bytes
		name_id_compression = NETWORK_NAME_ID_COMPRESSION_16;
		MAKE_ROOM(ofs + 2);
		encode_uint16(p_rpc_id, &(packet_cache.write[ofs]));
		ofs += 2;
	}

	int len;
	Error err = MultiplayerAPI::encode_and_compress_variants(p_arg, p_argcount, nullptr, len, &byte_only_or_no_args, multiplayer->is_object_decoding_allowed());
	ERR_FAIL_COND_MSG(err != OK, "Unable to encode RPC arguments. THIS IS LIKELY A BUG IN THE ENGINE!");
	if (byte_only_or_no_args) {
		MAKE_ROOM(ofs + len);
	} else {
		MAKE_ROOM(ofs + 1 + len);
		packet_cache.write[ofs] = p_argcount;
		ofs += 1;
	}
	if (len) {
		MultiplayerAPI::encode_and_compress_variants(p_arg, p_argcount, &packet_cache.write[ofs], len, &byte_only_or_no_args, multiplayer->is_object_decoding_allowed());
		ofs += len;
	}

	ERR_FAIL_COND(command_type > 7);
	ERR_FAIL_COND(node_id_compression > 3);
	ERR_FAIL_COND(name_id_compression > 1);

#ifdef DEBUG_ENABLED
	_profile_node_data("rpc_out", p_node->get_instance_id(), ofs);
#endif

	// We can now set the meta
	packet_cache.write[0] = command_type + (node_id_compression << NODE_ID_COMPRESSION_SHIFT) + (name_id_compression << NAME_ID_COMPRESSION_SHIFT) + (byte_only_or_no_args ? BYTE_ONLY_OR_NO_ARGS_FLAG : 0);

	// Take chance and set transfer mode, since all send methods will use it.
	peer->set_transfer_channel(p_config.channel);
	peer->set_transfer_mode(p_config.transfer_mode);

	if (has_all_peers) {
		for (const int P : targets) {
			multiplayer->send_command(P, packet_cache.ptr(), ofs);
		}
	} else {
		// Unreachable because the node ID is never compressed if the peers doesn't know it.
		CRASH_COND(node_id_compression != NETWORK_NODE_ID_COMPRESSION_32);

		// Not all verified path, so send one by one.

		// Append path at the end, since we will need it for some packets.
		CharString pname = String(multiplayer->get_root_path().rel_path_to(p_node->get_path())).utf8();
		int path_len = encode_cstring(pname.get_data(), nullptr);
		MAKE_ROOM(ofs + path_len);
		encode_cstring(pname.get_data(), &(packet_cache.write[ofs]));

		// Not all verified path, so check which needs the longer packet.
		for (const int P : targets) {
			bool confirmed = multiplayer_cache->is_cache_confirmed(p_node, P);
			if (confirmed) {
				// This one confirmed path, so use id.
				encode_uint32(psc_id, &(packet_cache.write[1]));
				multiplayer->send_command(P, packet_cache.ptr(), ofs);
			} else {
				// This one did not confirm path yet, so use entire path (sorry!).
				encode_uint32(0x80000000 | ofs, &(packet_cache.write[1])); // Offset to path and flag.
				multiplayer->send_command(P, packet_cache.ptr(), ofs + path_len);
			}
		}
	}
}

Error SceneRPCInterface::rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) {
	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	ERR_FAIL_COND_V_MSG(peer.is_null(), ERR_UNCONFIGURED, "Trying to call an RPC while no multiplayer peer is active.");
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V_MSG(!node || !node->is_inside_tree(), ERR_INVALID_PARAMETER, "The object must be a valid Node inside the SceneTree");
	ERR_FAIL_COND_V_MSG(peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED, ERR_CONNECTION_ERROR, "Trying to call an RPC via a multiplayer peer which is not connected.");

	int caller_id = multiplayer->get_unique_id();
	bool call_local_native = false;
	bool call_local_script = false;
	const RPCConfigCache &config_cache = _get_node_config(node);
	uint16_t rpc_id = config_cache.ids.has(p_method) ? config_cache.ids[p_method] : UINT16_MAX;
	ERR_FAIL_COND_V_MSG(rpc_id == UINT16_MAX, ERR_INVALID_PARAMETER,
			vformat("Unable to get the RPC configuration for the function \"%s\" at path: \"%s\". This happens when the method is missing or not marked for RPCs in the local script.", p_method, node->get_path()));
	const RPCConfig &config = config_cache.configs[rpc_id];

	ERR_FAIL_COND_V_MSG(p_peer_id == caller_id && !config.call_local, ERR_INVALID_PARAMETER, "RPC '" + p_method + "' on yourself is not allowed by selected mode.");

	if (p_peer_id == 0 || p_peer_id == caller_id || (p_peer_id < 0 && p_peer_id != -caller_id)) {
		if (rpc_id & (1 << 15)) {
			call_local_native = config.call_local;
		} else {
			call_local_script = config.call_local;
		}
	}

	if (p_peer_id != caller_id) {
		_send_rpc(node, p_peer_id, rpc_id, config, p_method, p_arg, p_argcount);
	}

	if (call_local_native) {
		Callable::CallError ce;

		multiplayer->set_remote_sender_override(multiplayer->get_unique_id());
		node->callp(p_method, p_arg, p_argcount, ce);
		multiplayer->set_remote_sender_override(0);

		if (ce.error != Callable::CallError::CALL_OK) {
			String error = Variant::get_call_error_text(node, p_method, p_arg, p_argcount, ce);
			error = "rpc() aborted in local call:  - " + error + ".";
			ERR_PRINT(error);
			return FAILED;
		}
	}

	if (call_local_script) {
		Callable::CallError ce;
		ce.error = Callable::CallError::CALL_OK;

		multiplayer->set_remote_sender_override(multiplayer->get_unique_id());
		node->get_script_instance()->callp(p_method, p_arg, p_argcount, ce);
		multiplayer->set_remote_sender_override(0);

		if (ce.error != Callable::CallError::CALL_OK) {
			String error = Variant::get_call_error_text(node, p_method, p_arg, p_argcount, ce);
			error = "rpc() aborted in script local call:  - " + error + ".";
			ERR_PRINT(error);
			return FAILED;
		}
	}
	return OK;
}
