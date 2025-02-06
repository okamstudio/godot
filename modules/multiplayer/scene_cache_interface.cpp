/**************************************************************************/
/*  scene_cache_interface.cpp                                             */
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

#include "scene_cache_interface.h"

#include "scene_multiplayer.h"

#include "core/io/marshalls.h"
#include "scene/main/node.h"
#include "scene/main/window.h"

SceneCacheInterface::NodeCache &SceneCacheInterface::_track(Node *p_node) {
	const ObjectID oid = p_node->get_instance_id();
	NodeCache *nc = nodes_cache.getptr(oid);
	if (!nc) {
		nodes_cache[oid] = NodeCache();
		p_node->connect(SceneStringName(tree_exited), callable_mp(this, &SceneCacheInterface::_remove_node_cache).bind(oid), Object::CONNECT_ONE_SHOT);
	}
	return nodes_cache[oid];
}

void SceneCacheInterface::_remove_node_cache(ObjectID p_oid) {
	NodeCache *nc = nodes_cache.getptr(p_oid);
	if (!nc) {
		return;
	}
	if (nc->cache_id) {
		assigned_ids.erase(nc->cache_id);
	}
#if 0
	// TODO: Find a way to cleanup recv_nodes without breaking visibility and RPCs interactions.
	for (KeyValue<int, int> &E : nc->recv_ids) {
		PeerInfo *pinfo = peers_info.getptr(E.key);
		ERR_CONTINUE(!pinfo);
		pinfo->recv_nodes.erase(E.value);
	}
#endif
	for (KeyValue<int, bool> &E : nc->confirmed_peers) {
		PeerInfo *pinfo = peers_info.getptr(E.key);
		ERR_CONTINUE(!pinfo);
		pinfo->sent_nodes.erase(p_oid);
	}
	nodes_cache.erase(p_oid);
}

void SceneCacheInterface::on_peer_change(int p_id, bool p_connected) {
	if (p_connected) {
		peers_info.insert(p_id, PeerInfo());
	} else {
		PeerInfo *pinfo = peers_info.getptr(p_id);
		ERR_FAIL_NULL(pinfo); // Bug.
		for (KeyValue<int, RecvNode> E : pinfo->recv_nodes) {
			NodeCache *nc = nodes_cache.getptr(E.value.oid);
			if (!nc) {
				// Node might have already been deleted locally.
				continue;
			}
			nc->recv_ids.erase(p_id);
		}
		for (const ObjectID &oid : pinfo->sent_nodes) {
			NodeCache *nc = nodes_cache.getptr(oid);
			ERR_CONTINUE(!nc);
			nc->confirmed_peers.erase(p_id);
		}
		peers_info.erase(p_id);
	}
}

void SceneCacheInterface::process_simplify_path(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND(!peers_info.has(p_from)); // Bug.
	ERR_FAIL_COND_MSG(p_packet_len < 38, "Invalid packet received. Size too small.");
	Node *root_node = SceneTree::get_singleton()->get_root()->get_node(multiplayer->get_root_path());
	ERR_FAIL_NULL(root_node);
	int ofs = 1;

	String methods_md5;
	methods_md5.parse_utf8((const char *)(p_packet + ofs), 32);
	ofs += 33;

	int id = decode_uint32(&p_packet[ofs]);
	ofs += 4;

	ERR_FAIL_COND_MSG(peers_info[p_from].recv_nodes.has(id), vformat("Duplicate remote cache ID %d for peer %d", id, p_from));

	String paths;
	paths.parse_utf8((const char *)(p_packet + ofs), p_packet_len - ofs);

	const NodePath path = paths;

	Node *node = root_node->get_node(path);
	ERR_FAIL_NULL(node);
	const bool valid_rpc_checksum = multiplayer->get_rpc_md5(node) == methods_md5;
	if (valid_rpc_checksum == false) {
		ERR_PRINT("The rpc node checksum failed. Make sure to have the same methods on both nodes. Node path: " + path);
	}

	peers_info[p_from].recv_nodes.insert(id, RecvNode(node->get_instance_id(), path));
	NodeCache &cache = _track(node);
	cache.recv_ids.insert(p_from, id);

	// Send ack.
	Vector<uint8_t> packet;
	packet.resize(1 + 1 + 4);
	packet.write[0] = SceneMultiplayer::NETWORK_COMMAND_CONFIRM_PATH;
	packet.write[1] = valid_rpc_checksum;
	encode_uint32(id, &packet.write[2]);

	Ref<MultiplayerPeer> multiplayer_peer = multiplayer->get_multiplayer_peer();
	ERR_FAIL_COND(multiplayer_peer.is_null());

	multiplayer_peer->set_transfer_channel(0);
	multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	multiplayer->send_command(p_from, packet.ptr(), packet.size());
}

