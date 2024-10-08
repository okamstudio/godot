/**************************************************************************/
/*  test_scene_multiplayer.h                                              */
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

#ifndef TEST_SCENE_MULTIPLAYER_H
#define TEST_SCENE_MULTIPLAYER_H

#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include "../scene_multiplayer.h"

namespace TestSceneMultiplayer {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

TEST_CASE("[Multiplayer][SceneMultiplayer] Defaults") {
	Ref<SceneMultiplayer> scene_multiplayer;
	scene_multiplayer.instantiate();

	REQUIRE(scene_multiplayer->has_multiplayer_peer());
	Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
	REQUIRE_MESSAGE(Object::cast_to<OfflineMultiplayerPeer>(multiplayer_peer.ptr()) != nullptr, "By default it must be an OfflineMultiplayerPeer instance.");
	CHECK_EQ(scene_multiplayer->poll(), Error::OK);
	CHECK_EQ(scene_multiplayer->get_unique_id(), MultiplayerPeer::TARGET_PEER_SERVER);
	CHECK_EQ(scene_multiplayer->get_peer_ids(), Vector<int>());
	CHECK_EQ(scene_multiplayer->get_remote_sender_id(), 0);
	CHECK_EQ(scene_multiplayer->get_root_path(), NodePath());
	CHECK(scene_multiplayer->get_connected_peers().is_empty());
	CHECK_FALSE(scene_multiplayer->is_refusing_new_connections());
	CHECK_FALSE(scene_multiplayer->is_object_decoding_allowed());
	CHECK(scene_multiplayer->is_server_relay_enabled());
	CHECK_EQ(scene_multiplayer->get_max_sync_packet_size(), 1350);
	CHECK_EQ(scene_multiplayer->get_max_delta_packet_size(), 65535);
	CHECK(scene_multiplayer->is_server());
}

TEST_CASE("[Multiplayer][SceneMultiplayer][SceneTree] SceneTree has a OfflineMultiplayerPeer by default") {
	Ref<SceneMultiplayer> scene_multiplayer = SceneTree::get_singleton()->get_multiplayer();
	REQUIRE(scene_multiplayer->has_multiplayer_peer());

	Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
	REQUIRE_MESSAGE(Object::cast_to<OfflineMultiplayerPeer>(multiplayer_peer.ptr()) != nullptr, "By default it must be an OfflineMultiplayerPeer instance.");
}

TEST_CASE("[Multiplayer][SceneMultiplayer][SceneTree] Object configuration add/remove") {
	Ref<SceneMultiplayer> scene_multiplayer;
	scene_multiplayer.instantiate();

	SUBCASE("Returns invalid parameter") {
		CHECK_EQ(scene_multiplayer->object_configuration_add(nullptr, "ImInvalid"), Error::ERR_INVALID_PARAMETER);
		CHECK_EQ(scene_multiplayer->object_configuration_remove(nullptr, "ImInvalid"), Error::ERR_INVALID_PARAMETER);

		NodePath foo_path("/Foo");
		NodePath bar_path("/Bar");
		CHECK_EQ(scene_multiplayer->object_configuration_add(nullptr, foo_path), Error::OK);
		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->object_configuration_remove(nullptr, bar_path), Error::ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
	}

	SUBCASE("Sets root path") {
		NodePath foo_path("/Foo");
		CHECK_EQ(scene_multiplayer->object_configuration_add(nullptr, foo_path), Error::OK);

		CHECK_EQ(scene_multiplayer->get_root_path(), foo_path);
	}

	SUBCASE("Unsets root path") {
		NodePath foo_path("/Foo");
		CHECK_EQ(scene_multiplayer->object_configuration_add(nullptr, foo_path), Error::OK);

		CHECK_EQ(scene_multiplayer->object_configuration_remove(nullptr, foo_path), Error::OK);
		CHECK_EQ(scene_multiplayer->get_root_path(), NodePath());
	}

	SUBCASE("Add/Remove a MultiplayerSpawner") {
		Node2D *node = memnew(Node2D);
		MultiplayerSpawner *spawner = memnew(MultiplayerSpawner);

		CHECK_EQ(scene_multiplayer->object_configuration_add(node, spawner), Error::OK);
		CHECK_EQ(scene_multiplayer->object_configuration_remove(node, spawner), Error::OK);

		memdelete(spawner);
		memdelete(node);
	}

	SUBCASE("Add/Remove a MultiplayerSynchronizer") {
		Node2D *node = memnew(Node2D);
		MultiplayerSynchronizer *synchronizer = memnew(MultiplayerSynchronizer);

		CHECK_EQ(scene_multiplayer->object_configuration_add(node, synchronizer), Error::OK);
		CHECK_EQ(scene_multiplayer->object_configuration_remove(node, synchronizer), Error::OK);

		memdelete(synchronizer);
		memdelete(node);
	}
}

TEST_CASE("[Multiplayer][SceneMultiplayer] Root Path") {
	Ref<SceneMultiplayer> scene_multiplayer;
	scene_multiplayer.instantiate();

	SUBCASE("Is set") {
		NodePath foo_path("/Foo");
		scene_multiplayer->set_root_path(foo_path);

		CHECK_EQ(scene_multiplayer->get_root_path(), foo_path);
	}

	SUBCASE("Fails when path is empty") {
		ERR_PRINT_OFF;
		scene_multiplayer->set_root_path(NodePath());
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when path is relative") {
		NodePath foo_path("Foo");
		ERR_PRINT_OFF;
		scene_multiplayer->set_root_path(foo_path);
		ERR_PRINT_ON;

		CHECK_EQ(scene_multiplayer->get_root_path(), NodePath());
	}
}

// This one could be a dummy callback because the current set of test is not actually testing the full auth flow.
static Variant auth_callback(Variant sv, Variant pvav) {
	return Variant();
}

TEST_CASE("[Multiplayer][SceneMultiplayer][SceneTree] Send Authentication") {
	Ref<SceneMultiplayer> scene_multiplayer;
	scene_multiplayer.instantiate();
	SceneTree::get_singleton()->set_multiplayer(scene_multiplayer);
	scene_multiplayer->set_auth_callback(callable_mp_static(auth_callback));

	SUBCASE("Is properly sent") {
		SIGNAL_WATCH(scene_multiplayer.ptr(), "peer_authenticating");

		// Adding a peer to MultiplayerPeer.
		Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
		int peer_id = 42;
		multiplayer_peer->emit_signal(SNAME("peer_connected"), peer_id);
		SIGNAL_CHECK("peer_authenticating", build_array(build_array(peer_id)));

		CHECK_EQ(scene_multiplayer->send_auth(peer_id, String("It's me").to_ascii_buffer()), Error::OK);

		Vector<int> expected_peer_ids = { peer_id };
		CHECK_EQ(scene_multiplayer->get_authenticating_peer_ids(), expected_peer_ids);

		SIGNAL_UNWATCH(scene_multiplayer.ptr(), "peer_authenticating");
	}

	SUBCASE("peer_authentication_failed is emitted when a peer is deleted before authentication is completed") {
		SIGNAL_WATCH(scene_multiplayer.ptr(), "peer_authentication_failed");

		// Adding a peer to MultiplayerPeer.
		Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
		int peer_id = 42;
		multiplayer_peer->emit_signal(SNAME("peer_connected"), peer_id);
		multiplayer_peer->emit_signal(SNAME("peer_disconnected"), peer_id);
		SIGNAL_CHECK("peer_authentication_failed", build_array(build_array(peer_id)));

		SIGNAL_UNWATCH(scene_multiplayer.ptr(), "peer_authentication_failed");
	}

	SUBCASE("peer_authentication_failed is emitted when authentication timeout") {
		SIGNAL_WATCH(scene_multiplayer.ptr(), "peer_authentication_failed");
		scene_multiplayer->set_auth_timeout(0.01);
		CHECK_EQ(scene_multiplayer->get_auth_timeout(), 0.01);

		// Adding two peesr to MultiplayerPeer.
		Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
		int first_peer_id = 42;
		int second_peer_id = 84;
		multiplayer_peer->emit_signal(SNAME("peer_connected"), first_peer_id);
		multiplayer_peer->emit_signal(SNAME("peer_connected"), second_peer_id);

		// Let timeout happens.
		OS::get_singleton()->delay_usec(500000);

		CHECK_EQ(scene_multiplayer->poll(), Error::OK);

		SIGNAL_CHECK("peer_authentication_failed", build_array(build_array(first_peer_id), build_array(second_peer_id)));

		SIGNAL_UNWATCH(scene_multiplayer.ptr(), "peer_authentication_failed");
	}

	SUBCASE("Fails when there is no MultiplayerPeer configured") {
		scene_multiplayer->set_multiplayer_peer(nullptr);

		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->send_auth(42, Vector<uint8_t>()), Error::ERR_UNCONFIGURED);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when the peer to send the auth is not pending") {
		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->send_auth(42, String("It's me").to_ascii_buffer()), Error::ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
	}
}

