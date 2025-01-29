/**************************************************************************/
/*  rename_dialog.cpp                                                     */
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

#include "rename_dialog.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/script_editor_plugin.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/control.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tab_container.h"

#include "modules/regex/regex.h"

RenameDialog::RenameDialog(SceneTreeEditor *p_scene_tree_editor) {
	scene_tree_editor = p_scene_tree_editor;
	preview_node = nullptr;

	set_title(TTR("Batch Rename"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	// -- Search/Replace Area

	GridContainer *grd_main = memnew(GridContainer);
	grd_main->set_columns(2);
	grd_main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbc->add_child(grd_main);

	// ---- 1st & 2nd row

	Label *lbl_search = memnew(Label);
	lbl_search->set_text(TTR("Search:"));

	lne_search = memnew(LineEdit);
	lne_search->set_name("lne_search");
	lne_search->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Label *lbl_replace = memnew(Label);
	lbl_replace->set_text(TTR("Replace:"));

	lne_replace = memnew(LineEdit);
	lne_replace->set_name("lne_replace");
	lne_replace->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	grd_main->add_child(lbl_search);
	grd_main->add_child(lbl_replace);
	grd_main->add_child(lne_search);
	grd_main->add_child(lne_replace);

	// ---- 3rd & 4th row

	Label *lbl_prefix = memnew(Label);
	lbl_prefix->set_text(TTR("Prefix:"));

	lne_prefix = memnew(LineEdit);
	lne_prefix->set_name("lne_prefix");
	lne_prefix->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Label *lbl_suffix = memnew(Label);
	lbl_suffix->set_text(TTR("Suffix:"));

	lne_suffix = memnew(LineEdit);
	lne_suffix->set_name("lne_suffix");
	lne_suffix->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	grd_main->add_child(lbl_prefix);
	grd_main->add_child(lbl_suffix);
	grd_main->add_child(lne_prefix);
	grd_main->add_child(lne_suffix);

	// -- Feature Tabs

	cbut_regex = memnew(CheckButton);
	cbut_regex->set_text(TTR("Use Regular Expressions"));
	vbc->add_child(cbut_regex);

	CheckButton *cbut_collapse_features = memnew(CheckButton);
	cbut_collapse_features->set_text(TTR("Advanced Options"));
	vbc->add_child(cbut_collapse_features);

	tabc_features = memnew(TabContainer);
	tabc_features->set_use_hidden_tabs_for_min_size(true);
	vbc->add_child(tabc_features);

	// ---- Tab Substitute

	VBoxContainer *vbc_substitute = memnew(VBoxContainer);
	vbc_substitute->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	vbc_substitute->set_name(TTR("Substitute"));
	tabc_features->add_child(vbc_substitute);

	cbut_substitute = memnew(CheckBox);
	cbut_substitute->set_text(TTR("Substitute"));
	vbc_substitute->add_child(cbut_substitute);

	GridContainer *grd_substitute = memnew(GridContainer);
	grd_substitute->set_columns(3);
	vbc_substitute->add_child(grd_substitute);

	// Name

	but_insert_name = memnew(Button);
	but_insert_name->set_text("NAME");
	but_insert_name->set_tooltip_text(String("${NAME}\n") + TTR("Node name."));
	but_insert_name->set_focus_mode(Control::FOCUS_NONE);
	but_insert_name->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${NAME}"));
	but_insert_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_name);

	// Parent

	but_insert_parent = memnew(Button);
	but_insert_parent->set_text("PARENT");
	but_insert_parent->set_tooltip_text(String("${PARENT}\n") + TTR("Node's parent name, if available."));
	but_insert_parent->set_focus_mode(Control::FOCUS_NONE);
	but_insert_parent->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${PARENT}"));
	but_insert_parent->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_parent);

	// Type

	but_insert_type = memnew(Button);
	but_insert_type->set_text("TYPE");
	but_insert_type->set_tooltip_text(String("${TYPE}\n") + TTR("Node type."));
	but_insert_type->set_focus_mode(Control::FOCUS_NONE);
	but_insert_type->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${TYPE}"));
	but_insert_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_type);

	// Scene

	but_insert_scene = memnew(Button);
	but_insert_scene->set_text("SCENE");
	but_insert_scene->set_tooltip_text(String("${SCENE}\n") + TTR("Current scene name."));
	but_insert_scene->set_focus_mode(Control::FOCUS_NONE);
	but_insert_scene->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${SCENE}"));
	but_insert_scene->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_scene);

	// Root

	but_insert_root = memnew(Button);
	but_insert_root->set_text("ROOT");
	but_insert_root->set_tooltip_text(String("${ROOT}\n") + TTR("Root node name."));
	but_insert_root->set_focus_mode(Control::FOCUS_NONE);
	but_insert_root->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${ROOT}"));
	but_insert_root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_root);

	// Count

	but_insert_count = memnew(Button);
	but_insert_count->set_text("COUNTER");
	but_insert_count->set_tooltip_text(String("${COUNTER}\n") + TTR("Sequential integer counter.\nCompare counter options."));
	but_insert_count->set_focus_mode(Control::FOCUS_NONE);
	but_insert_count->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_insert_text).bind("${COUNTER}"));
	but_insert_count->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grd_substitute->add_child(but_insert_count);

	chk_per_level_counter = memnew(CheckBox);
	chk_per_level_counter->set_text(TTR("Per-level Counter"));
	chk_per_level_counter->set_tooltip_text(TTR("If set, the counter restarts for each group of child nodes."));
	vbc_substitute->add_child(chk_per_level_counter);

	HBoxContainer *hbc_count_options = memnew(HBoxContainer);
	vbc_substitute->add_child(hbc_count_options);

	Label *lbl_count_start = memnew(Label);
	lbl_count_start->set_text(TTR("Start"));
	lbl_count_start->set_tooltip_text(TTR("Initial value for the counter."));
	hbc_count_options->add_child(lbl_count_start);

	spn_count_start = memnew(SpinBox);
	spn_count_start->set_tooltip_text(TTR("Initial value for the counter."));
	spn_count_start->set_step(1);
	spn_count_start->set_min(0);
	hbc_count_options->add_child(spn_count_start);

	Label *lbl_count_step = memnew(Label);
	lbl_count_step->set_text(TTR("Step"));
	lbl_count_step->set_tooltip_text(TTR("Amount by which counter is incremented for each node."));
	hbc_count_options->add_child(lbl_count_step);

	spn_count_step = memnew(SpinBox);
	spn_count_step->set_tooltip_text(TTR("Amount by which counter is incremented for each node."));
	spn_count_step->set_step(1);
	hbc_count_options->add_child(spn_count_step);

	Label *lbl_count_padding = memnew(Label);
	lbl_count_padding->set_text(TTR("Padding"));
	lbl_count_padding->set_tooltip_text(TTR("Minimum number of digits for the counter.\nMissing digits are padded with leading zeros."));
	hbc_count_options->add_child(lbl_count_padding);

	spn_count_padding = memnew(SpinBox);
	spn_count_padding->set_tooltip_text(TTR("Minimum number of digits for the counter.\nMissing digits are padded with leading zeros."));
	spn_count_padding->set_step(1);
	hbc_count_options->add_child(spn_count_padding);

	// ---- Tab Process

	VBoxContainer *vbc_process = memnew(VBoxContainer);
	vbc_process->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbc_process->set_name(TTR("Post-Process"));
	tabc_features->add_child(vbc_process);

	cbut_process = memnew(CheckBox);
	cbut_process->set_text(TTR("Post-Process"));
	vbc_process->add_child(cbut_process);

	// ------ Style

	HBoxContainer *hbc_style = memnew(HBoxContainer);
	vbc_process->add_child(hbc_style);

	Label *lbl_style = memnew(Label);
	lbl_style->set_text(TTR("Style"));
	hbc_style->add_child(lbl_style);

	opt_style = memnew(OptionButton);
	opt_style->add_item(TTR("Keep"));
	opt_style->add_item(TTR("PascalCase to snake_case"));
	opt_style->add_item(TTR("snake_case to PascalCase"));
	hbc_style->add_child(opt_style);

	// ------ Case

	HBoxContainer *hbc_case = memnew(HBoxContainer);
	vbc_process->add_child(hbc_case);

	Label *lbl_case = memnew(Label);
	lbl_case->set_text(TTR("Case"));
	hbc_case->add_child(lbl_case);

	opt_case = memnew(OptionButton);
	opt_case->add_item(TTR("Keep"));
	opt_case->add_item(TTR("To Lowercase"));
	opt_case->add_item(TTR("To Uppercase"));
	hbc_case->add_child(opt_case);

	// -- Preview

	HSeparator *sep_preview = memnew(HSeparator);
	sep_preview->set_custom_minimum_size(Size2(10, 20));
	vbc->add_child(sep_preview);

	lbl_preview_title = memnew(Label);
	vbc->add_child(lbl_preview_title);

	lbl_preview = memnew(Label);
	lbl_preview->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	lbl_preview->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	vbc->add_child(lbl_preview);

	// ---- Dialog related

	set_min_size(Size2(383, 0));
	set_ok_button_text(TTR("Rename"));
	Button *but_reset = add_button(TTR("Reset"));

	eh.errfunc = _error_handler;
	eh.userdata = this;

	// ---- Connections

	cbut_collapse_features->connect(SceneStringName(toggled), callable_mp(this, &RenameDialog::_features_toggled));

	// Substitute Buttons

	lne_search->connect(SceneStringName(focus_entered), callable_mp(this, &RenameDialog::_update_substitute));
	lne_search->connect(SceneStringName(focus_exited), callable_mp(this, &RenameDialog::_update_substitute));
	lne_replace->connect(SceneStringName(focus_entered), callable_mp(this, &RenameDialog::_update_substitute));
	lne_replace->connect(SceneStringName(focus_exited), callable_mp(this, &RenameDialog::_update_substitute));
	lne_prefix->connect(SceneStringName(focus_entered), callable_mp(this, &RenameDialog::_update_substitute));
	lne_prefix->connect(SceneStringName(focus_exited), callable_mp(this, &RenameDialog::_update_substitute));
	lne_suffix->connect(SceneStringName(focus_entered), callable_mp(this, &RenameDialog::_update_substitute));
	lne_suffix->connect(SceneStringName(focus_exited), callable_mp(this, &RenameDialog::_update_substitute));

	// Preview

	lne_prefix->connect(SceneStringName(text_changed), callable_mp(this, &RenameDialog::_update_preview));
	lne_suffix->connect(SceneStringName(text_changed), callable_mp(this, &RenameDialog::_update_preview));
	lne_search->connect(SceneStringName(text_changed), callable_mp(this, &RenameDialog::_update_preview));
	lne_replace->connect(SceneStringName(text_changed), callable_mp(this, &RenameDialog::_update_preview));
	spn_count_start->connect(SceneStringName(value_changed), callable_mp(this, &RenameDialog::_update_preview_int));
	spn_count_step->connect(SceneStringName(value_changed), callable_mp(this, &RenameDialog::_update_preview_int));
	spn_count_padding->connect(SceneStringName(value_changed), callable_mp(this, &RenameDialog::_update_preview_int));
	opt_style->connect(SceneStringName(item_selected), callable_mp(this, &RenameDialog::_update_preview_int));
	opt_case->connect(SceneStringName(item_selected), callable_mp(this, &RenameDialog::_update_preview_int));
	cbut_substitute->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_update_preview).bind(""));
	cbut_regex->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_update_preview).bind(""));
	cbut_process->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::_update_preview).bind(""));

	but_reset->connect(SceneStringName(pressed), callable_mp(this, &RenameDialog::reset));

	reset();
	_features_toggled(false);
}

