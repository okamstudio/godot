/**************************************************************************/
/*  multiplayer_api.h                                                     */
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

#pragma once

#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_peer.h"

class MultiplayerAPI : public RefCounted {
	GDCLASS(MultiplayerAPI, RefCounted);

private:
	static StringName default_interface;

protected:
	static void _bind_methods();
	Error _rpc_bind(int p_peer, Object *p_obj, const StringName &p_method, Array args = Array());

public:
	enum RPCMode {
		RPC_MODE_DISABLED, // No rpc for this method, calls to this will be blocked (default)
		RPC_MODE_ANY_PEER, // Any peer can call this RPC
		RPC_MODE_AUTHORITY, // Only the node's multiplayer authority (server by default) can call this RPC
	};

	static Ref<MultiplayerAPI> create_default_interface();
	static void set_default_interface(const StringName &p_interface);
	static StringName get_default_interface();

	static Error encode_and_compress_variant(const Variant &p_variant, uint8_t *p_buffer, int &r_len, bool p_allow_object_decoding);
	static Error decode_and_decompress_variant(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_object_decoding);
	static Error encode_and_compress_variants(const Variant **p_variants, int p_count, uint8_t *p_buffer, int &r_len, bool *r_raw = nullptr, bool p_allow_object_decoding = false);
	static Error decode_and_decompress_variants(Vector<Variant> &r_variants, const uint8_t *p_buffer, int p_len, int &r_len, bool p_raw = false, bool p_allow_object_decoding = false);

	virtual Error poll() = 0;
	virtual void set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) = 0;
	virtual Ref<MultiplayerPeer> get_multiplayer_peer() = 0;
	virtual int get_unique_id() = 0;
	virtual Vector<int> get_peer_ids() = 0;

	virtual Error rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) = 0;
	virtual int get_remote_sender_id() = 0;

	virtual Error object_configuration_add(Object *p_object, Variant p_config) = 0;
	virtual Error object_configuration_remove(Object *p_object, Variant p_config) = 0;

	bool has_multiplayer_peer() { return get_multiplayer_peer().is_valid(); }
	bool is_server() { return get_unique_id() == MultiplayerPeer::TARGET_PEER_SERVER; }

	MultiplayerAPI() {}
	virtual ~MultiplayerAPI() {}
};

VARIANT_ENUM_CAST(MultiplayerAPI::RPCMode);

class MultiplayerAPIExtension : public MultiplayerAPI {
	GDCLASS(MultiplayerAPIExtension, MultiplayerAPI);

protected:
	static void _bind_methods();

public:
	virtual Error poll() override;
	virtual void set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) override;
	virtual Ref<MultiplayerPeer> get_multiplayer_peer() override;
	virtual int get_unique_id() override;
	virtual Vector<int> get_peer_ids() override;

	virtual Error rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) override;
	virtual int get_remote_sender_id() override;

	virtual Error object_configuration_add(Object *p_object, Variant p_config) override;
	virtual Error object_configuration_remove(Object *p_object, Variant p_config) override;

	// Extensions
	GDVIRTUAL0R(Error, _poll);
	GDVIRTUAL1(_set_multiplayer_peer, Ref<MultiplayerPeer>);
	GDVIRTUAL0R(Ref<MultiplayerPeer>, _get_multiplayer_peer);
	GDVIRTUAL0RC(int, _get_unique_id);
	GDVIRTUAL0RC(PackedInt32Array, _get_peer_ids);
	GDVIRTUAL4R(Error, _rpc, int, Object *, StringName, Array);
	GDVIRTUAL0RC(int, _get_remote_sender_id);
	GDVIRTUAL2R(Error, _object_configuration_add, Object *, Variant);
	GDVIRTUAL2R(Error, _object_configuration_remove, Object *, Variant);
};
