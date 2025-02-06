/**************************************************************************/
/*  test_multiplayer_spawner.h                                            */
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

#ifndef TEST_MULTIPLAYER_SPAWNER_H
#define TEST_MULTIPLAYER_SPAWNER_H

#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include "../multiplayer_spawner.h"

namespace TestMultiplayerSpawner {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

class Wasp : public Node {
	GDCLASS(Wasp, Node);

	int _size = 0;

public:
	int get_size() const {
		return _size;
	}
	void set_size(int p_size) {
		_size = p_size;
	}

	Wasp() {
		set_name("Wasp");
		set_scene_file_path("wasp.tscn");
	}
};

class SpawnWasps : public Object {
	GDCLASS(SpawnWasps, Object);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("wasp", "size"), &SpawnWasps::create_wasps);
		{
			MethodInfo mi;
			mi.name = "wasp_error";
			mi.arguments.push_back(PropertyInfo(Variant::INT, "size"));

			ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "wasp_error", &SpawnWasps::create_wasps_error, mi, varray(), false);
		}
		ClassDB::bind_method(D_METHOD("echo", "size"), &SpawnWasps::echo_size);
	}

public:
	Wasp *create_wasps(int p_size) {
		Wasp *wasp = memnew(Wasp);
		wasp->set_size(p_size);
		return wasp;
	}

	Wasp *create_wasps_error(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		return nullptr;
	}

	int echo_size(int p_size) {
		return p_size;
	}
};

TEST_CASE("[Multiplayer][MultiplayerSpawner] Defaults") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);

	CHECK_EQ(multiplayer_spawner->get_configuration_warnings().size(), 1);
	CHECK_EQ(multiplayer_spawner->get_spawn_node(), nullptr);
	CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 0);
	CHECK_EQ(multiplayer_spawner->get_spawn_path(), NodePath());
	CHECK_EQ(multiplayer_spawner->get_spawn_limit(), 0);
	CHECK_EQ(multiplayer_spawner->get_spawn_function(), Callable());

	memdelete(multiplayer_spawner);
}

TEST_CASE("[Multiplayer][MultiplayerSpawner][SceneTree] Spawn Path warning") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);
	SceneTree::get_singleton()->get_root()->add_child(multiplayer_spawner);

	// If there is no spawn path, there should be a warning.
	PackedStringArray warning_messages = multiplayer_spawner->get_configuration_warnings();
	REQUIRE_EQ(warning_messages.size(), 1);
	CHECK_MESSAGE(warning_messages[0].contains("\"Spawn Path\""), "Invalid configuration warning");

	// If there is a spawn path, but it doesn't exist a node on it, there should be a warning.
	multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));
	warning_messages = multiplayer_spawner->get_configuration_warnings();
	REQUIRE_EQ(warning_messages.size(), 1);
	CHECK_MESSAGE(warning_messages[0].contains("\"Spawn Path\""), "Invalid configuration warning");

	// If there is a spawn path and a node on it, shouldn't be a warning.
	Node *foo = memnew(Node);
	foo->set_name("Foo");
	SceneTree::get_singleton()->get_root()->add_child(foo);
	warning_messages = multiplayer_spawner->get_configuration_warnings();
	CHECK_EQ(warning_messages.size(), 0);

	memdelete(foo);
	memdelete(multiplayer_spawner);
}

TEST_CASE("[Multiplayer][MultiplayerSpawner][SceneTree] Spawn node") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);
	SceneTree::get_singleton()->get_root()->add_child(multiplayer_spawner);
	CHECK_EQ(multiplayer_spawner->get_spawn_node(), nullptr);

	Node *foo = memnew(Node);
	foo->set_name("Foo");
	SceneTree::get_singleton()->get_root()->add_child(foo);

	SUBCASE("nullptr if spawn path doesn't exists") {
		multiplayer_spawner->set_spawn_path(NodePath("/root/NotExists"));

		CHECK_EQ(multiplayer_spawner->get_spawn_node(), nullptr);
	}

	SUBCASE("Get it after setting spawn path with no signal connections") {
		multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));

		CHECK_EQ(multiplayer_spawner->get_spawn_node(), foo);
		CHECK_FALSE(foo->has_connections("child_entered_tree"));
	}

	SUBCASE("Get it after setting spawn path with signal connections") {
		multiplayer_spawner->add_spawnable_scene("scene.tscn");
		multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));

		CHECK_EQ(multiplayer_spawner->get_spawn_node(), foo);
		CHECK(foo->has_connections("child_entered_tree"));
	}

	SUBCASE("Set a new one should disconnect signals from the old one") {
		multiplayer_spawner->add_spawnable_scene("scene.tscn");
		multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));

		CHECK_EQ(multiplayer_spawner->get_spawn_node(), foo);
		CHECK(foo->has_connections("child_entered_tree"));

		Node *bar = memnew(Node);
		bar->set_name("Bar");
		SceneTree::get_singleton()->get_root()->add_child(bar);
		multiplayer_spawner->set_spawn_path(NodePath("/root/Bar"));

		CHECK_EQ(multiplayer_spawner->get_spawn_node(), bar);
		CHECK(bar->has_connections("child_entered_tree"));
		CHECK_FALSE(foo->has_connections("child_entered_tree"));

		memdelete(bar);
	}

	memdelete(foo);
	memdelete(multiplayer_spawner);
}

