/**************************************************************************/
/*  test_text_edit.h                                                      */
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

#ifndef TEST_TEXT_EDIT_H
#define TEST_TEXT_EDIT_H

#include "scene/gui/text_edit.h"

#include "tests/test_macros.h"

namespace TestTextEdit {
static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}
static inline Array reverse_nested(Array array) {
	Array reversed_array = array.duplicate(true);
	reversed_array.reverse();
	for (int i = 0; i < reversed_array.size(); i++) {
		((Array)reversed_array[i]).reverse();
	}
	return reversed_array;
}

TEST_CASE("[SceneTree][TextEdit] text entry") {
	SceneTree::get_singleton()->get_root()->set_physics_object_picking(false);
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);
	text_edit->grab_focus();

	Array empty_signal_args;
	empty_signal_args.push_back(Array());

	SUBCASE("[TextEdit] text entry") {
		SIGNAL_WATCH(text_edit, "text_set");
		SIGNAL_WATCH(text_edit, "text_changed");
		SIGNAL_WATCH(text_edit, "lines_edited_from");
		SIGNAL_WATCH(text_edit, "caret_changed");

		Array lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

		SUBCASE("[TextEdit] clear and set text") {
			// "text_changed" should not be emitted on clear / set.
			text_edit->clear();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_line_count() == 1);
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->set_text("test text");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test text");
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_line_count() == 1);
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->clear();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");

			// Can undo / redo words when editable.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test text");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Cannot undo when not-editable but should still clear.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test text");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Can clear even if not editable.
			text_edit->set_editable(false);

			Array lines_edited_clear_args = build_array(build_array(1, 0));

			text_edit->clear();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_clear_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->set_editable(true);

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK_FALSE("text_set");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");

			// Can still undo set_text.
			text_edit->set_editable(false);

			text_edit->set_text("test text");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test text");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->set_editable(true);

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Any selections are removed.
			text_edit->set_text("test text");
			MessageQueue::get_singleton()->flush();
			text_edit->select_all();
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test text");
			CHECK(text_edit->get_caret_column() == 9);
			CHECK(text_edit->has_selection());
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->set_text("test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test");
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->select_all();
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			CHECK(text_edit->has_selection());

			text_edit->clear();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
		}

		SUBCASE("[TextEdit] insert text") {
			// insert_text is 0 indexed.
			ERR_PRINT_OFF;
			text_edit->insert_text("test", 1, 0);
			ERR_PRINT_ON;
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Insert text when there is no text.
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->insert_text("tes", 0, 0);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "tes");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Insert multiple lines.
			lines_edited_args = build_array(build_array(0, 1));

			text_edit->insert_text("t\ninserting text", 0, 3);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\ninserting text");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 14);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Can insert even if not editable.
			lines_edited_args = build_array(build_array(1, 1));

			text_edit->set_editable(false);
			text_edit->insert_text("mid", 1, 2);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\ninmidserting text");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 17);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Undo insert.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\ninserting text");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 14);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Redo insert.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\ninmidserting text");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 17);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Insert offsets carets after the edit.
			text_edit->add_caret(1, 1);
			text_edit->add_caret(1, 4);
			text_edit->select(1, 4, 1, 6, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 2));

			text_edit->insert_text("\n ", 1, 2);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nin\n midserting text");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 2);
			CHECK(text_edit->get_caret_column(0) == 16);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 1);
			CHECK(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 2);
			CHECK(text_edit->get_caret_column(2) == 5);
			CHECK(text_edit->get_selection_origin_line(2) == 2);
			CHECK(text_edit->get_selection_origin_column(2) == 3);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();
			text_edit->deselect();

			// Insert text outside of selections.
			text_edit->set_text("test text");
			text_edit->add_caret(0, 8);
			text_edit->select(0, 1, 0, 4, 0);
			text_edit->select(0, 4, 0, 8, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->insert_text("a", 0, 4, true, false);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testa text");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 1);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 9);
			CHECK(text_edit->get_selection_origin_line(1) == 0);
			CHECK(text_edit->get_selection_origin_column(1) == 5);

			// Insert text to beginning of selections.
			text_edit->set_text("test text");
			text_edit->add_caret(0, 8);
			text_edit->select(0, 1, 0, 4, 0);
			text_edit->select(0, 4, 0, 8, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->insert_text("a", 0, 4, false, false);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testa text");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 1);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 9);
			CHECK(text_edit->get_selection_origin_line(1) == 0);
			CHECK(text_edit->get_selection_origin_column(1) == 4);

			// Insert text to end of selections.
			text_edit->set_text("test text");
			text_edit->add_caret(0, 8);
			text_edit->select(0, 1, 0, 4, 0);
			text_edit->select(0, 4, 0, 8, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->insert_text("a", 0, 4, true, true);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testa text");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 1);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 9);
			CHECK(text_edit->get_selection_origin_line(1) == 0);
			CHECK(text_edit->get_selection_origin_column(1) == 5);

			// Insert text inside of selections.
			text_edit->set_text("test text");
			text_edit->add_caret(0, 8);
			text_edit->select(0, 1, 0, 4, 0);
			text_edit->select(0, 4, 0, 8, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->insert_text("a", 0, 4, false, true);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testa text");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 9);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 1);
		}

		SUBCASE("[TextEdit] remove text") {
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 2));

			text_edit->set_text("test\nremoveing text\nthird line");
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			// remove_text is 0 indexed.
			ERR_PRINT_OFF;
			text_edit->remove_text(3, 0, 3, 4);
			ERR_PRINT_ON;
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremoveing text\nthird line");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Remove multiple lines.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(10);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(2, 1));

			text_edit->remove_text(1, 9, 2, 2);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremoveingird line");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 17);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Can remove even if not editable.
			lines_edited_args = build_array(build_array(1, 1));

			text_edit->set_editable(false);
			text_edit->remove_text(1, 5, 1, 6);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremovingird line");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 16);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Undo remove.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremoveingird line");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 17);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Redo remove.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremovingird line");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 16);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Remove collapses carets and offsets carets after the edit.
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(9);
			text_edit->add_caret(1, 10);
			text_edit->select(1, 10, 1, 13, 1);
			text_edit->add_caret(1, 14);
			text_edit->add_caret(1, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			text_edit->remove_text(1, 8, 1, 11);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test\nremoving line");
			// Caret 0 was merged into the selection.
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 10);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 8);
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 11);
			CHECK(text_edit->get_caret_line(2) == 1);
			CHECK(text_edit->get_caret_column(2) == 2);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();
		}

		SUBCASE("[TextEdit] set and get line") {
			// Set / Get line is 0 indexed.
			text_edit->set_line(1, "test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("text_set");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");

			text_edit->set_line(0, "test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test");
			CHECK(text_edit->get_line(0) == "test");
			CHECK(text_edit->get_line(1) == "");
			CHECK(text_edit->get_line_count() == 1);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			SIGNAL_CHECK_FALSE("caret_changed");

			// Setting to a longer line, caret and selections should be preserved.
			text_edit->select_all();
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_line(0, "test text");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "test text");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "test");
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Setting to a shorter line, selection and caret should be adjusted. Also works if not editable.
			text_edit->set_editable(false);
			text_edit->set_line(0, "te");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "te");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "te");
			CHECK(text_edit->get_caret_column() == 2);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Undo / redo should work.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "test text");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "test");
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "te");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_column() == 2);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Out of range.
			ERR_PRINT_OFF;
			text_edit->set_line(-1, "test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "te");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->set_line(1, "test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "te");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("text_set");
			ERR_PRINT_ON;

			// Both ends of selection are adjusted and deselects.
			text_edit->set_text("test text");
			text_edit->select(0, 8, 0, 6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_line(0, "test");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "test");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Multiple carets adjust to keep visual position.
			text_edit->set_text("test text");
			text_edit->set_caret_column(2);
			text_edit->add_caret(0, 0);
			text_edit->add_caret(0, 1);
			text_edit->add_caret(0, 6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_line(0, "\tset line");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_line(0) == "\tset line");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK_FALSE(text_edit->has_selection());
			// In the default font, these are the same positions.
			CHECK(text_edit->get_caret_column(0) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			// The previous caret at index 2 was merged.
			CHECK(text_edit->get_caret_column(2) == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();

			// Insert multiple lines.
			text_edit->set_text("test text\nsecond line");
			text_edit->set_caret_column(5);
			text_edit->add_caret(1, 6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 1));

			text_edit->set_line(0, "multiple\nlines");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "multiple\nlines\nsecond line");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 3); // In the default font, this is the same position.
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 6);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();
		}

		SUBCASE("[TextEdit] swap lines") {
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 1));

			text_edit->set_text("testing\nswap");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->set_caret_column(text_edit->get_line(0).length());
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			// Emitted twice for each line.
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0), build_array(1, 1), build_array(1, 1));

			// Order does not matter. Works when not editable.
			text_edit->set_editable(false);
			text_edit->swap_lines(1, 0);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "swap\ntesting");
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Single undo/redo action.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "swap\ntesting");
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Out of range.
			ERR_PRINT_OFF;
			text_edit->swap_lines(-1, 0);
			CHECK(text_edit->get_text() == "swap\ntesting");
			text_edit->swap_lines(0, -1);
			CHECK(text_edit->get_text() == "swap\ntesting");
			text_edit->swap_lines(2, 0);
			CHECK(text_edit->get_text() == "swap\ntesting");
			text_edit->swap_lines(0, 2);
			CHECK(text_edit->get_text() == "swap\ntesting");
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("text_set");
			ERR_PRINT_ON;

			// Carets are also swapped.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(2);
			text_edit->select(0, 0, 0, 2);
			text_edit->add_caret(1, 6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 1), build_array(1, 1), build_array(0, 0), build_array(0, 0));

			text_edit->swap_lines(0, 1);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 2);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 6);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();

			// Swap non adjacent lines.
			text_edit->insert_line_at(1, "new line");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(5);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nnew line\nswap");
			SIGNAL_DISCARD("caret_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("text_changed");
			lines_edited_args = build_array(build_array(2, 2), build_array(2, 2), build_array(0, 0), build_array(0, 0));

			text_edit->swap_lines(0, 2);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "swap\nnew line\ntesting");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
		}

		SUBCASE("[TextEdit] insert line at") {
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 1));

			text_edit->set_text("testing\nswap");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			text_edit->select_all();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_to_line() == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Insert line at inserts a line before and moves caret and selection. Works when not editable.
			text_edit->set_editable(false);
			lines_edited_args = build_array(build_array(0, 1));
			text_edit->insert_line_at(0, "new");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new\ntesting\nswap");
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(2).size() - 1);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 1);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 2);
			CHECK(text_edit->get_selection_to_column() == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Can undo/redo as single action.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			CHECK(text_edit->has_selection());
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new\ntesting\nswap");
			CHECK(text_edit->has_selection());
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Adding inside selection extends selection.
			text_edit->select_all();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_to_line() == 2);
			SIGNAL_CHECK_FALSE("caret_changed");
			lines_edited_args = build_array(build_array(2, 3));

			text_edit->insert_line_at(2, "after");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new\ntesting\nafter\nswap");
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(3).size() - 1);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_to_line() == 3);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Out of range.
			ERR_PRINT_OFF;
			text_edit->insert_line_at(-1, "after");
			CHECK(text_edit->get_text() == "new\ntesting\nafter\nswap");
			text_edit->insert_line_at(4, "after");
			CHECK(text_edit->get_text() == "new\ntesting\nafter\nswap");
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("text_set");
			ERR_PRINT_ON;

			// Can insert multiple lines.
			text_edit->select(0, 1, 2, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(2, 4));

			text_edit->insert_line_at(2, "multiple\nlines");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new\ntesting\nmultiple\nlines\nafter\nswap");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 4);
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 1);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
		}

		SUBCASE("[TextEdit] remove line at") {
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 5));
			text_edit->set_text("testing\nremove line at\n\tremove\nlines\n\ntest");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nremove line at\n\tremove\nlines\n\ntest");
			SIGNAL_CHECK("text_set", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");

			// Remove line handles multiple carets.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(0);
			text_edit->add_caret(2, 7);
			text_edit->select(2, 1, 2, 7, 1);
			text_edit->add_caret(3, 1);
			text_edit->add_caret(4, 5);
			text_edit->add_caret(1, 5);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 2));

			text_edit->remove_line_at(2, true);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nremove line at\nlines\n\ntest");
			CHECK(text_edit->get_caret_count() == 5);
			CHECK_FALSE(text_edit->has_selection(0)); // Same line.
			CHECK(text_edit->get_caret_line(0) == 2);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->has_selection(1)); // Same line, clamped.
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 5);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 3); // In the default font, this is the same position.
			CHECK_FALSE(text_edit->has_selection(2)); // Moved up.
			CHECK(text_edit->get_caret_line(2) == 2);
			CHECK(text_edit->get_caret_column(2) == 1);
			CHECK_FALSE(text_edit->has_selection(3)); // Moved up.
			CHECK(text_edit->get_caret_line(3) == 3);
			CHECK(text_edit->get_caret_column(3) == 0);
			CHECK_FALSE(text_edit->has_selection(4)); // Didn't move.
			CHECK(text_edit->get_caret_line(4) == 1);
			CHECK(text_edit->get_caret_column(4) == 5);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();

			// Remove first line.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(5);
			text_edit->add_caret(4, 4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0));

			text_edit->remove_line_at(0, false);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at\nlines\n\ntest");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->remove_secondary_carets();

			// Remove empty line.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 2));

			text_edit->remove_line_at(2, false);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at\nlines\ntest");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Remove last line.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(2, 1));

			text_edit->remove_line_at(2, true);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at\nlines");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Out of bounds.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			ERR_PRINT_OFF
			text_edit->remove_line_at(2, true);
			ERR_PRINT_ON
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at\nlines");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 2);
			SIGNAL_CHECK_FALSE("lines_edited_from");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("text_set");

			// Remove regular line with move caret up and not editable.
			text_edit->set_editable(false);
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0));

			text_edit->remove_line_at(1, false);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 1); // In the default font, this is the same position.
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at\nlines");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 2);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "remove line at");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 1);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Remove only line removes line content.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(10);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->remove_line_at(0);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_line_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
		}

		SUBCASE("[TextEdit] insert text at caret") {
			lines_edited_args = build_array(build_array(0, 1));

			// Insert text at caret can insert multiple lines.
			text_edit->insert_text_at_caret("testing\nswap");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "testing\nswap");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(1).size() - 1);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Text is inserted at caret.
			text_edit->set_caret_line(0, false);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			lines_edited_args = build_array(build_array(0, 0));
			text_edit->insert_text_at_caret("mid");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "temidsting\nswap");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			// Selections are deleted then text is inserted. It also works even if not editable.
			text_edit->select(0, 0, 0, text_edit->get_line(0).length());
			CHECK(text_edit->has_selection());
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

			text_edit->set_editable(false);
			text_edit->insert_text_at_caret("new line");
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new line\nswap");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(0).size() - 1);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
			text_edit->set_editable(true);

			// Undo restores text and selection.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "temidsting\nswap");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(0).length());
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "new line\nswap");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_set");
		}

		SIGNAL_UNWATCH(text_edit, "text_set");
		SIGNAL_UNWATCH(text_edit, "text_changed");
		SIGNAL_UNWATCH(text_edit, "lines_edited_from");
		SIGNAL_UNWATCH(text_edit, "caret_changed");
	}

	SUBCASE("[TextEdit] indent level") {
		CHECK(text_edit->get_indent_level(0) == 0);
		CHECK(text_edit->get_first_non_whitespace_column(0) == 0);

		text_edit->set_line(0, "a");
		CHECK(text_edit->get_indent_level(0) == 0);
		CHECK(text_edit->get_first_non_whitespace_column(0) == 0);

		text_edit->set_line(0, "\t");
		CHECK(text_edit->get_indent_level(0) == 4);
		CHECK(text_edit->get_first_non_whitespace_column(0) == 1);

		text_edit->set_tab_size(8);
		CHECK(text_edit->get_indent_level(0) == 8);

		text_edit->set_line(0, "\t a");
		CHECK(text_edit->get_first_non_whitespace_column(0) == 2);
		CHECK(text_edit->get_indent_level(0) == 9);
	}

	SUBCASE("[TextEdit] selection") {
		SIGNAL_WATCH(text_edit, "text_set");
		SIGNAL_WATCH(text_edit, "text_changed");
		SIGNAL_WATCH(text_edit, "lines_edited_from");
		SIGNAL_WATCH(text_edit, "caret_changed");

		Array lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

		SUBCASE("[TextEdit] select all") {
			// Select when there is no text does not select.
			text_edit->select_all();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 0);
			CHECK(text_edit->get_selection_to_column() == 0);
			CHECK(text_edit->get_selected_text() == "");

			// Select all selects all text.
			text_edit->set_text("test\nselection");
			SEND_GUI_ACTION("ui_text_select_all");
			CHECK(text_edit->get_viewport()->is_input_handled());
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_selected_text() == "test\nselection");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 1);
			CHECK(text_edit->get_selection_to_column() == 9);
			CHECK(text_edit->get_selection_mode() == TextEdit::SelectionMode::SELECTION_MODE_SHIFT);
			CHECK(text_edit->is_caret_after_selection_origin());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Cannot select when disabled.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			text_edit->set_selecting_enabled(false);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);

			text_edit->select_all();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
		}

		SUBCASE("[TextEdit] select word under caret") {
			text_edit->set_text("\ntest   test\ntest   test");

			text_edit->set_caret_column(0);
			text_edit->set_caret_line(1);

			text_edit->add_caret(2, 0);
			text_edit->add_caret(2, 2);
			CHECK(text_edit->get_caret_count() == 3);

			MessageQueue::get_singleton()->flush();

			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Select word under caret with multiple carets.
			text_edit->select_word_under_caret();
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 2);
			CHECK(text_edit->get_selection_from_column(1) == 0);
			CHECK(text_edit->get_selection_to_line(1) == 2);
			CHECK(text_edit->get_selection_to_column(1) == 4);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 4);

			CHECK(text_edit->get_caret_count() == 2);

			// Select word under caret disables selection if there is already a selection.
			text_edit->select_word_under_caret();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");

			SEND_GUI_ACTION("ui_text_select_word_under_caret");
			CHECK(text_edit->get_viewport()->is_input_handled());
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 2);
			CHECK(text_edit->get_selection_from_column(1) == 0);
			CHECK(text_edit->get_selection_to_line(1) == 2);
			CHECK(text_edit->get_selection_to_column(1) == 4);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 4);

			CHECK(text_edit->get_selected_text() == "test\ntest");
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Cannot select when disabled.
			text_edit->set_selecting_enabled(false);
			text_edit->select_word_under_caret();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK_FALSE("caret_changed");
			text_edit->set_selecting_enabled(true);

			// Select word under caret when there is no word does not select.
			text_edit->set_caret_line(1, false, true, -1, 0);
			text_edit->set_caret_column(5, false, 0);
			text_edit->set_caret_line(2, false, true, -1, 1);
			text_edit->set_caret_column(5, false, 1);

			text_edit->select_word_under_caret();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");

			text_edit->select_word_under_caret();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 5);
			SIGNAL_CHECK_FALSE("caret_changed");
		}

		SUBCASE("[TextEdit] add selection for next occurrence") {
			text_edit->set_text("\ntest   other_test\nrandom   test\nword test word nonrandom");
			text_edit->set_caret_column(0);
			text_edit->set_caret_line(1);

			// First selection made by the implicit select_word_under_caret call.
			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);

			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 1);
			CHECK(text_edit->get_selection_from_column(1) == 13);
			CHECK(text_edit->get_selection_to_line(1) == 1);
			CHECK(text_edit->get_selection_to_column(1) == 17);
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 17);

			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->get_selected_text(2) == "test");
			CHECK(text_edit->get_selection_from_line(2) == 2);
			CHECK(text_edit->get_selection_from_column(2) == 9);
			CHECK(text_edit->get_selection_to_line(2) == 2);
			CHECK(text_edit->get_selection_to_column(2) == 13);
			CHECK(text_edit->get_caret_line(2) == 2);
			CHECK(text_edit->get_caret_column(2) == 13);

			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 4);
			CHECK(text_edit->get_selected_text(3) == "test");
			CHECK(text_edit->get_selection_from_line(3) == 3);
			CHECK(text_edit->get_selection_from_column(3) == 5);
			CHECK(text_edit->get_selection_to_line(3) == 3);
			CHECK(text_edit->get_selection_to_column(3) == 9);
			CHECK(text_edit->get_caret_line(3) == 3);
			CHECK(text_edit->get_caret_column(3) == 9);

			// A different word with a new manually added caret.
			text_edit->add_caret(2, 1);
			text_edit->select(2, 0, 2, 4, 4);
			CHECK(text_edit->get_selected_text(4) == "rand");

			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 6);
			CHECK(text_edit->get_selected_text(5) == "rand");
			CHECK(text_edit->get_selection_from_line(5) == 3);
			CHECK(text_edit->get_selection_from_column(5) == 18);
			CHECK(text_edit->get_selection_to_line(5) == 3);
			CHECK(text_edit->get_selection_to_column(5) == 22);
			CHECK(text_edit->get_caret_line(5) == 3);
			CHECK(text_edit->get_caret_column(5) == 22);

			// Make sure the previous selections are still active.
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selected_text(2) == "test");
			CHECK(text_edit->get_selected_text(3) == "test");
		}

		SUBCASE("[TextEdit] skip selection for next occurrence") {
			text_edit->set_text("\ntest   other_test\nrandom   test\nword test word nonrandom");
			text_edit->set_caret_column(0);
			text_edit->set_caret_line(1);

			// Without selection on the current caret, the caret as 'jumped' to the next occurrence of the word under the caret.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 13);

			// Repeating previous action.
			// This time caret is in 'other_test' (other_|test)
			// so the searched term will be 'other_test' or not just 'test'
			// => no occurrence, as a side effect, the caret will move to start of the term.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 7);

			// Repeating action again should do nothing now
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 7);

			// Moving back to the first 'test' occurrence.
			text_edit->set_caret_column(0);
			text_edit->set_caret_line(1);

			// But this time, create a selection of it.
			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);

			// Then, skipping it, but this time, selection has been made on the next occurrence.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 13);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 17);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 17);

			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 2);
			CHECK(text_edit->get_selection_from_column(0) == 9);
			CHECK(text_edit->get_selection_to_line(0) == 2);
			CHECK(text_edit->get_selection_to_column(0) == 13);
			CHECK(text_edit->get_caret_line(0) == 2);
			CHECK(text_edit->get_caret_column(0) == 13);

			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 3);
			CHECK(text_edit->get_selection_from_column(0) == 5);
			CHECK(text_edit->get_selection_to_line(0) == 3);
			CHECK(text_edit->get_selection_to_column(0) == 9);
			CHECK(text_edit->get_caret_line(0) == 3);
			CHECK(text_edit->get_caret_column(0) == 9);

			// Last skip, we are back to the first occurrence.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);

			// Adding first occurrence to selections/carets list
			// and select occurrence on 'other_test'.
			text_edit->add_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 1);
			CHECK(text_edit->get_selection_from_column(1) == 13);
			CHECK(text_edit->get_selection_to_line(1) == 1);
			CHECK(text_edit->get_selection_to_column(1) == 17);
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 17);

			// We don't want this occurrence.
			// Let's skip it.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");

			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 2);
			CHECK(text_edit->get_selection_from_column(1) == 9);
			CHECK(text_edit->get_selection_to_line(1) == 2);
			CHECK(text_edit->get_selection_to_column(1) == 13);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 13);

			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");

			CHECK(text_edit->get_selected_text(1) == "test");
			CHECK(text_edit->get_selection_from_line(1) == 3);
			CHECK(text_edit->get_selection_from_column(1) == 5);
			CHECK(text_edit->get_selection_to_line(1) == 3);
			CHECK(text_edit->get_selection_to_column(1) == 9);
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 9);

			// We are back the first occurrence.
			text_edit->skip_selection_for_next_occurrence();
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_selection_from_line(0) == 1);
			CHECK(text_edit->get_selection_from_column(0) == 0);
			CHECK(text_edit->get_selection_to_line(0) == 1);
			CHECK(text_edit->get_selection_to_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);
		}

		SUBCASE("[TextEdit] deselect on focus loss") {
			text_edit->set_text("test");

			text_edit->set_deselect_on_focus_loss_enabled(true);
			CHECK(text_edit->is_deselect_on_focus_loss_enabled());

			text_edit->grab_focus();
			text_edit->select_all();
			CHECK(text_edit->has_focus());
			CHECK(text_edit->has_selection());

			text_edit->release_focus();
			CHECK_FALSE(text_edit->has_focus());
			CHECK_FALSE(text_edit->has_selection());

			text_edit->set_deselect_on_focus_loss_enabled(false);
			CHECK_FALSE(text_edit->is_deselect_on_focus_loss_enabled());

			text_edit->grab_focus();
			text_edit->select_all();
			CHECK(text_edit->has_focus());
			CHECK(text_edit->has_selection());

			text_edit->release_focus();
			CHECK_FALSE(text_edit->has_focus());
			CHECK(text_edit->has_selection());

			text_edit->set_deselect_on_focus_loss_enabled(true);
			CHECK_FALSE(text_edit->has_selection());
		}

		SUBCASE("[TextEdit] key select") {
			text_edit->set_text("test");

			text_edit->grab_focus();
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT)
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "t");
			CHECK(text_edit->is_caret_after_selection_origin());

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT | KeyModifierMask::ALT)
#else
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL)
#endif
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "test");
			CHECK(text_edit->is_caret_after_selection_origin());

			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT)
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "tes");
			CHECK(text_edit->is_caret_after_selection_origin());

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT | KeyModifierMask::ALT)
#else
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL)
#endif
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");

			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT)
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "t");

			SEND_GUI_KEY_EVENT(Key::RIGHT)
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");

			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT)
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "t");
			CHECK_FALSE(text_edit->is_caret_after_selection_origin());

			SEND_GUI_KEY_EVENT(Key::LEFT)
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");

			// Cannot select when disabled.
			text_edit->set_selecting_enabled(false);
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT)
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "");
			text_edit->set_selecting_enabled(true);
		}

		SUBCASE("[TextEdit] mouse drag select") {
			// Set size for mouse input.
			text_edit->set_size(Size2(200, 200));

			text_edit->set_text("this is some text\nfor selection");
			text_edit->grab_focus();
			MessageQueue::get_singleton()->flush();

			// Click and drag to make a selection.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			// Add (2,0) to bring it past the center point of the grapheme and account for integer division flooring.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for s");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->is_caret_after_selection_origin());
			CHECK(text_edit->is_dragging_cursor());

			// Releasing finishes.
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for s");
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 9).get_center() + Point2i(2, 0), MouseButtonMask::NONE, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for s");
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->is_caret_after_selection_origin());

			// Clicking clears selection.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);

			// Cannot select when disabled, but caret still moves.
			text_edit->set_selecting_enabled(false);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			text_edit->set_selecting_enabled(true);

			// Only last caret is moved when adding a selection.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			text_edit->add_caret(0, 15);
			text_edit->select(0, 11, 0, 15, 1);
			MessageQueue::get_singleton()->flush();

			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::ALT);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 0).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selection_origin_line(1) == 0);
			CHECK(text_edit->get_selection_origin_column(1) == 11);
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 15);

			CHECK(text_edit->has_selection(2));
			CHECK(text_edit->get_selected_text(2) == "for s");
			CHECK(text_edit->get_selection_origin_line(2) == 1);
			CHECK(text_edit->get_selection_origin_column(2) == 5);
			CHECK(text_edit->get_caret_line(2) == 1);
			CHECK(text_edit->get_caret_column(2) == 0);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(2));

			// Overlapping carets and selections merges them.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 3).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "s is some text\nfor s");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 5);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 3);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin());

			// Entering text stops selecting.
			text_edit->insert_text_at_caret("a");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "thiaelection");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 10).get_center() + Point2i(2, 0), MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);

			// Wrapped lines.
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
			text_edit->set_text("this is some text\nfor selection");
			text_edit->set_size(Size2(110, 100));
			MessageQueue::get_singleton()->flush();

			// Line 0 wraps: 'this is ', 'some text'.
			// Line 1 wraps: 'for ', 'selection'.
			CHECK(text_edit->is_line_wrapped(0));

			// Select to the first character of a wrapped line.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 11).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 8).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "so");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 10);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK(text_edit->is_dragging_cursor());
		}

		SUBCASE("[TextEdit] mouse word select") {
			// Set size for mouse input.
			text_edit->set_size(Size2(200, 200));

			text_edit->set_text("this is some text\nfor selection\n");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			// Double click to select word.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(1, 2).get_center() + Point2i(2, 0), Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->get_selection_from_line() == 1);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 1);
			CHECK(text_edit->get_selection_to_column() == 3);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 3);
			CHECK(text_edit->is_caret_after_selection_origin());
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Moving mouse selects entire words at a time.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 6).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->get_selection_from_line() == 1);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 1);
			CHECK(text_edit->get_selection_to_column() == 13);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 13);
			CHECK(text_edit->is_caret_after_selection_origin());
			CHECK(text_edit->is_dragging_cursor());
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Moving to a word before the initial selected word reverses selection direction and keeps the initial word selected.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 10).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "some text\nfor");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_from_column() == 8);
			CHECK(text_edit->get_selection_to_line() == 1);
			CHECK(text_edit->get_selection_to_column() == 3);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin());
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Releasing finishes.
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(0, 10).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "some text\nfor");
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 2).get_center(), MouseButtonMask::NONE, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "some text\nfor");
			text_edit->deselect();

			// Can start word select mode on an empty line.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(2, 0).get_center() + Point2i(2, 0), Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 0);

			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 9).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "selection\n");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 4);
			CHECK(text_edit->get_selection_origin_line() == 2);
			CHECK(text_edit->get_selection_origin_column() == 0);

			// Clicking clears selection.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);

			// Can start word select mode when not on a word.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(0, 12).get_center() + Point2i(2, 0), Key::NONE);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 12);

			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 9).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == " text\nfor selection");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 13);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 12);

			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 10).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "some");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 12);

			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(0, 15).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);

			// Add a new selection without affecting the old one.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::ALT);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 8).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::ALT);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "some");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 8);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 12);

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "ele");
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 8);
			CHECK(text_edit->get_selection_origin_line(1) == 1);
			CHECK(text_edit->get_selection_origin_column(1) == 5);

			// Shift + double click to extend selection and start word select mode.
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 8).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			text_edit->remove_secondary_carets();
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), Key::NONE | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_WORD);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == " text\nfor selection");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 13);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 12);

			// Cannot select when disabled, but caret still moves to end of word.
			text_edit->set_selecting_enabled(false);
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(1, 1).get_center() + Point2i(2, 0), Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 3);
			text_edit->set_selecting_enabled(true);
		}

		SUBCASE("[TextEdit] mouse line select") {
			// Set size for mouse input.
			text_edit->set_size(Size2(200, 200));

			text_edit->set_text("this is some text\nfor selection\nwith 3 lines");
			MessageQueue::get_singleton()->flush();

			// Triple click to select line.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(1, 2).get_center(), Key::NONE);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 2).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection\n");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->get_selection_from_line() == 1);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 2);
			CHECK(text_edit->get_selection_to_column() == 0);
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->is_caret_after_selection_origin());

			// Moving mouse selects entire lines at a time. Selecting above reverses the selection direction.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 10).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "this is some text\nfor selection");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->get_selection_from_line() == 0);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 1);
			CHECK(text_edit->get_selection_to_column() == 13);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin());
			CHECK(text_edit->is_dragging_cursor());

			// Selecting to the last line puts the caret at end of the line.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(2, 10).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection\nwith 3 lines");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->get_selection_from_line() == 1);
			CHECK(text_edit->get_selection_from_column() == 0);
			CHECK(text_edit->get_selection_to_line() == 2);
			CHECK(text_edit->get_selection_to_column() == 12);
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK(text_edit->is_caret_after_selection_origin());

			// Releasing finishes.
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(2, 10).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection\nwith 3 lines");
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 2).get_center(), MouseButtonMask::NONE, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection\nwith 3 lines");

			// Clicking clears selection.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);

			// Can start line select mode on an empty line.
			text_edit->set_text("this is some text\n\nfor selection\nwith 4 lines");
			MessageQueue::get_singleton()->flush();
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(1, 0).get_center() + Point2i(2, 0), Key::NONE);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "\n");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 0);

			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(2, 9).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "\nfor selection\n");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 0);

			// Add a new selection without affecting the old one.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 3).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::ALT);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 4).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::ALT);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "\nfor selection\n");
			CHECK(text_edit->get_caret_line(0) == 3);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 0);

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "is");
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 4);
			CHECK(text_edit->get_selection_origin_line(1) == 0);
			CHECK(text_edit->get_selection_origin_column(1) == 2);
			text_edit->remove_secondary_carets();
			text_edit->deselect();

			// Selecting the last line puts caret at the end.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(3, 3).get_center(), Key::NONE);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(3, 3).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "with 4 lines");
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK(text_edit->get_selection_origin_line() == 3);
			CHECK(text_edit->get_selection_origin_column() == 0);

			// Selecting above reverses direction.
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(2, 10).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "for selection\nwith 4 lines");
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_selection_origin_line() == 3);
			CHECK(text_edit->get_selection_origin_column() == 12);

			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(2, 10).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);

			// Shift + triple click to extend selection and restart line select mode.
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(0, 9).get_center() + Point2i(2, 0), Key::NONE | KeyModifierMask::SHIFT);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 9).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_LINE);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "this is some text\n\nfor selection\nwith 4 lines");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_selection_origin_line() == 3);
			CHECK(text_edit->get_selection_origin_column() == 12);

			// Cannot select when disabled, but caret still moves to the start of the next line.
			text_edit->set_selecting_enabled(false);
			SEND_GUI_DOUBLE_CLICK(text_edit->get_rect_at_line_column(0, 2).get_center(), Key::NONE);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			text_edit->set_selecting_enabled(true);
		}

		SUBCASE("[TextEdit] mouse shift click select") {
			// Set size for mouse input.
			text_edit->set_size(Size2(200, 200));

			text_edit->set_text("this is some text\nfor selection");
			MessageQueue::get_singleton()->flush();

			// Shift click to make a selection from the previous caret position.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 1).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::SHIFT);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "or s");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 1);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->is_caret_after_selection_origin());

			// Shift click above to switch selection direction. Uses original selection position.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 6).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::SHIFT);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "s some text\nf");
			CHECK(text_edit->get_selection_mode() == TextEdit::SELECTION_MODE_POINTER);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 1);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin());

			// Clicking clears selection.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 7);

			// Cannot select when disabled, but caret still moves.
			text_edit->set_selecting_enabled(false);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE | KeyModifierMask::SHIFT);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			text_edit->set_selecting_enabled(true);
		}

		SUBCASE("[TextEdit] select and deselect") {
			text_edit->set_text("this is some text\nfor selection");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			// Select clamps input to full text.
			text_edit->select(-1, -1, 500, 500);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "this is some text\nfor selection");
			CHECK(text_edit->is_caret_after_selection_origin(0));
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 13);
			CHECK(text_edit->get_selection_from_line(0) == text_edit->get_selection_origin_line(0));
			CHECK(text_edit->get_selection_from_column(0) == text_edit->get_selection_origin_column(0));
			CHECK(text_edit->get_selection_to_line(0) == text_edit->get_caret_line(0));
			CHECK(text_edit->get_selection_to_column(0) == text_edit->get_caret_column(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			text_edit->deselect();
			MessageQueue::get_singleton()->flush();
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");

			// Select works in the other direction.
			text_edit->select(500, 500, -1, -1);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "this is some text\nfor selection");
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 13);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_selection_from_line(0) == text_edit->get_caret_line(0));
			CHECK(text_edit->get_selection_from_column(0) == text_edit->get_caret_column(0));
			CHECK(text_edit->get_selection_to_line(0) == text_edit->get_selection_origin_line(0));
			CHECK(text_edit->get_selection_to_column(0) == text_edit->get_selection_origin_column(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			text_edit->deselect();
			MessageQueue::get_singleton()->flush();
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");

			// Select part of a line.
			text_edit->select(0, 4, 0, 8);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == " is ");
			CHECK(text_edit->is_caret_after_selection_origin(0));
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 4);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 8);
			CHECK(text_edit->get_selection_from_line(0) == text_edit->get_selection_origin_line(0));
			CHECK(text_edit->get_selection_from_column(0) == text_edit->get_selection_origin_column(0));
			CHECK(text_edit->get_selection_to_line(0) == text_edit->get_caret_line(0));
			CHECK(text_edit->get_selection_to_column(0) == text_edit->get_caret_column(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			text_edit->deselect();
			MessageQueue::get_singleton()->flush();
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");

			// Select part of a line in the other direction.
			text_edit->select(0, 8, 0, 4);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == " is ");
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 8);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_from_line(0) == text_edit->get_caret_line(0));
			CHECK(text_edit->get_selection_from_column(0) == text_edit->get_caret_column(0));
			CHECK(text_edit->get_selection_to_line(0) == text_edit->get_selection_origin_line(0));
			CHECK(text_edit->get_selection_to_column(0) == text_edit->get_selection_origin_column(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);

			// Cannot select when disabled.
			text_edit->set_selecting_enabled(false);
			CHECK_FALSE(text_edit->has_selection());
			text_edit->select(0, 8, 0, 4);
			MessageQueue::get_singleton()->flush();
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");
			text_edit->set_selecting_enabled(true);
		}

		SUBCASE("[TextEdit] delete selection") {
			text_edit->set_text("this is some text\nfor selection");
			MessageQueue::get_singleton()->flush();

			// Delete selection does nothing if there is no selection.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(8);
			CHECK_FALSE(text_edit->has_selection());

			text_edit->delete_selection();
			CHECK(text_edit->get_text() == "this is some text\nfor selection");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);

			// Backspace removes selection.
			text_edit->select(0, 4, 0, 8);
			CHECK(text_edit->has_selection());
			SEND_GUI_ACTION("ui_text_backspace");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "thissome text\nfor selection");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);

			// Undo restores previous selection.
			text_edit->undo();
			CHECK(text_edit->get_text() == "this is some text\nfor selection");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 4);

			// Redo restores caret.
			text_edit->redo();
			CHECK(text_edit->get_text() == "thissome text\nfor selection");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);

			text_edit->undo();
			CHECK(text_edit->get_text() == "this is some text\nfor selection");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);

			text_edit->select(0, 4, 0, 8);
			CHECK(text_edit->has_selection());

			// Delete selection removes text, deselects, and moves caret.
			text_edit->delete_selection();
			CHECK(text_edit->get_text() == "thissome text\nfor selection");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);

			// Undo delete works.
			text_edit->undo();
			CHECK(text_edit->get_text() == "this is some text\nfor selection");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 4);

			// Redo delete works.
			text_edit->redo();
			CHECK(text_edit->get_text() == "thissome text\nfor selection");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);

			text_edit->undo();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_text() == "this is some text\nfor selection");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 8);

			// Can still delete if not editable.
			text_edit->set_editable(false);
			text_edit->delete_selection();
			text_edit->set_editable(false);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "thissome text\nfor selection");

			// Cannot undo since it was not editable.
			text_edit->undo();
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "thissome text\nfor selection");

			// Delete multiple adjacent selections on the same line.
			text_edit->select(0, 0, 0, 5);
			text_edit->add_caret(0, 8);
			text_edit->select(0, 5, 0, 8, 1);
			CHECK(text_edit->get_caret_count() == 2);
			text_edit->delete_selection();
			CHECK(text_edit->get_text() == " text\nfor selection");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);

			// Delete mulitline selection. Ignore non selections.
			text_edit->remove_secondary_carets();
			text_edit->select(1, 3, 0, 2);
			text_edit->add_caret(1, 7);
			text_edit->delete_selection();
			CHECK(text_edit->get_text() == " t selection");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 2);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 6);
		}

		SUBCASE("[TextEdit] text drag") {
			text_edit->set_size(Size2(200, 200));
			text_edit->set_text("drag test\ndrop here ''");
			text_edit->grab_click_focus();
			MessageQueue::get_singleton()->flush();

			// Drag and drop selected text to mouse position.
			text_edit->select(0, 0, 0, 4);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 11).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 11).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == " test\ndrop here 'drag'");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 15);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 11);

			// Undo.
			text_edit->undo();
			CHECK(text_edit->get_text() == "drag test\ndrop here ''");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);

			// Redo.
			text_edit->redo();
			CHECK(text_edit->get_text() == " test\ndrop here 'drag'");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 15);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 11);

			// Hold control when dropping to not delete selected text.
			text_edit->select(1, 10, 1, 16);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 12).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "'drag'");
			CHECK(text_edit->has_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_KEY_EVENT(Key::CMD_OR_CTRL);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			SEND_GUI_KEY_UP_EVENT(Key::CMD_OR_CTRL);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' test\ndrop here 'drag'");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);

			// Multiple caret drags entire selection.
			text_edit->select(0, 11, 0, 7, 0);
			text_edit->add_caret(1, 2);
			text_edit->select(1, 2, 1, 4, 1);
			text_edit->add_caret(1, 12);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 3).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection(true, 1));
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 12).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "test\nop");
			// Carets aren't removed from dragging, only dropping.
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 7);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 11);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 4);
			CHECK(text_edit->get_selection_origin_line(1) == 1);
			CHECK(text_edit->get_selection_origin_column(1) == 2);
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 1);
			CHECK(text_edit->get_caret_column(2) == 12);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 9).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 9).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' \ndr heretest\nop 'drag'");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 7);

			// Drop onto same selection should do effectively nothing.
			text_edit->select(1, 3, 1, 7);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(1, 6).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "here");
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' \ndr heretest\nop 'drag'");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 1);
			CHECK(text_edit->get_selection_origin_column() == 3);

			// Cannot drag when drag and drop selection is disabled. It becomes regular drag to select.
			text_edit->set_drag_and_drop_selection_enabled(false);
			text_edit->select(0, 1, 0, 5);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' \ndr heretest\nop 'drag'");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 2);
			text_edit->set_drag_and_drop_selection_enabled(true);

			// Cancel drag and drop from Escape key.
			text_edit->select(0, 1, 0, 5);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 3).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag");
			SEND_GUI_KEY_EVENT(Key::ESCAPE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' \ndr heretest\nop 'drag'");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 1);

			// Cancel drag and drop from caret move key input.
			text_edit->select(0, 1, 0, 5);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 3).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag");
			SEND_GUI_KEY_EVENT(Key::RIGHT);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'drag' \ndr heretest\nop 'drag'");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);

			// Cancel drag and drop from text key input.
			text_edit->select(0, 1, 0, 5);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 3).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag");
			SEND_GUI_KEY_EVENT(Key::A);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "'A' \ndr heretest\nop 'drag'");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 2);
		}

		SUBCASE("[TextEdit] text drag to another text edit") {
			TextEdit *target_text_edit = memnew(TextEdit);
			SceneTree::get_singleton()->get_root()->add_child(target_text_edit);

			target_text_edit->set_size(Size2(200, 200));
			target_text_edit->set_position(Point2(400, 0));

			text_edit->set_size(Size2(200, 200));

			CHECK_FALSE(text_edit->is_mouse_over_selection());
			text_edit->set_text("drag me");
			text_edit->select_all();
			text_edit->grab_click_focus();
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			MessageQueue::get_singleton()->flush();

			// Drag text between text edits.
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 0).get_center(), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 7).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag me");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);

			Point2i target_line0 = target_text_edit->get_position() + Point2i(1, target_text_edit->get_line_height() / 2);
			SEND_GUI_MOUSE_MOTION_EVENT(target_line0, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 0);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_line0, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(target_text_edit->get_text() == "drag me");
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 7);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 0);

			// Undo is separate per TextEdit.
			text_edit->undo();
			CHECK(text_edit->get_text() == "drag me");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(target_text_edit->get_text() == "drag me");
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 7);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 0);

			target_text_edit->undo();
			CHECK(text_edit->get_text() == "drag me");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(target_text_edit->get_text() == "");
			CHECK_FALSE(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 0);

			// Redo is also separate.
			text_edit->redo();
			CHECK(text_edit->get_text() == "");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(target_text_edit->get_text() == "");
			CHECK_FALSE(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 0);

			target_text_edit->redo();
			CHECK(text_edit->get_text() == "");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(target_text_edit->get_text() == "drag me");
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 7);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 0);

			// Hold control to not remove selected text.
			text_edit->set_text("drag test\ndrop test");
			MessageQueue::get_singleton()->flush();
			target_text_edit->select(0, 0, 0, 3, 0);
			target_text_edit->add_caret(0, 5);
			text_edit->select(0, 5, 0, 7, 0);
			text_edit->add_caret(0, 1);
			text_edit->select(0, 1, 0, 0, 1);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 5).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection(true, 0));
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 6).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "d\nte");
			CHECK(text_edit->has_selection());
			SEND_GUI_KEY_EVENT(Key::CMD_OR_CTRL);
			SEND_GUI_MOUSE_MOTION_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 6).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 6).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			SEND_GUI_KEY_UP_EVENT(Key::CMD_OR_CTRL);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "drag test\ndrop test");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 7);
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 0);
			CHECK(target_text_edit->get_text() == "drag md\ntee");
			CHECK(target_text_edit->get_caret_count() == 1);
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 1);
			CHECK(target_text_edit->get_caret_column() == 2);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 6);

			// Drop onto selected text deletes the selected text first.
			text_edit->set_deselect_on_focus_loss_enabled(false);
			target_text_edit->set_deselect_on_focus_loss_enabled(false);
			text_edit->remove_secondary_carets();
			text_edit->select(0, 5, 0, 9);
			target_text_edit->select(0, 6, 0, 8);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 6).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection(true, 0));
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center(), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "test");
			CHECK(text_edit->has_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "drag \ndrop test");
			CHECK(target_text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(target_text_edit->get_text() == "drag mdtest\ntee");
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 11);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 7);
			text_edit->set_deselect_on_focus_loss_enabled(true);
			target_text_edit->set_deselect_on_focus_loss_enabled(true);

			// Can drop even when drag and drop selection is disabled.
			target_text_edit->set_drag_and_drop_selection_enabled(false);
			text_edit->select(0, 4, 0, 5);
			target_text_edit->deselect();
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 4).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == " ");
			CHECK(text_edit->has_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 7).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "drag\ndrop test");
			CHECK(target_text_edit->get_text() == "drag md test\ntee");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 8);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 7);
			target_text_edit->set_drag_and_drop_selection_enabled(true);

			// Cannot drop when not editable.
			target_text_edit->set_editable(false);
			text_edit->select(0, 1, 0, 4);
			target_text_edit->deselect();
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "rag");
			CHECK(text_edit->has_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "drag\ndrop test");
			CHECK(target_text_edit->get_text() == "drag md test\ntee");
			CHECK(text_edit->has_selection());
			CHECK_FALSE(target_text_edit->has_selection());
			target_text_edit->set_editable(true);

			// Can drag when not editable, but text will not be removed.
			text_edit->set_editable(false);
			text_edit->select(0, 0, 0, 4);
			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->is_mouse_over_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 7).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			CHECK(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_viewport()->gui_get_drag_data() == "drag");
			CHECK(text_edit->has_selection());
			SEND_GUI_MOUSE_MOTION_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 4).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(target_text_edit->get_position() + target_text_edit->get_rect_at_line_column(0, 4).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(text_edit->get_viewport()->gui_is_dragging());
			CHECK(text_edit->get_text() == "drag\ndrop test");
			CHECK(target_text_edit->get_text() == "dragdrag md test\ntee");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			CHECK(target_text_edit->has_selection());
			CHECK(target_text_edit->get_caret_line() == 0);
			CHECK(target_text_edit->get_caret_column() == 8);
			CHECK(target_text_edit->get_selection_origin_line() == 0);
			CHECK(target_text_edit->get_selection_origin_column() == 4);
			text_edit->set_editable(true);

			memdelete(target_text_edit);
		}

		SIGNAL_UNWATCH(text_edit, "text_set");
		SIGNAL_UNWATCH(text_edit, "text_changed");
		SIGNAL_UNWATCH(text_edit, "lines_edited_from");
		SIGNAL_UNWATCH(text_edit, "caret_changed");
	}

	SUBCASE("[TextEdit] overridable actions") {
		DisplayServerMock *DS = (DisplayServerMock *)(DisplayServer::get_singleton());

		SIGNAL_WATCH(text_edit, "text_set");
		SIGNAL_WATCH(text_edit, "text_changed");
		SIGNAL_WATCH(text_edit, "lines_edited_from");
		SIGNAL_WATCH(text_edit, "caret_changed");

		Array lines_edited_args = build_array(build_array(0, 0));

		SUBCASE("[TextEdit] backspace") {
			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Cannot backspace at start of text.
			text_edit->backspace();
			MessageQueue::get_singleton()->flush();
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Backspace at start of line removes the line.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(2, 1));

			text_edit->backspace();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\nsome");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Backspace removes a character.
			lines_edited_args = build_array(build_array(1, 1));
			text_edit->backspace();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\nsom");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Backspace when text is selected removes the selection.
			text_edit->end_complex_operation();
			text_edit->select(1, 0, 1, 3);
			text_edit->backspace();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\n");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cannot backspace if not editable.
			text_edit->set_editable(false);
			text_edit->backspace();
			text_edit->set_editable(true);
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\n");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Undo restores text to the previous end of complex operation.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\nsom");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is\n");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// See ui_text_backspace for more backspace tests.
		}

		SUBCASE("[TextEdit] cut") {
			// Cut without a selection removes the entire line.
			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0));

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n");
			CHECK(text_edit->get_text() == "some\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 3); // In the default font, this is the same position.
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo restores the cut text.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n");
			CHECK(text_edit->get_text() == "this is\nsome\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n");
			CHECK(text_edit->get_text() == "some\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cut with a selection removes just the selection.
			text_edit->set_text("this is\nsome\n");
			text_edit->select(0, 5, 0, 7);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			SEND_GUI_ACTION("ui_cut");
			CHECK(text_edit->get_viewport()->is_input_handled());
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "is");
			CHECK(text_edit->get_text() == "this \nsome\n");
			CHECK_FALSE(text_edit->get_caret_line());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cut does not change the text if not editable. Text is still added to clipboard.
			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(5);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			text_edit->set_editable(true);
			CHECK(DS->clipboard_get() == "this is\n");
			CHECK(text_edit->get_text() == "this is\nsome\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 5);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Cut line with multiple carets.
			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(3);
			text_edit->add_caret(0, 2);
			text_edit->add_caret(0, 4);
			text_edit->add_caret(2, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0), build_array(1, 0));

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n\n");
			CHECK(text_edit->get_text() == "some");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 2); // In the default font, this is the same position.
			// The previous caret at index 1 was merged.
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 3); // In the default font, this is the same position.
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->remove_secondary_carets();

			// Cut on the only line removes the contents.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "some\n");
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_line_count() == 1);
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cut empty line.
			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "\n");
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			// These signals are emitted even if there is no change.
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cut multiple lines, in order.
			text_edit->set_text("this is\nsome\ntext to\nbe\n\ncut");
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(7);
			text_edit->add_caret(3, 0);
			text_edit->add_caret(0, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0), build_array(3, 2), build_array(2, 1));

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\ntext to\nbe\n");
			CHECK(text_edit->get_text() == "some\n\ncut");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->remove_secondary_carets();

			// Cut multiple selections, in order. Ignores regular carets.
			text_edit->set_text("this is\nsome\ntext to\nbe\n\ncut");
			text_edit->add_caret(3, 0);
			text_edit->add_caret(0, 2);
			text_edit->add_caret(2, 0);
			text_edit->select(1, 0, 1, 2, 0);
			text_edit->select(3, 0, 4, 0, 1);
			text_edit->select(0, 5, 0, 3, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 1), build_array(4, 3), build_array(0, 0));

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "s \nso\nbe\n");
			CHECK(text_edit->get_text() == "thiis\nme\ntext to\n\ncut");
			CHECK(text_edit->get_caret_count() == 4);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 3);
			CHECK(text_edit->get_caret_line(3) == 2);
			CHECK(text_edit->get_caret_column(3) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SUBCASE("[TextEdit] copy") {
			text_edit->set_text("this is\nsome\ntest\n\ntext");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Copy selected text.
			text_edit->select(0, 0, 1, 2, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			DS->clipboard_set_primary("");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\nso");
			CHECK(DS->clipboard_get_primary() == "");
			CHECK(text_edit->get_text() == "this is\nsome\ntest\n\ntext");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 0);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 2);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Copy with GUI action.
			text_edit->select(0, 0, 0, 2, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_copy");
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "th");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Can copy even if not editable.
			text_edit->select(2, 4, 1, 2, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			text_edit->copy();
			text_edit->set_editable(true);
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "me\ntest");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->deselect();

			// Copy full line when there is no selection.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Copy empty line.
			text_edit->set_caret_line(3);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "\n");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->deselect();

			// Copy full line with multiple carets on that line only copies once.
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			text_edit->add_caret(1, 0);
			text_edit->add_caret(1, 4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "some\n");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->remove_secondary_carets();

			// Copy selected text from all selections with `\n` in between, in order. Ignore regular carets.
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(4);
			text_edit->add_caret(4, 0);
			text_edit->add_caret(0, 4);
			text_edit->add_caret(1, 0);
			text_edit->select(1, 3, 2, 4, 0);
			text_edit->select(4, 4, 4, 0, 1);
			text_edit->select(0, 5, 0, 4, 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == " \ne\ntest\ntext");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->remove_secondary_carets();
			text_edit->deselect();

			// Copy multiple lines with multiple carets, in order.
			text_edit->set_caret_line(3);
			text_edit->set_caret_column(0);
			text_edit->add_caret(4, 2);
			text_edit->add_caret(0, 4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "this is\n\ntext\n");
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] paste") {
			// Paste text from clipboard at caret.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 1));
			DS->clipboard_set("paste");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste");
			CHECK(text_edit->get_text() == "this is\nsopasteme\n\ntext");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 7);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste");
			CHECK(text_edit->get_text() == "this is\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste");
			CHECK(text_edit->get_text() == "this is\nsopasteme\n\ntext");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 7);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste on empty line. Use GUI action.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(2);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(2, 2));
			DS->clipboard_set("paste2");

			SEND_GUI_ACTION("ui_paste");
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste2");
			CHECK(text_edit->get_text() == "this is\nsome\npaste2\ntext");
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste removes selection before pasting.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->select(0, 5, 1, 3);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0), build_array(0, 0));
			DS->clipboard_set("paste");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste");
			CHECK(text_edit->get_text() == "this pastee\n\ntext");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 10);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste multiple lines.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 3));
			DS->clipboard_set("multi\n\nline\npaste");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "multi\n\nline\npaste");
			CHECK(text_edit->get_text() == "tmulti\n\nline\npastehis is\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste full line after copying it.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 2));
			DS->clipboard_set("");
			text_edit->copy();
			text_edit->set_caret_column(3);
			CHECK(DS->clipboard_get() == "some\n");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "some\n");
			CHECK(text_edit->get_text() == "this is\nsome\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Do not paste as line since it wasn't copied.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1));
			DS->clipboard_set("paste\n");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste\n");
			CHECK(text_edit->get_text() == "thispaste\n is\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste text at each caret.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			text_edit->add_caret(3, 4);
			text_edit->add_caret(0, 4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(2, 3), build_array(5, 6));
			DS->clipboard_set("paste\ntest");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste\ntest");
			CHECK(text_edit->get_text() == "thispaste\ntest is\nsopaste\ntestme\n\ntextpaste\ntest");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->get_caret_line(0) == 3);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_caret_line(1) == 6);
			CHECK(text_edit->get_caret_column(1) == 4);
			CHECK(text_edit->get_caret_line(2) == 1);
			CHECK(text_edit->get_caret_column(2) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->remove_secondary_carets();

			// Paste line per caret when the amount of lines is equal to the number of carets.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(2);
			text_edit->add_caret(3, 4);
			text_edit->add_caret(0, 4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1), build_array(3, 3));
			DS->clipboard_set("paste\ntest\n1");

			text_edit->paste();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "paste\ntest\n1");
			CHECK(text_edit->get_text() == "thispaste is\nsotestme\n\ntext1");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 6);
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 5);
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->remove_secondary_carets();

			// Cannot paste when not editable.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			DS->clipboard_set("no paste");

			text_edit->set_editable(false);
			text_edit->paste();
			text_edit->set_editable(true);
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "no paste");
			CHECK(text_edit->get_text() == "this is\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] paste primary") {
			// Set size for mouse input.
			text_edit->set_size(Size2(200, 200));

			text_edit->grab_focus();
			DS->clipboard_set("");
			DS->clipboard_set_primary("");
			CHECK(DS->clipboard_get_primary() == "");

			// Select text with mouse to put into primary clipboard.
			text_edit->set_text("this is\nsome\n\ntext");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(0, 2).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 3).get_center() + Point2i(2, 0), MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(text_edit->get_rect_at_line_column(1, 3).get_center() + Point2i(2, 0), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			CHECK(DS->clipboard_get() == "");
			CHECK(DS->clipboard_get_primary() == "is is\nsom");
			CHECK(text_edit->get_text() == "this is\nsome\n\ntext");
			CHECK(text_edit->has_selection());
			CHECK(text_edit->get_selected_text() == "is is\nsom");
			CHECK(text_edit->get_selection_origin_line() == 0);
			CHECK(text_edit->get_selection_origin_column() == 2);
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK_FALSE("text_set");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Middle click to paste at mouse.
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 4));

			SEND_GUI_MOUSE_BUTTON_EVENT(text_edit->get_rect_at_line_column(3, 2).get_center() + Point2i(2, 0), MouseButton::MIDDLE, MouseButtonMask::MIDDLE, Key::NONE);
			CHECK(DS->clipboard_get_primary() == "is is\nsom");
			CHECK(text_edit->get_text() == "this is\nsome\n\nteis is\nsomxt");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 4);
			CHECK(text_edit->get_caret_column() == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste at mouse position if there is only one caret.
			text_edit->set_text("this is\nsome\n\ntext");
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::NONE, Key::NONE);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			DS->clipboard_set_primary("paste");
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->paste_primary_clipboard();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get_primary() == "paste");
			CHECK(text_edit->get_text() == "tpastehis is\nsome\n\ntext");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Paste at all carets if there are multiple carets.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(0);
			text_edit->add_caret(2, 0);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(0, 1).get_center() + Point2i(2, 0), MouseButtonMask::NONE, Key::NONE);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			DS->clipboard_set_primary("paste");
			lines_edited_args = build_array(build_array(1, 1), build_array(2, 2));

			text_edit->paste_primary_clipboard();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get_primary() == "paste");
			CHECK(text_edit->get_text() == "this is\npastesome\npaste\ntext");
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Cannot paste if not editable.
			text_edit->set_text("this is\nsome\n\ntext");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			SEND_GUI_MOUSE_MOTION_EVENT(text_edit->get_rect_at_line_column(1, 3).get_center() + Point2i(2, 0), MouseButtonMask::NONE, Key::NONE);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			DS->clipboard_set("no paste");

			text_edit->set_editable(false);
			text_edit->paste_primary_clipboard();
			text_edit->set_editable(true);
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "no paste");
			CHECK(text_edit->get_text() == "this is\nsome\n\ntext");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 4);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] cut when empty selection clipboard disabled") {
			text_edit->set_empty_selection_clipboard_enabled(false);
			DS->clipboard_set("");

			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			text_edit->cut();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "");
			CHECK(text_edit->get_text() == "this is\nsome\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] copy when empty selection clipboard disabled") {
			text_edit->set_empty_selection_clipboard_enabled(false);
			DS->clipboard_set("");

			text_edit->set_text("this is\nsome\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(6);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			text_edit->copy();
			MessageQueue::get_singleton()->flush();
			CHECK(DS->clipboard_get() == "");
			CHECK(text_edit->get_text() == "this is\nsome\n");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SIGNAL_UNWATCH(text_edit, "text_set");
		SIGNAL_UNWATCH(text_edit, "text_changed");
		SIGNAL_UNWATCH(text_edit, "lines_edited_from");
		SIGNAL_UNWATCH(text_edit, "caret_changed");
	}

	SUBCASE("[TextEdit] input") {
		SIGNAL_WATCH(text_edit, "text_set");
		SIGNAL_WATCH(text_edit, "text_changed");
		SIGNAL_WATCH(text_edit, "lines_edited_from");
		SIGNAL_WATCH(text_edit, "caret_changed");

		Array lines_edited_args = build_array(build_array(0, 0));

		SUBCASE("[TextEdit] ui_text_newline_above") {
			text_edit->set_text("this is some test text.\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Insert new line above.
			text_edit->select(0, 0, 0, 4);
			text_edit->add_caret(1, 4);
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(2, 3));

			SEND_GUI_ACTION("ui_text_newline_above");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\nthis is some test text.\n\nthis is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some test text.\nthis is some test text.");
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\nthis is some test text.\n\nthis is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(4);
			text_edit->set_caret_line(3, false, true, -1, 1);
			text_edit->set_caret_column(4, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_newline_above");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\nthis is some test text.\n\nthis is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// Works on first line, empty lines, and only happens at caret for selections.
			text_edit->select(1, 10, 0, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(4, 5));

			SEND_GUI_ACTION("ui_text_newline_above");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n\nthis is some test text.\n\n\nthis is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 4);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Insert multiple new lines above from one line.
			text_edit->set_text("test");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(1);
			text_edit->add_caret(0, 3);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(1, 2));

			SEND_GUI_ACTION("ui_text_newline_above");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n\ntest");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SUBCASE("[TextEdit] ui_text_newline_blank") {
			text_edit->set_text("this is some test text.\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");

			// Insert new line below.
			text_edit->select(0, 0, 0, 4);
			text_edit->add_caret(1, 4);
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(2, 3));

			SEND_GUI_ACTION("ui_text_newline_blank");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is some test text.\n\nthis is some test text.\n");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some test text.\nthis is some test text.");
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some test text.\n\nthis is some test text.\n");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_newline_blank");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is some test text.\n\nthis is some test text.\n");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// Insert multiple new lines below from one line.
			text_edit->set_text("test");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(1);
			text_edit->add_caret(0, 3);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 1), build_array(0, 1));

			SEND_GUI_ACTION("ui_text_newline_blank");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "test\n\n");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 2);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SUBCASE("[TextEdit] ui_text_newline") {
			text_edit->set_text("this is some test text.\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Insert new line at caret.
			text_edit->select(0, 0, 0, 4);
			text_edit->add_caret(1, 4);
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			// Lines edited: deletion, insert line, insert line.
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 1), build_array(2, 3));

			SEND_GUI_ACTION("ui_text_newline");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n is some test text.\nthis\n is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some test text.\nthis is some test text.");
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\n is some test text.\nthis\n is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_newline");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n is some test text.\nthis\n is some test text.");
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);
		}

		SUBCASE("[TextEdit] ui_text_backspace_all_to_left") {
			Ref<InputEvent> tmpevent = InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL);
			InputMap::get_singleton()->action_add_event("ui_text_backspace_all_to_left", tmpevent);

			text_edit->set_text("\nthis is some test text.\n\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Remove all text to the left.
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(5);
			text_edit->add_caret(1, 2);
			text_edit->add_caret(1, 8);
			lines_edited_args = build_array(build_array(1, 1));
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\nsome test text.\n\nthis is some test text.");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\nthis is some test text.\n\nthis is some test text.");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 1);
			CHECK(text_edit->get_caret_column(2) == 8);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\nsome test text.\n\nthis is some test text.");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Acts as a normal backspace with selections.
			text_edit->select(1, 5, 1, 9, 0);
			text_edit->add_caret(3, 4);
			text_edit->select(3, 7, 3, 4, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 3), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\nsome  text.\n\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Acts as a normal backspace when at the start of a line.
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 2), build_array(1, 0));

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "some  text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(text_edit->get_line(1).length(), false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "some  text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == text_edit->get_line(0).length());
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == text_edit->get_line(1).length());
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// Remove entire line content when at the end of the line.
			lines_edited_args = build_array(build_array(1, 1), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->remove_secondary_carets();

			// Removing newline effectively happens after removing text.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(0);
			text_edit->add_caret(1, 4);

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "tests");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			text_edit->remove_secondary_carets();

			// Removing newline effectively happens after removing text, reverse caret order.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(4);
			text_edit->add_caret(1, 0);

			SEND_GUI_ACTION("ui_text_backspace_all_to_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "tests");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			text_edit->remove_secondary_carets();

			InputMap::get_singleton()->action_erase_event("ui_text_backspace_all_to_left", tmpevent);
		}

		SUBCASE("[TextEdit] ui_text_backspace_word") {
			text_edit->set_text("\nthis is some test text.\n\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");

			// Acts as a normal backspace with selections.
			text_edit->select(1, 8, 1, 15);
			text_edit->add_caret(3, 6);
			text_edit->select(3, 10, 3, 6, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 3), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\nthis is st text.\n\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 8);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->end_complex_operation();

			lines_edited_args = build_array(build_array(3, 2), build_array(1, 0));

			// Start of line should also be a normal backspace.
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// FIXME: Remove after GH-77101 is fixed.
			text_edit->start_action(TextEdit::ACTION_NONE);

			// Remove text to the start of the word to the left of the caret.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(12, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 1), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st \nthis ime t text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 11);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 16);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 12);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is st \nthis ime t text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 11);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Removing newline effectively happens after removing text.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(0);
			text_edit->add_caret(1, 4);

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "tests");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			text_edit->remove_secondary_carets();

			// Removing newline effectively happens after removing text, reverse caret order.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(4);
			text_edit->add_caret(1, 0);

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "tests");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			text_edit->remove_secondary_carets();

			// Remove when there are no words, only symbols.
			text_edit->set_text("#{}");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(3);

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
		}

		SUBCASE("[TextEdit] ui_text_backspace_word same line") {
			text_edit->set_text("test longwordtest test");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Multiple carets on the same line is handled.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			text_edit->add_caret(0, 11);
			text_edit->add_caret(0, 15);
			text_edit->add_caret(0, 9);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " st test");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test longwordtest test");
			CHECK(text_edit->get_caret_count() == 4);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 11);
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 15);
			CHECK_FALSE(text_edit->has_selection(3));
			CHECK(text_edit->get_caret_line(3) == 0);
			CHECK(text_edit->get_caret_column(3) == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " st test");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SUBCASE("[TextEdit] ui_text_backspace") {
			text_edit->set_text("\nthis is some test text.\n\nthis is some test text.");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");

			// Remove selected text when there are selections.
			text_edit->select(1, 0, 1, 4);
			text_edit->add_caret(3, 4);
			text_edit->select(3, 5, 3, 2, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 3), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n is some test text.\n\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo remove selection.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_text() == "\nthis is some test text.\n\nthis is some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK(text_edit->get_selection_origin_line(1) == 3);
			CHECK(text_edit->get_selection_origin_column(1) == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo remove selection.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\n is some test text.\n\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Remove the newline when at start of line.
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(3, 2), build_array(1, 0));

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo remove newline.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "\n is some test text.\n\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 3);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo remove newline.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(15, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == text_edit->get_line(0).length());
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// FIXME: Remove after GH-77101 is fixed.
			text_edit->start_action(TextEdit::ACTION_NONE);

			// Backspace removes character to the left.
			lines_edited_args = build_array(build_array(1, 1), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text\nthis some testtext.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 18);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 14);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Backspace another character without changing caret.
			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test tex\nthis some testext.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 17);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 13);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo both backspaces.
			lines_edited_args = build_array(build_array(1, 1), build_array(0, 0), build_array(1, 1), build_array(0, 0));

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 19);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo both backspaces.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test tex\nthis some testext.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 17);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 13);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Backspace with multiple carets that will overlap.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(8);
			text_edit->add_caret(0, 7);
			text_edit->add_caret(0, 9);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is sotest tex\nthis some testext.");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Select each line of text, from right to left. Remove selection to column 0.
			text_edit->select(0, text_edit->get_line(0).length(), 0, 0);
			text_edit->add_caret(1, 0);
			text_edit->select(1, text_edit->get_line(1).length(), 1, 0, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 1), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_text() == "\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Backspace at start of first line does nothing.
			text_edit->remove_secondary_carets();
			text_edit->deselect();
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_backspace");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_delete_all_to_right") {
			Ref<InputEvent> tmpevent = InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL);
			InputMap::get_singleton()->action_add_event("ui_text_delete_all_to_right", tmpevent);

			text_edit->set_text("this is some test text.\nthis is some test text.\n");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");

			// Remove all text to right of caret.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(18);
			text_edit->add_caret(0, 16);
			text_edit->add_caret(0, 20);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0));

			SEND_GUI_ACTION("ui_text_delete_all_to_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is some tes\nthis is some test text.\n");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 16);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			lines_edited_args = build_array(build_array(0, 0));

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some test text.\nthis is some test text.\n");
			CHECK(text_edit->get_caret_count() == 3);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 18);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 16);
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 20);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is some tes\nthis is some test text.\n");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 16);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Acts as a normal delete with selections.
			text_edit->select(0, 0, 0, 4);
			text_edit->add_caret(1, 4);
			text_edit->select(1, 8, 1, 4, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_delete_all_to_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some tes\nthissome test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 4);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does nothing when caret is at end of line.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(text_edit->get_line(1).length(), false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_delete_all_to_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some tes\nthissome test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == text_edit->get_line(0).length());
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == text_edit->get_line(1).length());
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Does not work if not editable.
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_delete_all_to_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some tes\nthissome test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// Delete entire line.
			SEND_GUI_ACTION("ui_text_delete_all_to_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "\n\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			InputMap::get_singleton()->action_erase_event("ui_text_delete_all_to_right", tmpevent);
		}

		SUBCASE("[TextEdit] ui_text_delete_word") {
			text_edit->set_caret_mid_grapheme_enabled(true);
			CHECK(text_edit->is_caret_mid_grapheme_enabled());

			text_edit->set_text("this is some test text.\n\nthis is some test text.\n");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Acts as a normal delete with selections.
			text_edit->select(0, 8, 0, 15);
			text_edit->add_caret(2, 6);
			text_edit->select(2, 10, 2, 6, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(2, 2));

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st text.\n\nthis ime test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 8);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 6);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Removes newlines when at end of line.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(text_edit->get_line(2).length(), false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0), build_array(2, 1));

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == text_edit->get_line(0).length());
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == text_edit->get_line(1).length());
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(10, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 10);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// FIXME: Remove after GH-77101 is fixed.
			text_edit->start_action(TextEdit::ACTION_NONE);

			// Delete to the end of the word right of the caret.
			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is st text.\nthis ime t text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 10);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "this is st text.\nthis ime test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 10);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is st text.\nthis ime t text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 10);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Delete one word with multiple carets.
			text_edit->remove_secondary_carets();
			text_edit->set_text("onelongword test");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(6);
			text_edit->add_caret(0, 9);
			text_edit->add_caret(0, 3);
			lines_edited_args = build_array(build_array(0, 0));
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "one test");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 3);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Removing newline effectively happens after removing text.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(2);
			text_edit->add_caret(0, 4);

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "telines");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 2);
			text_edit->remove_secondary_carets();

			// Removing newline effectively happens after removing text, reverse caret order.
			text_edit->set_text("test\nlines");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			text_edit->add_caret(0, 2);

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "telines");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 2);
			text_edit->remove_secondary_carets();

			// Remove when there are no words, only symbols.
			text_edit->set_text("#{}");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK_FALSE(text_edit->has_selection());
			CHECK(text_edit->get_text() == "");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
		}

		SUBCASE("[TextEdit] ui_text_delete_word same line") {
			text_edit->set_text("test longwordtest test");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Multiple carets on the same line is handled.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			text_edit->add_caret(0, 11);
			text_edit->add_caret(0, 15);
			text_edit->add_caret(0, 9);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

			SEND_GUI_ACTION("ui_text_delete_word");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " long test");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0));

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "test longwordtest test");
			CHECK(text_edit->get_caret_count() == 4);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 11);
			CHECK_FALSE(text_edit->has_selection(2));
			CHECK(text_edit->get_caret_line(2) == 0);
			CHECK(text_edit->get_caret_column(2) == 15);
			CHECK_FALSE(text_edit->has_selection(3));
			CHECK(text_edit->get_caret_line(3) == 0);
			CHECK(text_edit->get_caret_column(3) == 9);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " long test");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 0);
			CHECK(text_edit->get_caret_column(1) == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SUBCASE("[TextEdit] ui_text_delete") {
			text_edit->set_caret_mid_grapheme_enabled(true);
			CHECK(text_edit->is_caret_mid_grapheme_enabled());

			text_edit->set_text("this is some test text.\n\nthis is some test text.\n");
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Remove selected text when there are selections.
			text_edit->select(0, 0, 0, 4);
			text_edit->add_caret(2, 2);
			text_edit->select(2, 5, 2, 2, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(0, 0), build_array(2, 2));

			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text.\n\nthis some test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo remove selection.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->get_text() == "this is some test text.\n\nthis is some test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 4);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 0);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 5);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo remove selection.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\n\nthis some test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Remove newline when at end of line.
			text_edit->set_caret_column(text_edit->get_line(0).length());
			text_edit->set_caret_column(text_edit->get_line(2).length(), false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			lines_edited_args = build_array(build_array(1, 0), build_array(2, 1));

			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 19);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 20);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo remove newline.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\n\nthis some test text.\n");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 19);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 20);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo remove newline.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 19);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 20);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(15, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			text_edit->set_editable(false);
			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			// FIXME: Remove after GH-77101 is fixed.
			text_edit->start_action(TextEdit::EditAction::ACTION_NONE);

			// Delete removes character to the right.
			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1));

			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "is some test text.\nthis some test ext.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Delete another character without changing caret.
			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "s some test text.\nthis some test xt.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo both deletes.
			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1), build_array(0, 0), build_array(1, 1));

			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == " is some test text.\nthis some test text.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo both deletes.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "s some test text.\nthis some test xt.");
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 15);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Delete at end of last line does nothing.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(18);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_delete");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "s some test text.\nthis some test xt.");
			CHECK(text_edit->get_caret_count() == 1);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 18);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_word_left") {
			text_edit->set_text("\nthis is some test text.\nthis is some test text.");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(15);
			text_edit->add_caret(2, 10);
			text_edit->select(1, 10, 1, 15);
			text_edit->select(2, 15, 2, 10, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Deselect to start of previous word when selection is right to left.
			// Select to start of next word when selection is left to right.
#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::ALT | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);

			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "me ");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 13);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 10);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "some te");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 8);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 15);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Select to start of word with shift.
			text_edit->deselect();
			text_edit->set_caret_column(7);
			text_edit->set_caret_column(16, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::ALT | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);

			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "is");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 5);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 7);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "tes");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 13);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 16);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Deselect and move caret to start of next word without shift.
			SEND_GUI_ACTION("ui_text_caret_word_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 8);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Moves to end of previous line when at start of line. Does nothing at start of text.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_caret_word_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 23);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Move when there are no words, only symbols.
			text_edit->set_text("#{}");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(3);

			SEND_GUI_ACTION("ui_text_caret_word_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
		}

		SUBCASE("[TextEdit] ui_text_caret_left") {
			text_edit->set_text("\nthis is some test text.\nthis is some test text.");
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(7);
			text_edit->select(1, 3, 1, 7);
			text_edit->add_caret(2, 3);
			text_edit->select(2, 7, 2, 3, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Remove one character from selection when selection is left to right.
			// Add one character to selection when selection is right to left.
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "s i");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 6);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 3);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "is is");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 7);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Deselect and put caret at selection start without shift.
			SEND_GUI_ACTION("ui_text_caret_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 3);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Move caret one character to the left.
			SEND_GUI_ACTION("ui_text_caret_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 2);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Select one character to the left with shift and no existing selection.
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "h");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 1);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 2);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "t");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 0);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 1);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Moves to end of previous line when at start of line. Does nothing at start of text.
			text_edit->deselect();
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);
			text_edit->set_caret_column(0, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_caret_left");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 23);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Selects to end of previous line when at start of line.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(0);
			text_edit->select(1, 1, 1, 0);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "\nt");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 1);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Merge selections when they overlap.
			text_edit->set_caret_line(1);
			text_edit->set_caret_column(4);
			text_edit->select(1, 6, 1, 4);
			text_edit->add_caret(1, 8);
			text_edit->select(1, 8, 1, 6, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			CHECK(text_edit->get_caret_count() == 2);

			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "s is ");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 3);
			CHECK(text_edit->get_selection_origin_line(0) == 1);
			CHECK(text_edit->get_selection_origin_column(0) == 8);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_word_right") {
			text_edit->set_text("this is some test text\n\nthis is some test text");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(15);
			text_edit->add_caret(2, 10);
			text_edit->select(0, 10, 0, 15);
			text_edit->select(2, 15, 2, 10, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Select to end of next word when selection is right to left.
			// Deselect to end of previous word when selection is left to right.
#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::ALT | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "me test");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 17);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 10);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == " te");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 12);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 15);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Select to end of word with shift.
			text_edit->deselect();
			text_edit->set_caret_column(13);
			text_edit->set_caret_column(15, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::ALT | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "test");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 17);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 13);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "st");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 17);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 15);
			CHECK(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Deselect and move caret to end of next word without shift.
			SEND_GUI_ACTION("ui_text_caret_word_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 22);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 22);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Moves to start of next line when at end of line. Does nothing at end of text.
			SEND_GUI_ACTION("ui_text_caret_word_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 22);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Move when there are no words, only symbols.
			text_edit->set_text("#{}");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);

			SEND_GUI_ACTION("ui_text_caret_word_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 3);
		}

		SUBCASE("[TextEdit] ui_text_caret_right") {
			text_edit->set_text("this is some test text\n\nthis is some test text");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(19);
			text_edit->select(0, 15, 0, 19);
			text_edit->add_caret(2, 15);
			text_edit->select(2, 19, 2, 15, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Remove one character from selection when selection is right to left.
			// Add one character to selection when selection is left to right.
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "st te");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 20);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 15);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "t t");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 16);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 19);
			CHECK_FALSE(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Deselect and put caret at selection end without shift.
			SEND_GUI_ACTION("ui_text_caret_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 20);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 19);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Move caret one character to the right.
			SEND_GUI_ACTION("ui_text_caret_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 21);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 20);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Select one character to the right with shift and no existing selection.
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "t");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 22);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 21);
			CHECK(text_edit->is_caret_after_selection_origin(0));

			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "x");
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 21);
			CHECK(text_edit->get_selection_origin_line(1) == 2);
			CHECK(text_edit->get_selection_origin_column(1) == 20);
			CHECK(text_edit->is_caret_after_selection_origin(1));

			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Moves to start of next line when at end of line. Does nothing at end of text.
			text_edit->deselect();
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(22);
			text_edit->set_caret_column(22, false, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_ACTION("ui_text_caret_right");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 2);
			CHECK_FALSE(text_edit->has_selection(0));
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			CHECK(text_edit->get_caret_line(1) == 2);
			CHECK(text_edit->get_caret_column(1) == 22);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Selects to start of next line when at end of line.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(22);
			text_edit->select(0, 21, 0, 22);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");

			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "t\n");
			CHECK(text_edit->get_caret_line(0) == 1);
			CHECK(text_edit->get_caret_column(0) == 0);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 21);
			CHECK(text_edit->is_caret_after_selection_origin(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Merge selections when they overlap.
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(4);
			text_edit->select(0, 4, 0, 6);
			text_edit->add_caret(0, 8);
			text_edit->select(0, 6, 0, 8, 1);
			MessageQueue::get_singleton()->flush();
			SIGNAL_DISCARD("caret_changed");
			CHECK(text_edit->get_caret_count() == 2);

			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_count() == 1);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == " is s");
			CHECK(text_edit->get_caret_line(0) == 0);
			CHECK(text_edit->get_caret_column(0) == 9);
			CHECK(text_edit->get_selection_origin_line(0) == 0);
			CHECK(text_edit->get_selection_origin_column(0) == 4);
			CHECK(text_edit->is_caret_after_selection_origin(0));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_up") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("this is some\nother test\nlines\ngo here\nthis is some\nother test\nlines\ngo here");
			text_edit->set_caret_line(3);
			text_edit->set_caret_column(7);

			text_edit->add_caret(7, 7);
			CHECK(text_edit->get_caret_count() == 2);

			MessageQueue::get_singleton()->flush();
			// Lines 0 and 4 are wrapped into 2 parts: 'this is ' and 'some'.
			CHECK(text_edit->is_line_wrapped(0));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Select + up should select everything to the left on that line.
			SEND_GUI_KEY_EVENT(Key::UP | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->get_selected_text(0) == "\ngo here");
			CHECK(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 6);
			CHECK(text_edit->get_caret_column(1) == 5);
			CHECK(text_edit->get_selected_text(1) == "\ngo here");
			CHECK(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Should deselect and move up.
			SEND_GUI_ACTION("ui_text_caret_up");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 5);
			CHECK(text_edit->get_caret_column(1) == 8);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal up over wrapped line.
			SEND_GUI_ACTION("ui_text_caret_up");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 4);
			CHECK(text_edit->get_caret_column(1) == 12);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal up over wrapped line to line 0.
			text_edit->set_caret_column(12, false);
			SEND_GUI_ACTION("ui_text_caret_up");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 4);
			CHECK(text_edit->get_caret_column(1) == 7);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal up from column 0 to a wrapped line.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(5);
			text_edit->set_caret_column(0);
			SEND_GUI_ACTION("ui_text_caret_up");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 4);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->has_selection(0));

			// Normal up to column 0 of a wrapped line.
			SEND_GUI_ACTION("ui_text_caret_up");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 4);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection(0));
		}

		SUBCASE("[TextEdit] ui_text_caret_down") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("go here\nlines\nother test\nthis is some\ngo here\nlines\nother test\nthis is some");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(7);

			text_edit->add_caret(4, 7);
			CHECK(text_edit->get_caret_count() == 2);

			MessageQueue::get_singleton()->flush();

			// Lines 3 and 7 are wrapped into 2 parts: 'this is ' and 'some'.
			CHECK(text_edit->is_line_wrapped(3));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			// Select + down should select everything to the right on that line.
			SEND_GUI_KEY_EVENT(Key::DOWN | KeyModifierMask::SHIFT);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 1);
			CHECK(text_edit->get_caret_column() == 5);
			CHECK(text_edit->get_selected_text(0) == "\nlines");
			CHECK(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 5);
			CHECK(text_edit->get_caret_column(1) == 5);
			CHECK(text_edit->get_selected_text(1) == "\nlines");
			CHECK(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Should deselect and move down.
			SEND_GUI_ACTION("ui_text_caret_down");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 2);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 6);
			CHECK(text_edit->get_caret_column(1) == 8);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal down over wrapped line.
			SEND_GUI_ACTION("ui_text_caret_down");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 7);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 7);
			CHECK(text_edit->get_caret_column(1) == 7);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal down over wrapped line to last wrapped line.
			text_edit->set_caret_column(7, false);
			SEND_GUI_ACTION("ui_text_caret_down");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 7);
			CHECK(text_edit->get_caret_column(1) == 12);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			// Normal down to column 0 of a wrapped line.
			text_edit->remove_secondary_carets();
			text_edit->set_caret_line(3);
			text_edit->set_caret_column(0);
			SEND_GUI_ACTION("ui_text_caret_down");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 8);
			CHECK_FALSE(text_edit->has_selection(0));

			// Normal down out of visual column 0 of a wrapped line moves to start of next line.
			SEND_GUI_ACTION("ui_text_caret_down");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 4);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection(0));
		}

		SUBCASE("[TextEdit] ui_text_caret_document_start") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("this is some\nother test\nlines\ngo here");
			text_edit->set_caret_line(4);
			text_edit->set_caret_column(7);

			text_edit->add_caret(3, 2);
			CHECK(text_edit->get_caret_count() == 2);

			MessageQueue::get_singleton()->flush();

			CHECK(text_edit->is_line_wrapped(0));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::UP | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::HOME | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is some\nother test\nlines\ngo here");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK(text_edit->get_selected_text() == "this is some\nother test\nlines\ngo here");
			CHECK(text_edit->has_selection());
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			CHECK(text_edit->get_caret_count() == 1);

			SEND_GUI_ACTION("ui_text_caret_document_start");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "this is some\nother test\nlines\ngo here");
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_document_end") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("go here\nlines\nother test\nthis is some");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);

			text_edit->add_caret(1, 0);
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();

			CHECK(text_edit->is_line_wrapped(3));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::DOWN | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::END | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "go here\nlines\nother test\nthis is some");
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK(text_edit->get_selected_text() == "go here\nlines\nother test\nthis is some");
			CHECK(text_edit->has_selection());
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			CHECK(text_edit->get_caret_count() == 1);

			SEND_GUI_ACTION("ui_text_caret_document_end");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "go here\nlines\nother test\nthis is some");
			CHECK(text_edit->get_caret_line() == 3);
			CHECK(text_edit->get_caret_column() == 12);
			CHECK_FALSE(text_edit->has_selection());
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_line_start") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("  this is some\n  this is some");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(text_edit->get_line(0).length());

			text_edit->add_caret(1, text_edit->get_line(1).length());
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();

			CHECK(text_edit->is_line_wrapped(0));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::LEFT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::HOME | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 10);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "some");

			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 10);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "some");
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			SEND_GUI_ACTION("ui_text_caret_line_start");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 2);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			SEND_GUI_ACTION("ui_text_caret_line_start");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 0);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 0);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			SEND_GUI_ACTION("ui_text_caret_line_start");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 2);
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 2);
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] ui_text_caret_line_end") {
			text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);

			text_edit->set_size(Size2(110, 100));
			text_edit->set_text("  this is some\n  this is some");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);

			text_edit->add_caret(1, 0);
			CHECK(text_edit->get_caret_count() == 2);
			MessageQueue::get_singleton()->flush();

			CHECK(text_edit->is_line_wrapped(0));
			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

