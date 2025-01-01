/**************************************************************************/
/*  multiplayer_api.cpp                                                   */
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

#include "multiplayer_api.h"

#include "core/io/marshalls.h"
StringName MultiplayerAPI::default_interface;

void MultiplayerAPI::set_default_interface(const StringName &p_interface) {
	ERR_FAIL_COND_MSG(!ClassDB::is_parent_class(p_interface, MultiplayerAPI::get_class_static()), vformat("Can't make %s the default multiplayer interface since it does not extend MultiplayerAPI.", p_interface));
	default_interface = StringName(p_interface, true);
}

StringName MultiplayerAPI::get_default_interface() {
	return default_interface;
}

Ref<MultiplayerAPI> MultiplayerAPI::create_default_interface() {
	if (default_interface != StringName()) {
		return Ref<MultiplayerAPI>(Object::cast_to<MultiplayerAPI>(ClassDB::instantiate(default_interface)));
	}
	return Ref<MultiplayerAPI>(memnew(MultiplayerAPIExtension));
}

// The variant is compressed and encoded; The first byte contains all the meta
// information and the format is:
// - The first LSB 6 bits are used for the variant type.
// - The next two bits are used to store the encoding mode.
// - Boolean values uses the encoding mode to store the value.
#define VARIANT_META_TYPE_MASK 0x3F
#define VARIANT_META_EMODE_MASK 0xC0
#define VARIANT_META_BOOL_MASK 0x80
#define ENCODE_8 0 << 6
#define ENCODE_16 1 << 6
#define ENCODE_32 2 << 6
#define ENCODE_64 3 << 6
Error MultiplayerAPI::encode_and_compress_variant(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_allow_object_decoding) {
	// Unreachable because `VARIANT_MAX` == 38 and `ENCODE_VARIANT_MASK` == 77
	CRASH_COND(p_variant.get_type() > VARIANT_META_TYPE_MASK);

	uint8_t *buf = r_buffer;
	r_len = 0;
	uint8_t encode_mode = 0;

	switch (p_variant.get_type()) {
		case Variant::BOOL: {
			if (buf) {
				// We don't use encode_mode for booleans, so we can use it to store the value.
				buf[0] = (p_variant.operator bool()) ? (1 << 7) : 0;
				buf[0] |= p_variant.get_type();
			}
			r_len += 1;
		} break;
		case Variant::INT: {
			if (buf) {
				// Reserve the first byte for the meta.
				buf += 1;
			}
			r_len += 1;
			int64_t val = p_variant;
			if (val <= (int64_t)INT8_MAX && val >= (int64_t)INT8_MIN) {
				// Use 8 bit
				encode_mode = ENCODE_8;
				if (buf) {
					buf[0] = val;
				}
				r_len += 1;
			} else if (val <= (int64_t)INT16_MAX && val >= (int64_t)INT16_MIN) {
				// Use 16 bit
				encode_mode = ENCODE_16;
				if (buf) {
					encode_uint16(val, buf);
				}
				r_len += 2;
			} else if (val <= (int64_t)INT32_MAX && val >= (int64_t)INT32_MIN) {
				// Use 32 bit
				encode_mode = ENCODE_32;
				if (buf) {
					encode_uint32(val, buf);
				}
				r_len += 4;
			} else {
				// Use 64 bit
				encode_mode = ENCODE_64;
				if (buf) {
					encode_uint64(val, buf);
				}
				r_len += 8;
			}
			// Store the meta
			if (buf) {
				buf -= 1;
				buf[0] = encode_mode | p_variant.get_type();
			}
		} break;
		default:
			// Any other case is not yet compressed.
			Error err = encode_variant(p_variant, r_buffer, r_len, p_allow_object_decoding);
			if (err != OK) {
				return err;
			}
			if (r_buffer) {
				// The first byte is not used by the marshaling, so store the type
				// so we know how to decompress and decode this variant.
				r_buffer[0] = p_variant.get_type();
			}
	}

	return OK;
}

