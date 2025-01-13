/**************************************************************************/
/*  replication_editor.cpp                                                */
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

#include "replication_editor.h"

#include "../multiplayer_synchronizer.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/scene_tree_editor.h"
#include "editor/inspector_dock.h"
#include "editor/property_selector.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/separator.h"
#include "scene/gui/tree.h"

void ReplicationEditor::_pick_node_filter_text_changed(const String &p_newtext) {
	TreeItem *root_item = pick_node->get_scene_tree()->get_scene_tree()->get_root();

	Vector<Node *> select_candidates;
	Node *to_select = nullptr;

	String filter = pick_node->get_filter_line_edit()->get_text();

	_pick_node_select_recursive(root_item, filter, select_candidates);

	if (!select_candidates.is_empty()) {
		for (int i = 0; i < select_candidates.size(); ++i) {
			Node *candidate = select_candidates[i];

			if (((String)candidate->get_name()).to_lower().begins_with(filter.to_lower())) {
				to_select = candidate;
				break;
			}
		}

		if (!to_select) {
			to_select = select_candidates[0];
		}
	}

	pick_node->get_scene_tree()->set_selected(to_select);
}

void ReplicationEditor::_pick_node_select_recursive(TreeItem *p_item, const String &p_filter, Vector<Node *> &p_select_candidates) {
	if (!p_item) {
		return;
	}

	NodePath np = p_item->get_metadata(0);
	Node *node = get_node(np);

	if (!p_filter.is_empty() && ((String)node->get_name()).containsn(p_filter)) {
		p_select_candidates.push_back(node);
	}

	TreeItem *c = p_item->get_first_child();

	while (c) {
		_pick_node_select_recursive(c, p_filter, p_select_candidates);
		c = c->get_next();
	}
}

void ReplicationEditor::_pick_node_selected(NodePath p_path) {
	Node *root = current->get_node(current->get_root_path());
	ERR_FAIL_NULL(root);
	Node *node = get_node(p_path);
	ERR_FAIL_NULL(node);
	NodePath path_to = root->get_path_to(node);
	adding_node_path = path_to;
	prop_selector->select_property_from_instance(node);
}

void ReplicationEditor::_pick_new_property() {
	if (current == nullptr) {
		EditorNode::get_singleton()->show_warning(TTR("Select a replicator node in order to pick a property to add to it."));
		return;
	}
	Node *root = current->get_node(current->get_root_path());
	if (!root) {
		EditorNode::get_singleton()->show_warning(TTR("Not possible to add a new property to synchronize without a root."));
		return;
	}
	pick_node->popup_scenetree_dialog(nullptr, current);
	pick_node->get_filter_line_edit()->clear();
	pick_node->get_filter_line_edit()->grab_focus();
}

