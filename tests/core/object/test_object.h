/**************************************************************************/
/*  test_object.h                                                         */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/script_language.h"

#include "tests/test_macros.h"

#ifdef SANITIZERS_ENABLED
#ifdef __has_feature
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define ASAN_OR_TSAN_ENABLED
#endif
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define ASAN_OR_TSAN_ENABLED
#endif
#endif

// Declared in global namespace because of GDCLASS macro warning (Windows):
// "Unqualified friend declaration referring to type outside of the nearest enclosing namespace
// is a Microsoft extension; add a nested name specifier".
class _TestDerivedObject : public Object {
	GDCLASS(_TestDerivedObject, Object);

	int property_value;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_property", "property"), &_TestDerivedObject::set_property);
		ClassDB::bind_method(D_METHOD("get_property"), &_TestDerivedObject::get_property);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "property"), "set_property", "get_property");
	}

public:
	void set_property(int value) { property_value = value; }
	int get_property() const { return property_value; }
};

namespace TestObject {

class _MockScriptInstance : public ScriptInstance {
	StringName property_name = "NO_NAME";
	Variant property_value;

public:
	bool set(const StringName &p_name, const Variant &p_value) override {
		property_name = p_name;
		property_value = p_value;
		return true;
	}
	bool get(const StringName &p_name, Variant &r_ret) const override {
		if (property_name == p_name) {
			r_ret = property_value;
			return true;
		}
		return false;
	}
	void get_property_list(List<PropertyInfo> *p_properties) const override {
	}
	Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid) const override {
		return Variant::PACKED_FLOAT32_ARRAY;
	}
	virtual void validate_property(PropertyInfo &p_property) const override {
	}
	bool property_can_revert(const StringName &p_name) const override {
		return false;
	}
	bool property_get_revert(const StringName &p_name, Variant &r_ret) const override {
		return false;
	}
	void get_method_list(List<MethodInfo> *p_list) const override {
	}
	bool has_method(const StringName &p_method) const override {
		return false;
	}
	int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
		return Variant();
	}
	void notification(int p_notification, bool p_reversed = false) override {
	}
	Ref<Script> get_script() const override {
		return Ref<Script>();
	}
	const Variant get_rpc_config() const override {
		return Variant();
	}
	ScriptLanguage *get_language() override {
		return nullptr;
	}
};

TEST_CASE("[Object] Core getters") {
	Object object;

	CHECK_MESSAGE(
			object.is_class("Object"),
			"is_class() should return the expected value.");
	CHECK_MESSAGE(
			object.get_class() == "Object",
			"The returned class should match the expected value.");
	CHECK_MESSAGE(
			object.get_class_name() == "Object",
			"The returned class name should match the expected value.");
	CHECK_MESSAGE(
			object.get_class_static() == "Object",
			"The returned static class should match the expected value.");
	CHECK_MESSAGE(
			object.get_save_class() == "Object",
			"The returned save class should match the expected value.");

	List<String> inheritance_list;
	object.get_inheritance_list_static(&inheritance_list);
	CHECK_MESSAGE(
			inheritance_list.size() == 1,
			"The inheritance list should consist of Object only");
	CHECK_MESSAGE(
			inheritance_list.front()->get() == "Object",
			"The inheritance list should consist of Object only");
}

TEST_CASE("[Object] Metadata") {
	const String meta_path = "complex_metadata_path";
	Object object;

	object.set_meta(meta_path, Color(0, 1, 0));
	CHECK_MESSAGE(
			Color(object.get_meta(meta_path)).is_equal_approx(Color(0, 1, 0)),
			"The returned object metadata after setting should match the expected value.");

	List<StringName> meta_list;
	object.get_meta_list(&meta_list);
	CHECK_MESSAGE(
			meta_list.size() == 1,
			"The metadata list should only contain 1 item after adding one metadata item.");

	object.remove_meta(meta_path);
	// Also try removing nonexistent metadata (it should do nothing, without printing an error message).
	object.remove_meta("I don't exist");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			object.get_meta(meta_path) == Variant(),
			"The returned object metadata after removing should match the expected value.");
	ERR_PRINT_ON;

	List<StringName> meta_list2;
	object.get_meta_list(&meta_list2);
	CHECK_MESSAGE(
			meta_list2.size() == 0,
			"The metadata list should contain 0 items after removing all metadata items.");

	Object other;
	object.set_meta("conflicting_meta", "string");
	object.set_meta("not_conflicting_meta", 123);
	other.set_meta("conflicting_meta", Color(0, 1, 0));
	other.set_meta("other_meta", "other");
	object.merge_meta_from(&other);

	CHECK_MESSAGE(
			Color(object.get_meta("conflicting_meta")).is_equal_approx(Color(0, 1, 0)),
			"String meta should be overwritten with Color after merging.");

	CHECK_MESSAGE(
			int(object.get_meta("not_conflicting_meta")) == 123,
			"Not conflicting meta on destination should be kept intact.");

	CHECK_MESSAGE(
			object.get_meta("other_meta", String()) == "other",
			"Not conflicting meta name on source should merged.");

	List<StringName> meta_list3;
	object.get_meta_list(&meta_list3);
	CHECK_MESSAGE(
			meta_list3.size() == 3,
			"The metadata list should contain 3 items after merging meta from two objects.");
}