void RenameDialog::_bind_methods() {
	ClassDB::bind_method("rename", &RenameDialog::rename);
}

void RenameDialog::_update_substitute() {
	LineEdit *focus_owner_line_edit = Object::cast_to<LineEdit>(get_viewport()->gui_get_focus_owner());
	bool is_main_field = _is_main_field(focus_owner_line_edit);

	but_insert_name->set_disabled(!is_main_field);
	but_insert_parent->set_disabled(!is_main_field);
	but_insert_type->set_disabled(!is_main_field);
	but_insert_scene->set_disabled(!is_main_field);
	but_insert_root->set_disabled(!is_main_field);
	but_insert_count->set_disabled(!is_main_field);

	// The focus mode seems to be reset when disabling/re-enabling
	but_insert_name->set_focus_mode(Control::FOCUS_NONE);
	but_insert_parent->set_focus_mode(Control::FOCUS_NONE);
	but_insert_type->set_focus_mode(Control::FOCUS_NONE);
	but_insert_scene->set_focus_mode(Control::FOCUS_NONE);
	but_insert_root->set_focus_mode(Control::FOCUS_NONE);
	but_insert_count->set_focus_mode(Control::FOCUS_NONE);
}

void RenameDialog::_post_popup() {
	ConfirmationDialog::_post_popup();

	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	preview_node = nullptr;

	Array selected_node_list = editor_selection->get_selected_nodes();
	ERR_FAIL_COND(selected_node_list.is_empty());

	preview_node = Object::cast_to<Node>(selected_node_list[0]);

	_update_preview();
	_update_substitute();
}