TEST_CASE("[Multiplayer][SceneMultiplayer][SceneTree] Complete Authentication") {
	Ref<SceneMultiplayer> scene_multiplayer;
	scene_multiplayer.instantiate();
	SceneTree::get_singleton()->set_multiplayer(scene_multiplayer);
	scene_multiplayer->set_auth_callback(callable_mp_static(auth_callback));

	SUBCASE("Is properly completed") {
		Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
		int peer_id = 42;
		multiplayer_peer->emit_signal(SNAME("peer_connected"), peer_id);
		CHECK_EQ(scene_multiplayer->send_auth(peer_id, String("It's me").to_ascii_buffer()), Error::OK);

		CHECK_EQ(scene_multiplayer->complete_auth(peer_id), Error::OK);
	}

	SUBCASE("Fails when there is no MultiplayerPeer configured") {
		scene_multiplayer->set_multiplayer_peer(nullptr);

		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->complete_auth(42), Error::ERR_UNCONFIGURED);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when the peer to complete the auth is not pending") {
		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->complete_auth(42), Error::ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails to send auth or completed for a second time") {
		Ref<MultiplayerPeer> multiplayer_peer = scene_multiplayer->get_multiplayer_peer();
		int peer_id = 42;
		multiplayer_peer->emit_signal(SNAME("peer_connected"), peer_id);
		CHECK_EQ(scene_multiplayer->send_auth(peer_id, String("It's me").to_ascii_buffer()), Error::OK);
		CHECK_EQ(scene_multiplayer->complete_auth(peer_id), Error::OK);

		ERR_PRINT_OFF;
		CHECK_EQ(scene_multiplayer->send_auth(peer_id, String("It's me").to_ascii_buffer()), Error::ERR_FILE_CANT_WRITE);
		CHECK_EQ(scene_multiplayer->complete_auth(peer_id), Error::ERR_FILE_CANT_WRITE);
		ERR_PRINT_ON;
	}
}

} // namespace TestSceneMultiplayer

#endif // TEST_SCENE_MULTIPLAYER_H