#ifdef MACOS_ENABLED
			SEND_GUI_KEY_EVENT(Key::RIGHT | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT);
#else
			SEND_GUI_KEY_EVENT(Key::END | KeyModifierMask::SHIFT);
#endif
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == 9);
			CHECK(text_edit->has_selection(0));
			CHECK(text_edit->get_selected_text(0) == "  this is");

			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == 9);
			CHECK(text_edit->has_selection(1));
			CHECK(text_edit->get_selected_text(1) == "  this is");
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");

			SEND_GUI_ACTION("ui_text_caret_line_end");
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line() == 0);
			CHECK(text_edit->get_caret_column() == text_edit->get_line(0).length());
			CHECK_FALSE(text_edit->has_selection(0));

			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_caret_line(1) == 1);
			CHECK(text_edit->get_caret_column(1) == text_edit->get_line(1).length());
			CHECK_FALSE(text_edit->has_selection(1));
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
		}

		SUBCASE("[TextEdit] unicode") {
			text_edit->set_text("\n");
			text_edit->set_caret_line(0);
			text_edit->set_caret_column(0);

			text_edit->add_caret(1, 0);
			CHECK(text_edit->get_caret_count() == 2);
			text_edit->insert_text_at_caret("a");
			MessageQueue::get_singleton()->flush();

			SIGNAL_DISCARD("text_set");
			SIGNAL_DISCARD("text_changed");
			SIGNAL_DISCARD("lines_edited_from");
			SIGNAL_DISCARD("caret_changed");

			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1));

			SEND_GUI_KEY_EVENT(Key::A);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "aA\naA");
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Undo reverts both carets.
			text_edit->undo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "a\na");
			CHECK(text_edit->get_caret_column() == 1);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", reverse_nested(lines_edited_args));

			// Redo.
			text_edit->redo();
			MessageQueue::get_singleton()->flush();
			CHECK(text_edit->get_text() == "aA\naA");
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			// Does not work if not editable.
			text_edit->set_editable(false);
			SEND_GUI_KEY_EVENT(Key::A);
			CHECK_FALSE(text_edit->get_viewport()->is_input_handled()); // Should this be handled?
			CHECK(text_edit->get_text() == "aA\naA");
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK_FALSE("caret_changed");
			SIGNAL_CHECK_FALSE("text_changed");
			SIGNAL_CHECK_FALSE("lines_edited_from");
			text_edit->set_editable(true);

			lines_edited_args = build_array(build_array(0, 0), build_array(0, 0), build_array(1, 1), build_array(1, 1));

			text_edit->select(0, 0, 0, 1);
			text_edit->select(1, 0, 1, 1, 1);
			SEND_GUI_KEY_EVENT(Key::B);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "BA\nBA");
			CHECK(text_edit->get_caret_column() == 1);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			SEND_GUI_ACTION("ui_text_toggle_insert_mode");
			CHECK(text_edit->is_overtype_mode_enabled());

			SEND_GUI_KEY_EVENT(Key::B);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "BB\nBB");
			CHECK(text_edit->get_caret_column() == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);

			text_edit->select(0, 0, 0, 1);
			text_edit->select(1, 0, 1, 1, 1);
			SEND_GUI_KEY_EVENT(Key::A);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "AB\nAB");
			CHECK(text_edit->get_caret_column() == 1);
			CHECK(text_edit->get_caret_column(1) == 1);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
			text_edit->set_overtype_mode_enabled(false);
			CHECK_FALSE(text_edit->is_overtype_mode_enabled());

			lines_edited_args = build_array(build_array(0, 0), build_array(1, 1));

			SEND_GUI_KEY_EVENT(Key::TAB);
			CHECK(text_edit->get_viewport()->is_input_handled());
			CHECK(text_edit->get_text() == "A\tB\nA\tB");
			CHECK(text_edit->get_caret_column() == 2);
			CHECK(text_edit->get_caret_column(1) == 2);
			SIGNAL_CHECK("caret_changed", empty_signal_args);
			SIGNAL_CHECK("text_changed", empty_signal_args);
			SIGNAL_CHECK("lines_edited_from", lines_edited_args);
		}

		SIGNAL_UNWATCH(text_edit, "text_set");
		SIGNAL_UNWATCH(text_edit, "text_changed");
		SIGNAL_UNWATCH(text_edit, "lines_edited_from");
		SIGNAL_UNWATCH(text_edit, "caret_changed");
	}

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] context menu") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	text_edit->set_size(Size2(800, 200));
	text_edit->set_line(0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	MessageQueue::get_singleton()->flush();

	text_edit->set_context_menu_enabled(false);
	CHECK_FALSE(text_edit->is_context_menu_enabled());

	CHECK_FALSE(text_edit->is_menu_visible());
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(600, 10), MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
	CHECK_FALSE(text_edit->is_menu_visible());

	text_edit->set_context_menu_enabled(true);
	CHECK(text_edit->is_context_menu_enabled());

	CHECK_FALSE(text_edit->is_menu_visible());
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(700, 10), MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
	CHECK(text_edit->is_menu_visible());

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] versioning") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	// Action undo / redo states are tested in the action test e.g selection_delete.
	CHECK_FALSE(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());
	CHECK(text_edit->get_version() == 0);
	CHECK(text_edit->get_saved_version() == 0);

	text_edit->begin_complex_operation();
	text_edit->begin_complex_operation();
	text_edit->begin_complex_operation();

	text_edit->insert_text_at_caret("test");
	CHECK(text_edit->get_version() == 1);
	CHECK(text_edit->get_saved_version() == 0);
	CHECK(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());

	text_edit->end_complex_operation();

	// Can undo and redo mid op.
	text_edit->insert_text_at_caret(" nested");
	CHECK(text_edit->get_version() == 2);
	CHECK(text_edit->get_saved_version() == 0);
	CHECK(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());
	text_edit->undo();

	CHECK(text_edit->has_redo());
	text_edit->redo();

	text_edit->end_complex_operation();

	text_edit->insert_text_at_caret(" ops");
	CHECK(text_edit->get_version() == 3);
	CHECK(text_edit->get_saved_version() == 0);
	CHECK(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());

	text_edit->end_complex_operation();

	text_edit->tag_saved_version();
	CHECK(text_edit->get_saved_version() == 3);

	text_edit->undo();
	CHECK(text_edit->get_line(0) == "");
	CHECK(text_edit->get_version() == 0);
	CHECK(text_edit->get_saved_version() == 3);
	CHECK_FALSE(text_edit->has_undo());
	CHECK(text_edit->has_redo());

	text_edit->redo();
	CHECK(text_edit->get_line(0) == "test ops nested");
	CHECK(text_edit->get_version() == 3);
	CHECK(text_edit->get_saved_version() == 3);
	CHECK(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());

	text_edit->clear_undo_history();
	CHECK_FALSE(text_edit->has_undo());
	CHECK_FALSE(text_edit->has_redo());
	CHECK(text_edit->get_version() == 3); // Should this be cleared?
	CHECK(text_edit->get_saved_version() == 0);

	SUBCASE("[TextEdit] versioning selection") {
		text_edit->set_text("Godot Engine\nWaiting for Godot\nTest Text for multi carat\nLine 4 Text");
		text_edit->set_multiple_carets_enabled(true);

		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(0);

		CHECK(text_edit->get_caret_count() == 1);

		Array caret_index;
		caret_index.push_back(0);

		for (int i = 1; i < 4; i++) {
			caret_index.push_back(text_edit->add_caret(i, 0));
			CHECK((int)caret_index.back() >= 0);
		}

		CHECK(text_edit->get_caret_count() == 4);

		for (int i = 0; i < 4; i++) {
			text_edit->select(i, 0, i, 5, caret_index[i]);
		}

		CHECK(text_edit->get_caret_count() == 4);
		for (int i = 0; i < 4; i++) {
			CHECK(text_edit->has_selection(caret_index[i]));
			CHECK(text_edit->get_selection_from_line(caret_index[i]) == i);
			CHECK(text_edit->get_selection_from_column(caret_index[i]) == 0);
			CHECK(text_edit->get_selection_to_line(caret_index[i]) == i);
			CHECK(text_edit->get_selection_to_column(caret_index[i]) == 5);
		}
		text_edit->begin_complex_operation();
		text_edit->deselect();
		text_edit->set_text("New Line Text");
		text_edit->select(0, 0, 0, 7, 0);
		text_edit->end_complex_operation();

		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->get_selected_text(0) == "New Lin");

		text_edit->undo();

		CHECK(text_edit->get_caret_count() == 4);
		for (int i = 0; i < 4; i++) {
			CHECK(text_edit->has_selection(caret_index[i]));
			CHECK(text_edit->get_selection_from_line(caret_index[i]) == i);
			CHECK(text_edit->get_selection_from_column(caret_index[i]) == 0);
			CHECK(text_edit->get_selection_to_line(caret_index[i]) == i);
			CHECK(text_edit->get_selection_to_column(caret_index[i]) == 5);
		}
	}

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] search") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	text_edit->set_text("hay needle, hay\nHAY NEEDLE, HAY\nwordword.word.word");
	int length = text_edit->get_line(1).length();

	CHECK(text_edit->search("test", 0, 0, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_MATCH_CASE, 0, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_WHOLE_WORDS, 0, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(-1, -1));

	CHECK(text_edit->search("test", 0, 1, length) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_MATCH_CASE, 1, length) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_WHOLE_WORDS, 1, length) == Point2i(-1, -1));
	CHECK(text_edit->search("test", TextEdit::SEARCH_BACKWARDS, 1, length) == Point2i(-1, -1));

	CHECK(text_edit->search("needle", 0, 0, 0) == Point2i(4, 0));
	CHECK(text_edit->search("needle", 0, 1, length) == Point2i(4, 0));
	CHECK(text_edit->search("needle", 0, 0, 5) == Point2i(4, 1));
	CHECK(text_edit->search("needle", TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(4, 1));
	CHECK(text_edit->search("needle", TextEdit::SEARCH_BACKWARDS, 1, 5) == Point2i(4, 1));
	CHECK(text_edit->search("needle", TextEdit::SEARCH_BACKWARDS, 1, 3) == Point2i(4, 0));

	CHECK(text_edit->search("needle", TextEdit::SEARCH_MATCH_CASE, 0, 0) == Point2i(4, 0));
	CHECK(text_edit->search("needle", TextEdit::SEARCH_MATCH_CASE | TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(4, 0));

	CHECK(text_edit->search("needle", TextEdit::SEARCH_WHOLE_WORDS | TextEdit::SEARCH_MATCH_CASE, 0, 0) == Point2i(4, 0));
	CHECK(text_edit->search("needle", TextEdit::SEARCH_WHOLE_WORDS | TextEdit::SEARCH_MATCH_CASE | TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(4, 0));

	CHECK(text_edit->search("need", TextEdit::SEARCH_MATCH_CASE, 0, 0) == Point2i(4, 0));
	CHECK(text_edit->search("need", TextEdit::SEARCH_MATCH_CASE | TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(4, 0));

	CHECK(text_edit->search("need", TextEdit::SEARCH_WHOLE_WORDS | TextEdit::SEARCH_MATCH_CASE, 0, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("need", TextEdit::SEARCH_WHOLE_WORDS | TextEdit::SEARCH_MATCH_CASE | TextEdit::SEARCH_BACKWARDS, 0, 0) == Point2i(-1, -1));

	CHECK(text_edit->search("word", TextEdit::SEARCH_WHOLE_WORDS, 2, 0) == Point2i(9, 2));
	CHECK(text_edit->search("word", TextEdit::SEARCH_WHOLE_WORDS, 2, 10) == Point2i(14, 2));
	CHECK(text_edit->search(".word", TextEdit::SEARCH_WHOLE_WORDS, 2, 0) == Point2i(8, 2));
	CHECK(text_edit->search("word.", TextEdit::SEARCH_WHOLE_WORDS, 2, 0) == Point2i(9, 2));

	ERR_PRINT_OFF;
	CHECK(text_edit->search("", 0, 0, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("needle", 0, -1, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("needle", 0, 0, -1) == Point2i(-1, -1));
	CHECK(text_edit->search("needle", 0, 100, 0) == Point2i(-1, -1));
	CHECK(text_edit->search("needle", 0, 0, 100) == Point2i(-1, -1));
	ERR_PRINT_ON;

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] mouse") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	text_edit->set_size(Size2(800, 200));

	CHECK(text_edit->get_rect_at_line_column(0, 0).get_position() == Point2i(0, 0));

	text_edit->set_line(0, "A");
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_rect_at_line_column(0, 1).get_position().x > 0);

	text_edit->clear(); // Necessary, otherwise the following test cases fail.

	text_edit->set_line(0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	MessageQueue::get_singleton()->flush();

	CHECK(text_edit->get_word_at_pos(text_edit->get_pos_at_line_column(0, 1)) == "Lorem");
	CHECK(text_edit->get_word_at_pos(text_edit->get_pos_at_line_column(0, 9)) == "ipsum");

	ERR_PRINT_OFF;
	CHECK(text_edit->get_pos_at_line_column(0, -1) == Point2i(-1, -1));
	CHECK(text_edit->get_pos_at_line_column(-1, 0) == Point2i(-1, -1));
	CHECK(text_edit->get_pos_at_line_column(-1, -1) == Point2i(-1, -1));

	CHECK(text_edit->get_pos_at_line_column(0, 500) == Point2i(-1, -1));
	CHECK(text_edit->get_pos_at_line_column(2, 0) == Point2i(-1, -1));
	CHECK(text_edit->get_pos_at_line_column(2, 500) == Point2i(-1, -1));

	// Out of view.
	CHECK(text_edit->get_pos_at_line_column(0, text_edit->get_line(0).length() - 1) == Point2i(-1, -1));
	ERR_PRINT_ON;

	// Add method to get drawn column count?
	Point2i start_pos = text_edit->get_pos_at_line_column(0, 0);
	Point2i end_pos = text_edit->get_pos_at_line_column(0, 105);

	CHECK(text_edit->get_line_column_at_pos(Point2i(start_pos.x, start_pos.y)) == Point2i(0, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x, end_pos.y)) == Point2i(104, 0));

	// Should this return Point2i(-1, -1) if its also < 0 not just > vis_lines.
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y), false) == Point2i(90, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x, end_pos.y + 100), false) == Point2i(-1, -1));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y + 100), false) == Point2i(-1, -1));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x, end_pos.y - 100), false) == Point2i(104, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y - 100), false) == Point2i(90, 0));

	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y)) == Point2i(90, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x, end_pos.y + 100)) == Point2i(140, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y + 100)) == Point2i(140, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x, end_pos.y - 100)) == Point2i(104, 0));
	CHECK(text_edit->get_line_column_at_pos(Point2i(end_pos.x - 100, end_pos.y - 100)) == Point2i(90, 0));

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] caret") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_context_menu_enabled(false); // Prohibit sending InputEvents to the context menu.
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	text_edit->set_size(Size2(800, 200));
	text_edit->grab_focus();
	text_edit->set_line(0, "ffi");

	text_edit->set_caret_mid_grapheme_enabled(true);
	CHECK(text_edit->is_caret_mid_grapheme_enabled());

	SEND_GUI_ACTION("ui_text_caret_right");
	CHECK(text_edit->get_caret_column() == 1);

	SEND_GUI_ACTION("ui_text_caret_right");
	CHECK(text_edit->get_caret_column() == 2);

	SEND_GUI_ACTION("ui_text_caret_right");
	CHECK(text_edit->get_caret_column() == 3);

	SEND_GUI_ACTION("ui_text_caret_left");
	CHECK(text_edit->get_caret_column() == 2);

	text_edit->set_line(0, "Lorem  ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	for (int i = 0; i < 3; i++) {
		text_edit->insert_line_at(0, "Lorem  ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	}
	MessageQueue::get_singleton()->flush();

	text_edit->set_caret_blink_enabled(false);
	CHECK_FALSE(text_edit->is_caret_blink_enabled());

	text_edit->set_caret_blink_enabled(true);
	CHECK(text_edit->is_caret_blink_enabled());

	text_edit->set_caret_blink_interval(10);
	CHECK(text_edit->get_caret_blink_interval() == 10);

	ERR_PRINT_OFF;
	text_edit->set_caret_blink_interval(-1);
	CHECK(text_edit->get_caret_blink_interval() == 10);

	text_edit->set_caret_blink_interval(0);
	CHECK(text_edit->get_caret_blink_interval() == 10);
	ERR_PRINT_ON;

	text_edit->set_caret_type(TextEdit::CaretType::CARET_TYPE_LINE);
	CHECK(text_edit->get_caret_type() == TextEdit::CaretType::CARET_TYPE_LINE);

	text_edit->set_caret_type(TextEdit::CaretType::CARET_TYPE_BLOCK);
	CHECK(text_edit->get_caret_type() == TextEdit::CaretType::CARET_TYPE_BLOCK);

	text_edit->set_caret_type(TextEdit::CaretType::CARET_TYPE_LINE);
	CHECK(text_edit->get_caret_type() == TextEdit::CaretType::CARET_TYPE_LINE);

	int caret_col = text_edit->get_caret_column();
	text_edit->set_move_caret_on_right_click_enabled(false);
	CHECK_FALSE(text_edit->is_move_caret_on_right_click_enabled());

	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(100, 1), MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
	CHECK(text_edit->get_caret_column() == caret_col);

	text_edit->set_move_caret_on_right_click_enabled(true);
	CHECK(text_edit->is_move_caret_on_right_click_enabled());

	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(100, 1), MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
	CHECK(text_edit->get_caret_column() != caret_col);

	text_edit->set_move_caret_on_right_click_enabled(false);
	CHECK_FALSE(text_edit->is_move_caret_on_right_click_enabled());

	text_edit->set_caret_column(0);
	CHECK(text_edit->get_word_under_caret() == "Lorem");

	text_edit->set_caret_column(4);
	CHECK(text_edit->get_word_under_caret() == "Lorem");

	text_edit->set_caret_column(1);
	text_edit->add_caret(0, 15);
	CHECK(text_edit->get_word_under_caret() == "Lorem\ndolor");
	text_edit->remove_secondary_carets();

	// Should this work?
	text_edit->set_caret_column(5);
	CHECK(text_edit->get_word_under_caret() == "");

	text_edit->set_caret_column(6);
	CHECK(text_edit->get_word_under_caret() == "");

	text_edit->set_caret_line(1);
	CHECK(text_edit->get_caret_line() == 1);

	text_edit->set_caret_line(-1);
	CHECK(text_edit->get_caret_line() == 0);
	text_edit->set_caret_line(100);
	CHECK(text_edit->get_caret_line() == 3);

	text_edit->set_caret_column(-1);
	CHECK(text_edit->get_caret_column() == 0);
	text_edit->set_caret_column(10000000);
	CHECK(text_edit->get_caret_column() == 141);

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] multicaret") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);
	text_edit->set_multiple_carets_enabled(true);

	Array empty_signal_args;
	empty_signal_args.push_back(Array());

	SIGNAL_WATCH(text_edit, "caret_changed");

	text_edit->set_text("this is\nsome test\ntext");
	text_edit->set_caret_line(0);
	text_edit->set_caret_column(0);
	MessageQueue::get_singleton()->flush();
	SIGNAL_DISCARD("caret_changed");

	SUBCASE("[TextEdit] add remove caret") {
		// Overlapping.
		CHECK(text_edit->add_caret(0, 0) == -1);
		MessageQueue::get_singleton()->flush();
		SIGNAL_CHECK_FALSE("caret_changed");

		// Select.
		text_edit->select(2, 4, 0, 0);

		// Cannot add in selection.
		CHECK(text_edit->add_caret(0, 0) == -1);
		CHECK(text_edit->add_caret(2, 4) == -1);
		CHECK(text_edit->add_caret(1, 2) == -1);

		// Cannot add when out of bounds.
		CHECK(text_edit->add_caret(-1, 0) == -1);
		CHECK(text_edit->add_caret(5, 0) == -1);
		CHECK(text_edit->add_caret(0, 100) == -1);

		MessageQueue::get_singleton()->flush();
		SIGNAL_CHECK_FALSE("caret_changed");

		CHECK(text_edit->get_caret_count() == 1);

		text_edit->deselect();
		SIGNAL_CHECK_FALSE("caret_changed");

		CHECK(text_edit->add_caret(0, 1) == 1);
		MessageQueue::get_singleton()->flush();
		SIGNAL_CHECK("caret_changed", empty_signal_args);
		CHECK(text_edit->get_caret_count() == 2);

		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 0);

		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 1);

		ERR_PRINT_OFF;
		text_edit->remove_caret(-1);
		text_edit->remove_caret(5);
		ERR_PRINT_ON;
		CHECK(text_edit->get_caret_count() == 2);
		SIGNAL_CHECK_FALSE("caret_changed");

		text_edit->remove_caret(0);
		SIGNAL_CHECK_FALSE("caret_changed");
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 1);

		ERR_PRINT_OFF;
		text_edit->remove_caret(0);
		CHECK(text_edit->get_caret_count() == 1);
		ERR_PRINT_ON;
	}

	SUBCASE("[TextEdit] sort carets") {
		Vector<int> sorted_carets = { 0, 1, 2 };

		// Ascending order.
		text_edit->remove_secondary_carets();
		text_edit->add_caret(0, 1);
		text_edit->add_caret(1, 0);
		CHECK(text_edit->get_sorted_carets() == sorted_carets);

		// Descending order.
		sorted_carets = { 2, 1, 0 };
		text_edit->remove_secondary_carets();
		text_edit->set_caret_line(1);
		text_edit->add_caret(0, 1);
		text_edit->add_caret(0, 0);
		CHECK(text_edit->get_sorted_carets() == sorted_carets);

		// Mixed order.
		sorted_carets = { 0, 2, 1, 3 };
		text_edit->remove_secondary_carets();
		text_edit->set_caret_line(0);
		text_edit->add_caret(1, 0);
		text_edit->add_caret(0, 1);
		text_edit->add_caret(1, 1);
		CHECK(text_edit->get_sorted_carets() == sorted_carets);

		// Overlapping carets.
		sorted_carets = { 0, 1, 3, 2 };
		text_edit->remove_secondary_carets();
		text_edit->add_caret(0, 1);
		text_edit->add_caret(1, 2);
		text_edit->add_caret(0, 2);
		text_edit->set_caret_column(1, false, 3);
		CHECK(text_edit->get_sorted_carets() == sorted_carets);

		// Sorted by selection start.
		sorted_carets = { 1, 0 };
		text_edit->remove_secondary_carets();
		text_edit->select(1, 3, 1, 5);
		text_edit->add_caret(2, 0);
		text_edit->select(1, 0, 2, 0, 1);
		CHECK(text_edit->get_sorted_carets() == sorted_carets);
	}

	SUBCASE("[TextEdit] merge carets") {
		text_edit->set_text("this is some text\nfor selection");
		MessageQueue::get_singleton()->flush();

		// Don't merge carets that are not overlapping.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(4);
		text_edit->add_caret(0, 6);
		text_edit->add_caret(1, 6);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 3);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 6);
		CHECK(text_edit->get_caret_line(2) == 1);
		CHECK(text_edit->get_caret_column(2) == 6);
		text_edit->remove_secondary_carets();

		// Don't merge when in a multicaret edit.
		text_edit->begin_multicaret_edit();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(4);
		text_edit->add_caret(0, 4);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->is_in_mulitcaret_edit());
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 4);

		// Merge overlapping carets. Merge at the end of the multicaret edit.
		text_edit->end_multicaret_edit();
		CHECK_FALSE(text_edit->is_in_mulitcaret_edit());
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);

		// Don't merge selections that are not overlapping.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(4);
		text_edit->add_caret(0, 2);
		text_edit->add_caret(1, 4);
		text_edit->select(0, 4, 1, 2, 0);
		text_edit->select(0, 2, 0, 3, 1);
		text_edit->select(1, 4, 1, 8, 2);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->has_selection(1));
		CHECK(text_edit->has_selection(2));
		text_edit->remove_secondary_carets();
		text_edit->deselect();

		// Don't merge selections that are only touching.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(4);
		text_edit->add_caret(1, 2);
		text_edit->select(0, 4, 1, 2, 0);
		text_edit->select(1, 2, 1, 5, 1);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->has_selection(1));
		text_edit->remove_secondary_carets();
		text_edit->deselect();

		// Merge carets into selection.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(3);
		text_edit->add_caret(0, 2);
		text_edit->add_caret(1, 4);
		text_edit->add_caret(1, 8);
		text_edit->add_caret(1, 10);
		text_edit->select(0, 2, 1, 8, 0);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_from_line(0) == 0);
		CHECK(text_edit->get_selection_from_column(0) == 2);
		CHECK(text_edit->get_selection_to_line(0) == 1);
		CHECK(text_edit->get_selection_to_column(0) == 8);
		CHECK(text_edit->is_caret_after_selection_origin(0));
		CHECK_FALSE(text_edit->has_selection(1));
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 10);
		text_edit->remove_secondary_carets();
		text_edit->deselect();

		// Merge partially overlapping selections.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(1);
		text_edit->add_caret(0, 2);
		text_edit->add_caret(0, 3);
		text_edit->select(0, 2, 0, 6, 0);
		text_edit->select(0, 4, 1, 3, 1);
		text_edit->select(1, 0, 1, 5, 2);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_from_line(0) == 0);
		CHECK(text_edit->get_selection_from_column(0) == 2);
		CHECK(text_edit->get_selection_to_line(0) == 1);
		CHECK(text_edit->get_selection_to_column(0) == 5);
		CHECK(text_edit->is_caret_after_selection_origin(0));
		text_edit->remove_secondary_carets();
		text_edit->deselect();

		// Merge smaller overlapping selection into a bigger one.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(1);
		text_edit->add_caret(0, 2);
		text_edit->add_caret(0, 3);
		text_edit->select(0, 2, 0, 6, 0);
		text_edit->select(0, 8, 1, 3, 1);
		text_edit->select(0, 2, 1, 5, 2);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_from_line(0) == 0);
		CHECK(text_edit->get_selection_from_column(0) == 2);
		CHECK(text_edit->get_selection_to_line(0) == 1);
		CHECK(text_edit->get_selection_to_column(0) == 5);
		CHECK(text_edit->is_caret_after_selection_origin(0));
		text_edit->remove_secondary_carets();
		text_edit->deselect();

		// Merge equal overlapping selections.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(1);
		text_edit->add_caret(0, 2);
		text_edit->select(0, 2, 1, 6, 0);
		text_edit->select(0, 2, 1, 6, 1);
		text_edit->merge_overlapping_carets();
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_from_line(0) == 0);
		CHECK(text_edit->get_selection_from_column(0) == 2);
		CHECK(text_edit->get_selection_to_line(0) == 1);
		CHECK(text_edit->get_selection_to_column(0) == 6);
		CHECK(text_edit->is_caret_after_selection_origin(0));
	}

	SUBCASE("[TextEdit] collapse carets") {
		text_edit->set_text("this is some text\nfor selection");

		// Collapse carets in range, dont affect other carets.
		text_edit->add_caret(0, 9);
		text_edit->add_caret(1, 0);
		text_edit->add_caret(1, 2);
		text_edit->add_caret(1, 6);
		text_edit->begin_multicaret_edit();

		text_edit->collapse_carets(0, 8, 1, 2);
		CHECK(text_edit->get_caret_count() == 5);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 0);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 8);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 8);
		CHECK(text_edit->get_caret_line(3) == 1);
		CHECK(text_edit->get_caret_column(3) == 2);
		CHECK(text_edit->get_caret_line(4) == 1);
		CHECK(text_edit->get_caret_column(4) == 6);
		CHECK_FALSE(text_edit->multicaret_edit_ignore_caret(0));
		CHECK(text_edit->multicaret_edit_ignore_caret(1));
		CHECK(text_edit->multicaret_edit_ignore_caret(2));
		CHECK_FALSE(text_edit->multicaret_edit_ignore_caret(3));
		CHECK_FALSE(text_edit->multicaret_edit_ignore_caret(4));

		// Collapsed carets get merged at the end of the edit.
		text_edit->end_multicaret_edit();
		CHECK(text_edit->get_caret_count() == 4);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 0);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 8);
		CHECK(text_edit->get_caret_line(2) == 1);
		CHECK(text_edit->get_caret_column(2) == 2);
		CHECK(text_edit->get_caret_line(3) == 1);
		CHECK(text_edit->get_caret_column(3) == 6);
		text_edit->remove_secondary_carets();

		// Collapse inclusive.
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(3);
		text_edit->add_caret(1, 2);
		text_edit->collapse_carets(0, 3, 1, 2, true);
		CHECK(text_edit->get_caret_count() == 1);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line() == 0);
		CHECK(text_edit->get_caret_column() == 3);
		text_edit->remove_secondary_carets();

		// Deselect if selection was encompassed.
		text_edit->select(0, 5, 0, 7);
		text_edit->collapse_carets(0, 3, 1, 2);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line() == 0);
		CHECK(text_edit->get_caret_column() == 3);

		// Clamp only caret end of selection.
		text_edit->select(0, 1, 0, 7);
		text_edit->collapse_carets(0, 3, 1, 2);
		CHECK(text_edit->has_selection());
		CHECK(text_edit->get_caret_line() == 0);
		CHECK(text_edit->get_caret_column() == 3);
		CHECK(text_edit->get_selection_origin_line() == 0);
		CHECK(text_edit->get_selection_origin_column() == 1);
		text_edit->deselect();

		// Clamp only selection origin end of selection.
		text_edit->select(0, 7, 0, 1);
		text_edit->collapse_carets(0, 3, 1, 2);
		CHECK(text_edit->has_selection());
		CHECK(text_edit->get_caret_line() == 0);
		CHECK(text_edit->get_caret_column() == 1);
		CHECK(text_edit->get_selection_origin_line() == 0);
		CHECK(text_edit->get_selection_origin_column() == 3);
		text_edit->deselect();
	}

	SUBCASE("[TextEdit] add caret at carets") {
		text_edit->remove_secondary_carets();
		text_edit->set_caret_line(1);
		text_edit->set_caret_column(9);

		// Add caret below. Column will clamp.
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 2);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 9);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 4);

		// Cannot add below when at last line.
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 2);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 9);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 4);

		// Add caret above. Column will clamp.
		text_edit->add_caret_at_carets(false);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 9);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 7);

		// Cannot add above when at first line.
		text_edit->add_caret_at_carets(false);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 9);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 7);

		// Cannot add below when at the last line for selection.
		text_edit->remove_secondary_carets();
		text_edit->select(2, 1, 2, 4);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 2);
		CHECK(text_edit->get_selection_origin_column(0) == 1);
		CHECK(text_edit->get_caret_line(0) == 2);
		CHECK(text_edit->get_caret_column(0) == 4);

		// Cannot add above when at the first line for selection.
		text_edit->select(0, 1, 0, 4);
		text_edit->add_caret_at_carets(false);
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 1);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);

		// Add selection below.
		text_edit->select(0, 0, 0, 4);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 0);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->has_selection(1));
		CHECK(text_edit->get_selection_origin_line(1) == 1);
		CHECK(text_edit->get_selection_origin_column(1) == 0);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 3); // In the default font, this is the same position.

		// Add selection below again.
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 0);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->has_selection(1));
		CHECK(text_edit->get_selection_origin_line(1) == 1);
		CHECK(text_edit->get_selection_origin_column(1) == 0);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 3);
		CHECK(text_edit->has_selection(2));
		CHECK(text_edit->get_selection_origin_line(2) == 2);
		CHECK(text_edit->get_selection_origin_column(2) == 0);
		CHECK(text_edit->get_caret_line(2) == 2);
		CHECK(text_edit->get_caret_column(2) == 4);

		text_edit->set_text("\tthis is\nsome\n\ttest text");
		MessageQueue::get_singleton()->flush();

		// Last fit x is preserved when adding below.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(6);
		text_edit->add_caret_at_carets(true);
		text_edit->add_caret_at_carets(true);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 6);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->get_caret_line(2) == 2);
		CHECK(text_edit->get_caret_column(2) == 6);

		// Last fit x is preserved when adding above.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(2);
		text_edit->set_caret_column(9);
		text_edit->add_caret_at_carets(false);
		text_edit->add_caret_at_carets(false);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->get_caret_line(0) == 2);
		CHECK(text_edit->get_caret_column(0) == 9);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 8);

		// Last fit x is preserved when selection adding below.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->select(0, 8, 0, 5);
		text_edit->add_caret_at_carets(true);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 8);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 5);
		CHECK_FALSE(text_edit->has_selection(1));
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->has_selection(2));
		CHECK(text_edit->get_selection_origin_line(2) == 2);
		CHECK(text_edit->get_selection_origin_column(2) == 7);
		CHECK(text_edit->get_caret_line(2) == 2);
		CHECK(text_edit->get_caret_column(2) == 5);

		// Last fit x is preserved when selection adding above.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->select(2, 9, 2, 5);
		text_edit->add_caret_at_carets(false);
		text_edit->add_caret_at_carets(false);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 2);
		CHECK(text_edit->get_selection_origin_column(0) == 9);
		CHECK(text_edit->get_caret_line(0) == 2);
		CHECK(text_edit->get_caret_column(0) == 5);
		CHECK_FALSE(text_edit->has_selection(1));
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->has_selection(2));
		CHECK(text_edit->get_selection_origin_line(2) == 0);
		CHECK(text_edit->get_selection_origin_column(2) == 8);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 5);

		// Selections are merged when they overlap.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->select(0, 1, 0, 5);
		text_edit->add_caret(1, 0);
		text_edit->select(1, 1, 1, 3, 1);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 1);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 5);
		CHECK(text_edit->has_selection(1));
		CHECK(text_edit->get_selection_origin_line(1) == 1);
		CHECK(text_edit->get_selection_origin_column(1) == 1);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 4);
		CHECK(text_edit->has_selection(2));
		CHECK(text_edit->get_selection_origin_line(2) == 2);
		CHECK(text_edit->get_selection_origin_column(2) == 0);
		CHECK(text_edit->get_caret_line(2) == 2);
		CHECK(text_edit->get_caret_column(2) == 3);

		// Multiline selection.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(1);
		text_edit->select(0, 3, 1, 1);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->has_selection(0));
		CHECK(text_edit->get_selection_origin_line(0) == 0);
		CHECK(text_edit->get_selection_origin_column(0) == 3);
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 1);
		CHECK(text_edit->has_selection(1));
		CHECK(text_edit->get_selection_origin_line(1) == 1);
		CHECK(text_edit->get_selection_origin_column(1) == 3);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 0);

		text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
		text_edit->set_size(Size2(50, 100));
		// Line wraps: `\t,this, is\nso,me\n\t,test, ,text`.
		CHECK(text_edit->is_line_wrapped(0));
		MessageQueue::get_singleton()->flush();

		// Add caret below on next line wrap.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(4);
		text_edit->add_caret_at_carets(true);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 8);

		// Add caret below from end of line wrap.
		text_edit->add_caret_at_carets(true);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 0);
		CHECK(text_edit->get_caret_column(1) == 8);
		CHECK(text_edit->get_caret_line(2) == 1);
		CHECK(text_edit->get_caret_column(2) == 1);

		// Add caret below from last line and not last line wrap.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(2);
		text_edit->set_caret_column(5);
		text_edit->add_caret_at_carets(true);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->get_caret_line(0) == 2);
		CHECK(text_edit->get_caret_column(0) == 5);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 6);

		// Cannot add caret below from last line last line wrap.
		text_edit->add_caret_at_carets(true);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->get_caret_line(0) == 2);
		CHECK(text_edit->get_caret_column(0) == 5);
		CHECK(text_edit->get_caret_line(1) == 2);
		CHECK(text_edit->get_caret_column(1) == 6);

		// Add caret above from not first line wrap.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(1);
		text_edit->set_caret_column(4);
		text_edit->add_caret_at_carets(false);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 2);
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 1);

		// Add caret above from first line wrap.
		text_edit->add_caret_at_carets(false);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 3);
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 1);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 8);

		// Add caret above from first line and not first line wrap.
		text_edit->add_caret_at_carets(false);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 4);
		CHECK(text_edit->get_caret_line(0) == 1);
		CHECK(text_edit->get_caret_column(0) == 4);
		CHECK(text_edit->get_caret_line(1) == 1);
		CHECK(text_edit->get_caret_column(1) == 1);
		CHECK(text_edit->get_caret_line(2) == 0);
		CHECK(text_edit->get_caret_column(2) == 8);
		CHECK(text_edit->get_caret_line(3) == 0);
		CHECK(text_edit->get_caret_column(3) == 4);

		// Cannot add caret above from first line first line wrap.
		text_edit->remove_secondary_carets();
		text_edit->deselect();
		text_edit->set_caret_line(0);
		text_edit->set_caret_column(0);
		text_edit->add_caret_at_carets(false);
		CHECK_FALSE(text_edit->has_selection());
		CHECK(text_edit->get_caret_count() == 1);
		CHECK(text_edit->get_caret_line(0) == 0);
		CHECK(text_edit->get_caret_column(0) == 0);

		// Does nothing if multiple carets are disabled.
		text_edit->set_multiple_carets_enabled(false);
		text_edit->add_caret_at_carets(true);
		CHECK(text_edit->get_caret_count() == 1);
	}

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] line wrapping") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);
	text_edit->grab_focus();

	// Set size for boundary.
	text_edit->set_size(Size2(800, 200));
	text_edit->set_line(0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	CHECK_FALSE(text_edit->is_line_wrapped(0));
	CHECK(text_edit->get_line_wrap_count(0) == 0);
	CHECK(text_edit->get_line_wrap_index_at_column(0, 130) == 0);
	CHECK(text_edit->get_line_wrapped_text(0).size() == 1);

	SIGNAL_WATCH(text_edit, "text_set");
	SIGNAL_WATCH(text_edit, "text_changed");
	SIGNAL_WATCH(text_edit, "lines_edited_from");
	SIGNAL_WATCH(text_edit, "caret_changed");

	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	SIGNAL_CHECK_FALSE("text_set");
	SIGNAL_CHECK_FALSE("text_changed");
	SIGNAL_CHECK_FALSE("lines_edited_from");
	SIGNAL_CHECK_FALSE("caret_changed");

	CHECK(text_edit->is_line_wrapped(0));
	CHECK(text_edit->get_line_wrap_count(0) == 1);
	CHECK(text_edit->get_line_wrap_index_at_column(0, 130) == 1);
	CHECK(text_edit->get_line_wrapped_text(0).size() == 2);

	SIGNAL_UNWATCH(text_edit, "text_set");
	SIGNAL_UNWATCH(text_edit, "text_changed");
	SIGNAL_UNWATCH(text_edit, "lines_edited_from");
	SIGNAL_UNWATCH(text_edit, "caret_changed");

	ERR_PRINT_OFF;
	CHECK_FALSE(text_edit->is_line_wrapped(-1));
	CHECK_FALSE(text_edit->is_line_wrapped(1));
	CHECK(text_edit->get_line_wrap_count(-1) == 0);
	CHECK(text_edit->get_line_wrap_count(1) == 0);
	CHECK(text_edit->get_line_wrap_index_at_column(-1, 0) == 0);
	CHECK(text_edit->get_line_wrap_index_at_column(0, -1) == 0);
	CHECK(text_edit->get_line_wrap_index_at_column(1, 0) == 0);
	CHECK(text_edit->get_line_wrap_index_at_column(0, 10000) == 0);
	CHECK(text_edit->get_line_wrapped_text(-1).size() == 0);
	CHECK(text_edit->get_line_wrapped_text(1).size() == 0);
	ERR_PRINT_ON;

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] viewport") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	// No subcases here for performance.
	text_edit->set_size(Size2(800, 600));
	for (int i = 0; i < 50; i++) {
		text_edit->insert_line_at(0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vasius mattis leo, sed porta ex lacinia bibendum. Nunc bibendum pellentesque.");
	}
	MessageQueue::get_singleton()->flush();

	const int visible_lines = text_edit->get_visible_line_count();
	const int total_visible_lines = text_edit->get_total_visible_line_count();
	CHECK(total_visible_lines == 51);

	// First visible line.
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->set_line_as_first_visible(visible_lines);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_v_scroll() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	ERR_PRINT_OFF;
	text_edit->set_line_as_first_visible(-1);
	text_edit->set_line_as_first_visible(500);
	text_edit->set_line_as_first_visible(0, -1);
	text_edit->set_line_as_first_visible(0, 500);
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	ERR_PRINT_ON;

	// Wrap.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() > total_visible_lines);

	text_edit->set_line_as_first_visible(5, 1);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 5);
	CHECK(text_edit->get_v_scroll() == 11);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 6);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 1);

	// Reset.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_NONE);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() == total_visible_lines);
	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Last visible line.
	text_edit->set_line_as_last_visible(visible_lines * 2);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_v_scroll() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	ERR_PRINT_OFF;
	text_edit->set_line_as_last_visible(-1);
	text_edit->set_line_as_last_visible(500);
	text_edit->set_line_as_last_visible(0, -1);
	text_edit->set_line_as_last_visible(0, 500);
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	ERR_PRINT_ON;

	// Wrap.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() > total_visible_lines);

	text_edit->set_line_as_last_visible(visible_lines + 5, 1);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 16);
	CHECK(text_edit->get_v_scroll() == 32.0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines + 5);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Reset.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_NONE);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() == total_visible_lines);
	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Center.
	text_edit->set_line_as_center_visible(visible_lines + (visible_lines / 2));
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_v_scroll() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	ERR_PRINT_OFF;
	text_edit->set_line_as_last_visible(-1);
	text_edit->set_line_as_last_visible(500);
	text_edit->set_line_as_last_visible(0, -1);
	text_edit->set_line_as_last_visible(0, 500);
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	ERR_PRINT_ON;

	// Wrap.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() > total_visible_lines);

	text_edit->set_line_as_center_visible(visible_lines + (visible_lines / 2) + 5, 1);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines + (visible_lines / 2));
	CHECK(text_edit->get_v_scroll() == (visible_lines * 3));
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 1);

	// Scroll past eof.
	int line_count = text_edit->get_line_count();
	text_edit->set_scroll_past_end_of_file_enabled(true);
	MessageQueue::get_singleton()->flush();
	text_edit->set_line_as_center_visible(line_count - 1);
	MessageQueue::get_singleton()->flush();

	CHECK(text_edit->get_first_visible_line() == (visible_lines * 2) + 3);
	CHECK(text_edit->get_v_scroll() == (visible_lines * 4) + 6);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) + 8);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->set_scroll_past_end_of_file_enabled(false);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == (visible_lines * 2) + 3);
	CHECK(text_edit->get_v_scroll() == (visible_lines * 4) - 4);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) + 8);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Reset.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_NONE);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() == total_visible_lines);
	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Auto adjust - todo: horizontal scroll.
	// Below.
	MessageQueue::get_singleton()->flush();
	CHECK_FALSE(text_edit->is_caret_visible());
	text_edit->set_caret_line(visible_lines + 5, false);
	CHECK_FALSE(text_edit->is_caret_visible());
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->is_caret_visible());
	CHECK(text_edit->get_first_visible_line() == 5);
	CHECK(text_edit->get_v_scroll() == 5);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines - 1) + 5);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->center_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines - 5);
	CHECK(text_edit->get_v_scroll() == visible_lines - 5);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 6);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Caret visible, do nothing.
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines - 5);
	CHECK(text_edit->get_v_scroll() == visible_lines - 5);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 6);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Above.
	text_edit->set_caret_line(1, false);
	MessageQueue::get_singleton()->flush();
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->is_caret_visible());
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 1);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Wrap.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() > total_visible_lines);

	text_edit->set_caret_line(visible_lines + 5, false, true, 1);
	MessageQueue::get_singleton()->flush();
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();

	CHECK(text_edit->get_first_visible_line() == (visible_lines / 2) + 6);
	CHECK(text_edit->get_v_scroll() == (visible_lines + (visible_lines / 2)) + 1);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines) + 5);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 1);

	text_edit->center_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_v_scroll() == (visible_lines * 2) + 1);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 11);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 1);

	// Caret visible, do nothing.
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == visible_lines);
	CHECK(text_edit->get_v_scroll() == (visible_lines * 2) + 1);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 11);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 1);

	// Above.
	text_edit->set_caret_line(1, false, true, 1);
	MessageQueue::get_singleton()->flush();
	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->is_caret_visible());
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 3);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines / 2) + 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 1);
	CHECK(text_edit->get_caret_wrap_index() == 1);

	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->is_caret_visible());
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 11);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->adjust_viewport_to_caret();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 11);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	// Reset.
	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_NONE);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_total_visible_line_count() == total_visible_lines);
	text_edit->set_line_as_first_visible(0);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	// Smooth scroll.
	text_edit->set_v_scroll_speed(10);
	CHECK(text_edit->get_v_scroll_speed() == 10);
	ERR_PRINT_OFF;
	text_edit->set_v_scroll_speed(-1);
	CHECK(text_edit->get_v_scroll_speed() == 10);

	text_edit->set_v_scroll_speed(0);
	CHECK(text_edit->get_v_scroll_speed() == 10);

	text_edit->set_v_scroll_speed(1);
	CHECK(text_edit->get_v_scroll_speed() == 1);
	ERR_PRINT_ON;

	// Scroll.
	int v_scroll = text_edit->get_v_scroll();
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_DOWN, 0, Key::NONE);
	CHECK(text_edit->get_v_scroll() > v_scroll);
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_UP, 0, Key::NONE);
	CHECK(text_edit->get_v_scroll() == v_scroll);

	// smooth scroll speed.
	text_edit->set_smooth_scroll_enabled(true);

	v_scroll = text_edit->get_v_scroll();
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_DOWN, 0, Key::NONE);
	text_edit->notification(TextEdit::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	CHECK(text_edit->get_v_scroll() >= v_scroll);
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_UP, 0, Key::NONE);
	text_edit->notification(TextEdit::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	CHECK(text_edit->get_v_scroll() == v_scroll);

	v_scroll = text_edit->get_v_scroll();
	text_edit->set_v_scroll_speed(10000);
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_DOWN, 0, Key::NONE);
	text_edit->notification(TextEdit::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	CHECK(text_edit->get_v_scroll() >= v_scroll);
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(10, 10), MouseButton::WHEEL_UP, 0, Key::NONE);
	text_edit->notification(TextEdit::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	CHECK(text_edit->get_v_scroll() == v_scroll);

	ERR_PRINT_OFF;
	CHECK(text_edit->get_scroll_pos_for_line(-1) == 0);
	CHECK(text_edit->get_scroll_pos_for_line(1000) == 0);
	CHECK(text_edit->get_scroll_pos_for_line(1, -1) == 0);
	CHECK(text_edit->get_scroll_pos_for_line(1, 100) == 0);
	ERR_PRINT_ON;

	text_edit->set_h_scroll(-100);
	CHECK(text_edit->get_h_scroll() == 0);

	text_edit->set_h_scroll(10000000);
	CHECK(text_edit->get_h_scroll() == 306);
	CHECK(text_edit->get_h_scroll_bar()->get_combined_minimum_size().x == 8);

	text_edit->set_h_scroll(-100);
	CHECK(text_edit->get_h_scroll() == 0);

	text_edit->set_smooth_scroll_enabled(false);

	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);

	text_edit->grab_focus();
	SEND_GUI_ACTION("ui_text_scroll_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 1);
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 1);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_scroll_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 1);
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	// Page down, similar to VSCode, to end of page then scroll.
	SEND_GUI_ACTION("ui_text_caret_page_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 21);
	CHECK(text_edit->get_first_visible_line() == 0);
	CHECK(text_edit->get_v_scroll() == 0);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 41);
	CHECK(text_edit->get_first_visible_line() == 20);
	CHECK(text_edit->get_v_scroll() == 20);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines - 1) * 2);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 21);
	CHECK(text_edit->get_first_visible_line() == 20);
	CHECK(text_edit->get_v_scroll() == 20);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines - 1) * 2);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 1);
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 1);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	text_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_NONE);
	MessageQueue::get_singleton()->flush();

	text_edit->grab_focus();
	SEND_GUI_ACTION("ui_text_scroll_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 2);
	CHECK(text_edit->get_first_visible_line() == 2);
	CHECK(text_edit->get_v_scroll() == 2);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines + 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_scroll_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 2);
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 1);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	// Page down, similar to VSCode, to end of page then scroll.
	SEND_GUI_ACTION("ui_text_caret_page_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 22);
	CHECK(text_edit->get_first_visible_line() == 1);
	CHECK(text_edit->get_v_scroll() == 1);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_down");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 42);
	CHECK(text_edit->get_first_visible_line() == 21);
	CHECK(text_edit->get_v_scroll() == 21);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 22);
	CHECK(text_edit->get_first_visible_line() == 21);
	CHECK(text_edit->get_v_scroll() == 21);
	CHECK(text_edit->get_last_full_visible_line() == (visible_lines * 2) - 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	SEND_GUI_ACTION("ui_text_caret_page_up");
	CHECK(text_edit->get_viewport()->is_input_handled());
	CHECK(text_edit->get_caret_line() == 2);
	CHECK(text_edit->get_first_visible_line() == 2);
	CHECK(text_edit->get_v_scroll() == 2);
	CHECK(text_edit->get_last_full_visible_line() == visible_lines + 1);
	CHECK(text_edit->get_last_full_visible_line_wrap_index() == 0);
	CHECK(text_edit->get_caret_wrap_index() == 0);

	// Typing and undo / redo should adjust viewport.
	text_edit->set_caret_line(0);
	text_edit->set_caret_column(0);
	text_edit->set_line_as_first_visible(5);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 5);

	SEND_GUI_KEY_EVENT(Key::A);
	CHECK(text_edit->get_first_visible_line() == 0);

	text_edit->set_line_as_first_visible(5);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 5);

	text_edit->undo();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);

	text_edit->set_line_as_first_visible(5);
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 5);

	text_edit->redo();
	MessageQueue::get_singleton()->flush();
	CHECK(text_edit->get_first_visible_line() == 0);

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] setter getters") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	SUBCASE("[TextEdit] set and get placeholder") {
		text_edit->set_placeholder("test\nplaceholder");
		CHECK(text_edit->get_placeholder() == "test\nplaceholder");

		CHECK(text_edit->get_text() == "");
		CHECK(text_edit->get_line_count() == 1);
		CHECK(text_edit->get_last_full_visible_line() == 0);
	}

	SUBCASE("[TextEdit] highlight current line") {
		text_edit->set_highlight_current_line(true);
		CHECK(text_edit->is_highlight_current_line_enabled());
		text_edit->set_highlight_current_line(false);
		CHECK_FALSE(text_edit->is_highlight_current_line_enabled());
	}

	SUBCASE("[TextEdit] highlight all occurrences") {
		text_edit->set_highlight_all_occurrences(true);
		CHECK(text_edit->is_highlight_all_occurrences_enabled());
		text_edit->set_highlight_all_occurrences(false);
		CHECK_FALSE(text_edit->is_highlight_all_occurrences_enabled());
	}

	SUBCASE("[TextEdit] draw control chars") {
		text_edit->set_draw_control_chars(true);
		CHECK(text_edit->get_draw_control_chars());
		text_edit->set_draw_control_chars(false);
		CHECK_FALSE(text_edit->get_draw_control_chars());
	}

	SUBCASE("[TextEdit] draw tabs") {
		text_edit->set_draw_tabs(true);
		CHECK(text_edit->is_drawing_tabs());
		text_edit->set_draw_tabs(false);
		CHECK_FALSE(text_edit->is_drawing_tabs());
	}

	SUBCASE("[TextEdit] draw spaces") {
		text_edit->set_draw_spaces(true);
		CHECK(text_edit->is_drawing_spaces());
		text_edit->set_draw_spaces(false);
		CHECK_FALSE(text_edit->is_drawing_spaces());
	}

	SUBCASE("[TextEdit] draw minimap") {
		text_edit->set_draw_minimap(true);
		CHECK(text_edit->is_drawing_minimap());
		text_edit->set_draw_minimap(false);
		CHECK_FALSE(text_edit->is_drawing_minimap());
	}

	SUBCASE("[TextEdit] minimap width") {
		text_edit->set_minimap_width(-1);
		CHECK(text_edit->get_minimap_width() == -1);
		text_edit->set_minimap_width(1000);
		CHECK(text_edit->get_minimap_width() == 1000);
	}

	SUBCASE("[TextEdit] line color background") {
		ERR_PRINT_OFF;
		text_edit->set_line_background_color(-1, Color("#ff0000"));
		text_edit->set_line_background_color(0, Color("#00ff00"));
		text_edit->set_line_background_color(1, Color("#0000ff"));

		CHECK(text_edit->get_line_background_color(-1) == Color());
		CHECK(text_edit->get_line_background_color(0) == Color("#00ff00"));
		CHECK(text_edit->get_line_background_color(1) == Color());
		ERR_PRINT_ON;

		text_edit->set_line_background_color(0, Color("#ffff00"));
		CHECK(text_edit->get_line_background_color(0) == Color("#ffff00"));
	}

	memdelete(text_edit);
}

