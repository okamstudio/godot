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
#include "scene/2d/node_2d.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

Node *SceneTreeEditor::get_scene_node() const {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	return get_tree()->get_edited_scene_root();
}

PackedStringArray SceneTreeEditor::_get_node_configuration_warnings(Node *p_node) {
	PackedStringArray warnings = p_node->get_configuration_warnings();
	if (p_node == get_scene_node()) {
		Node2D *node_2d = Object::cast_to<Node2D>(p_node);
		if (node_2d) {
			// Note: Warn for Node2D but not all CanvasItems, don't warn for Control nodes.
			// Control nodes may have reasons to use a transformed root node like anchors.
			if (!node_2d->get_transform().is_equal_approx(Transform2D())) {
				warnings.append(TTR("The root node of a scene is recommended to not be transformed, since instances of the scene will usually override this. Reset the transform and reload the scene to remove this warning."));
			}
		}
		Node3D *node_3d = Object::cast_to<Node3D>(p_node);
		if (node_3d) {
			if (!node_3d->get_transform().is_equal_approx(Transform3D())) {
				warnings.append(TTR("The root node of a scene is recommended to not be transformed, since instances of the scene will usually override this. Reset the transform and reload the scene to remove this warning."));
			}
		}
	}
	return warnings;
}

void SceneTreeEditor::_cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	if (connect_to_script_mode) {
		return; // Don't do anything in this mode.
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
		if (script_typed.is_valid()) {
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
		const PackedStringArray warnings = _get_node_configuration_warnings(n);

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
		// We don't want the first two newlines.
		all_warnings = String("\n").join(lines).indent("    ").replace(U"    •", U"\n•").substr(2);

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

void SceneTreeEditor::_update_node_path(Node *p_node, bool p_recursive) {
	if (!p_node) {
		return;
	}

	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node);
	if (!I) {
		return;
	}

	I->value.item->set_metadata(0, p_node->get_path());

	if (!p_recursive) {
		return;
	}

	int cc = p_node->get_child_count(false);
	for (int i = 0; i < cc; i++) {
		Node *c = p_node->get_child(i, false);
		_update_node_path(c, p_recursive);
	}
}

void SceneTreeEditor::_update_node_subtree(Node *p_node, TreeItem *p_parent, bool p_force) {
	if (!p_node) {
		return;
	}

	// Only owned nodes are editable, since nodes can create their own (manually owned) child nodes,
	// which the editor needs not to know about.

	bool part_of_subscene = false;

	if (!display_foreign && p_node->get_owner() != get_scene_node() && p_node != get_scene_node()) {
		if ((show_enabled_subscene || can_open_instance) && p_node->get_owner() && (get_scene_node()->is_editable_instance(p_node->get_owner()))) {
			part_of_subscene = true;
			// Allow.
		} else {
			// Stale node, remove recursively.
			node_cache.remove(p_node, true);
			return;
		}
	} else {
		part_of_subscene = p_node != get_scene_node() && get_scene_node()->get_scene_inherited_state().is_valid() && get_scene_node()->get_scene_inherited_state()->find_node_by_path(get_scene_node()->get_path_to(p_node)) >= 0;
	}

	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node);
	TreeItem *item = nullptr;

	bool is_new = false;

	if (I) {
		item = I->value.item;
		TreeItem *current_parent = item->get_parent();

		// Our parent might be re-created because of a changed type.
		if (p_parent && p_parent != current_parent) {
			if (current_parent) {
				current_parent->remove_child(item);
			}
			p_parent->add_child(item);
			I->value.removed = false;
			_move_node_item(p_parent, I);
		}

		if (I->value.has_moved_children) {
			_move_node_children(I);
		}
	} else {
		int index = -1;
		// Check to see if there is a root node for us to reuse.
		if (!p_parent) {
			item = tree->get_root();
			if (!item) {
				item = tree->create_item(nullptr);
				index = 0;
			}
		} else {
			index = p_node->get_index(false);
			item = tree->create_item(p_parent, index);
		}

		I = node_cache.add(p_node, item);
		I->value.index = index;
		is_new = true;
	}

	if (!(p_force || I->value.dirty)) {
		// Nothing to do.
		return;
	}

	_update_node(p_node, item, part_of_subscene);
	I->value.dirty = false;
	I->value.can_process = p_node->can_process();

	// Force update all our children if we are new or if we were forced to update.
	bool force_update_children = p_force || is_new;
	// Update all our children.
	for (int i = 0; i < p_node->get_child_count(false); i++) {
		_update_node_subtree(p_node->get_child(i, false), item, force_update_children);
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
			item->deselect(0);
			if (selected == p_node) {
				selected = nullptr;
			}
		}
	}
}