void SceneCacheInterface::process_confirm_path(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND_MSG(p_packet_len != 6, "Invalid packet received. Size too small.");
	Node *root_node = SceneTree::get_singleton()->get_root()->get_node(multiplayer->get_root_path());
	ERR_FAIL_NULL(root_node);

	const bool valid_rpc_checksum = p_packet[1];
	int id = decode_uint32(&p_packet[2]);

	const ObjectID *oid = assigned_ids.getptr(id);
	if (oid == nullptr) {
		return; // May be trying to confirm a node that was removed.
	}

	if (valid_rpc_checksum == false) {
		const Node *node = (*oid).get_object<Node>();
		ERR_FAIL_NULL(node); // Bug.
		ERR_PRINT("The rpc node checksum failed. Make sure to have the same methods on both nodes. Node path: " + node->get_path());
	}

	NodeCache *cache = nodes_cache.getptr(*oid);
	ERR_FAIL_NULL(cache); // Bug.

	bool *confirmed = cache->confirmed_peers.getptr(p_from);
	ERR_FAIL_NULL_MSG(confirmed, "Invalid packet received. Tries to confirm a node which was not requested.");
	*confirmed = true;
}

Error SceneCacheInterface::_send_confirm_path(Node *p_node, NodeCache &p_cache, const List<int> &p_peers) {
	// Encode function name.
	const CharString path = String(multiplayer->get_root_path().rel_path_to(p_node->get_path())).utf8();
	const int path_len = encode_cstring(path.get_data(), nullptr);

	// Extract MD5 from rpc methods list.
	const String methods_md5 = multiplayer->get_rpc_md5(p_node);
	const int methods_md5_len = 33; // 32 + 1 for the `0` that is added by the encoder.

	Vector<uint8_t> packet;
	packet.resize(1 + 4 + path_len + methods_md5_len);
	int ofs = 0;

	packet.write[ofs] = SceneMultiplayer::NETWORK_COMMAND_SIMPLIFY_PATH;
	ofs += 1;

	ofs += encode_cstring(methods_md5.utf8().get_data(), &packet.write[ofs]);

	ofs += encode_uint32(p_cache.cache_id, &packet.write[ofs]);

	ofs += encode_cstring(path.get_data(), &packet.write[ofs]);

	Ref<MultiplayerPeer> multiplayer_peer = multiplayer->get_multiplayer_peer();
	ERR_FAIL_COND_V(multiplayer_peer.is_null(), ERR_BUG);

	Error err = OK;
	for (int peer_id : p_peers) {
		multiplayer_peer->set_transfer_channel(0);
		multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
		err = multiplayer->send_command(peer_id, packet.ptr(), packet.size());
		ERR_FAIL_COND_V(err != OK, err);
		// Insert into confirmed, but as false since it was not confirmed.
		p_cache.confirmed_peers.insert(peer_id, false);
		ERR_CONTINUE(!peers_info.has(peer_id));
		peers_info[peer_id].sent_nodes.insert(p_node->get_instance_id());
	}
	return err;
}

