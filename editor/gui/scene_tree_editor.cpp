/**************************************************************************/
/*  scene_tree_editor.cpp                                                 */
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

#include "scene_tree_editor.h"

#include "core/config/project_settings.h"
#include "core/object/script_language.h"
#include "editor/editor_dock_manager.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/node_dock.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

Node *SceneTreeEditor::get_scene_node() const {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	return get_tree()->get_edited_scene_root();
}

void SceneTreeEditor::_cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	if (connect_to_script_mode) {
		return; //don't do anything in this mode
	}

	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	ERR_FAIL_NULL(item);

	NodePath np = item->get_metadata(0);

	Node *n = get_node(np);
	ERR_FAIL_NULL(n);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (p_id == BUTTON_SUBSCENE) {
		if (n == get_scene_node()) {
			if (n && n->get_scene_inherited_state().is_valid()) {
				emit_signal(SNAME("open"), n->get_scene_inherited_state()->get_path());
			}
		} else {
			emit_signal(SNAME("open"), n->get_scene_file_path());
		}
	} else if (p_id == BUTTON_SCRIPT) {
		Ref<Script> script_typed = n->get_script();
		if (!script_typed.is_null()) {
			emit_signal(SNAME("open_script"), script_typed);
		}

	} else if (p_id == BUTTON_VISIBILITY) {
		undo_redo->create_action(TTR("Toggle Visible"));
		_toggle_visible(n);
		List<Node *> selection = editor_selection->get_selected_node_list();
		if (selection.size() > 1 && selection.find(n) != nullptr) {
			for (Node *nv : selection) {
				ERR_FAIL_NULL(nv);
				if (nv == n) {
					continue;
				}
				_toggle_visible(nv);
			}
		}
		undo_redo->commit_action();
	} else if (p_id == BUTTON_LOCK) {
		undo_redo->create_action(TTR("Unlock Node"));
		undo_redo->add_do_method(n, "remove_meta", "_edit_lock_");
		undo_redo->add_undo_method(n, "set_meta", "_edit_lock_", true);
		undo_redo->add_do_method(this, "_update_tree");
		undo_redo->add_undo_method(this, "_update_tree");
		undo_redo->add_do_method(this, "emit_signal", "node_changed");
		undo_redo->add_undo_method(this, "emit_signal", "node_changed");
		undo_redo->commit_action();
	} else if (p_id == BUTTON_PIN) {
		if (n->is_class("AnimationMixer")) {
			AnimationPlayerEditor::get_singleton()->unpin();
			_update_tree();
		}

	} else if (p_id == BUTTON_GROUP) {
		undo_redo->create_action(TTR("Ungroup Children"));

		if (n->is_class("CanvasItem") || n->is_class("Node3D")) {
			undo_redo->add_do_method(n, "remove_meta", "_edit_group_");
			undo_redo->add_undo_method(n, "set_meta", "_edit_group_", true);
			undo_redo->add_do_method(this, "_update_tree");
			undo_redo->add_undo_method(this, "_update_tree");
			undo_redo->add_do_method(this, "emit_signal", "node_changed");
			undo_redo->add_undo_method(this, "emit_signal", "node_changed");
		}
		undo_redo->commit_action();
	} else if (p_id == BUTTON_WARNING) {
		const PackedStringArray warnings = n->get_configuration_warnings();

		if (warnings.is_empty()) {
			return;
		}

		// Improve looks on tooltip, extra spacing on non-bullet point newlines.
		const String bullet_point = U"•  ";
		String all_warnings;
		for (const String &w : warnings) {
			all_warnings += "\n" + bullet_point + w;
		}

		// Limit the line width while keeping some padding.
		// It is not efficient, but it does not have to be.
		const PackedInt32Array boundaries = TS->string_get_word_breaks(all_warnings, "", 80);
		PackedStringArray lines;
		for (int i = 0; i < boundaries.size(); i += 2) {
			const int start = boundaries[i];
			const int end = boundaries[i + 1];
			const String line = all_warnings.substr(start, end - start);
			lines.append(line);
		}
		all_warnings = String("\n").join(lines).indent("    ").replace(U"    •", U"\n•").substr(2); // We don't want the first two newlines.

		warning->set_text(all_warnings);
		warning->popup_centered();

	} else if (p_id == BUTTON_SIGNALS) {
		editor_selection->clear();
		editor_selection->add_node(n);

		set_selected(n);

		EditorDockManager::get_singleton()->focus_dock(NodeDock::get_singleton());
		NodeDock::get_singleton()->show_connections();
	} else if (p_id == BUTTON_GROUPS) {
		editor_selection->clear();
		editor_selection->add_node(n);

		set_selected(n);

		EditorDockManager::get_singleton()->focus_dock(NodeDock::get_singleton());
		NodeDock::get_singleton()->show_groups();
	} else if (p_id == BUTTON_UNIQUE) {
		bool ask_before_revoking_unique_name = EDITOR_GET("docks/scene_tree/ask_before_revoking_unique_name");
		revoke_node = n;
		if (ask_before_revoking_unique_name) {
			String msg = vformat(TTR("Revoke unique name for node \"%s\"?"), n->get_name());
			ask_before_revoke_checkbox->set_pressed(false);
			revoke_dialog_label->set_text(msg);
			revoke_dialog->reset_size();
			revoke_dialog->popup_centered();
		} else {
			_revoke_unique_name();
		}
	}
}

void SceneTreeEditor::_update_ask_before_revoking_unique_name() {
	if (ask_before_revoke_checkbox->is_pressed()) {
		EditorSettings::get_singleton()->set("docks/scene_tree/ask_before_revoking_unique_name", false);
		ask_before_revoke_checkbox->set_pressed(false);
	}

	_revoke_unique_name();
}

void SceneTreeEditor::_revoke_unique_name() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(TTR("Disable Scene Unique Name"));
	undo_redo->add_do_method(revoke_node, "set_unique_name_in_owner", false);
	undo_redo->add_undo_method(revoke_node, "set_unique_name_in_owner", true);
	undo_redo->add_do_method(this, "_update_tree");
	undo_redo->add_undo_method(this, "_update_tree");
	undo_redo->commit_action();
}

void SceneTreeEditor::_toggle_visible(Node *p_node) {
	if (p_node->has_method("is_visible") && p_node->has_method("set_visible")) {
		bool v = bool(p_node->call("is_visible"));
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->add_do_method(p_node, "set_visible", !v);
		undo_redo->add_undo_method(p_node, "set_visible", v);
	}
}