Error MultiplayerAPI::decode_and_decompress_variant(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_object_decoding) {
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 1, ERR_INVALID_DATA);
	uint8_t type = buf[0] & VARIANT_META_TYPE_MASK;
	uint8_t encode_mode = buf[0] & VARIANT_META_EMODE_MASK;

	ERR_FAIL_COND_V(type >= Variant::VARIANT_MAX, ERR_INVALID_DATA);

	switch (type) {
		case Variant::BOOL: {
			bool val = (buf[0] & VARIANT_META_BOOL_MASK) > 0;
			r_variant = val;
			if (r_len) {
				*r_len = 1;
			}
		} break;
		case Variant::INT: {
			buf += 1;
			len -= 1;
			if (r_len) {
				*r_len = 1;
			}
			if (encode_mode == ENCODE_8) {
				// 8 bits.
				ERR_FAIL_COND_V(len < 1, ERR_INVALID_DATA);
				int8_t val = buf[0];
				r_variant = val;
				if (r_len) {
					(*r_len) += 1;
				}
			} else if (encode_mode == ENCODE_16) {
				// 16 bits.
				ERR_FAIL_COND_V(len < 2, ERR_INVALID_DATA);
				int16_t val = decode_uint16(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 2;
				}
			} else if (encode_mode == ENCODE_32) {
				// 32 bits.
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			} else {
				// 64 bits.
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}
			}
		} break;
		default:
			Error err = decode_variant(r_variant, p_buffer, p_len, r_len, p_allow_object_decoding);
			if (err != OK) {
				return err;
			}
	}

	return OK;
}

Error MultiplayerAPI::encode_and_compress_variants(const Variant **p_variants, int p_count, uint8_t *p_buffer, int &r_len, bool *r_raw, bool p_allow_object_decoding) {
	r_len = 0;
	int size = 0;

	if (p_count == 0) {
		if (r_raw) {
			*r_raw = true;
		}
		return OK;
	}

	// Try raw encoding optimization.
	if (r_raw && p_count == 1) {
		*r_raw = false;
		const Variant &v = *(p_variants[0]);
		if (v.get_type() == Variant::PACKED_BYTE_ARRAY) {
			*r_raw = true;
			const PackedByteArray pba = v;
			if (p_buffer) {
				memcpy(p_buffer, pba.ptr(), pba.size());
			}
			r_len += pba.size();
		} else {
			encode_and_compress_variant(v, p_buffer, size, p_allow_object_decoding);
			r_len += size;
		}
		return OK;
	}

	// Regular encoding.
	for (int i = 0; i < p_count; i++) {
		const Variant &v = *(p_variants[i]);
		encode_and_compress_variant(v, p_buffer ? p_buffer + r_len : nullptr, size, p_allow_object_decoding);
		r_len += size;
	}
	return OK;
}

Error MultiplayerAPI::decode_and_decompress_variants(Vector<Variant> &r_variants, const uint8_t *p_buffer, int p_len, int &r_len, bool p_raw, bool p_allow_object_decoding) {
	r_len = 0;
	int argc = r_variants.size();
	if (argc == 0 && p_raw) {
		return OK;
	}
	ERR_FAIL_COND_V(p_raw && argc != 1, ERR_INVALID_DATA);
	if (p_raw) {
		r_len = p_len;
		PackedByteArray pba;
		pba.resize(p_len);
		memcpy(pba.ptrw(), p_buffer, p_len);
		r_variants.write[0] = pba;
		return OK;
	}

	for (int i = 0; i < argc; i++) {
		ERR_FAIL_COND_V_MSG(r_len >= p_len, ERR_INVALID_DATA, "Invalid packet received. Size too small.");

		int vlen;
		Error err = MultiplayerAPI::decode_and_decompress_variant(r_variants.write[i], &p_buffer[r_len], p_len - r_len, &vlen, p_allow_object_decoding);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Invalid packet received. Unable to decode state variable.");
		r_len += vlen;
	}
	return OK;
}

Error MultiplayerAPI::_rpc_bind(int p_peer, Object *p_object, const StringName &p_method, Array p_args) {
	Vector<Variant> args;
	Vector<const Variant *> argsp;
	args.resize(p_args.size());
	argsp.resize(p_args.size());
	Variant *ptr = args.ptrw();
	const Variant **pptr = argsp.ptrw();
	for (int i = 0; i < p_args.size(); i++) {
		ptr[i] = p_args[i];
		pptr[i] = &ptr[i];
	}
	return rpcp(p_object, p_peer, p_method, argsp.size() ? argsp.ptrw() : nullptr, argsp.size());
}