void RenameDialog::_update_preview_int(int new_value) {
	_update_preview();
}

void RenameDialog::_update_preview(const String &new_text) {
	if (lock_preview_update || preview_node == nullptr) {
		return;
	}

	has_errors = false;
	add_error_handler(&eh);

	String new_name = _apply_rename(preview_node, spn_count_start->get_value());

	if (!has_errors) {
		lbl_preview_title->set_text(TTR("Preview:"));
		lbl_preview->set_text(new_name);

		if (new_name == preview_node->get_name()) {
			// New name is identical to the old one. Don't color it as much to avoid distracting the user.
			const Color accent_color = EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("accent_color"), EditorStringName(Editor));
			const Color text_color = EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("default_color"), SNAME("RichTextLabel"));
			lbl_preview->add_theme_color_override(SceneStringName(font_color), accent_color.lerp(text_color, 0.5));
		} else {
			lbl_preview->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("success_color"), EditorStringName(Editor)));
		}
	}

	remove_error_handler(&eh);
}

String RenameDialog::_apply_rename(const Node *node, int count) {
	String search = lne_search->get_text();
	String replace = lne_replace->get_text();
	String prefix = lne_prefix->get_text();
	String suffix = lne_suffix->get_text();
	String new_name = node->get_name();

	if (cbut_substitute->is_pressed()) {
		search = _substitute(search, node, count);
		replace = _substitute(replace, node, count);
		prefix = _substitute(prefix, node, count);
		suffix = _substitute(suffix, node, count);
	}

	if (cbut_regex->is_pressed()) {
		new_name = _regex(search, new_name, replace);
	} else {
		new_name = new_name.replace(search, replace);
	}

	new_name = prefix + new_name + suffix;

	if (cbut_process->is_pressed()) {
		new_name = _postprocess(new_name);
	}

	return new_name;
}