void SceneTreeEditor::_add_nodes(Node *p_node, TreeItem *p_parent) {
	if (!p_node) {
		return;
	}

	// only owned nodes are editable, since nodes can create their own (manually owned) child nodes,
	// which the editor needs not to know about.

	bool part_of_subscene = false;

	if (!display_foreign && p_node->get_owner() != get_scene_node() && p_node != get_scene_node()) {
		if ((show_enabled_subscene || can_open_instance) && p_node->get_owner() && (get_scene_node()->is_editable_instance(p_node->get_owner()))) {
			part_of_subscene = true;
			//allow
		} else {
			return;
		}
	} else {
		part_of_subscene = p_node != get_scene_node() && get_scene_node()->get_scene_inherited_state().is_valid() && get_scene_node()->get_scene_inherited_state()->find_node_by_path(get_scene_node()->get_path_to(p_node)) >= 0;
	}

	TreeItem *item = tree->create_item(p_parent);

	item->set_text(0, p_node->get_name());
	item->set_text_overrun_behavior(0, TextServer::OVERRUN_NO_TRIMMING);
	if (can_rename && !part_of_subscene) {
		item->set_editable(0, true);
	}

	item->set_selectable(0, true);
	if (can_rename) {
		bool collapsed = p_node->is_displayed_folded();
		if (collapsed) {
			item->set_collapsed(true);
		}
	}

	Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(p_node, "Node");
	item->set_icon(0, icon);
	item->set_metadata(0, p_node->get_path());

	if (connecting_signal) {
		// Add script icons for all scripted nodes.
		Ref<Script> scr = p_node->get_script();
		if (scr.is_valid()) {
			item->add_button(0, get_editor_theme_icon(SNAME("Script")), BUTTON_SCRIPT);
			if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr) {
				// Disable button on custom scripts (pure visual cue).
				item->set_button_disabled(0, item->get_button_count(0) - 1, true);
			}
		}
	}

	if (connect_to_script_mode) {
		Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));

		Ref<Script> scr = p_node->get_script();
		bool has_custom_script = scr.is_valid() && EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr;
		if (scr.is_null() || has_custom_script) {
			_set_item_custom_color(item, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
			item->set_selectable(0, false);

			accent.a *= 0.7;
		}

		if (marked.has(p_node)) {
			String node_name = p_node->get_name();
			if (connecting_signal) {
				node_name += " " + TTR("(Connecting From)");
			}
			item->set_text(0, node_name);
			_set_item_custom_color(item, accent);
		}
	} else if (part_of_subscene) {
		if (valid_types.size() == 0) {
			_set_item_custom_color(item, get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		}
	} else if (marked.has(p_node)) {
		String node_name = p_node->get_name();
		if (connecting_signal) {
			node_name += " " + TTR("(Connecting From)");
		}
		item->set_text(0, node_name);
		item->set_selectable(0, marked_selectable);
		_set_item_custom_color(item, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
	} else if (!p_node->can_process()) {
		_set_item_custom_color(item, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	} else if (!marked_selectable && !marked_children_selectable) {
		Node *node = p_node;
		while (node) {
			if (marked.has(node)) {
				item->set_selectable(0, false);
				_set_item_custom_color(item, get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
				break;
			}
			node = node->get_parent();
		}
	}

	if (can_rename) { //should be can edit..

		const PackedStringArray warnings = p_node->get_configuration_warnings();
		const int num_warnings = warnings.size();
		if (num_warnings > 0) {
			StringName warning_icon;
			if (num_warnings == 1) {
				warning_icon = SNAME("NodeWarning");
			} else if (num_warnings <= 3) {
				warning_icon = vformat("NodeWarnings%d", num_warnings);
			} else {
				warning_icon = SNAME("NodeWarnings4Plus");
			}

			// Improve looks on tooltip, extra spacing on non-bullet point newlines.
			const String bullet_point = U"•  ";
			String all_warnings;
			for (const String &w : warnings) {
				all_warnings += "\n\n" + bullet_point + w.replace("\n", "\n    ");
			}
			if (num_warnings == 1) {
				all_warnings.remove_at(0); // With only one warning, two newlines do not look great.
			}

			item->add_button(0, get_editor_theme_icon(warning_icon), BUTTON_WARNING, false, TTR("Node configuration warning:") + all_warnings);
		}

		if (p_node->is_unique_name_in_owner()) {
			const bool disabled = p_node->get_owner() != EditorNode::get_singleton()->get_edited_scene();
			String button_text = vformat(TTR("This node can be accessed from anywhere within the scene it belongs to by using the '%s' prefix in the node path."), UNIQUE_NODE_PREFIX);
			if (!disabled) {
				button_text += "\n" + TTR("Click to disable this.");
			}
			item->add_button(0, get_editor_theme_icon(SNAME("SceneUniqueName")), BUTTON_UNIQUE, disabled, button_text);
		}

		int num_connections = p_node->get_persistent_signal_connection_count();
		int num_groups = p_node->get_persistent_group_count();

		String msg_temp;
		if (num_connections >= 1) {
			Array arr;
			arr.push_back(num_connections);
			msg_temp += TTRN("Node has one connection.", "Node has {num} connections.", num_connections).format(arr, "{num}");
			if (num_groups >= 1) {
				msg_temp += "\n";
			}
		}
		if (num_groups >= 1) {
			msg_temp += TTRN("Node is in this group:", "Node is in the following groups:", num_groups) + "\n";

			List<GroupInfo> groups;
			p_node->get_groups(&groups);
			for (const GroupInfo &E : groups) {
				if (E.persistent) {
					msg_temp += String::utf8("•  ") + String(E.name) + "\n";
				}
			}
		} else {
			msg_temp += "\n";
		}

		Ref<Texture2D> icon_temp;
		SceneTreeEditorButton signal_temp = BUTTON_SIGNALS;
		String msg_temp_end = TTR("Click to show signals dock.");

		if (num_connections >= 1 && num_groups >= 1) {
			icon_temp = get_editor_theme_icon(SNAME("SignalsAndGroups"));
		} else if (num_connections >= 1) {
			icon_temp = get_editor_theme_icon(SNAME("Signals"));
		} else if (num_groups >= 1) {
			icon_temp = get_editor_theme_icon(SNAME("Groups"));
			signal_temp = BUTTON_GROUPS;
			msg_temp_end = TTR("Click to show groups dock.");
		}

		if (num_connections >= 1 || num_groups >= 1) {
			msg_temp += msg_temp_end;
			item->add_button(0, icon_temp, signal_temp, false, msg_temp);
		}
	}

	{
		_update_node_tooltip(p_node, item);
		Callable delay_update_tooltip = callable_mp(this, &SceneTreeEditor::_queue_update_node_tooltip);
		if (p_node->is_connected("editor_description_changed", delay_update_tooltip)) {
			p_node->disconnect("editor_description_changed", delay_update_tooltip);
		}
		p_node->connect("editor_description_changed", delay_update_tooltip.bind(item));
	}

	if (can_open_instance && is_scene_tree_dock) { // Show buttons only when necessary (SceneTreeDock) to avoid crashes.
		if (!p_node->is_connected(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
			p_node->connect(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed).bind(p_node));
		}

		Ref<Script> scr = p_node->get_script();
		if (!scr.is_null()) {
			String additional_notes;
			Color button_color = Color(1, 1, 1);
			// Can't set tooltip after adding button, need to do it before.
			if (scr->is_tool()) {
				additional_notes += "\n" + TTR("This script is currently running in the editor.");
				button_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			}
			if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr) {
				additional_notes += "\n" + TTR("This script is a custom type.");
				button_color.a = 0.5;
			}
			item->add_button(0, get_editor_theme_icon(SNAME("Script")), BUTTON_SCRIPT, false, TTR("Open Script:") + " " + scr->get_path() + additional_notes);
			item->set_button_color(0, item->get_button_count(0) - 1, button_color);
		}

		if (p_node->has_meta("_edit_lock_")) {
			item->add_button(0, get_editor_theme_icon(SNAME("Lock")), BUTTON_LOCK, false, TTR("Node is locked.\nClick to unlock it."));
		}
		if (p_node->has_meta("_edit_group_")) {
			item->add_button(0, get_editor_theme_icon(SNAME("Group")), BUTTON_GROUP, false, TTR("Children are not selectable.\nClick to make them selectable."));
		}

		if (p_node->has_method("is_visible") && p_node->has_method("set_visible") && p_node->has_signal(SceneStringName(visibility_changed))) {
			bool is_visible = p_node->call("is_visible");
			if (is_visible) {
				item->add_button(0, get_editor_theme_icon(SNAME("GuiVisibilityVisible")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			} else {
				item->add_button(0, get_editor_theme_icon(SNAME("GuiVisibilityHidden")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			}
			const Callable vis_changed = callable_mp(this, &SceneTreeEditor::_node_visibility_changed);
			if (!p_node->is_connected(SceneStringName(visibility_changed), vis_changed)) {
				p_node->connect(SceneStringName(visibility_changed), vis_changed.bind(p_node));
			}
			_update_visibility_color(p_node, item);
		}

		if (p_node->is_class("AnimationMixer")) {
			bool is_pinned = AnimationPlayerEditor::get_singleton()->get_editing_node() == p_node && AnimationPlayerEditor::get_singleton()->is_pinned();

			if (is_pinned) {
				item->add_button(0, get_editor_theme_icon(SNAME("Pin")), BUTTON_PIN, false, TTR("AnimationPlayer is pinned.\nClick to unpin."));
			}
		}
	}

	if (editor_selection) {
		if (editor_selection->is_selected(p_node)) {
			item->select(0);
		}
	}

	if (selected == p_node) {
		if (!editor_selection) {
			item->select(0);
		}
		item->set_as_cursor(0);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_nodes(p_node->get_child(i), item);
	}

	if (valid_types.size()) {
		bool valid = false;
		for (const StringName &E : valid_types) {
			if (p_node->is_class(E) ||
					EditorNode::get_singleton()->is_object_of_custom_type(p_node, E)) {
				valid = true;
				break;
			} else {
				Ref<Script> node_script = p_node->get_script();
				while (node_script.is_valid()) {
					if (node_script->get_path() == E) {
						valid = true;
						break;
					}
					node_script = node_script->get_base_script();
				}
				if (valid) {
					break;
				}
			}
		}

		if (!valid) {
			_set_item_custom_color(item, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
			item->set_selectable(0, false);
		}
	}
}

void SceneTreeEditor::_queue_update_node_tooltip(Node *p_node, TreeItem *p_item) {
	Callable update_tooltip = callable_mp(this, &SceneTreeEditor::_update_node_tooltip);
	if (update_node_tooltip_delay->is_connected("timeout", update_tooltip)) {
		update_node_tooltip_delay->disconnect("timeout", update_tooltip);
	}

	update_node_tooltip_delay->connect("timeout", update_tooltip.bind(p_node, p_item));
	update_node_tooltip_delay->start();
}

void SceneTreeEditor::_update_node_tooltip(Node *p_node, TreeItem *p_item) {
	// Display the node name in all tooltips so that long node names can be previewed
	// without having to rename them.
	String tooltip = p_node->get_name();

	if (p_node == get_scene_node() && p_node->get_scene_inherited_state().is_valid()) {
		if (p_item->get_button_by_id(0, BUTTON_SUBSCENE) == -1) {
			p_item->add_button(0, get_editor_theme_icon(SNAME("InstanceOptions")), BUTTON_SUBSCENE, false, TTR("Open in Editor"));
		}
		tooltip += String("\n" + TTR("Inherits:") + " " + p_node->get_scene_inherited_state()->get_path());
	} else if (p_node != get_scene_node() && !p_node->get_scene_file_path().is_empty() && can_open_instance) {
		if (p_item->get_button_by_id(0, BUTTON_SUBSCENE) == -1) {
			p_item->add_button(0, get_editor_theme_icon(SNAME("InstanceOptions")), BUTTON_SUBSCENE, false, TTR("Open in Editor"));
		}
		tooltip += String("\n" + TTR("Instance:") + " " + p_node->get_scene_file_path());
	}

	StringName custom_type = EditorNode::get_singleton()->get_object_custom_type_name(p_node);
	tooltip += "\n" + TTR("Type:") + " " + (custom_type != StringName() ? String(custom_type) : p_node->get_class());

	if (!p_node->get_editor_description().is_empty()) {
		const PackedInt32Array boundaries = TS->string_get_word_breaks(p_node->get_editor_description(), "", 80);
		tooltip += "\n";

		for (int i = 0; i < boundaries.size(); i += 2) {
			const int start = boundaries[i];
			const int end = boundaries[i + 1];
			tooltip += "\n" + p_node->get_editor_description().substr(start, end - start + 1).rstrip("\n");
		}
	}

	p_item->set_tooltip_text(0, tooltip);
}

void SceneTreeEditor::_node_visibility_changed(Node *p_node) {
	if (!p_node || (p_node != get_scene_node() && !p_node->get_owner())) {
		return;
	}

	TreeItem *item = _find(tree->get_root(), p_node->get_path());

	if (!item) {
		return;
	}

	int idx = item->get_button_by_id(0, BUTTON_VISIBILITY);
	ERR_FAIL_COND(idx == -1);

	bool node_visible = false;

	if (p_node->has_method("is_visible")) {
		node_visible = p_node->call("is_visible");
		if (p_node->is_class("CanvasItem") || p_node->is_class("CanvasLayer") || p_node->is_class("Window")) {
			CanvasItemEditor::get_singleton()->get_viewport_control()->queue_redraw();
		}
	}

	if (node_visible) {
		item->set_button(0, idx, get_editor_theme_icon(SNAME("GuiVisibilityVisible")));
	} else {
		item->set_button(0, idx, get_editor_theme_icon(SNAME("GuiVisibilityHidden")));
	}

	_update_visibility_color(p_node, item);
}

void SceneTreeEditor::_update_visibility_color(Node *p_node, TreeItem *p_item) {
	if (p_node->has_method("is_visible_in_tree")) {
		Color color(1, 1, 1, 1);
		bool visible_on_screen = p_node->call("is_visible_in_tree");
		if (!visible_on_screen) {
			color.a = 0.6;
		}
		int idx = p_item->get_button_by_id(0, BUTTON_VISIBILITY);
		p_item->set_button_color(0, idx, color);
	}
}

void SceneTreeEditor::_set_item_custom_color(TreeItem *p_item, Color p_color) {
	p_item->set_custom_color(0, p_color);
	p_item->set_meta(SNAME("custom_color"), p_color);
}

void SceneTreeEditor::_node_script_changed(Node *p_node) {
	if (tree_dirty) {
		return;
	}

	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
}

void SceneTreeEditor::_node_removed(Node *p_node) {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; //speed up exit
	}

	if (p_node->is_connected(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
		p_node->disconnect(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed));
	}

	if (p_node->has_signal(SceneStringName(visibility_changed))) {
		if (p_node->is_connected(SceneStringName(visibility_changed), callable_mp(this, &SceneTreeEditor::_node_visibility_changed))) {
			p_node->disconnect(SceneStringName(visibility_changed), callable_mp(this, &SceneTreeEditor::_node_visibility_changed));
		}
	}

	if (p_node == selected) {
		selected = nullptr;
	}
}

void SceneTreeEditor::_node_renamed(Node *p_node) {
	if (p_node != get_scene_node() && !get_scene_node()->is_ancestor_of(p_node)) {
		return;
	}

	emit_signal(SNAME("node_renamed"));

	if (!tree_dirty) {
		callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
		tree_dirty = true;
	}
}

void SceneTreeEditor::_update_tree(bool p_scroll_to_selected) {
	if (!is_inside_tree()) {
		tree_dirty = false;
		return;
	}

	if (tree->is_editing()) {
		return;
	}

	updating_tree = true;
	tree->clear();
	last_hash = hash_djb2_one_64(0);
	if (get_scene_node()) {
		_add_nodes(get_scene_node(), nullptr);
		_compute_hash(get_scene_node(), last_hash);
	}
	updating_tree = false;
	tree_dirty = false;

	if (!filter.strip_edges().is_empty() || !show_all_nodes) {
		_update_filter(nullptr, p_scroll_to_selected);
	}
}

bool SceneTreeEditor::_update_filter(TreeItem *p_parent, bool p_scroll_to_selected) {
	if (!p_parent) {
		p_parent = tree->get_root();
		filter_term_warning.clear();
	}

	if (!p_parent) {
		// Tree is empty, nothing to do here.
		return false;
	}

	bool keep_for_children = false;
	for (TreeItem *child = p_parent->get_first_child(); child; child = child->get_next()) {
		// Always keep if at least one of the children are kept.
		keep_for_children = _update_filter(child, p_scroll_to_selected) || keep_for_children;
	}

	// Now find other reasons to keep this Node, too.
	PackedStringArray terms = filter.to_lower().split_spaces();
	bool keep = _item_matches_all_terms(p_parent, terms);

	bool selectable = keep;
	if (keep && !valid_types.is_empty()) {
		selectable = false;
		Node *n = get_node(p_parent->get_metadata(0));

		for (const StringName &E : valid_types) {
			if (n->is_class(E) ||
					EditorNode::get_singleton()->is_object_of_custom_type(n, E)) {
				selectable = true;
				break;
			} else {
				Ref<Script> node_script = n->get_script();
				while (node_script.is_valid()) {
					if (node_script->get_path() == E) {
						selectable = true;
						break;
					}
					node_script = node_script->get_base_script();
				}
				if (selectable) {
					break;
				}
			}
		}
	}

	if (show_all_nodes) {
		p_parent->set_visible(keep_for_children || keep);
	} else {
		// Show only selectable nodes, or parents of selectable.
		p_parent->set_visible(keep_for_children || selectable);
	}

	if (selectable) {
		Color custom_color = p_parent->get_meta(SNAME("custom_color"), Color(0, 0, 0, 0));
		if (custom_color == Color(0, 0, 0, 0)) {
			p_parent->clear_custom_color(0);
		} else {
			p_parent->set_custom_color(0, custom_color);
		}
		p_parent->set_selectable(0, true);
	} else if (keep_for_children) {
		p_parent->set_custom_color(0, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
		p_parent->set_selectable(0, false);
		p_parent->deselect(0);
	}

	if (editor_selection) {
		Node *n = get_node(p_parent->get_metadata(0));
		if (selectable) {
			if (p_scroll_to_selected && n && editor_selection->is_selected(n)) {
				tree->scroll_to_item(p_parent);
			}
		} else {
			if (n && p_parent->is_selected(0)) {
				editor_selection->remove_node(n);
				p_parent->deselect(0);
			}
		}
	}

	return p_parent->is_visible();
}

bool SceneTreeEditor::_item_matches_all_terms(TreeItem *p_item, const PackedStringArray &p_terms) {
	if (p_terms.is_empty()) {
		return true;
	}

	for (int i = 0; i < p_terms.size(); i++) {
		const String &term = p_terms[i];

		// Recognize special filter.
		if (term.contains_char(':') && !term.get_slicec(':', 0).is_empty()) {
			String parameter = term.get_slicec(':', 0);
			String argument = term.get_slicec(':', 1);

			if (parameter == "type" || parameter == "t") {
				// Filter by Type.
				String type = get_node(p_item->get_metadata(0))->get_class();
				bool term_in_inherited_class = false;
				// Every Node is a Node, duh!
				while (type != "Node") {
					if (type.to_lower().contains(argument)) {
						term_in_inherited_class = true;
						break;
					}

					type = ClassDB::get_parent_class(type);
				}
				if (!term_in_inherited_class) {
					return false;
				}
			} else if (parameter == "group" || parameter == "g") {
				// Filter by Group.
				Node *node = get_node(p_item->get_metadata(0));

				if (argument.is_empty()) {
					// When argument is empty, match all Nodes belonging to any exposed group.
					if (node->get_persistent_group_count() == 0) {
						return false;
					}
				} else {
					List<Node::GroupInfo> group_info_list;
					node->get_groups(&group_info_list);

					bool term_in_groups = false;
					for (const Node::GroupInfo &group_info : group_info_list) {
						if (!group_info.persistent) {
							continue; // Ignore internal groups.
						}
						if (String(group_info.name).to_lower().contains(argument)) {
							term_in_groups = true;
							break;
						}
					}
					if (!term_in_groups) {
						return false;
					}
				}
			} else if (filter_term_warning.is_empty()) {
				filter_term_warning = vformat(TTR("\"%s\" is not a known filter."), parameter);
				continue;
			}
		} else {
			// Default.
			if (!p_item->get_text(0).to_lower().contains(term)) {
				return false;
			}
		}
	}

	return true;
}

void SceneTreeEditor::_compute_hash(Node *p_node, uint64_t &hash) {
	hash = hash_djb2_one_64(p_node->get_instance_id(), hash);
	if (p_node->get_parent()) {
		hash = hash_djb2_one_64(p_node->get_parent()->get_instance_id(), hash); //so a reparent still produces a different hash
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_compute_hash(p_node->get_child(i), hash);
	}
}

void SceneTreeEditor::_test_update_tree() {
	pending_test_update = false;

	if (!is_inside_tree()) {
		return;
	}

	if (tree_dirty) {
		return; // don't even bother
	}

	uint64_t hash = hash_djb2_one_64(0);
	if (get_scene_node()) {
		_compute_hash(get_scene_node(), hash);
	}
	//test hash
	if (hash == last_hash) {
		return; // did not change
	}

	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
}

void SceneTreeEditor::_tree_process_mode_changed() {
	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
}

void SceneTreeEditor::_tree_changed() {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; //speed up exit
	}
	if (pending_test_update) {
		return;
	}
	if (tree_dirty) {
		return;
	}

	callable_mp(this, &SceneTreeEditor::_test_update_tree).call_deferred();
	pending_test_update = true;
}

void SceneTreeEditor::_selected_changed() {
	TreeItem *s = tree->get_selected();
	ERR_FAIL_NULL(s);
	NodePath np = s->get_metadata(0);

	Node *n = get_node(np);

	if (n == selected) {
		return;
	}

	selected = get_node(np);

	blocked++;
	emit_signal(SNAME("node_selected"));
	blocked--;
}

void SceneTreeEditor::_deselect_items() {
	// Clear currently selected items in scene tree dock.
	if (editor_selection) {
		editor_selection->clear();
		emit_signal(SNAME("node_changed"));
	}
}

void SceneTreeEditor::_cell_multi_selected(Object *p_object, int p_cell, bool p_selected) {
	TreeItem *item = Object::cast_to<TreeItem>(p_object);
	ERR_FAIL_NULL(item);

	if (!item->is_visible()) {
		return;
	}

	NodePath np = item->get_metadata(0);

	Node *n = get_node(np);

	if (!n) {
		return;
	}

	if (!editor_selection) {
		return;
	}

	if (p_selected) {
		editor_selection->add_node(n);

	} else {
		editor_selection->remove_node(n);
	}

	// Emitted "selected" in _selected_changed() when select single node, so select multiple node emit "changed"
	if (editor_selection->get_selected_nodes().size() > 1) {
		emit_signal(SNAME("node_changed"));
	}
}

void SceneTreeEditor::_tree_scroll_to_item(ObjectID p_item_id) {
	ERR_FAIL_NULL(tree);
	TreeItem *item = Object::cast_to<TreeItem>(ObjectDB::get_instance(p_item_id));
	if (item) {
		tree->scroll_to_item(item, true);
	}
}

void SceneTreeEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("tree_changed", callable_mp(this, &SceneTreeEditor::_tree_changed));
			get_tree()->connect("tree_process_mode_changed", callable_mp(this, &SceneTreeEditor::_tree_process_mode_changed));
			get_tree()->connect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->connect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			get_tree()->connect(SceneStringName(node_configuration_warning_changed), callable_mp(this, &SceneTreeEditor::_warning_changed));

			tree->connect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));

			_update_tree();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("tree_changed", callable_mp(this, &SceneTreeEditor::_tree_changed));
			get_tree()->disconnect("tree_process_mode_changed", callable_mp(this, &SceneTreeEditor::_tree_process_mode_changed));
			get_tree()->disconnect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->disconnect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			tree->disconnect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));
			get_tree()->disconnect(SceneStringName(node_configuration_warning_changed), callable_mp(this, &SceneTreeEditor::_warning_changed));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			tree->add_theme_constant_override("icon_max_width", get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)));

			_update_tree();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				TreeItem *item = nullptr;
				if (selected) {
					// Scroll to selected node.
					item = _find(tree->get_root(), selected->get_path());
				} else if (marked.size() == 1) {
					// Scroll to a single marked node.
					Node *marked_node = *marked.begin();
					if (marked_node) {
						item = _find(tree->get_root(), marked_node->get_path());
					}
				}

				if (item) {
					// Must wait until tree is properly sized before scrolling.
					ObjectID item_id = item->get_instance_id();
					callable_mp(this, &SceneTreeEditor::_tree_scroll_to_item).call_deferred(item_id);
				}
			}
		} break;
	}
}