TEST_CASE("[Object] Construction") {
	Object object;

	CHECK_MESSAGE(
			!object.is_ref_counted(),
			"Object is not a RefCounted.");

	Object *p_db = ObjectDB::get_instance(object.get_instance_id());
	CHECK_MESSAGE(
			p_db == &object,
			"The database pointer returned by the object id should reference same object.");
}

TEST_CASE("[Object] Script instance property setter") {
	Object object;
	_MockScriptInstance *script_instance = memnew(_MockScriptInstance);
	object.set_script_instance(script_instance);

	bool valid = false;
	object.set("some_name", 100, &valid);
	CHECK(valid);
	Variant actual_value;
	CHECK_MESSAGE(
			script_instance->get("some_name", actual_value),
			"The assigned script instance should successfully retrieve value by name.");
	CHECK_MESSAGE(
			actual_value == Variant(100),
			"The returned value should equal the one which was set by the object.");
}

TEST_CASE("[Object] Script instance property getter") {
	Object object;
	_MockScriptInstance *script_instance = memnew(_MockScriptInstance);
	script_instance->set("some_name", 100); // Make sure script instance has the property
	object.set_script_instance(script_instance);

	bool valid = false;
	const Variant &actual_value = object.get("some_name", &valid);
	CHECK(valid);
	CHECK_MESSAGE(
			actual_value == Variant(100),
			"The returned value should equal the one which was set by the script instance.");
}

TEST_CASE("[Object] Built-in property setter") {
	GDREGISTER_CLASS(_TestDerivedObject);
	_TestDerivedObject derived_object;

	bool valid = false;
	derived_object.set("property", 100, &valid);
	CHECK(valid);
	CHECK_MESSAGE(
			derived_object.get_property() == 100,
			"The property value should equal the one which was set with built-in setter.");
}

TEST_CASE("[Object] Built-in property getter") {
	GDREGISTER_CLASS(_TestDerivedObject);
	_TestDerivedObject derived_object;
	derived_object.set_property(100);

	bool valid = false;
	const Variant &actual_value = derived_object.get("property", &valid);
	CHECK(valid);
	CHECK_MESSAGE(
			actual_value == Variant(100),
			"The returned value should equal the one which was set with built-in setter.");
}

TEST_CASE("[Object] Script property setter") {
	Object object;
	Variant script;

	bool valid = false;
	object.set(CoreStringName(script), script, &valid);
	CHECK(valid);
	CHECK_MESSAGE(
			object.get_script() == script,
			"The object script should be equal to the assigned one.");
}

TEST_CASE("[Object] Script property getter") {
	Object object;
	Variant script;
	object.set_script(script);

	bool valid = false;
	const Variant &actual_value = object.get(CoreStringName(script), &valid);
	CHECK(valid);
	CHECK_MESSAGE(
			actual_value == script,
			"The returned value should be equal to the assigned script.");
}

TEST_CASE("[Object] Absent name setter") {
	Object object;

	bool valid = true;
	object.set("absent_name", 100, &valid);
	CHECK(!valid);
}

TEST_CASE("[Object] Absent name getter") {
	Object object;

	bool valid = true;
	const Variant &actual_value = object.get("absent_name", &valid);
	CHECK(!valid);
	CHECK_MESSAGE(
			actual_value == Variant(),
			"The returned value should equal nil variant.");
}