String RenameDialog::_substitute(const String &subject, const Node *node, int count) {
	String result = subject.replace("${COUNTER}", vformat("%0" + itos(spn_count_padding->get_value()) + "d", count));

	if (node) {
		result = result.replace("${NAME}", node->get_name());
		result = result.replace("${TYPE}", node->get_class());
	}

	int current = EditorNode::get_editor_data().get_edited_scene();
	// Always request the scene title with the extension stripped.
	// Otherwise, the result could vary depending on whether a scene with the same name
	// (but different extension) is currently open.
	result = result.replace("${SCENE}", EditorNode::get_editor_data().get_scene_title(current, true));

	Node *root_node = SceneTree::get_singleton()->get_edited_scene_root();
	if (root_node) {
		result = result.replace("${ROOT}", root_node->get_name());
	}
	if (node) {
		Node *parent_node = node->get_parent();
		if (parent_node) {
			if (node == root_node) {
				// Can not substitute parent of root.
				result = result.replace("${PARENT}", "");
			} else {
				result = result.replace("${PARENT}", parent_node->get_name());
			}
		}
	}
	return result;
}

void RenameDialog::_error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	RenameDialog *self = (RenameDialog *)p_self;
	String source_file = String::utf8(p_file);

	// Only show first error that is related to "regex"
	if (self->has_errors || !source_file.contains("regex")) {
		return;
	}

	String err_str;
	if (p_errorexp && p_errorexp[0]) {
		err_str = String::utf8(p_errorexp);
	} else {
		err_str = String::utf8(p_error);
	}

	self->has_errors = true;
	self->lbl_preview_title->set_text(TTR("Regular Expression Error:"));
	self->lbl_preview->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));
	self->lbl_preview->set_text(vformat(TTR("At character %s"), err_str));
}

String RenameDialog::_regex(const String &pattern, const String &subject, const String &replacement) {
	RegEx regex(pattern);

	return regex.sub(subject, replacement, true);
}