void SceneTreeEditor::_update_node(Node *p_node, TreeItem *p_item, bool p_part_of_subscene) {
	// Reset item properties that are not explicitly set in the default case.
	p_item->clear_buttons();
	p_item->remove_meta(SNAME("custom_color"));
	p_item->clear_custom_color(0);
	p_item->set_selectable(0, true);

	p_item->set_text(0, p_node->get_name());
	p_item->set_text_overrun_behavior(0, TextServer::OVERRUN_NO_TRIMMING);
	if (can_rename && !p_part_of_subscene) {
		p_item->set_editable(0, true);
	}

	if (can_rename) {
		bool collapsed = p_node->is_displayed_folded();
		if (collapsed) {
			p_item->set_collapsed(true);
		}
	}

	Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(p_node, "Node");
	p_item->set_icon(0, icon);
	p_item->set_metadata(0, p_node->get_path());

	if (!p_node->is_connected("child_order_changed", callable_mp(this, &SceneTreeEditor::_node_child_order_changed))) {
		p_node->connect("child_order_changed", callable_mp(this, &SceneTreeEditor::_node_child_order_changed).bind(p_node));
	}

	if (!p_node->is_connected("editor_state_changed", callable_mp(this, &SceneTreeEditor::_node_editor_state_changed))) {
		p_node->connect("editor_state_changed", callable_mp(this, &SceneTreeEditor::_node_editor_state_changed).bind(p_node));
	}

	if (connecting_signal || (can_open_instance && is_scene_tree_dock)) {
		if (!p_node->is_connected(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
			p_node->connect(CoreStringName(script_changed), callable_mp(this, &SceneTreeEditor::_node_script_changed).bind(p_node));
		}
	}

	if (connecting_signal) {
		// Add script icons for all scripted nodes.
		Ref<Script> scr = p_node->get_script();
		if (scr.is_valid()) {
			p_item->add_button(0, get_editor_theme_icon(SNAME("Script")), BUTTON_SCRIPT);
			if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr) {
				// Disable button on custom scripts (pure visual cue).
				p_item->set_button_disabled(0, p_item->get_button_count(0) - 1, true);
			}
		}
	}

	if (connect_to_script_mode) {
		Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));

		Ref<Script> scr = p_node->get_script();
		bool has_custom_script = scr.is_valid() && EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr;
		if (scr.is_null() || has_custom_script) {
			_set_item_custom_color(p_item, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
			p_item->set_selectable(0, false);

			accent.a *= 0.7;
		}

		if (marked.has(p_node)) {
			String node_name = p_node->get_name();
			if (connecting_signal) {
				node_name += " " + TTR("(Connecting From)");
			}
			p_item->set_text(0, node_name);
			_set_item_custom_color(p_item, accent);
		}
	} else if (p_part_of_subscene) {
		if (valid_types.size() == 0) {
			_set_item_custom_color(p_item, get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		}
	} else if (marked.has(p_node)) {
		String node_name = p_node->get_name();
		if (connecting_signal) {
			node_name += " " + TTR("(Connecting From)");
		}
		p_item->set_text(0, node_name);
		p_item->set_selectable(0, marked_selectable);
		_set_item_custom_color(p_item, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
	} else if (!p_node->can_process()) {
		_set_item_custom_color(p_item, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	} else if (!marked_selectable && !marked_children_selectable) {
		Node *node = p_node;
		while (node) {
			if (marked.has(node)) {
				p_item->set_selectable(0, false);
				_set_item_custom_color(p_item, get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
				break;
			}
			node = node->get_parent();
		}
	}

	if (can_rename) { // TODO Should be can edit..
		const PackedStringArray warnings = _get_node_configuration_warnings(p_node);
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

			p_item->add_button(0, get_editor_theme_icon(warning_icon), BUTTON_WARNING, false, TTR("Node configuration warning:") + all_warnings);
		}

		if (p_node->is_unique_name_in_owner()) {
			const bool disabled = p_node->get_owner() != EditorNode::get_singleton()->get_edited_scene();
			String button_text = vformat(TTR("This node can be accessed from anywhere within the scene it belongs to by using the '%s' prefix in the node path."), UNIQUE_NODE_PREFIX);
			if (!disabled) {
				button_text += "\n" + TTR("Click to disable this.");
			}
			p_item->add_button(0, get_editor_theme_icon(SNAME("SceneUniqueName")), BUTTON_UNIQUE, disabled, button_text);
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
			p_item->add_button(0, icon_temp, signal_temp, false, msg_temp);
		}
	}

	{
		_update_node_tooltip(p_node, p_item);
		Callable delay_update_tooltip = callable_mp(this, &SceneTreeEditor::_queue_update_node_tooltip);
		if (p_node->is_connected("editor_description_changed", delay_update_tooltip)) {
			p_node->disconnect("editor_description_changed", delay_update_tooltip);
		}
		p_node->connect("editor_description_changed", delay_update_tooltip.bind(p_item));
	}

	// Show buttons only when necessary (SceneTreeDock) to avoid crashes.
	if (can_open_instance && is_scene_tree_dock) {
		Ref<Script> scr = p_node->get_script();
		if (scr.is_valid()) {
			String additional_notes;
			Color button_color = Color(1, 1, 1);
			// Can't set tooltip after adding button, need to do it before.
			if (scr->is_tool()) {
				if (Engine::get_singleton()->is_recovery_mode_hint()) {
					additional_notes += "\n" + TTR("This script can run in the editor.\nIt is currently disabled due to recovery mode.");
					button_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
				} else {
					additional_notes += "\n" + TTR("This script is currently running in the editor.");
					button_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
				}
			}
			if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == scr) {
				additional_notes += "\n" + TTR("This script is a custom type.");
				button_color.a = 0.5;
			}
			p_item->add_button(0, get_editor_theme_icon(SNAME("Script")), BUTTON_SCRIPT, false, TTR("Open Script:") + " " + scr->get_path() + additional_notes);
			p_item->set_button_color(0, p_item->get_button_count(0) - 1, button_color);
		}

		if (p_node->has_meta("_edit_lock_")) {
			p_item->add_button(0, get_editor_theme_icon(SNAME("Lock")), BUTTON_LOCK, false, TTR("Node is locked.\nClick to unlock it."));
		}
		if (p_node->has_meta("_edit_group_")) {
			p_item->add_button(0, get_editor_theme_icon(SNAME("Group")), BUTTON_GROUP, false, TTR("Children are not selectable.\nClick to make them selectable."));
		}

		if (p_node->has_method("is_visible") && p_node->has_method("set_visible") && p_node->has_signal(SceneStringName(visibility_changed))) {
			bool is_visible = p_node->call("is_visible");
			if (is_visible) {
				p_item->add_button(0, get_editor_theme_icon(SNAME("GuiVisibilityVisible")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			} else {
				p_item->add_button(0, get_editor_theme_icon(SNAME("GuiVisibilityHidden")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			}
			const Callable vis_changed = callable_mp(this, &SceneTreeEditor::_node_visibility_changed);
			if (!p_node->is_connected(SceneStringName(visibility_changed), vis_changed)) {
				p_node->connect(SceneStringName(visibility_changed), vis_changed.bind(p_node));
			}
			_update_visibility_color(p_node, p_item);
		}

		if (p_node->is_class("AnimationMixer")) {
			bool is_pinned = AnimationPlayerEditor::get_singleton()->get_editing_node() == p_node && AnimationPlayerEditor::get_singleton()->is_pinned();

			if (is_pinned) {
				p_item->add_button(0, get_editor_theme_icon(SNAME("Pin")), BUTTON_PIN, false, TTR("AnimationPlayer is pinned.\nClick to unpin."));
			}
		}
	}

	if (editor_selection) {
		if (editor_selection->is_selected(p_node)) {
			p_item->select(0);
		}
	}

	if (selected == p_node) {
		if (!editor_selection) {
			p_item->select(0);
		}
		p_item->set_as_cursor(0);
	}
}

void SceneTreeEditor::_update_if_clean() {
	if (tree_dirty) {
		return;
	}

	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
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
	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node, false);
	if (!I) {
		// We leave these signals connected when switching tabs.
		// If the node is not in cache it was for a different tab.
		return;
	}

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
	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node, false);
	if (!I) {
		// We leave these signals connected when switching tabs.
		// If the node is not in cache it was for a different tab.
		return;
	}

	node_cache.mark_dirty(p_node);

	_update_if_clean();
}

void SceneTreeEditor::_move_node_children(HashMap<Node *, CachedNode>::Iterator &p_I) {
	TreeItem *item = p_I->value.item;
	Node *node = p_I->key;
	int cc = node->get_child_count(false);

	for (int i = 0; i < cc; i++) {
		HashMap<Node *, CachedNode>::Iterator CI = node_cache.get(node->get_child(i, false));
		if (CI) {
			_move_node_item(item, CI);
		}
	}

	p_I->value.has_moved_children = false;
}

void SceneTreeEditor::_move_node_item(TreeItem *p_parent, HashMap<Node *, CachedNode>::Iterator &p_I) {
	if (!p_parent) {
		return;
	}

	Node *node = p_I->key;

	int current_node_index = node->get_index(false);
	int current_item_index = -1;
	TreeItem *item = p_I->value.item;

	if (item->get_parent() != p_parent) {
		TreeItem *p = item->get_parent();
		if (p) {
			item->get_parent()->remove_child(item);
		}
		p_parent->add_child(item);
		p_I->value.removed = false;
		current_item_index = p_parent->get_child_count() - 1;
		p_I->value.index = current_item_index;
	}

	if (p_I->value.index != current_node_index) {
		// If we just re-parented we know our index.
		if (current_item_index == -1) {
			current_item_index = item->get_index();
		}

		// Are we already in the right place?
		if (current_node_index == current_item_index) {
			p_I->value.index = current_node_index;
			return;
		}

		// Are we the first node?
		if (current_node_index == 0) {
			// There has to be at least 1 other node, otherwise we would not have gotten here.
			TreeItem *neighbor_item = p_parent->get_child(0);
			item->move_before(neighbor_item);
		} else {
			TreeItem *neighbor_item = p_parent->get_child(CLAMP(current_node_index - 1, 0, p_parent->get_child_count() - 1));
			item->move_after(neighbor_item);
		}

		p_I->value.index = current_node_index;
	}
}

void SceneTreeEditor::_node_child_order_changed(Node *p_node) {
	// Do not try to change children on nodes currently marked for removal.
	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node, false);
	if (I) {
		node_cache.mark_dirty(I->key);
		I->value.has_moved_children = true;
	}

	_update_if_clean();
}

void SceneTreeEditor::_node_editor_state_changed(Node *p_node) {
	node_cache.mark_dirty(p_node);
	HashMap<Node *, CachedNode>::Iterator I = node_cache.get(p_node, false);
	if (I) {
		if (p_node->is_inside_tree() && p_node->can_process() != I->value.can_process) {
			// All our children also change process mode.
			node_cache.mark_children_dirty(p_node, true);
		}
	}

	_update_if_clean();
}

void SceneTreeEditor::_node_added(Node *p_node) {
	if (!get_scene_node()) {
		return;
	}

	if (p_node != get_scene_node() && !get_scene_node()->is_ancestor_of(p_node)) {
		return;
	}

	node_cache.mark_dirty(p_node);
	_update_if_clean();
}

void SceneTreeEditor::_node_removed(Node *p_node) {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; // Speed up exit.
	}

	if (EditorNode::get_singleton()->is_changing_scene()) {
		return; // Switching tabs we will be destroying node cache anyway.
	}

	if (!get_scene_node()) {
		return;
	}

	if (p_node != get_scene_node() && !get_scene_node()->is_ancestor_of(p_node)) {
		return;
	}

	node_cache.remove(p_node);
	_update_if_clean();
}

void SceneTreeEditor::_node_renamed(Node *p_node) {
	if (!get_scene_node()) {
		return;
	}

	if (p_node != get_scene_node() && !get_scene_node()->is_ancestor_of(p_node)) {
		return;
	}

	node_cache.mark_dirty(p_node);
	// Recursively update child node paths.
	_update_node_path(p_node, true);

	emit_signal(SNAME("node_renamed"));

	_update_if_clean();
}

void SceneTreeEditor::_update_tree(bool p_scroll_to_selected) {
	if (!is_inside_tree()) {
		tree_dirty = false;
		return;
	}

	Node *scene_node = get_scene_node();

	if (node_cache.current_scene_node != scene_node) {
		_reset();
		marked.clear();
		node_cache.current_scene_node = scene_node;
		node_cache.force_update = true;
	}

	if (!update_when_invisible && !is_visible_in_tree()) {
		return;
	}

	if (tree->is_editing()) {
		return;
	}

	updating_tree = true;

	last_hash = hash_djb2_one_64(0);

	if (node_cache.current_scene_node) {
		// Handle pinning/unpinning the animation player only do this once per iteration.
		Node *pinned_node = AnimationPlayerEditor::get_singleton()->get_editing_node();
		// If pinned state changed, update the currently pinned node.
		if (AnimationPlayerEditor::get_singleton()->is_pinned() != node_cache.current_has_pin) {
			node_cache.current_has_pin = AnimationPlayerEditor::get_singleton()->is_pinned();
			node_cache.mark_dirty(pinned_node);
		}
		// If the current pinned node changed update both the old and new node.
		if (node_cache.current_pinned_node != pinned_node) {
			node_cache.mark_dirty(pinned_node);
			node_cache.mark_dirty(node_cache.current_pinned_node);
			node_cache.current_pinned_node = pinned_node;
		}

		_update_node_subtree(get_scene_node(), nullptr, node_cache.force_update);
		_compute_hash(get_scene_node(), last_hash);

		node_cache.delete_pending();
	}

	updating_tree = false;
	tree_dirty = false;
	node_cache.force_update = false;

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

	// Now find other reasons to keep this Node, too.
	PackedStringArray terms = filter.to_lower().split_spaces();
	bool keep = _item_matches_all_terms(p_parent, terms);

	bool selectable = keep;
	bool is_root = p_parent == tree->get_root();

	if (keep) {
		Node *n = get_node(p_parent->get_metadata(0));
		if (!p_parent->is_visible() || (is_root && tree->is_root_hidden())) {
			// Place back moved out children from when this item has hidden.
			HashMap<Node *, CachedNode>::Iterator I = node_cache.get(n, false);
			if (I && I->value.has_moved_children) {
				_update_node_subtree(I->value.node, nullptr, true);
			}
		}

		if (!valid_types.is_empty()) {
			selectable = false;
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
	}

	bool keep_for_children = false;
	for (TreeItem *child = p_parent->get_first_child(); child; child = child->get_next()) {
		// Always keep if at least one of the children are kept.
		keep_for_children = _update_filter(child, p_scroll_to_selected) || keep_for_children;
	}

	if (!is_root) {
		if (show_all_nodes) {
			p_parent->set_visible(keep_for_children || keep);
		} else {
			// Show only selectable nodes, or parents of selectable.
			p_parent->set_visible(keep_for_children || selectable);
		}
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
		p_parent->set_visible(!hide_filtered_out_parents || is_root);

		if (!p_parent->is_visible()) {
			TreeItem *filtered_parent = p_parent->get_parent();
			while (filtered_parent) {
				if (filtered_parent == tree->get_root() || (filtered_parent->is_selectable(0) && filtered_parent->is_visible())) {
					break;
				}
				filtered_parent = filtered_parent->get_parent();
			}

			if (filtered_parent) {
				for (Variant &item : p_parent->get_children()) {
					TreeItem *ti = Object::cast_to<TreeItem>(item);
					bool is_selected = ti->is_selected(0);

					p_parent->remove_child(ti);
					filtered_parent->add_child(ti);
					TreeItem *prev = p_parent->get_prev();
					if (prev) {
						ti->move_after(prev);
					}

					if (is_selected) {
						ti->select(0);
					}

					HashMap<Node *, CachedNode>::Iterator I = node_cache.get(get_node(p_parent->get_metadata(0)), false);
					if (I) {
						I->value.has_moved_children = true;
					}
				}

				return false;
			}
		} else {
			p_parent->set_custom_color(0, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
			p_parent->set_selectable(0, false);
			p_parent->deselect(0);
		}
	}
	if (is_root) {
		tree->set_hide_root(hide_filtered_out_parents && !selectable);
		if (tree->is_root_hidden()) {
			p_parent->set_collapsed(false);
		}
	}

	if (editor_selection) {
		Node *n = get_node(p_parent->get_metadata(0));
		if (selectable) {
			if (p_scroll_to_selected && n && editor_selection->is_selected(n)) {
				// Needs to be deferred to account for possible root visibility change.
				callable_mp(tree, &Tree::scroll_to_item).call_deferred(p_parent, false);
			}
		} else if (n && p_parent->is_selected(0)) {
			editor_selection->remove_node(n);
			p_parent->deselect(0);
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
	// Nodes are added and removed by Node* pointers.
	hash = hash_djb2_one_64((ptrdiff_t)p_node, hash);
	// This hash is non-commutative: if the node order changes so will the hash.
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_compute_hash(p_node->get_child(i), hash);
	}
}

void SceneTreeEditor::_reset() {
	tree->clear();
	node_cache.clear();
}

void SceneTreeEditor::_test_update_tree() {
	pending_test_update = false;

	if (!is_inside_tree()) {
		return;
	}

	if (tree_dirty) {
		return; // Don't even bother.
	}

	uint64_t hash = hash_djb2_one_64(0);
	if (get_scene_node()) {
		_compute_hash(get_scene_node(), hash);
	}

	// Test hash.
	if (hash == last_hash) {
		return; // Did not change.
	}

	_update_if_clean();
}

void SceneTreeEditor::_tree_process_mode_changed() {
	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
}

void SceneTreeEditor::_tree_changed() {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; // Speed up exit.
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

	selected = n;

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

	// Emitted "selected" in _selected_changed() when select single node, so select multiple node emit "changed".
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
			get_tree()->connect("node_added", callable_mp(this, &SceneTreeEditor::_node_added));
			get_tree()->connect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->connect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			get_tree()->connect(SceneStringName(node_configuration_warning_changed), callable_mp(this, &SceneTreeEditor::_warning_changed));

			tree->connect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));

			_update_tree();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("tree_changed", callable_mp(this, &SceneTreeEditor::_tree_changed));
			get_tree()->disconnect("tree_process_mode_changed", callable_mp(this, &SceneTreeEditor::_tree_process_mode_changed));
			get_tree()->disconnect("node_added", callable_mp(this, &SceneTreeEditor::_node_added));
			get_tree()->disconnect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->disconnect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			tree->disconnect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));
			get_tree()->disconnect(SceneStringName(node_configuration_warning_changed), callable_mp(this, &SceneTreeEditor::_warning_changed));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			tree->add_theme_constant_override("icon_max_width", get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)));

			// When we change theme we need to re-do everything.
			_reset();

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

				bool has_item = item;

				if (update_when_invisible) {
					if (has_item) {
						ObjectID item_id = item->get_instance_id();
						callable_mp(this, &SceneTreeEditor::_tree_scroll_to_item).call_deferred(item_id);
					}
				} else {
					callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(has_item);
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
	bool check_for_unique_name_token = !p_name.is_empty() && p_name[0] == '%';
	String substr_name = p_name;

	if (check_for_unique_name_token) {
		substr_name = p_name.substr(1);

		// No need to do anything else with this if already unique.
		if (p_node->is_unique_name_in_owner()) {
			check_for_unique_name_token = false;
			// Do not set scene root as unique.
		} else if (get_tree()->get_edited_scene_root() == p_node) {
			check_for_unique_name_token = false;
			String text = TTR("Root nodes cannot be accessed as unique names in their own scene. Instantiate in another scene and set as unique name there.");
			if (error->is_visible()) {
				error->set_text(error->get_text() + "\n\n" + text);
			} else {
				error->set_text(text);
				error->popup_centered();
			}
		}
	}

	String new_name = substr_name.validate_node_name();

	// If p_name only has "%" at the beginning and no other invalid characters, do not error.
	if (new_name != substr_name) {
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

	// Trim leading/trailing whitespace to prevent node names from containing accidental whitespace,
	// which would make it more difficult to get the node via `get_node()`.
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
		// If setting name as unique, check for existing unique node below first.
		if (!check_for_unique_name_token) {
			return;
		}
	}

	// We previously made sure name is not the same as current name
	// so that it won't complain about already used unique name when not changing name.
	if ((check_for_unique_name_token || p_node->is_unique_name_in_owner()) && get_tree()->get_edited_scene_root()->get_node_or_null("%" + new_name)) {
		check_for_unique_name_token = false;
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
		if (p_node->is_unique_name_in_owner()) {
			return;
		}
	}

	// If same name and check_for_unique_name_token is still true, now set as unique.
	// This is separate from final action so "Rename Node" is not added to undo history.
	if (new_name == p_node->get_name()) {
		if (check_for_unique_name_token) {
			if (!is_scene_tree_dock) {
				p_node->set_unique_name_in_owner(true);
			} else {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Enable Scene Unique Name(s)"));
				undo_redo->add_undo_method(p_node, "set_unique_name_in_owner", false);
				undo_redo->add_do_method(p_node, "set_unique_name_in_owner", true);
				undo_redo->commit_action();
			}
		}
		return;
	}

	if (!is_scene_tree_dock) {
		p_node->set_name(new_name);
		if (check_for_unique_name_token) {
			p_node->set_unique_name_in_owner(true);
		}
		item->set_metadata(0, p_node->get_path());
		emit_signal(SNAME("node_renamed"));
	} else {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Rename Node"), UndoRedo::MERGE_DISABLE, p_node);

		if (check_for_unique_name_token) {
			undo_redo->add_undo_method(p_node, "set_unique_name_in_owner", false);
		}

		emit_signal(SNAME("node_prerename"), p_node, new_name);

		undo_redo->add_undo_method(p_node, "set_name", p_node->get_name());
		undo_redo->add_undo_method(item, "set_metadata", 0, p_node->get_path());
		undo_redo->add_undo_method(item, "set_text", 0, p_node->get_name());

		p_node->set_name(new_name);
		undo_redo->add_do_method(p_node, "set_name", new_name);
		undo_redo->add_do_method(item, "set_metadata", 0, p_node->get_path());
		undo_redo->add_do_method(item, "set_text", 0, new_name);

		if (check_for_unique_name_token) {
			undo_redo->add_do_method(p_node, "set_unique_name_in_owner", true);
		}

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

void SceneTreeEditor::_update_marking_list(const HashSet<Node *> &p_marked) {
	for (Node *N : p_marked) {
		HashMap<Node *, CachedNode>::Iterator I = node_cache.get(N);
		if (I) {
			node_cache.mark_dirty(N);
			node_cache.mark_children_dirty(N, true);
		}
	}
}

void SceneTreeEditor::set_marked(const HashSet<Node *> &p_marked, bool p_selectable, bool p_children_selectable) {
	_update_if_clean();

	_update_marking_list(marked);
	_update_marking_list(p_marked);

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
	node_cache.force_update = true;
	callable_mp(this, &SceneTreeEditor::_update_tree).call_deferred(false);
	tree_dirty = true;
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
			if (previous_cursor_item) {
				previous_cursor_item->set_as_cursor(0);
			}
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
		return Variant(); // Not editable tree.
	}

	if (tree->get_button_id_at_position(p_point) != -1) {
		return Variant(); // Dragging from button.
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
		return false; // Not editable tree.
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
			return false; // TODO Weird?
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
	node_cache.mark_dirty(p_for_node);

	// Should use a timer.
	update_timer->start();
}

void SceneTreeEditor::set_auto_expand_selected(bool p_auto, bool p_update_settings) {
	if (p_update_settings) {
		EditorSettings::get_singleton()->set("docks/scene_tree/auto_expand_to_selected", p_auto);
	}
	auto_expand_selected = p_auto;
}

void SceneTreeEditor::set_hide_filtered_out_parents(bool p_hide, bool p_update_settings) {
	if (p_hide == hide_filtered_out_parents) {
		return;
	}

	if (p_update_settings) {
		EditorSettings::get_singleton()->set("docks/scene_tree/hide_filtered_out_parents", p_hide);
	}
	hide_filtered_out_parents = p_hide;

	if (hide_filtered_out_parents) {
		_update_filter();
	} else {
		node_cache.force_update = true;
		_update_tree();
	}
}

void SceneTreeEditor::set_connect_to_script_mode(bool p_enable) {
	connect_to_script_mode = p_enable;
	_update_tree();
}

void SceneTreeEditor::set_connecting_signal(bool p_enable) {
	connecting_signal = p_enable;
	_update_tree();
}

void SceneTreeEditor::set_update_when_invisible(bool p_enable) {
	update_when_invisible = p_enable;
	_update_tree();
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

SceneTreeEditor::SceneTreeEditor(bool p_label, bool p_can_rename, bool p_can_open_instance) :
		node_cache(this) {
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
	tree->set_update_when_invisible(false);
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

/******** CACHE *********/

HashMap<Node *, SceneTreeEditor::CachedNode>::Iterator SceneTreeEditor::NodeCache::add(Node *p_node, TreeItem *p_item) {
	if (!p_node) {
		return HashMap<Node *, CachedNode>::Iterator();
	}

	return cache.insert(p_node, CachedNode(p_node, p_item));
}

HashMap<Node *, SceneTreeEditor::CachedNode>::Iterator SceneTreeEditor::NodeCache::get(Node *p_node, bool p_deleted_ok) {
	if (!p_node) {
		return HashMap<Node *, CachedNode>::Iterator();
	}

	HashMap<Node *, CachedNode>::Iterator I = cache.find(p_node);
	if (I) {
		if (I->value.delete_serial != UINT16_MAX) {
			// Don't give us a node marked for deletion.
			if (!p_deleted_ok) {
				return HashMap<Node *, CachedNode>::Iterator();
			}

			to_delete.erase(&I->value);
			I->value.delete_serial = UINT16_MAX;

			// If we were resurrected from near-death we might have been renamed.
			// Make sure that we are updated properly.
			mark_dirty(p_node);
			mark_children_dirty(p_node, true);
		}
	}

	return I;
}

void SceneTreeEditor::NodeCache::remove(Node *p_node, bool p_recursive) {
	if (!p_node) {
		return;
	}

	if (p_node == editor->selected) {
		editor->selected = nullptr;
	}

	editor->marked.erase(p_node);

	HashMap<Node *, CachedNode>::Iterator I = cache.find(p_node);
	if (I) {
		if (p_recursive) {
			int cc = p_node->get_child_count(false);

			for (int i = 0; i < cc; i++) {
				remove(p_node->get_child(i, false), p_recursive);
			}
		}

		if (current_scene_node != p_node) {
			// Do not remove from the Tree control here. See delete_pending below.
			I->value.item->deselect(0);
			I->value.delete_serial = delete_serial;
			I->value.index = -1;
			I->value.cache_iterator = I;
			to_delete.insert(&I->value);
		} else {
			// If it is the root node, we leave the TreeItem and reuse it later.
			cache.remove(I);
		}
	}
}

void SceneTreeEditor::NodeCache::mark_dirty(Node *p_node, bool p_parents) {
	Node *node = p_node;
	while (node) {
		HashMap<Node *, CachedNode>::Iterator I = cache.find(node);
		if (I) {
			I->value.dirty = true;
		}

		if (!p_parents) {
			break;
		}
		node = node->get_parent();
	}
}

void SceneTreeEditor::NodeCache::mark_children_dirty(Node *p_node, bool p_recursive) {
	if (!p_node) {
		return;
	}

	int cc = p_node->get_child_count(false);
	for (int i = 0; i < cc; i++) {
		Node *c = p_node->get_child(i, false);
		HashMap<Node *, CachedNode>::Iterator IC = cache.find(c);

		if (IC) {
			IC->value.dirty = true;

			if (p_recursive) {
				mark_children_dirty(c, p_recursive);
			}
		}
	}
}

void SceneTreeEditor::NodeCache::delete_pending() {
	HashSet<CachedNode *>::Iterator I = to_delete.begin();
	while (I) {
		// We want to keep TreeItems around just long enough for a Node removal,
		// and immediate reinsertion. This is what happens with moves and
		// type changes.
		if (Math::abs((*I)->delete_serial - delete_serial) >= 2) {
			memdelete((*I)->item);
			cache.remove((*I)->cache_iterator);
			to_delete.remove(I);
		} else if (!(*I)->removed) {
			// We don't remove from the tree until now because if the node got
			// deleted from a @tool script the SceneTreeEditor might have had it
			// marked or selected before the node was removed. If we immediately
			// remove from the Tree control then we end up trying to scroll to an
			// Item without a parent.
			//
			// We might already be removed (and thus not have a parent) by rapid
			// undo/redo.
			if (!(*I)->removed) {
				TreeItem *parent = (*I)->item->get_parent();
				parent->remove_child((*I)->item);
			}
			(*I)->removed = true;
		}
		++I;
	}

	++delete_serial;
}

void SceneTreeEditor::NodeCache::clear() {
	for (CachedNode *E : to_delete) {
		// Only removed entries won't be automatically cleaned up by Tree::clear().
		if (E->removed) {
			memdelete(E->item);
		}
	}
	cache.clear();
	to_delete.clear();
}