TEST_CASE("[Object] Signals") {
	Object object;

	CHECK_FALSE(object.has_signal("my_custom_signal"));

	List<MethodInfo> signals_before;
	object.get_signal_list(&signals_before);

	object.add_user_signal(MethodInfo("my_custom_signal"));

	CHECK(object.has_signal("my_custom_signal"));

	List<MethodInfo> signals_after;
	object.get_signal_list(&signals_after);

	// Should be one more signal.
	CHECK_EQ(signals_before.size() + 1, signals_after.size());

	SUBCASE("Adding the same user signal again should not have any effect") {
		CHECK(object.has_signal("my_custom_signal"));
		ERR_PRINT_OFF;
		object.add_user_signal(MethodInfo("my_custom_signal"));
		ERR_PRINT_ON;
		CHECK(object.has_signal("my_custom_signal"));

		List<MethodInfo> signals_after_existing_added;
		object.get_signal_list(&signals_after_existing_added);

		CHECK_EQ(signals_after.size(), signals_after_existing_added.size());
	}

	SUBCASE("Trying to add a preexisting signal should not have any effect") {
		CHECK(object.has_signal("script_changed"));
		ERR_PRINT_OFF;
		object.add_user_signal(MethodInfo("script_changed"));
		ERR_PRINT_ON;
		CHECK(object.has_signal("script_changed"));

		List<MethodInfo> signals_after_existing_added;
		object.get_signal_list(&signals_after_existing_added);

		CHECK_EQ(signals_after.size(), signals_after_existing_added.size());
	}

	SUBCASE("Adding an empty signal should not have any effect") {
		CHECK_FALSE(object.has_signal(""));
		ERR_PRINT_OFF;
		object.add_user_signal(MethodInfo(""));
		ERR_PRINT_ON;
		CHECK_FALSE(object.has_signal(""));

		List<MethodInfo> signals_after_empty_added;
		object.get_signal_list(&signals_after_empty_added);

		CHECK_EQ(signals_after.size(), signals_after_empty_added.size());
	}

	SUBCASE("Deleting an object with connected signals should disconnect them") {
		List<Object::Connection> signal_connections;

		{
			Object target;
			target.add_user_signal(MethodInfo("my_custom_signal"));

			ERR_PRINT_OFF;
			target.connect("nonexistent_signal1", callable_mp(&object, &Object::notify_property_list_changed));
			target.connect("my_custom_signal", callable_mp(&object, &Object::notify_property_list_changed));
			target.connect("nonexistent_signal2", callable_mp(&object, &Object::notify_property_list_changed));
			ERR_PRINT_ON;

			signal_connections.clear();
			object.get_all_signal_connections(&signal_connections);
			CHECK(signal_connections.size() == 0);
			signal_connections.clear();
			object.get_signals_connected_to_this(&signal_connections);
			CHECK(signal_connections.size() == 1);

			ERR_PRINT_OFF;
			object.connect("nonexistent_signal1", callable_mp(&target, &Object::notify_property_list_changed));
			object.connect("my_custom_signal", callable_mp(&target, &Object::notify_property_list_changed));
			object.connect("nonexistent_signal2", callable_mp(&target, &Object::notify_property_list_changed));
			ERR_PRINT_ON;

			signal_connections.clear();
			object.get_all_signal_connections(&signal_connections);
			CHECK(signal_connections.size() == 1);
			signal_connections.clear();
			object.get_signals_connected_to_this(&signal_connections);
			CHECK(signal_connections.size() == 1);
		}

		signal_connections.clear();
		object.get_all_signal_connections(&signal_connections);
		CHECK(signal_connections.size() == 0);
		signal_connections.clear();
		object.get_signals_connected_to_this(&signal_connections);
		CHECK(signal_connections.size() == 0);
	}

	SUBCASE("Emitting a non existing signal will return an error") {
		Error err = object.emit_signal("some_signal");
		CHECK(err == ERR_UNAVAILABLE);
	}

	SUBCASE("Emitting an existing signal should call the connected method") {
		Array empty_signal_args;
		empty_signal_args.push_back(Array());

		SIGNAL_WATCH(&object, "my_custom_signal");
		SIGNAL_CHECK_FALSE("my_custom_signal");

		Error err = object.emit_signal("my_custom_signal");
		CHECK(err == OK);

		SIGNAL_CHECK("my_custom_signal", empty_signal_args);
		SIGNAL_UNWATCH(&object, "my_custom_signal");
	}

	SUBCASE("Connecting and then disconnecting many signals should not leave anything behind") {
		List<Object::Connection> signal_connections;
		Object targets[100];

		for (int i = 0; i < 10; i++) {
			ERR_PRINT_OFF;
			for (Object &target : targets) {
				object.connect("my_custom_signal", callable_mp(&target, &Object::notify_property_list_changed));
			}
			ERR_PRINT_ON;
			signal_connections.clear();
			object.get_all_signal_connections(&signal_connections);
			CHECK(signal_connections.size() == 100);
		}

		for (Object &target : targets) {
			object.disconnect("my_custom_signal", callable_mp(&target, &Object::notify_property_list_changed));
		}
		signal_connections.clear();
		object.get_all_signal_connections(&signal_connections);
		CHECK(signal_connections.size() == 0);
	}
}