TreeItem *SceneTreeEditor::_find(TreeItem *p_node, const NodePath &p_path) {
	if (!p_node) {
		return nullptr;
	}

	NodePath np = p_node->get_metadata(0);
	if (np == p_path) {
		return p_node;
	}

	TreeItem *children = p_node->get_first_child();
	while (children) {
		TreeItem *n = _find(children, p_path);
		if (n) {
			return n;
		}
		children = children->get_next();
	}

	return nullptr;
}

void SceneTreeEditor::set_selected(Node *p_node, bool p_emit_selected) {
	ERR_FAIL_COND(blocked > 0);

	if (pending_test_update) {
		_test_update_tree();
	}
	if (tree_dirty) {
		_update_tree();
	}

	if (selected == p_node) {
		return;
	}

	TreeItem *item = p_node ? _find(tree->get_root(), p_node->get_path()) : nullptr;

	if (item) {
		selected = p_node;
		if (auto_expand_selected) {
			// Make visible when it's collapsed.
			TreeItem *node = item->get_parent();
			while (node && node != tree->get_root()) {
				node->set_collapsed(false);
				node = node->get_parent();
			}
			item->select(0);
			item->set_as_cursor(0);
			tree->ensure_cursor_is_visible();
		} else {
			// Ensure the node is selected and visible for the user if the node
			// is not collapsed.
			bool collapsed = false;
			TreeItem *node = item;
			while (node && node != tree->get_root()) {
				if (node->is_collapsed()) {
					collapsed = true;
					break;
				}
				node = node->get_parent();
			}
			if (!collapsed) {
				item->select(0);
				item->set_as_cursor(0);
				tree->ensure_cursor_is_visible();
			}
		}
	} else {
		if (!p_node) {
			selected = nullptr;
		}
		selected = p_node;
	}

	if (p_emit_selected) {
		emit_signal(SNAME("node_selected"));
	}
}