bool SceneCacheInterface::is_cache_confirmed(Node *p_node, int p_peer) {
	ERR_FAIL_NULL_V(p_node, false);
	const ObjectID oid = p_node->get_instance_id();
	NodeCache *cache = nodes_cache.getptr(oid);
	bool *confirmed = cache ? cache->confirmed_peers.getptr(p_peer) : nullptr;
	return confirmed && *confirmed;
}

int SceneCacheInterface::make_object_cache(Object *p_obj) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_NULL_V(node, -1);
	NodeCache &cache = _track(node);
	if (cache.cache_id == 0) {
		cache.cache_id = last_send_cache_id++;
		assigned_ids[cache.cache_id] = p_obj->get_instance_id();
	}
	return cache.cache_id;
}

bool SceneCacheInterface::send_object_cache(Object *p_obj, int p_peer_id, int &r_id) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_NULL_V(node, false);
	// See if the path is cached.
	NodeCache &cache = _track(node);
	if (cache.cache_id == 0) {
		cache.cache_id = last_send_cache_id++;
		assigned_ids[cache.cache_id] = p_obj->get_instance_id();
	}
	r_id = cache.cache_id;

	bool has_all_peers = true;
	List<int> peers_to_add; // If one is missing, take note to add it.

	if (p_peer_id > 0) {
		// Fast single peer check.
		ERR_FAIL_COND_V_MSG(!peers_info.has(p_peer_id), false, "Peer doesn't exist: " + itos(p_peer_id));

		bool *confirmed = cache.confirmed_peers.getptr(p_peer_id);
		if (!confirmed) {
			peers_to_add.push_back(p_peer_id); // Need to also be notified.
			has_all_peers = false;
		} else if (!(*confirmed)) {
			has_all_peers = false;
		}
	} else {
		// Long and painful.
		for (KeyValue<int, PeerInfo> &E : peers_info) {
			if (p_peer_id < 0 && E.key == -p_peer_id) {
				continue; // Continue, excluded.
			}

			bool *confirmed = cache.confirmed_peers.getptr(E.key);
			if (!confirmed) {
				peers_to_add.push_back(E.key); // Need to also be notified.
				has_all_peers = false;
			} else if (!(*confirmed)) {
				has_all_peers = false;
			}
		}
	}

	if (peers_to_add.size()) {
		_send_confirm_path(node, cache, peers_to_add);
	}

	return has_all_peers;
}

Object *SceneCacheInterface::get_cached_object(int p_from, uint32_t p_cache_id) {
	PeerInfo *pinfo = peers_info.getptr(p_from);
	ERR_FAIL_NULL_V(pinfo, nullptr);

	RecvNode *recv_node = pinfo->recv_nodes.getptr(p_cache_id);
	ERR_FAIL_NULL_V_MSG(recv_node, nullptr, vformat("ID %d not found in cache of peer %d.", p_cache_id, p_from));
	Node *node = recv_node->oid.get_object<Node>();
	if (!node) {
		// Fallback to path lookup.
		Node *root_node = SceneTree::get_singleton()->get_root()->get_node(multiplayer->get_root_path());
		ERR_FAIL_NULL_V(root_node, nullptr);
		node = root_node->get_node(recv_node->path);
		if (node) {
			recv_node->oid = node->get_instance_id();
		}
	}
	ERR_FAIL_NULL_V_MSG(node, nullptr, vformat("Failed to get cached node from peer %d with cache ID %d.", p_from, p_cache_id));
	return node;
}

void SceneCacheInterface::clear() {
	for (KeyValue<ObjectID, NodeCache> &E : nodes_cache) {
		Object *obj = ObjectDB::get_instance(E.key);
		ERR_CONTINUE(!obj);
		obj->disconnect(SceneStringName(tree_exited), callable_mp(this, &SceneCacheInterface::_remove_node_cache));
	}
	peers_info.clear();
	nodes_cache.clear();
	assigned_ids.clear();
	last_send_cache_id = 1;
}