class NotificationObject1 : public Object {
	GDCLASS(NotificationObject1, Object);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case 12345: {
				order_internal1 = order_global++;
			} break;
		}
	}

public:
	static int order_global;
	int order_internal1 = -1;

	void reset_order() {
		order_internal1 = -1;
		order_global = 1;
	}
};

int NotificationObject1::order_global = 1;

class NotificationObject2 : public NotificationObject1 {
	GDCLASS(NotificationObject2, NotificationObject1);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case 12345: {
				order_internal2 = order_global++;
			} break;
		}
	}

public:
	int order_internal2 = -1;
	void reset_order() {
		NotificationObject1::reset_order();
		order_internal2 = -1;
	}
};

TEST_CASE("[Object] Notification order") { // GH-52325
	NotificationObject2 *test_notification_object = memnew(NotificationObject2);

	SUBCASE("regular order") {
		test_notification_object->notification(12345, false);

		CHECK_EQ(test_notification_object->order_internal1, 1);
		CHECK_EQ(test_notification_object->order_internal2, 2);

		test_notification_object->reset_order();
	}

	SUBCASE("reverse order") {
		test_notification_object->notification(12345, true);

		CHECK_EQ(test_notification_object->order_internal1, 2);
		CHECK_EQ(test_notification_object->order_internal2, 1);

		test_notification_object->reset_order();
	}

	memdelete(test_notification_object);
}

TEST_CASE("[Object] Destruction at the end of the call chain is safe") {
	Object *object = memnew(Object);
	ObjectID obj_id = object->get_instance_id();

	class _SelfDestroyingScriptInstance : public _MockScriptInstance {
		Object *self = nullptr;

		// This has to be static because ~Object() also destroys the script instance.
		static void free_self(Object *p_self) {
#if defined(ASAN_OR_TSAN_ENABLED)
			// Regular deletion is enough becausa asan/tsan will catch a potential heap-after-use.
			memdelete(p_self);
#else
			// Without asan/tsan, try at least to force a crash by replacing the otherwise seemingly good data with garbage.
			// Operations such as dereferencing pointers or decreasing a refcount would fail.
			// Unfortunately, we may not poison the memory after the deletion, because the memory would no longer belong to us
			// and on doing so we may cause a more generalized crash on some platforms (allocator implementations).
			p_self->~Object();
			memset((void *)p_self, 0, sizeof(Object));
			Memory::free_static(p_self, false);
#endif
		}

	public:
		Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
			free_self(self);
			return Variant();
		}
		Variant call_const(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
			free_self(self);
			return Variant();
		}
		bool has_method(const StringName &p_method) const override {
			return p_method == "some_method";
		}

	public:
		_SelfDestroyingScriptInstance(Object *p_self) :
				self(p_self) {}
	};

	_SelfDestroyingScriptInstance *script_instance = memnew(_SelfDestroyingScriptInstance(object));
	object->set_script_instance(script_instance);

	SUBCASE("Within callp()") {
		SUBCASE("Through call()") {
			object->call("some_method");
		}
		SUBCASE("Through callv()") {
			object->callv("some_method", Array());
		}
	}
	SUBCASE("Within call_const()") {
		Callable::CallError call_error;
		object->call_const("some_method", nullptr, 0, call_error);
	}
	SUBCASE("Within signal handling (from emit_signalp(), through emit_signal())") {
		Object emitter;
		emitter.add_user_signal(MethodInfo("some_signal"));
		emitter.connect("some_signal", Callable(object, "some_method"));
		emitter.emit_signal("some_signal");
	}

	CHECK_MESSAGE(
			ObjectDB::get_instance(obj_id) == nullptr,
			"Object was tail-deleted without crashes.");
}

} // namespace TestObject