String RenameDialog::_postprocess(const String &subject) {
	int style_id = opt_style->get_selected();

	String result = subject;

	if (style_id == 1) {
		// PascalCase to snake_case

		result = result.to_snake_case();
		result = _regex("_+", result, "_");

	} else if (style_id == 2) {
		// snake_case to PascalCase

		RegEx pattern("_+(.?)");
		Array matches = pattern.search_all(result);

		// The name `_` would become empty; ignore it.
		if (matches.size() && result != "_") {
			String buffer;
			int start = 0;
			int end = 0;
			for (int i = 0; i < matches.size(); ++i) {
				start = ((Ref<RegExMatch>)matches[i])->get_start(1);
				buffer += result.substr(end, start - end - 1);
				buffer += result.substr(start, 1).to_upper();
				end = start + 1;
			}
			buffer += result.substr(end, result.size() - (end + 1));
			result = buffer.to_pascal_case();
		}
	}

	int case_id = opt_case->get_selected();

	if (case_id == 1) {
		// To Lowercase
		result = result.to_lower();
	} else if (case_id == 2) {
		// To Uppercase
		result = result.to_upper();
	}

	return result;
}

void RenameDialog::_iterate_scene(const Node *node, const Array &selection, int *counter) {
	if (!node) {
		return;
	}

	if (selection.has(node)) {
		String new_name = _apply_rename(node, *counter);

		if (node->get_name() != new_name) {
			Pair<NodePath, String> rename_item;
			rename_item.first = node->get_path();
			rename_item.second = new_name;
			to_rename.push_back(rename_item);
		}

		*counter += spn_count_step->get_value();
	}

	int *cur_counter = counter;
	int level_counter = spn_count_start->get_value();

	if (chk_per_level_counter->is_pressed()) {
		cur_counter = &level_counter;
	}

	for (int i = 0; i < node->get_child_count(); ++i) {
		_iterate_scene(node->get_child(i), selection, cur_counter);
	}
}

void RenameDialog::rename() {
	// Editor selection is not ordered via scene tree. Instead iterate
	// over scene tree until all selected nodes are found in order.

	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	Array selected_node_list = editor_selection->get_selected_nodes();
	Node *root_node = SceneTree::get_singleton()->get_edited_scene_root();

	global_count = spn_count_start->get_value();
	to_rename.clear();

	// Forward recursive as opposed to the actual renaming.
	_iterate_scene(root_node, selected_node_list, &global_count);

	if (!to_rename.is_empty()) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Batch Rename"), UndoRedo::MERGE_DISABLE, root_node, true);

		// Make sure to iterate reversed so that child nodes will find parents.
		for (List<Pair<NodePath, String>>::Element *E = to_rename.back(); E; E = E->prev()) {
			Node *n = root_node->get_node(E->get().first);
			const String &new_name = E->get().second;

			if (!n) {
				ERR_PRINT("Skipping missing node: " + E->get().first.get_concatenated_subnames());
				continue;
			}
			scene_tree_editor->rename_node(n, new_name);
		}

		undo_redo->commit_action();
	}
}

void RenameDialog::reset() {
	lock_preview_update = true;

	lne_prefix->clear();
	lne_suffix->clear();
	lne_search->clear();
	lne_replace->clear();

	cbut_substitute->set_pressed(false);
	cbut_regex->set_pressed(false);
	cbut_process->set_pressed(false);

	chk_per_level_counter->set_pressed(true);

	spn_count_start->set_value(1);
	spn_count_step->set_value(1);
	spn_count_padding->set_value(1);

	opt_style->select(0);
	opt_case->select(0);

	lock_preview_update = false;
	_update_preview();
}

bool RenameDialog::_is_main_field(LineEdit *line_edit) {
	return line_edit &&
			(line_edit == lne_search || line_edit == lne_replace || line_edit == lne_prefix || line_edit == lne_suffix);
}

void RenameDialog::_insert_text(const String &text) {
	LineEdit *focus_owner = Object::cast_to<LineEdit>(get_viewport()->gui_get_focus_owner());

	if (_is_main_field(focus_owner)) {
		focus_owner->selection_delete();
		focus_owner->insert_text_at_caret(text);
		_update_preview();
	}
}

void RenameDialog::_features_toggled(bool pressed) {
	if (pressed) {
		tabc_features->show();
	} else {
		tabc_features->hide();
	}

	// Adjust to minimum size in y
	Size2i new_size = get_size();
	new_size.y = 0;
	set_size(new_size);
}