void SceneTreeEditor::rename_node(Node *p_node, const String &p_name, TreeItem *p_item) {
	TreeItem *item;
	if (p_item) {
		item = p_item; // During batch rename the paths may change, so using _find() is unreliable.
	} else {
		item = _find(tree->get_root(), p_node->get_path());
	}
	ERR_FAIL_NULL(item);
	String new_name = p_name.validate_node_name();

	if (new_name != p_name) {
		String text = TTR("Invalid node name, the following characters are not allowed:") + "\n" + String::get_invalid_node_name_characters();
		if (error->is_visible()) {
			if (!error->get_meta("invalid_character", false)) {
				error->set_text(error->get_text() + "\n\n" + text);
				error->set_meta("invalid_character", true);
			}
		} else {
			error->set_text(text);
			error->set_meta("invalid_character", true);
			error->set_meta("same_unique_name", false);
			error->popup_centered();
		}
	}

	// Trim leading/trailing whitespace to prevent node names from containing accidental whitespace, which would make it more difficult to get the node via `get_node()`.
	new_name = new_name.strip_edges();
	if (new_name.is_empty() && p_node->get_owner() != nullptr && !p_node->get_scene_file_path().is_empty()) {
		// If name is empty and node is root of an instance, revert to the original name.
		const Ref<PackedScene> node_scene = ResourceLoader::load(p_node->get_scene_file_path());
		if (node_scene.is_valid()) {
			const Ref<SceneState> &state = node_scene->get_state();
			if (state->get_node_count() > 0) {
				new_name = state->get_node_name(0); // Root's name.
			}
		}
	}

	if (new_name.is_empty()) {
		// If name is still empty, fallback to class name.
		if (GLOBAL_GET("editor/naming/node_name_casing").operator int() != NAME_CASING_PASCAL_CASE) {
			new_name = Node::adjust_name_casing(p_node->get_class());
		} else {
			new_name = p_node->get_class();
		}
	}

	new_name = p_node->get_parent()->prevalidate_child_name(p_node, new_name);
	if (new_name == p_node->get_name()) {
		item->set_text(0, new_name);
		return;
	}

	// We previously made sure name is not the same as current name so that it won't complain about already used unique name when not changing name.
	if (p_node->is_unique_name_in_owner() && get_tree()->get_edited_scene_root()->get_node_or_null("%" + new_name)) {
		String text = vformat(TTR("A node with the unique name %s already exists in this scene."), new_name);
		if (error->is_visible()) {
			if (!error->get_meta("same_unique_name", false)) {
				error->set_text(error->get_text() + "\n\n" + text);
				error->set_meta("same_unique_name", true);
			}
		} else {
			error->set_text(text);
			error->set_meta("same_unique_name", true);
			error->set_meta("invalid_character", false);
			error->popup_centered();
		}
		item->set_text(0, p_node->get_name());
		return;
	}

	if (!is_scene_tree_dock) {
		p_node->set_name(new_name);
		item->set_metadata(0, p_node->get_path());
		emit_signal(SNAME("node_renamed"));
	} else {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Rename Node"), UndoRedo::MERGE_DISABLE, p_node);

		emit_signal(SNAME("node_prerename"), p_node, new_name);

		undo_redo->add_undo_method(p_node, "set_name", p_node->get_name());
		undo_redo->add_undo_method(item, "set_metadata", 0, p_node->get_path());
		undo_redo->add_undo_method(item, "set_text", 0, p_node->get_name());

		p_node->set_name(new_name);
		undo_redo->add_do_method(p_node, "set_name", new_name);
		undo_redo->add_do_method(item, "set_metadata", 0, p_node->get_path());
		undo_redo->add_do_method(item, "set_text", 0, new_name);

		undo_redo->commit_action();
	}
}