void MultiplayerAPI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_multiplayer_peer"), &MultiplayerAPI::has_multiplayer_peer);
	ClassDB::bind_method(D_METHOD("get_multiplayer_peer"), &MultiplayerAPI::get_multiplayer_peer);
	ClassDB::bind_method(D_METHOD("set_multiplayer_peer", "peer"), &MultiplayerAPI::set_multiplayer_peer);
	ClassDB::bind_method(D_METHOD("get_unique_id"), &MultiplayerAPI::get_unique_id);
	ClassDB::bind_method(D_METHOD("is_server"), &MultiplayerAPI::is_server);
	ClassDB::bind_method(D_METHOD("get_remote_sender_id"), &MultiplayerAPI::get_remote_sender_id);
	ClassDB::bind_method(D_METHOD("poll"), &MultiplayerAPI::poll);
	ClassDB::bind_method(D_METHOD("rpc", "peer", "object", "method", "arguments"), &MultiplayerAPI::_rpc_bind, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("object_configuration_add", "object", "configuration"), &MultiplayerAPI::object_configuration_add);
	ClassDB::bind_method(D_METHOD("object_configuration_remove", "object", "configuration"), &MultiplayerAPI::object_configuration_remove);

	ClassDB::bind_method(D_METHOD("get_peers"), &MultiplayerAPI::get_peer_ids);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiplayer_peer", PROPERTY_HINT_RESOURCE_TYPE, "MultiplayerPeer", PROPERTY_USAGE_NONE), "set_multiplayer_peer", "get_multiplayer_peer");

	ClassDB::bind_static_method("MultiplayerAPI", D_METHOD("set_default_interface", "interface_name"), &MultiplayerAPI::set_default_interface);
	ClassDB::bind_static_method("MultiplayerAPI", D_METHOD("get_default_interface"), &MultiplayerAPI::get_default_interface);
	ClassDB::bind_static_method("MultiplayerAPI", D_METHOD("create_default_interface"), &MultiplayerAPI::create_default_interface);

	ADD_SIGNAL(MethodInfo("peer_connected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("peer_disconnected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("connected_to_server"));
	ADD_SIGNAL(MethodInfo("connection_failed"));
	ADD_SIGNAL(MethodInfo("server_disconnected"));

	BIND_ENUM_CONSTANT(RPC_MODE_DISABLED);
	BIND_ENUM_CONSTANT(RPC_MODE_ANY_PEER);
	BIND_ENUM_CONSTANT(RPC_MODE_AUTHORITY);
}

/// MultiplayerAPIExtension

Error MultiplayerAPIExtension::poll() {
	Error err = OK;
	GDVIRTUAL_CALL(_poll, err);
	return err;
}

void MultiplayerAPIExtension::set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) {
	GDVIRTUAL_CALL(_set_multiplayer_peer, p_peer);
}

Ref<MultiplayerPeer> MultiplayerAPIExtension::get_multiplayer_peer() {
	Ref<MultiplayerPeer> peer;
	GDVIRTUAL_CALL(_get_multiplayer_peer, peer);
	return peer;
}

int MultiplayerAPIExtension::get_unique_id() {
	int id = 1;
	GDVIRTUAL_CALL(_get_unique_id, id);
	return id;
}

Vector<int> MultiplayerAPIExtension::get_peer_ids() {
	Vector<int> ids;
	GDVIRTUAL_CALL(_get_peer_ids, ids);
	return ids;
}

Error MultiplayerAPIExtension::rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) {
	if (!GDVIRTUAL_IS_OVERRIDDEN(_rpc)) {
		return ERR_UNAVAILABLE;
	}
	Array args;
	for (int i = 0; i < p_argcount; i++) {
		args.push_back(*p_arg[i]);
	}
	Error ret = FAILED;
	GDVIRTUAL_CALL(_rpc, p_peer_id, p_obj, p_method, args, ret);
	return ret;
}

int MultiplayerAPIExtension::get_remote_sender_id() {
	int id = 0;
	GDVIRTUAL_CALL(_get_remote_sender_id, id);
	return id;
}

Error MultiplayerAPIExtension::object_configuration_add(Object *p_object, Variant p_config) {
	Error err = ERR_UNAVAILABLE;
	GDVIRTUAL_CALL(_object_configuration_add, p_object, p_config, err);
	return err;
}

Error MultiplayerAPIExtension::object_configuration_remove(Object *p_object, Variant p_config) {
	Error err = ERR_UNAVAILABLE;
	GDVIRTUAL_CALL(_object_configuration_remove, p_object, p_config, err);
	return err;
}

void MultiplayerAPIExtension::_bind_methods() {
	GDVIRTUAL_BIND(_poll);
	GDVIRTUAL_BIND(_set_multiplayer_peer, "multiplayer_peer");
	GDVIRTUAL_BIND(_get_multiplayer_peer);
	GDVIRTUAL_BIND(_get_unique_id);
	GDVIRTUAL_BIND(_get_peer_ids);
	GDVIRTUAL_BIND(_rpc, "peer", "object", "method", "args");
	GDVIRTUAL_BIND(_get_remote_sender_id);
	GDVIRTUAL_BIND(_object_configuration_add, "object", "configuration");
	GDVIRTUAL_BIND(_object_configuration_remove, "object", "configuration");
}