void ReplicationEditor::_add_sync_property(String p_path) {
	config = current->get_replication_config();

	if (config.is_valid() && config->has_property(p_path)) {
		EditorNode::get_singleton()->show_warning(TTR("Property is already being synchronized."));
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add property to synchronizer"));

	if (config.is_null()) {
		config.instantiate();
		current->set_replication_config(config);
		undo_redo->add_do_method(current, "set_replication_config", config);
		undo_redo->add_undo_method(current, "set_replication_config", Ref<SceneReplicationConfig>());
		_update_config();
	}

	undo_redo->add_do_method(config.ptr(), "add_property", p_path);
	undo_redo->add_undo_method(config.ptr(), "remove_property", p_path);
	undo_redo->add_do_method(this, "_update_config");
	undo_redo->add_undo_method(this, "_update_config");
	undo_redo->commit_action();
}

void ReplicationEditor::_pick_node_property_selected(String p_name) {
	String adding_prop_path = String(adding_node_path) + ":" + p_name;

	_add_sync_property(adding_prop_path);
}

/// ReplicationEditor
ReplicationEditor::ReplicationEditor() {
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_custom_minimum_size(Size2(0, 200) * EDSCALE);

	delete_dialog = memnew(ConfirmationDialog);
	delete_dialog->connect("canceled", callable_mp(this, &ReplicationEditor::_dialog_closed).bind(false));
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &ReplicationEditor::_dialog_closed).bind(true));
	add_child(delete_dialog);

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(vb);

	pick_node = memnew(SceneTreeDialog);
	add_child(pick_node);
	pick_node->set_title(TTR("Pick a node to synchronize:"));
	pick_node->connect("selected", callable_mp(this, &ReplicationEditor::_pick_node_selected));
	pick_node->get_filter_line_edit()->connect(SceneStringName(text_changed), callable_mp(this, &ReplicationEditor::_pick_node_filter_text_changed));

	prop_selector = memnew(PropertySelector);
	add_child(prop_selector);
	// Filter out properties that cannot be synchronized.
	// * RIDs do not match across network.
	// * Objects are too large for replication.
	Vector<Variant::Type> types = {
		Variant::BOOL,
		Variant::INT,
		Variant::FLOAT,
		Variant::STRING,

		Variant::VECTOR2,
		Variant::VECTOR2I,
		Variant::RECT2,
		Variant::RECT2I,
		Variant::VECTOR3,
		Variant::VECTOR3I,
		Variant::TRANSFORM2D,
		Variant::VECTOR4,
		Variant::VECTOR4I,
		Variant::PLANE,
		Variant::QUATERNION,
		Variant::AABB,
		Variant::BASIS,
		Variant::TRANSFORM3D,
		Variant::PROJECTION,

		Variant::COLOR,
		Variant::STRING_NAME,
		Variant::NODE_PATH,
		// Variant::RID,
		// Variant::OBJECT,
		Variant::SIGNAL,
		Variant::DICTIONARY,
		Variant::ARRAY,

		Variant::PACKED_BYTE_ARRAY,
		Variant::PACKED_INT32_ARRAY,
		Variant::PACKED_INT64_ARRAY,
		Variant::PACKED_FLOAT32_ARRAY,
		Variant::PACKED_FLOAT64_ARRAY,
		Variant::PACKED_STRING_ARRAY,
		Variant::PACKED_VECTOR2_ARRAY,
		Variant::PACKED_VECTOR3_ARRAY,
		Variant::PACKED_COLOR_ARRAY,
		Variant::PACKED_VECTOR4_ARRAY,
	};
	prop_selector->set_type_filter(types);
	prop_selector->connect("selected", callable_mp(this, &ReplicationEditor::_pick_node_property_selected));

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	add_pick_button = memnew(Button);
	add_pick_button->connect(SceneStringName(pressed), callable_mp(this, &ReplicationEditor::_pick_new_property));
	add_pick_button->set_text(TTR("Add property to sync..."));
	hb->add_child(add_pick_button);

	VSeparator *vs = memnew(VSeparator);
	vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	hb->add_child(vs);
	hb->add_child(memnew(Label(TTR("Path:"))));

	np_line_edit = memnew(LineEdit);
	np_line_edit->set_placeholder(":property");
	np_line_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	np_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &ReplicationEditor::_np_text_submitted));
	hb->add_child(np_line_edit);

	add_from_path_button = memnew(Button);
	add_from_path_button->connect(SceneStringName(pressed), callable_mp(this, &ReplicationEditor::_add_pressed));
	add_from_path_button->set_text(TTR("Add from path"));
	hb->add_child(add_from_path_button);

	vs = memnew(VSeparator);
	vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	hb->add_child(vs);

	pin = memnew(Button);
	pin->set_theme_type_variation(SceneStringName(FlatButton));
	pin->set_toggle_mode(true);
	pin->set_tooltip_text(TTR("Pin replication editor"));
	hb->add_child(pin);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_columns(4);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Properties"));
	tree->set_column_expand(0, true);
	tree->set_column_title(1, TTR("Spawn"));
	tree->set_column_expand(1, false);
	tree->set_column_custom_minimum_width(1, 100);
	tree->set_column_title(2, TTR("Replicate"));
	tree->set_column_custom_minimum_width(2, 100);
	tree->set_column_expand(2, false);
	tree->set_column_expand(3, false);
	tree->create_item();
	tree->connect("button_clicked", callable_mp(this, &ReplicationEditor::_tree_button_pressed));
	tree->connect("item_edited", callable_mp(this, &ReplicationEditor::_tree_item_edited));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	vb->add_child(tree);

	drop_label = memnew(Label);
	drop_label->set_text(TTR("Add properties using the options above, or\ndrag them from the inspector and drop them here."));
	drop_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	drop_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	tree->add_child(drop_label);
	drop_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	SET_DRAG_FORWARDING_CDU(tree, ReplicationEditor);
}

void ReplicationEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_config"), &ReplicationEditor::_update_config);
	ClassDB::bind_method(D_METHOD("_update_value", "property", "column", "value"), &ReplicationEditor::_update_value);
}

bool ReplicationEditor::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}
	String t = d["type"];
	if (t != "obj_property") {
		return false;
	}
	Object *obj = d["object"];
	if (!obj) {
		return false;
	}
	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		return false;
	}

	return true;
}