void SceneTreeEditor::_edited() {
	TreeItem *which = tree->get_next_selected(nullptr);
	ERR_FAIL_NULL(which);
	TreeItem *edited = tree->get_edited();
	ERR_FAIL_NULL(edited);

	if (is_scene_tree_dock && tree->get_next_selected(which)) {
		List<Node *> nodes_to_rename;
		for (TreeItem *item = which; item; item = tree->get_next_selected(item)) {
			Node *n = get_node(item->get_metadata(0));
			ERR_FAIL_NULL(n);
			nodes_to_rename.push_back(n);
		}
		ERR_FAIL_COND(nodes_to_rename.is_empty());

		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Rename Nodes"), UndoRedo::MERGE_DISABLE, nodes_to_rename.front()->get(), true);

		TreeItem *item = which;
		String new_name = edited->get_text(0);
		for (Node *n : nodes_to_rename) {
			rename_node(n, new_name, item);
			item = tree->get_next_selected(item);
		}

		undo_redo->commit_action();
	} else {
		Node *n = get_node(which->get_metadata(0));
		ERR_FAIL_NULL(n);
		rename_node(n, which->get_text(0));
	}
}

Node *SceneTreeEditor::get_selected() {
	return selected;
}

void SceneTreeEditor::set_marked(const HashSet<Node *> &p_marked, bool p_selectable, bool p_children_selectable) {
	if (tree_dirty) {
		_update_tree();
	}
	marked = p_marked;
	marked_selectable = p_selectable;
	marked_children_selectable = p_children_selectable;
	_update_tree();
}

