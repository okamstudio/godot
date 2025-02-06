/**************************************************************************/
/*  test_undo_redo.h                                                      */
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

#include "core/object/undo_redo.h"
#include "tests/test_macros.h"

// Declared in global namespace because of GDCLASS macro warning (Windows):
// "Unqualified friend declaration referring to type outside of the nearest enclosing namespace
// is a Microsoft extension; add a nested name specifier".
class _TestUndoRedoObject : public Object {
	GDCLASS(_TestUndoRedoObject, Object);
	int property_value = 0;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_property", "property"), &_TestUndoRedoObject::set_property);
		ClassDB::bind_method(D_METHOD("get_property"), &_TestUndoRedoObject::get_property);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "property"), "set_property", "get_property");
	}

public:
	void set_property(int value) { property_value = value; }
	int get_property() const { return property_value; }
	void add_to_property(int value) { property_value += value; }
	void subtract_from_property(int value) { property_value -= value; }
};

namespace TestUndoRedo {

void set_property_action(UndoRedo *undo_redo, const String &name, _TestUndoRedoObject *test_object, int value, UndoRedo::MergeMode merge_mode = UndoRedo::MERGE_DISABLE) {
	undo_redo->create_action(name, merge_mode);
	undo_redo->add_do_property(test_object, "property", value);
	undo_redo->add_undo_property(test_object, "property", test_object->get_property());
	undo_redo->commit_action();
}

void increment_property_action(UndoRedo *undo_redo, const String &name, _TestUndoRedoObject *test_object, int value, UndoRedo::MergeMode merge_mode = UndoRedo::MERGE_DISABLE) {
	undo_redo->create_action(name, merge_mode);
	undo_redo->add_do_method(callable_mp(test_object, &_TestUndoRedoObject::add_to_property).bind(value));
	undo_redo->add_undo_method(callable_mp(test_object, &_TestUndoRedoObject::subtract_from_property).bind(value));
	undo_redo->commit_action();
}

TEST_CASE("[UndoRedo] Simple Property UndoRedo") {
	GDREGISTER_CLASS(_TestUndoRedoObject);
	UndoRedo *undo_redo = memnew(UndoRedo());

	_TestUndoRedoObject *test_object = memnew(_TestUndoRedoObject());

	CHECK(test_object->get_property() == 0);
	CHECK(undo_redo->get_version() == 1);
	CHECK(undo_redo->get_history_count() == 0);

	set_property_action(undo_redo, "Set Property", test_object, 10);

	CHECK(test_object->get_property() == 10);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	undo_redo->undo();

	CHECK(test_object->get_property() == 0);
	CHECK(undo_redo->get_version() == 1);
	CHECK(undo_redo->get_history_count() == 1);

	undo_redo->redo();

	CHECK(test_object->get_property() == 10);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	set_property_action(undo_redo, "Set Property", test_object, 100);

	CHECK(test_object->get_property() == 100);
	CHECK(undo_redo->get_version() == 3);
	CHECK(undo_redo->get_history_count() == 2);

	set_property_action(undo_redo, "Set Property", test_object, 1000);

	CHECK(test_object->get_property() == 1000);
	CHECK(undo_redo->get_version() == 4);
	CHECK(undo_redo->get_history_count() == 3);

	undo_redo->undo();

	CHECK(test_object->get_property() == 100);
	CHECK(undo_redo->get_version() == 3);
	CHECK(undo_redo->get_history_count() == 3);

	memdelete(test_object);
	memdelete(undo_redo);
}

TEST_CASE("[UndoRedo] Merge Property UndoRedo") {
	GDREGISTER_CLASS(_TestUndoRedoObject);
	UndoRedo *undo_redo = memnew(UndoRedo());

	_TestUndoRedoObject *test_object = memnew(_TestUndoRedoObject());

	CHECK(test_object->get_property() == 0);
	CHECK(undo_redo->get_version() == 1);
	CHECK(undo_redo->get_history_count() == 0);

	set_property_action(undo_redo, "Merge Action 1", test_object, 10, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 10);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	set_property_action(undo_redo, "Merge Action 1", test_object, 100, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 100);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	set_property_action(undo_redo, "Merge Action 1", test_object, 1000, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 1000);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	memdelete(test_object);
	memdelete(undo_redo);
}

TEST_CASE("[UndoRedo] Merge Method UndoRedo") {
	GDREGISTER_CLASS(_TestUndoRedoObject);
	UndoRedo *undo_redo = memnew(UndoRedo());

	_TestUndoRedoObject *test_object = memnew(_TestUndoRedoObject());

	CHECK(test_object->get_property() == 0);
	CHECK(undo_redo->get_version() == 1);
	CHECK(undo_redo->get_history_count() == 0);

	increment_property_action(undo_redo, "Merge Increment 1", test_object, 10, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 10);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	increment_property_action(undo_redo, "Merge Increment 1", test_object, 10, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 20);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	increment_property_action(undo_redo, "Merge Increment 1", test_object, 10, UndoRedo::MERGE_ALL);

	CHECK(test_object->get_property() == 30);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	undo_redo->undo();

	CHECK(test_object->get_property() == 0);
	CHECK(undo_redo->get_version() == 1);
	CHECK(undo_redo->get_history_count() == 1);

	undo_redo->redo();

	CHECK(test_object->get_property() == 30);
	CHECK(undo_redo->get_version() == 2);
	CHECK(undo_redo->get_history_count() == 1);

	memdelete(test_object);
	memdelete(undo_redo);
}

} //namespace TestUndoRedo