void ReplicationEditor::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (current == nullptr) {
		EditorNode::get_singleton()->show_warning(TTR("Select a replicator node in order to pick a property to add to it."));
		return;
	}
	Node *root = current->get_node(current->get_root_path());
	if (!root) {
		EditorNode::get_singleton()->show_warning(TTR("Not possible to add a new property to synchronize without a root."));
		return;
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}
	String t = d["type"];
	if (t != "obj_property") {
		return;
	}
	Object *obj = d["object"];
	if (!obj) {
		return;
	}
	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		return;
	}

	String path = root->get_path_to(node);
	path += ":" + String(d["property"]);

	_add_sync_property(path);
}

void ReplicationEditor::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorThemeManager::is_generated_theme_outdated()) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_ENTER_TREE: {
			add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SceneStringName(panel), SNAME("Panel")));
			add_pick_button->set_button_icon(get_theme_icon(SNAME("Add"), EditorStringName(EditorIcons)));
			pin->set_button_icon(get_theme_icon(SNAME("Pin"), EditorStringName(EditorIcons)));
		} break;
	}
}

void ReplicationEditor::_add_pressed() {
	if (!current) {
		EditorNode::get_singleton()->show_warning(TTR("Please select a MultiplayerSynchronizer first."));
		return;
	}
	if (current->get_root_path().is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("The MultiplayerSynchronizer needs a root path."));
		return;
	}
	String np_text = np_line_edit->get_text();

	if (np_text.is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("Property/path must not be empty."));
		return;
	}

	int idx = np_text.find_char(':');
	if (idx == -1) {
		np_text = ".:" + np_text;
	} else if (idx == 0) {
		np_text = "." + np_text;
	}
	NodePath path = NodePath(np_text);
	if (path.is_empty()) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Invalid property path: '%s'"), np_text));
		return;
	}

	_add_sync_property(path);
}

void ReplicationEditor::_np_text_submitted(const String &p_newtext) {
	_add_pressed();
}

void ReplicationEditor::_tree_item_edited() {
	TreeItem *ti = tree->get_edited();
	if (!ti || config.is_null()) {
		return;
	}
	int column = tree->get_edited_column();
	ERR_FAIL_COND(column < 1 || column > 2);
	const NodePath prop = ti->get_metadata(0);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	if (column == 1) {
		undo_redo->create_action(TTR("Set spawn property"));
		bool value = ti->is_checked(column);
		undo_redo->add_do_method(config.ptr(), "property_set_spawn", prop, value);
		undo_redo->add_undo_method(config.ptr(), "property_set_spawn", prop, !value);
		undo_redo->add_do_method(this, "_update_value", prop, column, value ? 1 : 0);
		undo_redo->add_undo_method(this, "_update_value", prop, column, value ? 0 : 1);
		undo_redo->commit_action();
	} else if (column == 2) {
		undo_redo->create_action(TTR("Set sync property"));
		int value = ti->get_range(column);
		int old_value = config->property_get_replication_mode(prop);
		// We have a hard limit of 64 watchable properties per synchronizer.
		if (value == SceneReplicationConfig::REPLICATION_MODE_ON_CHANGE && config->get_watch_properties().size() >= 64) {
			EditorNode::get_singleton()->show_warning(TTR("Each MultiplayerSynchronizer can have no more than 64 watched properties."));
			ti->set_range(column, old_value);
			return;
		}
		undo_redo->add_do_method(config.ptr(), "property_set_replication_mode", prop, value);
		undo_redo->add_undo_method(config.ptr(), "property_set_replication_mode", prop, old_value);
		undo_redo->add_do_method(this, "_update_value", prop, column, value);
		undo_redo->add_undo_method(this, "_update_value", prop, column, old_value);
		undo_redo->commit_action();
	} else {
		ERR_FAIL();
	}
}

void ReplicationEditor::_tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	if (!ti) {
		return;
	}
	deleting = ti->get_metadata(0);
	delete_dialog->set_text(TTR("Delete Property?") + "\n\"" + ti->get_text(0) + "\"");
	delete_dialog->popup_centered();
}

void ReplicationEditor::_dialog_closed(bool p_confirmed) {
	if (deleting.is_empty() || config.is_null()) {
		return;
	}
	if (p_confirmed) {
		const NodePath prop = deleting;
		int idx = config->property_get_index(prop);
		bool spawn = config->property_get_spawn(prop);
		SceneReplicationConfig::ReplicationMode mode = config->property_get_replication_mode(prop);
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Remove Property"));
		undo_redo->add_do_method(config.ptr(), "remove_property", prop);
		undo_redo->add_undo_method(config.ptr(), "add_property", prop, idx);
		undo_redo->add_undo_method(config.ptr(), "property_set_spawn", prop, spawn);
		undo_redo->add_undo_method(config.ptr(), "property_set_replication_mode", prop, mode);
		undo_redo->add_do_method(this, "_update_config");
		undo_redo->add_undo_method(this, "_update_config");
		undo_redo->commit_action();
	}
	deleting = NodePath();
}