void SceneTreeEditor::set_marked(Node *p_marked, bool p_selectable, bool p_children_selectable) {
	HashSet<Node *> s;
	if (p_marked) {
		s.insert(p_marked);
	}
	set_marked(s, p_selectable, p_children_selectable);
}

void SceneTreeEditor::set_filter(const String &p_filter) {
	filter = p_filter;
	_update_filter(nullptr, true);
}

String SceneTreeEditor::get_filter() const {
	return filter;
}

String SceneTreeEditor::get_filter_term_warning() {
	return filter_term_warning;
}

void SceneTreeEditor::set_show_all_nodes(bool p_show_all_nodes) {
	show_all_nodes = p_show_all_nodes;
	_update_filter(nullptr, true);
}

void SceneTreeEditor::set_as_scene_tree_dock() {
	is_scene_tree_dock = true;
}

void SceneTreeEditor::set_display_foreign_nodes(bool p_display) {
	display_foreign = p_display;
	_update_tree();
}

void SceneTreeEditor::set_valid_types(const Vector<StringName> &p_valid) {
	valid_types = p_valid;
}

void SceneTreeEditor::set_editor_selection(EditorSelection *p_selection) {
	editor_selection = p_selection;
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_cursor_can_exit_tree(false);
	editor_selection->connect("selection_changed", callable_mp(this, &SceneTreeEditor::_selection_changed));
}

void SceneTreeEditor::_update_selection(TreeItem *item) {
	ERR_FAIL_NULL(item);

	NodePath np = item->get_metadata(0);

	if (!has_node(np)) {
		return;
	}

	Node *n = get_node(np);

	if (!n) {
		return;
	}

	if (editor_selection->is_selected(n)) {
		if (!item->is_selected(0)) {
			item->select(0);
		}
	} else {
		if (item->is_selected(0)) {
			TreeItem *previous_cursor_item = tree->get_selected();
			item->deselect(0);
			previous_cursor_item->set_as_cursor(0);
		}
	}

	TreeItem *c = item->get_first_child();

	while (c) {
		_update_selection(c);
		c = c->get_next();
	}
}

void SceneTreeEditor::_selection_changed() {
	if (!editor_selection) {
		return;
	}

	TreeItem *root = tree->get_root();

	if (!root) {
		return;
	}
	_update_selection(root);
}

void SceneTreeEditor::_cell_collapsed(Object *p_obj) {
	if (updating_tree) {
		return;
	}
	if (!can_rename) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_obj);
	if (!ti) {
		return;
	}

	bool collapsed = ti->is_collapsed();

	NodePath np = ti->get_metadata(0);

	Node *n = get_node(np);
	ERR_FAIL_NULL(n);

	n->set_display_folded(collapsed);
}

Variant SceneTreeEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (!can_rename) {
		return Variant(); //not editable tree
	}

	if (tree->get_button_id_at_position(p_point) != -1) {
		return Variant(); //dragging from button
	}

	Vector<Node *> selected_nodes;
	Vector<Ref<Texture2D>> icons;
	TreeItem *next = tree->get_next_selected(nullptr);
	while (next) {
		NodePath np = next->get_metadata(0);

		Node *n = get_node(np);
		if (n) {
			selected_nodes.push_back(n);
			icons.push_back(next->get_icon(0));
		}
		next = tree->get_next_selected(next);
	}

	if (selected_nodes.is_empty()) {
		return Variant();
	}

	VBoxContainer *vb = memnew(VBoxContainer);
	Array objs;
	int list_max = 10;
	float opacity_step = 1.0f / list_max;
	float opacity_item = 1.0f;
	for (int i = 0; i < selected_nodes.size(); i++) {
		if (i < list_max) {
			HBoxContainer *hb = memnew(HBoxContainer);
			TextureRect *tf = memnew(TextureRect);
			int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
			tf->set_custom_minimum_size(Size2(icon_size, icon_size));
			tf->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			tf->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
			tf->set_texture(icons[i]);
			hb->add_child(tf);
			Label *label = memnew(Label(selected_nodes[i]->get_name()));
			label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
			hb->add_child(label);
			vb->add_child(hb);
			hb->set_modulate(Color(1, 1, 1, opacity_item));
			opacity_item -= opacity_step;
		}
		NodePath p = selected_nodes[i]->get_path();
		objs.push_back(p);
	}

	set_drag_preview(vb);
	Dictionary drag_data;
	drag_data["type"] = "nodes";
	drag_data["nodes"] = objs;

	tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
	emit_signal(SNAME("nodes_dragged"));

	return drag_data;
}

bool SceneTreeEditor::_is_script_type(const StringName &p_type) const {
	return (script_types->find(p_type));
}