TEST_CASE("[SceneTree][TextEdit] gutters") {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);

	Array empty_signal_args;
	empty_signal_args.push_back(Array());

	SIGNAL_WATCH(text_edit, "gutter_clicked");
	SIGNAL_WATCH(text_edit, "gutter_added");
	SIGNAL_WATCH(text_edit, "gutter_removed");

	SUBCASE("[TextEdit] gutter add and remove") {
		text_edit->add_gutter();
		CHECK(text_edit->get_gutter_count() == 1);
		CHECK(text_edit->get_gutter_width(0) == 24);
		CHECK(text_edit->get_total_gutter_width() == 24 + 2);
		SIGNAL_CHECK("gutter_added", empty_signal_args);

		text_edit->set_gutter_name(0, "test_gutter");
		CHECK(text_edit->get_gutter_name(0) == "test_gutter");

		text_edit->set_gutter_width(0, 10);
		CHECK(text_edit->get_gutter_width(0) == 10);
		CHECK(text_edit->get_total_gutter_width() == 10 + 2);

		text_edit->add_gutter(-100);
		text_edit->set_gutter_width(1, 10);
		CHECK(text_edit->get_gutter_width(1) == 10);
		CHECK(text_edit->get_total_gutter_width() == 20 + 2);
		CHECK(text_edit->get_gutter_count() == 2);
		CHECK(text_edit->get_gutter_name(0) == "test_gutter");
		SIGNAL_CHECK("gutter_added", empty_signal_args);

		text_edit->set_gutter_draw(1, false);
		CHECK(text_edit->get_total_gutter_width() == 10 + 2);

		text_edit->add_gutter(100);
		CHECK(text_edit->get_gutter_count() == 3);
		CHECK(text_edit->get_gutter_width(2) == 24);
		CHECK(text_edit->get_total_gutter_width() == 34 + 2);
		CHECK(text_edit->get_gutter_name(0) == "test_gutter");
		SIGNAL_CHECK("gutter_added", empty_signal_args);

		text_edit->add_gutter(0);
		CHECK(text_edit->get_gutter_count() == 4);
		CHECK(text_edit->get_gutter_width(0) == 24);
		CHECK(text_edit->get_total_gutter_width() == 58 + 2);
		CHECK(text_edit->get_gutter_name(1) == "test_gutter");
		SIGNAL_CHECK("gutter_added", empty_signal_args);

		text_edit->remove_gutter(2);
		CHECK(text_edit->get_gutter_name(1) == "test_gutter");
		CHECK(text_edit->get_gutter_count() == 3);
		CHECK(text_edit->get_total_gutter_width() == 58 + 2);
		SIGNAL_CHECK("gutter_removed", empty_signal_args);

		text_edit->remove_gutter(0);
		CHECK(text_edit->get_gutter_name(0) == "test_gutter");
		CHECK(text_edit->get_gutter_count() == 2);
		CHECK(text_edit->get_total_gutter_width() == 34 + 2);
		SIGNAL_CHECK("gutter_removed", empty_signal_args);

		ERR_PRINT_OFF;
		text_edit->remove_gutter(-1);
		SIGNAL_CHECK_FALSE("gutter_removed");

		text_edit->remove_gutter(100);
		SIGNAL_CHECK_FALSE("gutter_removed");

		CHECK(text_edit->get_gutter_name(-1) == "");
		CHECK(text_edit->get_gutter_name(100) == "");
		ERR_PRINT_ON;
	}

	SUBCASE("[TextEdit] gutter data") {
		text_edit->add_gutter();
		CHECK(text_edit->get_gutter_count() == 1);
		SIGNAL_CHECK("gutter_added", empty_signal_args);

		text_edit->set_gutter_name(0, "test_gutter");
		CHECK(text_edit->get_gutter_name(0) == "test_gutter");

		text_edit->set_gutter_width(0, 10);
		CHECK(text_edit->get_gutter_width(0) == 10);

		text_edit->set_gutter_clickable(0, true);
		CHECK(text_edit->is_gutter_clickable(0));

		text_edit->set_gutter_overwritable(0, true);
		CHECK(text_edit->is_gutter_overwritable(0));

		text_edit->set_gutter_type(0, TextEdit::GutterType::GUTTER_TYPE_CUSTOM);
		CHECK(text_edit->get_gutter_type(0) == TextEdit::GutterType::GUTTER_TYPE_CUSTOM);

		text_edit->set_text("test\ntext");

		ERR_PRINT_OFF;
		text_edit->set_line_gutter_metadata(1, 0, "test");
		text_edit->set_line_gutter_metadata(0, -1, "test");
		text_edit->set_line_gutter_metadata(0, 2, "test");
		text_edit->set_line_gutter_metadata(2, 0, "test");
		text_edit->set_line_gutter_metadata(-1, 0, "test");

		CHECK(text_edit->get_line_gutter_metadata(1, 0) == "test");
		CHECK(text_edit->get_line_gutter_metadata(0, -1) == "");
		CHECK(text_edit->get_line_gutter_metadata(0, 2) == "");
		CHECK(text_edit->get_line_gutter_metadata(2, 0) == "");
		CHECK(text_edit->get_line_gutter_metadata(-1, 0) == "");

		text_edit->set_line_gutter_text(1, 0, "test");
		text_edit->set_line_gutter_text(0, -1, "test");
		text_edit->set_line_gutter_text(0, 2, "test");
		text_edit->set_line_gutter_text(2, 0, "test");
		text_edit->set_line_gutter_text(-1, 0, "test");

		CHECK(text_edit->get_line_gutter_text(1, 0) == "test");
		CHECK(text_edit->get_line_gutter_text(0, -1) == "");
		CHECK(text_edit->get_line_gutter_text(0, 2) == "");
		CHECK(text_edit->get_line_gutter_text(2, 0) == "");
		CHECK(text_edit->get_line_gutter_text(-1, 0) == "");

		text_edit->set_line_gutter_item_color(1, 0, Color(1, 0, 0));
		text_edit->set_line_gutter_item_color(0, -1, Color(1, 0, 0));
		text_edit->set_line_gutter_item_color(0, 2, Color(1, 0, 0));
		text_edit->set_line_gutter_item_color(2, 0, Color(1, 0, 0));
		text_edit->set_line_gutter_item_color(-1, 0, Color(1, 0, 0));

		CHECK(text_edit->get_line_gutter_item_color(1, 0) == Color(1, 0, 0));
		CHECK(text_edit->get_line_gutter_item_color(0, -1) == Color());
		CHECK(text_edit->get_line_gutter_item_color(0, 2) == Color());
		CHECK(text_edit->get_line_gutter_item_color(2, 0) == Color());
		CHECK(text_edit->get_line_gutter_item_color(-1, 0) == Color());

		text_edit->set_line_gutter_clickable(1, 0, true);
		text_edit->set_line_gutter_clickable(0, -1, true);
		text_edit->set_line_gutter_clickable(0, 2, true);
		text_edit->set_line_gutter_clickable(2, 0, true);
		text_edit->set_line_gutter_clickable(-1, 0, true);

		CHECK(text_edit->is_line_gutter_clickable(1, 0) == true);
		CHECK(text_edit->is_line_gutter_clickable(0, -1) == false);
		CHECK(text_edit->is_line_gutter_clickable(0, 2) == false);
		CHECK(text_edit->is_line_gutter_clickable(2, 0) == false);
		CHECK(text_edit->is_line_gutter_clickable(-1, 0) == false);
		ERR_PRINT_ON;

		// Merging tested via CodeEdit gutters.
	}

	SUBCASE("[TextEdit] gutter mouse") {
		DisplayServerMock *DS = (DisplayServerMock *)(DisplayServer::get_singleton());
		// Set size for mouse input.
		text_edit->set_size(Size2(200, 200));

		text_edit->set_text("test1\ntest2\ntest3\ntest4");
		text_edit->grab_focus();

		text_edit->add_gutter();
		text_edit->set_gutter_name(0, "test_gutter");
		text_edit->set_gutter_width(0, 10);
		text_edit->set_gutter_clickable(0, true);

		text_edit->add_gutter();
		text_edit->set_gutter_name(1, "test_gutter_not_clickable");
		text_edit->set_gutter_width(1, 10);
		text_edit->set_gutter_clickable(1, false);

		text_edit->add_gutter();
		CHECK(text_edit->get_gutter_count() == 3);
		text_edit->set_gutter_name(2, "test_gutter_3");
		text_edit->set_gutter_width(2, 10);
		text_edit->set_gutter_clickable(2, true);

		MessageQueue::get_singleton()->flush();
		const int line_height = text_edit->get_line_height();

		// Defaults to none.
		CHECK(text_edit->get_hovered_gutter() == Vector2i(-1, -1));
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);

		// Hover over gutter.
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(5, line_height + line_height / 2), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(0, 1));
		SIGNAL_CHECK_FALSE("gutter_clicked");
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_POINTING_HAND);

		// Click on gutter.
		SEND_GUI_MOUSE_BUTTON_EVENT(Point2(5, line_height / 2), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(0, 0));
		SIGNAL_CHECK("gutter_clicked", build_array(build_array(0, 0)));

		// Click on gutter on another line.
		SEND_GUI_MOUSE_BUTTON_EVENT(Point2(5, line_height * 3 + line_height / 2), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(0, 3));
		SIGNAL_CHECK("gutter_clicked", build_array(build_array(3, 0)));

		// Unclickable gutter can be hovered.
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(15, line_height + line_height / 2), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(1, 1));
		SIGNAL_CHECK_FALSE("gutter_clicked");
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);

		// Unclickable gutter can be clicked.
		SEND_GUI_MOUSE_BUTTON_EVENT(Point2(15, line_height * 2 + line_height / 2), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(1, 2));
		SIGNAL_CHECK("gutter_clicked", build_array(build_array(2, 1)));
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);

		// Hover past last line.
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(5, line_height * 5), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(-1, -1));
		SIGNAL_CHECK_FALSE("gutter_clicked");
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);

		// Click on gutter past last line.
		SEND_GUI_MOUSE_BUTTON_EVENT(Point2(5, line_height * 5), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(-1, -1));
		SIGNAL_CHECK_FALSE("gutter_clicked");

		// Mouse exit resets hover.
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(5, line_height + line_height / 2), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(0, 1));
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(-1, -1), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(-1, -1));

		// Removing gutter updates hover.
		SEND_GUI_MOUSE_MOTION_EVENT(Point2(25, line_height + line_height / 2), MouseButtonMask::NONE, Key::NONE);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(2, 1));
		text_edit->remove_gutter(2);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(-1, -1));

		// Updating size updates hover.
		text_edit->set_gutter_width(1, 20);
		CHECK(text_edit->get_hovered_gutter() == Vector2i(1, 1));
	}

	SIGNAL_UNWATCH(text_edit, "gutter_clicked");
	SIGNAL_UNWATCH(text_edit, "gutter_added");
	SIGNAL_UNWATCH(text_edit, "gutter_removed");
	memdelete(text_edit);
}

} // namespace TestTextEdit

#endif // TEST_TEXT_EDIT_H