TEST_CASE("[Multiplayer][MultiplayerSpawner][SceneTree] Spawnable scene") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);
	SceneTree::get_singleton()->get_root()->add_child(multiplayer_spawner);
	CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 0);

	SUBCASE("Add one") {
		multiplayer_spawner->add_spawnable_scene("scene.tscn");

		CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 1);
		CHECK_EQ(multiplayer_spawner->get_spawnable_scene(0), "scene.tscn");
	}

	SUBCASE("Add one and if there is a valid spawn path add a connection to it") {
		Node *foo = memnew(Node);
		foo->set_name("Foo");
		multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));
		CHECK_FALSE(foo->has_connections("child_entered_tree"));

		// Adding now foo to the tree to avoid set_spawn_path() making the connection.
		SceneTree::get_singleton()->get_root()->add_child(foo);
		multiplayer_spawner->notification(Node::NOTIFICATION_POST_ENTER_TREE);
		CHECK_FALSE(foo->has_connections("child_entered_tree"));
		multiplayer_spawner->add_spawnable_scene("scene.tscn");
		CHECK(foo->has_connections("child_entered_tree"));

		memdelete(foo);
	}

	SUBCASE("Add multiple") {
		multiplayer_spawner->add_spawnable_scene("scene.tscn");
		multiplayer_spawner->add_spawnable_scene("other_scene.tscn");
		multiplayer_spawner->add_spawnable_scene("yet_another_scene.tscn");

		CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 3);
		CHECK_EQ(multiplayer_spawner->get_spawnable_scene(0), "scene.tscn");
		CHECK_EQ(multiplayer_spawner->get_spawnable_scene(1), "other_scene.tscn");
		CHECK_EQ(multiplayer_spawner->get_spawnable_scene(2), "yet_another_scene.tscn");
	}

	SUBCASE("Clear") {
		Node *foo = memnew(Node);
		foo->set_name("Foo");
		SceneTree::get_singleton()->get_root()->add_child(foo);
		multiplayer_spawner->set_spawn_path(NodePath("/root/Foo"));

		multiplayer_spawner->add_spawnable_scene("scene.tscn");
		multiplayer_spawner->add_spawnable_scene("other_scene.tscn");
		multiplayer_spawner->add_spawnable_scene("yet_another_scene.tscn");
		CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 3);
		CHECK(foo->has_connections("child_entered_tree"));

		multiplayer_spawner->clear_spawnable_scenes();

		CHECK_EQ(multiplayer_spawner->get_spawnable_scene_count(), 0);
		CHECK_FALSE(foo->has_connections("child_entered_tree"));
	}

	memdelete(multiplayer_spawner);
}

TEST_CASE("[Multiplayer][MultiplayerSpawner][SceneTree] Instantiate custom") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);
	SceneTree::get_singleton()->get_root()->add_child(multiplayer_spawner);
	CHECK_EQ(multiplayer_spawner->get_spawn_node(), nullptr);

	Node *nest = memnew(Node);
	nest->set_name("Nest");
	SceneTree::get_singleton()->get_root()->add_child(nest);
	multiplayer_spawner->set_spawn_path(NodePath("/root/Nest"));

	SpawnWasps *spawn_wasps = memnew(SpawnWasps);

	SUBCASE("Instantiates a node properly") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");

		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));
		Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->instantiate_custom(Variant(42)));
		CHECK_NE(wasp, nullptr);
		CHECK_EQ(wasp->get_name(), "Wasp");
		CHECK_EQ(wasp->get_size(), 42);

		memdelete(wasp);
	}

	SUBCASE("Instantiates multiple nodes properly if there is no spawn limit") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));

		for (int i = 0; i < 10; i++) {
			Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->instantiate_custom(Variant(i)));
			CHECK_NE(wasp, nullptr);
			CHECK_EQ(wasp->get_name(), "Wasp");
			CHECK_EQ(wasp->get_size(), i);
			nest->add_child(wasp, true);
		}
	}

	SUBCASE("Fails if spawn limit is reached") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");

		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));

		// This one works.
		Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->instantiate_custom(Variant(42)));
		CHECK_NE(wasp, nullptr);
		CHECK_EQ(wasp->get_name(), "Wasp");
		CHECK_EQ(wasp->get_size(), 42);
		// Adding to the spawner node to get it tracked.
		nest->add_child(wasp);

		// This one fails because spawn limit is reached.
		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->instantiate_custom(Variant(255)), nullptr);
		ERR_PRINT_ON;

		memdelete(wasp);
	}

	SUBCASE("Fails if spawn function is not set") {
		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->instantiate_custom(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when spawn function fails") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");

		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp_error"));

		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->instantiate_custom(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when spawn function not returns a node") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");

		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "echo"));

		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->instantiate_custom(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	memdelete(spawn_wasps);
	memdelete(nest);
	memdelete(multiplayer_spawner);
}