bool SceneTreeEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (!can_rename) {
		return false; //not editable tree
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	TreeItem *item = tree->get_item_at_position(p_point);
	if (!item) {
		return false;
	}

	int section = tree->get_drop_section_at_position(p_point);
	if (section < -1 || (section == -1 && !item->get_parent())) {
		return false;
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false; //weird
		}

		if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(files[0]))) {
			tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
			return true;
		}

		bool scene_drop = true;
		bool audio_drop = true;
		for (int i = 0; i < files.size(); i++) {
			String ftype = EditorFileSystem::get_singleton()->get_file_type(files[i]);
			if (ftype != "PackedScene") {
				scene_drop = false;
			}
			if (audio_drop && !ClassDB::is_parent_class(ftype, "AudioStream")) {
				audio_drop = false;
			}
		}

		if (scene_drop) {
			tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
			return true;
		}

		if (audio_drop) {
			if (files.size() > 1) {
				tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
			} else {
				tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
			}
			return true;
		}

		if (files.size() > 1) {
			return false;
		}
		tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);

		return true;
	}

	if (String(d["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(d["script_list_element"]);
		if (se) {
			String sp = se->get_edited_resource()->get_path();
			if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
				tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
				return true;
			}
		}
	}

	if (filter.is_empty() && String(d["type"]) == "nodes") {
		Array nodes = d["nodes"];

		for (int i = 0; i < nodes.size(); i++) {
			Node *n = get_node(nodes[i]);
			// Nodes from an instantiated scene can't be rearranged.
			if (n && n->get_owner() && n->get_owner() != get_scene_node() && !n->get_owner()->get_scene_file_path().is_empty()) {
				return false;
			}
		}

		return true;
	}

	return false;
}

void SceneTreeEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	TreeItem *item = tree->get_item_at_position(p_point);
	if (!item) {
		return;
	}
	int section = tree->get_drop_section_at_position(p_point);
	if (section < -1) {
		return;
	}

	NodePath np = item->get_metadata(0);
	Node *n = get_node(np);
	if (!n) {
		return;
	}

	Dictionary d = p_data;

	if (String(d["type"]) == "nodes") {
		Array nodes = d["nodes"];
		emit_signal(SNAME("nodes_rearranged"), nodes, np, section);
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		String ftype = EditorFileSystem::get_singleton()->get_file_type(files[0]);
		if (_is_script_type(ftype)) {
			emit_signal(SNAME("script_dropped"), files[0], np);
		} else {
			emit_signal(SNAME("files_dropped"), files, np, section);
		}
	}

	if (String(d["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(d["script_list_element"]);
		if (se) {
			String sp = se->get_edited_resource()->get_path();
			if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
				emit_signal(SNAME("script_dropped"), sp, np);
			}
		}
	}
}

void SceneTreeEditor::_empty_clicked(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	_rmb_select(p_pos);
}

void SceneTreeEditor::_rmb_select(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	emit_signal(SNAME("rmb_pressed"), tree->get_screen_position() + p_pos);
}

void SceneTreeEditor::update_warning() {
	_warning_changed(nullptr);
}

void SceneTreeEditor::_warning_changed(Node *p_for_node) {
	//should use a timer
	update_timer->start();
}

void SceneTreeEditor::set_auto_expand_selected(bool p_auto, bool p_update_settings) {
	if (p_update_settings) {
		EditorSettings::get_singleton()->set("docks/scene_tree/auto_expand_to_selected", p_auto);
	}
	auto_expand_selected = p_auto;
}

void SceneTreeEditor::set_connect_to_script_mode(bool p_enable) {
	connect_to_script_mode = p_enable;
	update_tree();
}

void SceneTreeEditor::set_connecting_signal(bool p_enable) {
	connecting_signal = p_enable;
	update_tree();
}

void SceneTreeEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_tree"), &SceneTreeEditor::_update_tree, DEFVAL(false)); // Still used by UndoRedo.

	ClassDB::bind_method(D_METHOD("update_tree"), &SceneTreeEditor::update_tree);

	ADD_SIGNAL(MethodInfo("node_selected"));
	ADD_SIGNAL(MethodInfo("node_renamed"));
	ADD_SIGNAL(MethodInfo("node_prerename"));
	ADD_SIGNAL(MethodInfo("node_changed"));
	ADD_SIGNAL(MethodInfo("nodes_dragged"));
	ADD_SIGNAL(MethodInfo("nodes_rearranged", PropertyInfo(Variant::ARRAY, "paths"), PropertyInfo(Variant::NODE_PATH, "to_path"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files"), PropertyInfo(Variant::NODE_PATH, "to_path"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("script_dropped", PropertyInfo(Variant::STRING, "file"), PropertyInfo(Variant::NODE_PATH, "to_path")));
	ADD_SIGNAL(MethodInfo("rmb_pressed", PropertyInfo(Variant::VECTOR2, "position")));

	ADD_SIGNAL(MethodInfo("open"));
	ADD_SIGNAL(MethodInfo("open_script"));
}

SceneTreeEditor::SceneTreeEditor(bool p_label, bool p_can_rename, bool p_can_open_instance) {
	selected = nullptr;

	can_rename = p_can_rename;
	can_open_instance = p_can_open_instance;
	editor_selection = nullptr;

	if (p_label) {
		Label *label = memnew(Label);
		label->set_theme_type_variation("HeaderSmall");
		label->set_position(Point2(10, 0));
		label->set_text(TTR("Scene Tree (Nodes):"));

		add_child(label);
	}

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_anchor(SIDE_RIGHT, ANCHOR_END);
	tree->set_anchor(SIDE_BOTTOM, ANCHOR_END);
	tree->set_begin(Point2(0, p_label ? 18 : 0));
	tree->set_end(Point2(0, 0));
	tree->set_allow_reselect(true);
	tree->add_theme_constant_override("button_margin", 0);

	add_child(tree);

	SET_DRAG_FORWARDING_GCD(tree, SceneTreeEditor);
	if (p_can_rename) {
		tree->set_allow_rmb_select(true);
		tree->connect("item_mouse_selected", callable_mp(this, &SceneTreeEditor::_rmb_select));
		tree->connect("empty_clicked", callable_mp(this, &SceneTreeEditor::_empty_clicked));
	}

	tree->connect("cell_selected", callable_mp(this, &SceneTreeEditor::_selected_changed));
	tree->connect("item_edited", callable_mp(this, &SceneTreeEditor::_edited));
	tree->connect("multi_selected", callable_mp(this, &SceneTreeEditor::_cell_multi_selected));
	tree->connect("button_clicked", callable_mp(this, &SceneTreeEditor::_cell_button_pressed));
	tree->connect("nothing_selected", callable_mp(this, &SceneTreeEditor::_deselect_items));

	error = memnew(AcceptDialog);
	add_child(error);

	warning = memnew(AcceptDialog);
	add_child(warning);
	warning->set_title(TTR("Node Configuration Warning!"));
	warning->set_flag(Window::FLAG_POPUP, true);

	last_hash = 0;
	blocked = 0;

	update_timer = memnew(Timer);
	update_timer->connect("timeout", callable_mp(this, &SceneTreeEditor::_update_tree).bind(false));
	update_timer->set_one_shot(true);
	update_timer->set_wait_time(0.5);
	add_child(update_timer);

	update_node_tooltip_delay = memnew(Timer);
	update_node_tooltip_delay->set_wait_time(0.5);
	update_node_tooltip_delay->set_one_shot(true);
	add_child(update_node_tooltip_delay);

	revoke_dialog = memnew(ConfirmationDialog);
	revoke_dialog->set_ok_button_text(TTR("Revoke"));
	add_child(revoke_dialog);
	revoke_dialog->connect(SceneStringName(confirmed), callable_mp(this, &SceneTreeEditor::_update_ask_before_revoking_unique_name));
	VBoxContainer *vb = memnew(VBoxContainer);
	revoke_dialog->add_child(vb);
	revoke_dialog_label = memnew(Label);
	vb->add_child(revoke_dialog_label);
	ask_before_revoke_checkbox = memnew(CheckBox(TTR("Don't Ask Again")));
	ask_before_revoke_checkbox->set_tooltip_text(TTR("This dialog can also be enabled/disabled in the Editor Settings: Docks > Scene Tree > Ask Before Revoking Unique Name."));
	vb->add_child(ask_before_revoke_checkbox);

	script_types = memnew(List<StringName>);
	ClassDB::get_inheriters_from_class("Script", script_types);
}

SceneTreeEditor::~SceneTreeEditor() {
	memdelete(script_types);
}

/******** DIALOG *********/

void SceneTreeDialog::popup_scenetree_dialog(Node *p_selected_node, Node *p_marked_node, bool p_marked_node_selectable, bool p_marked_node_children_selectable) {
	get_scene_tree()->set_marked(p_marked_node, p_marked_node_selectable, p_marked_node_children_selectable);
	get_scene_tree()->set_selected(p_selected_node);
	popup_centered_clamped(Size2(350, 700) * EDSCALE);
}

void SceneTreeDialog::_show_all_nodes_changed(bool p_button_pressed) {
	EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "show_all_nodes_for_node_selection", p_button_pressed);
	tree->set_show_all_nodes(p_button_pressed);
}

void SceneTreeDialog::set_valid_types(const Vector<StringName> &p_valid) {
	if (allowed_types_hbox) {
		allowed_types_hbox->queue_free();
		allowed_types_hbox = nullptr;
		valid_type_icons.clear();
	}

	tree->set_valid_types(p_valid);

	if (p_valid.is_empty()) {
		return;
	}

	allowed_types_hbox = memnew(HBoxContainer);
	content->add_child(allowed_types_hbox);
	content->move_child(allowed_types_hbox, 0);

	{
		Label *label = memnew(Label);
		allowed_types_hbox->add_child(label);
		label->set_text(TTR("Allowed:"));
	}

	HFlowContainer *hflow = memnew(HFlowContainer);
	allowed_types_hbox->add_child(hflow);
	hflow->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	for (const StringName &type : p_valid) {
		HBoxContainer *hb = memnew(HBoxContainer);
		hflow->add_child(hb);

		// Attempt to get the correct name and icon for script path types.
		String name = type;
		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(type);

		// If we can't find a global class icon, try to find one for the script.
		if (icon.is_null() && ResourceLoader::exists(type, "Script")) {
			Ref<Script> node_script = ResourceLoader::load(type);
			if (node_script.is_valid()) {
				name = name.get_file();
				icon = EditorNode::get_singleton()->get_object_icon(node_script.ptr());
			}
		}

		TextureRect *trect = memnew(TextureRect);
		hb->add_child(trect);
		trect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		trect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		trect->set_meta("icon", icon);
		valid_type_icons.push_back(trect);

		Label *label = memnew(Label);
		hb->add_child(label);
		label->set_text(name);
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	}

	show_all_nodes->show();
	if (is_inside_tree()) {
		_update_valid_type_icons();
	}
}

void SceneTreeDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				tree->update_tree();

				// Select the search bar by default.
				callable_mp((Control *)filter, &Control::grab_focus).call_deferred();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(confirmed), callable_mp(this, &SceneTreeDialog::_select));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_valid_type_icons();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			disconnect(SceneStringName(confirmed), callable_mp(this, &SceneTreeDialog::_select));
		} break;
	}
}

void SceneTreeDialog::_update_valid_type_icons() {
	filter->set_right_icon(get_editor_theme_icon(SNAME("Search")));
	for (TextureRect *trect : valid_type_icons) {
		trect->set_custom_minimum_size(Vector2(get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)), 0));
		trect->set_texture(trect->get_meta("icon"));
	}
}

void SceneTreeDialog::_cancel() {
	hide();
}

void SceneTreeDialog::_select() {
	if (tree->get_selected()) {
		// The signal may cause another dialog to be displayed, so be sure to hide this one first.
		hide();
		emit_signal(SNAME("selected"), tree->get_selected()->get_path());
	}
}

void SceneTreeDialog::_selected_changed() {
	get_ok_button()->set_disabled(!tree->get_selected());
}

void SceneTreeDialog::_filter_changed(const String &p_filter) {
	tree->set_filter(p_filter);
}

void SceneTreeDialog::_on_filter_gui_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			tree->get_scene_tree()->gui_input(key);
			filter->accept_event();
		}
	}
}

void SceneTreeDialog::_bind_methods() {
	ClassDB::bind_method("_cancel", &SceneTreeDialog::_cancel);

	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::NODE_PATH, "path")));
}

SceneTreeDialog::SceneTreeDialog() {
	set_title(TTR("Select a Node"));
	content = memnew(VBoxContainer);
	add_child(content);

	HBoxContainer *filter_hbc = memnew(HBoxContainer);
	content->add_child(filter_hbc);

	filter = memnew(LineEdit);
	filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter->set_placeholder(TTR("Filter Nodes"));
	filter->set_clear_button_enabled(true);
	filter->add_theme_constant_override("minimum_character_width", 0);
	filter->connect(SceneStringName(text_changed), callable_mp(this, &SceneTreeDialog::_filter_changed));
	filter->connect(SceneStringName(gui_input), callable_mp(this, &SceneTreeDialog::_on_filter_gui_input));

	register_text_enter(filter);

	filter_hbc->add_child(filter);

	// Add 'Show All' button to HBoxContainer next to the filter, visible only when valid_types is defined.
	show_all_nodes = memnew(CheckButton);
	show_all_nodes->set_text(TTR("Show All"));
	show_all_nodes->connect(SceneStringName(toggled), callable_mp(this, &SceneTreeDialog::_show_all_nodes_changed));
	show_all_nodes->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	show_all_nodes->hide();
	filter_hbc->add_child(show_all_nodes);

	tree = memnew(SceneTreeEditor(false, false, true));
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->get_scene_tree()->connect("item_activated", callable_mp(this, &SceneTreeDialog::_select));
	// Initialize button state, must be done after the tree has been created to update its 'show_all_nodes' flag.
	// This is also done before adding the tree to the content to avoid triggering unnecessary tree filtering.
	show_all_nodes->set_pressed(EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "show_all_nodes_for_node_selection", false));
	content->add_child(tree);

	// Disable the OK button when no node is selected.
	get_ok_button()->set_disabled(!tree->get_selected());
	tree->connect("node_selected", callable_mp(this, &SceneTreeDialog::_selected_changed));
}

SceneTreeDialog::~SceneTreeDialog() {
}