void ReplicationEditor::_update_value(const NodePath &p_prop, int p_column, int p_value) {
	if (!tree->get_root()) {
		return;
	}
	TreeItem *ti = tree->get_root()->get_first_child();
	while (ti) {
		if (ti->get_metadata(0).operator NodePath() == p_prop) {
			if (p_column == 1) {
				ti->set_checked(p_column, p_value != 0);
			} else if (p_column == 2) {
				ti->set_range(p_column, p_value);
			}
			return;
		}
		ti = ti->get_next();
	}
}

void ReplicationEditor::_update_config() {
	deleting = NodePath();
	tree->clear();
	tree->create_item();
	drop_label->set_visible(true);
	if (!config.is_valid()) {
		return;
	}
	TypedArray<NodePath> props = config->get_properties();
	if (props.size()) {
		drop_label->set_visible(false);
	}
	for (int i = 0; i < props.size(); i++) {
		const NodePath path = props[i];
		_add_property(path, config->property_get_spawn(path), config->property_get_replication_mode(path));
	}
}

void ReplicationEditor::edit(MultiplayerSynchronizer *p_sync) {
	if (current == p_sync) {
		return;
	}
	current = p_sync;
	if (current) {
		config = current->get_replication_config();
	} else {
		config.unref();
	}
	_update_config();
}

Ref<Texture2D> ReplicationEditor::_get_class_icon(const Node *p_node) {
	if (!p_node || !has_theme_icon(p_node->get_class(), EditorStringName(EditorIcons))) {
		return get_theme_icon(SNAME("ImportFail"), EditorStringName(EditorIcons));
	}
	return get_theme_icon(p_node->get_class(), EditorStringName(EditorIcons));
}

static bool can_sync(const Variant &p_var) {
	switch (p_var.get_type()) {
		case Variant::RID:
		case Variant::OBJECT:
			return false;
		case Variant::ARRAY: {
			const Array &arr = p_var;
			if (arr.is_typed()) {
				const uint32_t type = arr.get_typed_builtin();
				return (type != Variant::RID) && (type != Variant::OBJECT);
			}
			return true;
		}
		default:
			return true;
	}
}

void ReplicationEditor::_add_property(const NodePath &p_property, bool p_spawn, SceneReplicationConfig::ReplicationMode p_mode) {
	String prop = String(p_property);
	TreeItem *item = tree->create_item();
	item->set_selectable(0, false);
	item->set_selectable(1, false);
	item->set_selectable(2, false);
	item->set_selectable(3, false);
	item->set_text(0, prop);
	item->set_metadata(0, prop);
	Node *root_node = current && !current->get_root_path().is_empty() ? current->get_node(current->get_root_path()) : nullptr;
	Ref<Texture2D> icon = _get_class_icon(root_node);
	if (root_node) {
		String path = prop.substr(0, prop.find_char(':'));
		String subpath = prop.substr(path.size());
		Node *node = root_node->get_node_or_null(path);
		if (!node) {
			node = root_node;
		}
		item->set_text(0, String(node->get_name()) + ":" + subpath);
		icon = _get_class_icon(node);
		bool valid = false;
		Variant value = node->get(subpath, &valid);
		if (valid && !can_sync(value)) {
			item->set_icon(0, get_theme_icon(SNAME("StatusWarning"), EditorStringName(EditorIcons)));
			item->set_tooltip_text(0, TTR("Property of this type not supported."));
		} else {
			item->set_icon(0, icon);
		}
	} else {
		item->set_icon(0, icon);
	}
	item->add_button(3, get_theme_icon(SNAME("Remove"), EditorStringName(EditorIcons)));
	item->set_text_alignment(1, HORIZONTAL_ALIGNMENT_CENTER);
	item->set_cell_mode(1, TreeItem::CELL_MODE_CHECK);
	item->set_checked(1, p_spawn);
	item->set_editable(1, true);
	item->set_text_alignment(2, HORIZONTAL_ALIGNMENT_CENTER);
	item->set_cell_mode(2, TreeItem::CELL_MODE_RANGE);
	item->set_range_config(2, 0, 2, 1);
	item->set_text(2, TTR("Never", "Replication Mode") + "," + TTR("Always", "Replication Mode") + "," + TTR("On Change", "Replication Mode"));
	item->set_range(2, (int)p_mode);
	item->set_editable(2, true);
}