TEST_CASE("[Multiplayer][MultiplayerSpawner][SceneTree] Spawn") {
	MultiplayerSpawner *multiplayer_spawner = memnew(MultiplayerSpawner);

	SUBCASE("Fails because is not inside tree") {
		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->spawn(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	SceneTree::get_singleton()->get_root()->add_child(multiplayer_spawner);
	CHECK_EQ(multiplayer_spawner->get_spawn_node(), nullptr);

	Node *nest = memnew(Node);
	nest->set_name("Nest");
	SceneTree::get_singleton()->get_root()->add_child(nest);
	multiplayer_spawner->set_spawn_path(NodePath("/root/Nest"));

	SpawnWasps *spawn_wasps = memnew(SpawnWasps);
	multiplayer_spawner->add_spawnable_scene("wasp.tscn");

	SUBCASE("Spawns a node, track it and add it to spawn node") {
		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));
		Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->spawn(Variant(42)));
		CHECK_NE(wasp, nullptr);
		CHECK_EQ(wasp->get_name(), "Wasp");
		CHECK_EQ(wasp->get_size(), 42);
		CHECK_EQ(wasp->get_parent(), nest);
		CHECK_EQ(nest->get_child_count(), 1);
		CHECK_EQ(nest->get_child(0), wasp);
	}

	SUBCASE("Spawns multiple nodes properly if there is no spawn limit") {
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));

		for (int i = 0; i < 10; i++) {
			Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->spawn(Variant(i)));
			CHECK_NE(wasp, nullptr);
			CHECK_EQ(wasp->get_name(), "Wasp" + String((i == 0) ? "" : itos(i + 1)));
			CHECK_EQ(wasp->get_size(), i);
			CHECK_EQ(wasp->get_parent(), nest);
			CHECK_EQ(nest->get_child_count(), i + 1);
			CHECK_EQ(nest->get_child(i), wasp);
		}
	}

	SUBCASE("Fails if spawn limit is reached") {
		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));

		// This one works.
		Wasp *wasp = Object::cast_to<Wasp>(multiplayer_spawner->spawn(Variant(42)));
		CHECK_NE(wasp, nullptr);
		CHECK_EQ(wasp->get_name(), "Wasp");
		CHECK_EQ(wasp->get_size(), 42);
		CHECK_EQ(wasp->get_parent(), nest);
		CHECK_EQ(nest->get_child_count(), 1);
		CHECK_EQ(nest->get_child(0), wasp);

		// This one fails because spawn limit is reached.
		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->spawn(Variant(255)), nullptr);
		ERR_PRINT_ON;

		memdelete(wasp);
	}

	SUBCASE("Fails if spawn function is not set") {
		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->spawn(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails if spawn node cannot be found") {
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "wasp"));
		multiplayer_spawner->set_spawn_path(NodePath(""));

		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->spawn(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	SUBCASE("Fails when instantiate_custom not returns a node") {
		multiplayer_spawner->add_spawnable_scene("wasp.tscn");

		multiplayer_spawner->set_spawn_limit(1);
		multiplayer_spawner->set_spawn_function(Callable(spawn_wasps, "echo"));

		ERR_PRINT_OFF;
		CHECK_EQ(multiplayer_spawner->spawn(Variant(42)), nullptr);
		ERR_PRINT_ON;
	}

	memdelete(spawn_wasps);
	memdelete(nest);
	memdelete(multiplayer_spawner);
}

} // namespace TestMultiplayerSpawner

#endif // TEST_MULTIPLAYER_SPAWNER_H
