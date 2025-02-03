/**************************************************************************/
/*  scene_tree_dock.cpp                                                   */
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

#include "scene_tree_dock.h"

#include "core/io/resource_saver.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_node.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/multi_node_edit.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/plugins/spatial_editor_plugin.h"
#include "editor/script_editor_debugger.h"
#include "scene/main/viewport.h"
#include "scene/property_utils.h"
#include "scene/resources/packed_scene.h"

#include "modules/modules_enabled.gen.h" // For regex.

void SceneTreeDock::_nodes_drag_begin() {
	pending_click_select = nullptr;
}

void SceneTreeDock::_quick_open() {
	instance_scenes(quick_open->get_selected_files(), scene_tree->get_selected());
}

void SceneTreeDock::_input(Ref<InputEvent> p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && (mb->get_button_index() == BUTTON_LEFT || mb->get_button_index() == BUTTON_RIGHT)) {
		if (mb->is_pressed() && scene_tree->get_rect().has_point(scene_tree->get_local_mouse_position())) {
			tree_clicked = true;
		} else if (!mb->is_pressed()) {
			tree_clicked = false;
		}

		if (!mb->is_pressed() && pending_click_select) {
			_push_item(pending_click_select);
			pending_click_select = nullptr;
		}
	}
}

void SceneTreeDock::_unhandled_key_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (get_viewport()->get_modal_stack_top()) {
		return; //ignore because of modal window
	}

	if (get_focus_owner() && get_focus_owner()->is_text_field()) {
		return;
	}

	if (!p_event->is_pressed() || p_event->is_echo()) {
		return;
	}

	if (ED_IS_SHORTCUT("scene_tree/rename", p_event)) {
		_tool_selected(TOOL_RENAME);
#ifdef MODULE_REGEX_ENABLED
	} else if (ED_IS_SHORTCUT("scene_tree/batch_rename", p_event)) {
		_tool_selected(TOOL_BATCH_RENAME);
#endif // MODULE_REGEX_ENABLED
	} else if (ED_IS_SHORTCUT("scene_tree/add_child_node", p_event)) {
		_tool_selected(TOOL_NEW);
	} else if (ED_IS_SHORTCUT("scene_tree/instance_scene", p_event)) {
		_tool_selected(TOOL_INSTANCE);
	} else if (ED_IS_SHORTCUT("scene_tree/expand_collapse_all", p_event)) {
		_tool_selected(TOOL_EXPAND_COLLAPSE);
	} else if (ED_IS_SHORTCUT("scene_tree/cut_node", p_event)) {
		_tool_selected(TOOL_CUT);
	} else if (ED_IS_SHORTCUT("scene_tree/copy_node", p_event)) {
		_tool_selected(TOOL_COPY);
	} else if (ED_IS_SHORTCUT("scene_tree/paste_node", p_event)) {
		_tool_selected(TOOL_PASTE);
	} else if (ED_IS_SHORTCUT("scene_tree/change_node_type", p_event)) {
		_tool_selected(TOOL_REPLACE);
	} else if (ED_IS_SHORTCUT("scene_tree/duplicate", p_event)) {
		_tool_selected(TOOL_DUPLICATE);
	} else if (ED_IS_SHORTCUT("scene_tree/attach_script", p_event)) {
		_tool_selected(TOOL_ATTACH_SCRIPT);
	} else if (ED_IS_SHORTCUT("scene_tree/detach_script", p_event)) {
		_tool_selected(TOOL_DETACH_SCRIPT);
	} else if (ED_IS_SHORTCUT("scene_tree/move_up", p_event)) {
		_tool_selected(TOOL_MOVE_UP);
	} else if (ED_IS_SHORTCUT("scene_tree/move_down", p_event)) {
		_tool_selected(TOOL_MOVE_DOWN);
	} else if (ED_IS_SHORTCUT("scene_tree/reparent", p_event)) {
		_tool_selected(TOOL_REPARENT);
	} else if (ED_IS_SHORTCUT("scene_tree/merge_from_scene", p_event)) {
		_tool_selected(TOOL_MERGE_FROM_SCENE);
	} else if (ED_IS_SHORTCUT("scene_tree/save_branch_as_scene", p_event)) {
		_tool_selected(TOOL_NEW_SCENE_FROM);
	} else if (ED_IS_SHORTCUT("scene_tree/delete_no_confirm", p_event)) {
		_tool_selected(TOOL_ERASE, true);
	} else if (ED_IS_SHORTCUT("scene_tree/copy_node_path", p_event)) {
		_tool_selected(TOOL_COPY_NODE_PATH);
	} else if (ED_IS_SHORTCUT("scene_tree/toggle_unique_name", p_event)) {
		_tool_selected(TOOL_TOGGLE_SCENE_UNIQUE_NAME);
	} else if (ED_IS_SHORTCUT("scene_tree/delete", p_event)) {
		_tool_selected(TOOL_ERASE);
	}
}

void SceneTreeDock::instance(const String &p_file) {
	Vector<String> scenes;
	scenes.push_back(p_file);
	instance_scenes(scenes, scene_tree->get_selected());
}

void SceneTreeDock::instance_scenes(const Vector<String> &p_files, Node *p_parent) {
	Node *parent = p_parent;

	if (!parent) {
		parent = scene_tree->get_selected();
	}

	if (!parent) {
		parent = edited_scene;
	}

	if (!parent) {
		if (p_files.size() == 1) {
			accept->set_text(TTR("No parent to instance a child at."));
		} else {
			accept->set_text(TTR("No parent to instance the scenes at."));
		}
		accept->popup_centered_minsize();
		return;
	};

	_perform_instance_scenes(p_files, parent, -1);
}

void SceneTreeDock::_perform_instance_scenes(const Vector<String> &p_files, Node *parent, int p_pos) {
	ERR_FAIL_COND(!parent);

	Vector<Node *> instances;

	bool error = false;

	for (int i = 0; i < p_files.size(); i++) {
		Ref<PackedScene> sdata = ResourceLoader::load(p_files[i]);
		if (!sdata.is_valid()) {
			current_option = -1;
			accept->set_text(vformat(TTR("Error loading scene from %s"), p_files[i]));
			accept->popup_centered_minsize();
			error = true;
			break;
		}

		Node *instanced_scene = sdata->instance(PackedScene::GEN_EDIT_STATE_INSTANCE);
		if (!instanced_scene) {
			current_option = -1;
			accept->set_text(vformat(TTR("Error instancing scene from %s"), p_files[i]));
			accept->popup_centered_minsize();
			error = true;
			break;
		}

		if (edited_scene->get_filename() != "") {
			if (_cyclical_dependency_exists(edited_scene->get_filename(), instanced_scene)) {
				accept->set_text(vformat(TTR("Cannot instance the scene '%s' because the current scene exists within one of its nodes."), p_files[i]));
				accept->popup_centered_minsize();
				error = true;
				break;
			}
		}

		instanced_scene->set_filename(ProjectSettings::get_singleton()->localize_path(p_files[i]));

		instances.push_back(instanced_scene);
	}

	if (error) {
		for (int i = 0; i < instances.size(); i++) {
			memdelete(instances[i]);
		}
		return;
	}

	editor_data->get_undo_redo().create_action(TTR("Instance Scene(s)"));

	for (int i = 0; i < instances.size(); i++) {
		Node *instanced_scene = instances[i];

		editor_data->get_undo_redo().add_do_method(parent, "add_child", instanced_scene);
		if (p_pos >= 0) {
			editor_data->get_undo_redo().add_do_method(parent, "move_child", instanced_scene, p_pos + i);
		}
		editor_data->get_undo_redo().add_do_method(instanced_scene, "set_owner", edited_scene);
		editor_data->get_undo_redo().add_do_method(editor_selection, "clear");
		editor_data->get_undo_redo().add_do_method(editor_selection, "add_node", instanced_scene);
		editor_data->get_undo_redo().add_do_reference(instanced_scene);
		editor_data->get_undo_redo().add_undo_method(parent, "remove_child", instanced_scene);

		String new_name = parent->validate_child_name(instanced_scene);
		ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();
		editor_data->get_undo_redo().add_do_method(sed, "live_debug_instance_node", edited_scene->get_path_to(parent), p_files[i], new_name);
		editor_data->get_undo_redo().add_undo_method(sed, "live_debug_remove_node", NodePath(String(edited_scene->get_path_to(parent)).plus_file(new_name)));
	}

	editor_data->get_undo_redo().commit_action();
	_push_item(instances[instances.size() - 1]);
	for (int i = 0; i < instances.size(); i++) {
		emit_signal("node_created", instances[i]);
	}
}

void SceneTreeDock::_replace_with_branch_scene(const String &p_file, Node *base) {
	Ref<PackedScene> sdata = ResourceLoader::load(p_file);
	if (!sdata.is_valid()) {
		accept->set_text(vformat(TTR("Error loading scene from %s"), p_file));
		accept->popup_centered_minsize();
		return;
	}

	Node *instanced_scene = sdata->instance(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (!instanced_scene) {
		accept->set_text(vformat(TTR("Error instancing scene from %s"), p_file));
		accept->popup_centered_minsize();
		return;
	}

	UndoRedo *undo_redo = editor->get_undo_redo();
	undo_redo->create_action(TTR("Replace with Branch Scene"));

	Node *parent = base->get_parent();
	int pos = base->get_index();
	undo_redo->add_do_method(parent, "remove_child", base);
	undo_redo->add_undo_method(parent, "remove_child", instanced_scene);
	undo_redo->add_do_method(parent, "add_child", instanced_scene);
	undo_redo->add_undo_method(parent, "add_child", base);
	undo_redo->add_do_method(parent, "move_child", instanced_scene, pos);
	undo_redo->add_undo_method(parent, "move_child", base, pos);

	List<Node *> owned;
	base->get_owned_by(base->get_owner(), &owned);
	Array owners;
	for (List<Node *>::Element *F = owned.front(); F; F = F->next()) {
		owners.push_back(F->get());
	}
	undo_redo->add_do_method(instanced_scene, "set_owner", edited_scene);
	undo_redo->add_undo_method(this, "_set_owners", edited_scene, owners);

	undo_redo->add_do_method(editor_selection, "clear");
	undo_redo->add_undo_method(editor_selection, "clear");
	undo_redo->add_do_method(editor_selection, "add_node", instanced_scene);
	undo_redo->add_undo_method(editor_selection, "add_node", base);
	undo_redo->add_do_property(scene_tree, "set_selected", instanced_scene);
	undo_redo->add_undo_property(scene_tree, "set_selected", base);

	undo_redo->add_do_reference(instanced_scene);
	undo_redo->add_undo_reference(base);
	undo_redo->commit_action();
}

bool SceneTreeDock::_cyclical_dependency_exists(const String &p_target_scene_path, Node *p_desired_node) {
	int childCount = p_desired_node->get_child_count();

	if (_track_inherit(p_target_scene_path, p_desired_node)) {
		return true;
	}

	for (int i = 0; i < childCount; i++) {
		Node *child = p_desired_node->get_child(i);

		if (_cyclical_dependency_exists(p_target_scene_path, child)) {
			return true;
		}
	}

	return false;
}

bool SceneTreeDock::_track_inherit(const String &p_target_scene_path, Node *p_desired_node) {
	Node *p = p_desired_node;
	bool result = false;
	Vector<Node *> instances;
	while (true) {
		if (p->get_filename() == p_target_scene_path) {
			result = true;
			break;
		}
		Ref<SceneState> ss = p->get_scene_inherited_state();
		if (ss.is_valid()) {
			String path = ss->get_path();
			Ref<PackedScene> data = ResourceLoader::load(path);
			if (data.is_valid()) {
				p = data->instance(PackedScene::GEN_EDIT_STATE_INSTANCE);
				if (!p) {
					continue;
				}
				instances.push_back(p);
			} else {
				break;
			}
		} else {
			break;
		}
	}
	for (int i = 0; i < instances.size(); i++) {
		memdelete(instances[i]);
	}
	return result;
}

void SceneTreeDock::_tool_selected(int p_tool, bool p_confirm_override) {
	current_option = p_tool;

	switch (p_tool) {
#ifdef MODULE_REGEX_ENABLED
		case TOOL_BATCH_RENAME: {
			if (!profile_allow_editing) {
				break;
			}
			if (editor_selection->get_selected_node_list().size() > 1) {
				rename_dialog->popup_centered();
			}
		} break;
#endif // MODULE_REGEX_ENABLED
		case TOOL_RENAME: {
			if (!profile_allow_editing) {
				break;
			}
			Tree *tree = scene_tree->get_scene_tree();
			if (tree->is_anything_selected()) {
				tree->grab_focus();
				tree->edit_selected();
			}
		} break;
		case TOOL_NEW:
		case TOOL_REPARENT_TO_NEW_NODE: {
			if (!profile_allow_editing) {
				break;
			}

			if (reset_create_dialog && !p_confirm_override) {
				create_dialog->set_base_type("Node");
				reset_create_dialog = false;
			}

			// Prefer nodes that inherit from the current scene root.
			Node *current_edited_scene_root = EditorNode::get_singleton()->get_edited_scene();
			if (current_edited_scene_root) {
				String root_class = current_edited_scene_root->get_class_name();
				static Vector<String> preferred_types;
				if (preferred_types.empty()) {
					preferred_types.push_back("Control");
					preferred_types.push_back("Node2D");
					preferred_types.push_back("Spatial");
				}

				for (int i = 0; i < preferred_types.size(); i++) {
					if (ClassDB::is_parent_class(root_class, preferred_types[i])) {
						create_dialog->set_preferred_search_result_type(preferred_types[i]);
						break;
					}
				}
			}

			create_dialog->popup_create(true);
			if (!p_confirm_override) {
				emit_signal("add_node_used");
			}
		} break;
		case TOOL_INSTANCE: {
			if (!profile_allow_editing) {
				break;
			}
			Node *scene = edited_scene;

			if (!scene) {
				EditorNode::get_singleton()->new_inherited_scene();
				break;
			}

			quick_open->popup_dialog("PackedScene", true);
			quick_open->set_title(TTR("Instance Child Scene"));
			if (!p_confirm_override) {
				emit_signal("add_node_used");
			}
		} break;
		case TOOL_EXPAND_COLLAPSE: {
			if (!scene_tree->get_selected()) {
				break;
			}

			Tree *tree = scene_tree->get_scene_tree();
			TreeItem *selected_item = tree->get_selected();

			if (!selected_item) {
				selected_item = tree->get_root();
			}

			bool collapsed = _is_collapsed_recursive(selected_item);
			_set_collapsed_recursive(selected_item, !collapsed);

			tree->ensure_cursor_is_visible();

		} break;
		case TOOL_CUT:
		case TOOL_COPY: {
			if (!edited_scene || (p_tool == TOOL_CUT && !_validate_no_foreign())) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			if (selection.size() == 0) {
				break;
			}

			if (!node_clipboard.empty()) {
				_clear_clipboard();
			}
			clipboard_source_scene = editor->get_edited_scene()->get_filename();

			selection.sort_custom<Node::Comparator>();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Node *node = E->get();
				Map<const Node *, Node *> duplimap;
				Node *dup = node->duplicate_from_editor(duplimap);

				ERR_CONTINUE(!dup);

				node_clipboard.push_back(dup);
			}

			if (p_tool == TOOL_CUT) {
				_delete_confirm(true);
			}
		} break;
		case TOOL_PASTE: {
			if (node_clipboard.empty() || !edited_scene) {
				break;
			}

			bool has_cycle = false;
			if (edited_scene->get_filename() != String()) {
				for (List<Node *>::Element *E = node_clipboard.front(); E; E = E->next()) {
					if (edited_scene->get_filename() == E->get()->get_filename()) {
						has_cycle = true;
						break;
					}
				}
			}

			if (has_cycle) {
				current_option = -1;
				accept->set_text(TTR("Can't paste root node into the same scene."));
				accept->popup_centered();
				break;
			}

			Node *paste_parent = edited_scene;
			List<Node *> selection = editor_selection->get_selected_node_list();
			if (selection.size() > 0) {
				paste_parent = selection.back()->get();
			}

			Node *owner = paste_parent->get_owner();
			if (!owner) {
				owner = paste_parent;
			}

			editor_data->get_undo_redo().create_action(TTR("Paste Node(s)"));
			editor_data->get_undo_redo().add_do_method(editor_selection, "clear");

			Map<RES, RES> resource_remap;
			String target_scene = editor->get_edited_scene()->get_filename();
			if (target_scene != clipboard_source_scene) {
				if (!clipboard_resource_remap.has(target_scene)) {
					Map<RES, RES> remap;
					for (List<Node *>::Element *E = node_clipboard.front(); E; E = E->next()) {
						_create_remap_for_node(E->get(), remap);
					}
					clipboard_resource_remap[target_scene] = remap;
				}
				resource_remap = clipboard_resource_remap[target_scene];
			}

			for (List<Node *>::Element *E = node_clipboard.front(); E; E = E->next()) {
				Node *node = E->get();
				Map<const Node *, Node *> duplimap;

				Node *dup = node->duplicate_from_editor(duplimap, resource_remap);

				ERR_CONTINUE(!dup);

				editor_data->get_undo_redo().add_do_method(paste_parent, "add_child", dup);

				for (Map<const Node *, Node *>::Element *E2 = duplimap.front(); E2; E2 = E2->next()) {
					Node *d = E2->value();
					editor_data->get_undo_redo().add_do_method(d, "set_owner", owner);
				}

				editor_data->get_undo_redo().add_do_method(dup, "set_owner", owner);
				editor_data->get_undo_redo().add_do_method(editor_selection, "add_node", dup);
				editor_data->get_undo_redo().add_undo_method(paste_parent, "remove_child", dup);
				editor_data->get_undo_redo().add_do_reference(dup);

				if (node_clipboard.size() == 1) {
					editor_data->get_undo_redo().add_do_method(editor, "push_item", dup);
				}
			}

			editor_data->get_undo_redo().commit_action();
		} break;
		case TOOL_REPLACE: {
			if (!profile_allow_editing) {
				break;
			}

			if (!_validate_no_foreign()) {
				break;
			}

			if (!_validate_no_instance()) {
				break;
			}

			if (reset_create_dialog) {
				create_dialog->set_base_type("Node");
				reset_create_dialog = false;
			}

			Node *selected = scene_tree->get_selected();
			if (!selected && !editor_selection->get_selected_node_list().empty()) {
				selected = editor_selection->get_selected_node_list().front()->get();
			}

			if (selected) {
				create_dialog->popup_create(false, true, selected->get_class());
			}

		} break;
		case TOOL_EXTEND_SCRIPT: {
			attach_script_to_selected(true);
		} break;
		case TOOL_ATTACH_SCRIPT: {
			attach_script_to_selected(false);
		} break;
		case TOOL_DETACH_SCRIPT: {
			if (!profile_allow_script_editing) {
				break;
			}

			Array selection = editor_selection->get_selected_nodes();

			if (selection.empty()) {
				return;
			}

			editor_data->get_undo_redo().create_action(TTR("Detach Script"));
			editor_data->get_undo_redo().add_do_method(editor, "push_item", (Script *)nullptr);

			for (int i = 0; i < selection.size(); i++) {
				Node *n = Object::cast_to<Node>(selection[i]);
				Ref<Script> existing = n->get_script();
				Ref<Script> empty = EditorNode::get_singleton()->get_object_custom_type_base(n);
				if (existing != empty) {
					editor_data->get_undo_redo().add_do_method(n, "set_script", empty);
					editor_data->get_undo_redo().add_undo_method(n, "set_script", existing);
				}
			}

			editor_data->get_undo_redo().add_do_method(this, "_update_script_button");
			editor_data->get_undo_redo().add_undo_method(this, "_update_script_button");

			editor_data->get_undo_redo().commit_action();
		} break;
		case TOOL_MOVE_UP:
		case TOOL_MOVE_DOWN: {
			if (!profile_allow_editing) {
				break;
			}

			if (!scene_tree->get_selected()) {
				break;
			}

			if (scene_tree->get_selected() == edited_scene) {
				current_option = -1;
				accept->set_text(TTR("This operation can't be done on the tree root."));
				accept->popup_centered_minsize();
				break;
			}

			if (!_validate_no_foreign()) {
				break;
			}

			bool MOVING_DOWN = (p_tool == TOOL_MOVE_DOWN);
			bool MOVING_UP = !MOVING_DOWN;

			Node *common_parent = scene_tree->get_selected()->get_parent();
			List<Node *> selection = editor_selection->get_selected_node_list();
			selection.sort_custom<Node::Comparator>(); // sort by index
			if (MOVING_DOWN) {
				selection.invert();
			}

			int lowest_id = common_parent->get_child_count() - 1;
			int highest_id = 0;
			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				int index = E->get()->get_index();

				if (index > highest_id) {
					highest_id = index;
				}
				if (index < lowest_id) {
					lowest_id = index;
				}

				if (E->get()->get_parent() != common_parent) {
					common_parent = nullptr;
				}
			}

			if (!common_parent || (MOVING_DOWN && highest_id >= common_parent->get_child_count() - MOVING_DOWN) || (MOVING_UP && lowest_id == 0)) {
				break; // one or more nodes can not be moved
			}

			if (selection.size() == 1) {
				editor_data->get_undo_redo().create_action(TTR("Move Node In Parent"));
			}
			if (selection.size() > 1) {
				editor_data->get_undo_redo().create_action(TTR("Move Nodes In Parent"));
			}

			for (int i = 0; i < selection.size(); i++) {
				Node *top_node = selection[i];
				Node *bottom_node = selection[selection.size() - 1 - i];

				ERR_FAIL_COND(!top_node->get_parent());
				ERR_FAIL_COND(!bottom_node->get_parent());

				int bottom_node_pos = bottom_node->get_index();
				int top_node_pos_next = top_node->get_index() + (MOVING_DOWN ? 1 : -1);

				editor_data->get_undo_redo().add_do_method(top_node->get_parent(), "move_child", top_node, top_node_pos_next);
				editor_data->get_undo_redo().add_undo_method(bottom_node->get_parent(), "move_child", bottom_node, bottom_node_pos);
			}

			editor_data->get_undo_redo().commit_action();

		} break;
		case TOOL_DUPLICATE: {
			if (!profile_allow_editing) {
				break;
			}

			if (!edited_scene) {
				break;
			}

			if (editor_selection->is_selected(edited_scene)) {
				current_option = -1;
				accept->set_text(TTR("This operation can't be done on the tree root."));
				accept->popup_centered_minsize();
				break;
			}

			if (!_validate_no_foreign()) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			if (selection.size() == 0) {
				break;
			}

			editor_data->get_undo_redo().create_action(TTR("Duplicate Node(s)"));
			editor_data->get_undo_redo().add_do_method(editor_selection, "clear");

			Node *dupsingle = nullptr;

			selection.sort_custom<Node::Comparator>();

			Node *add_below_node = selection.back()->get();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Node *node = E->get();
				Node *parent = node->get_parent();

				List<Node *> owned;
				node->get_owned_by(node->get_owner(), &owned);

				Map<const Node *, Node *> duplimap;
				Node *dup = node->duplicate_from_editor(duplimap);

				ERR_CONTINUE(!dup);

				if (selection.size() == 1) {
					dupsingle = dup;
				}

				dup->set_name(parent->validate_child_name(dup));

				editor_data->get_undo_redo().add_do_method(parent, "add_child_below_node", add_below_node, dup);
				for (List<Node *>::Element *F = owned.front(); F; F = F->next()) {
					if (!duplimap.has(F->get())) {
						continue;
					}
					Node *d = duplimap[F->get()];
					editor_data->get_undo_redo().add_do_method(d, "set_owner", node->get_owner());
				}
				editor_data->get_undo_redo().add_do_method(editor_selection, "add_node", dup);
				editor_data->get_undo_redo().add_undo_method(parent, "remove_child", dup);
				editor_data->get_undo_redo().add_do_reference(dup);

				ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();

				editor_data->get_undo_redo().add_do_method(sed, "live_debug_duplicate_node", edited_scene->get_path_to(node), dup->get_name());
				editor_data->get_undo_redo().add_undo_method(sed, "live_debug_remove_node", NodePath(String(edited_scene->get_path_to(parent)).plus_file(dup->get_name())));

				add_below_node = dup;
			}

			editor_data->get_undo_redo().commit_action();

			if (dupsingle) {
				_push_item(dupsingle);
			}
		} break;
		case TOOL_REPARENT: {
			if (!profile_allow_editing) {
				break;
			}

			if (!scene_tree->get_selected()) {
				break;
			}

			if (editor_selection->is_selected(edited_scene)) {
				current_option = -1;
				accept->set_text(TTR("This operation can't be done on the tree root."));
				accept->popup_centered_minsize();
				break;
			}

			if (!_validate_no_foreign()) {
				break;
			}

			List<Node *> nodes = editor_selection->get_selected_node_list();
			Set<Node *> nodeset;
			for (List<Node *>::Element *E = nodes.front(); E; E = E->next()) {
				nodeset.insert(E->get());
			}
			reparent_dialog->popup_centered_ratio();
			reparent_dialog->set_current(nodeset);

		} break;
		case TOOL_MAKE_ROOT: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> nodes = editor_selection->get_selected_node_list();
			ERR_FAIL_COND(nodes.size() != 1);

			Node *node = nodes.front()->get();
			Node *root = get_tree()->get_edited_scene_root();

			if (node == root) {
				return;
			}

			//check that from node to root, all owners are right

			if (root->get_scene_inherited_state().is_valid()) {
				accept->set_text(TTR("Can't reparent nodes in inherited scenes, order of nodes can't change."));
				accept->popup_centered_minsize();
				return;
			}

			if (node->get_owner() != root) {
				accept->set_text(TTR("Node must belong to the edited scene to become root."));
				accept->popup_centered_minsize();
				return;
			}

			if (node->get_filename() != String()) {
				accept->set_text(TTR("Instantiated scenes can't become root"));
				accept->popup_centered_minsize();
				return;
			}

			editor_data->get_undo_redo().create_action(TTR("Make node as Root"));
			editor_data->get_undo_redo().add_do_method(node->get_parent(), "remove_child", node);
			editor_data->get_undo_redo().add_do_method(editor, "set_edited_scene", node);
			editor_data->get_undo_redo().add_do_method(node, "add_child", root);
			editor_data->get_undo_redo().add_do_method(node, "set_filename", root->get_filename());
			editor_data->get_undo_redo().add_do_method(root, "set_filename", String());
			editor_data->get_undo_redo().add_do_method(node, "set_owner", (Object *)nullptr);
			editor_data->get_undo_redo().add_do_method(root, "set_owner", node);
			_node_replace_owner(root, root, node, MODE_DO);

			editor_data->get_undo_redo().add_undo_method(root, "set_filename", root->get_filename());
			editor_data->get_undo_redo().add_undo_method(node, "set_filename", String());
			editor_data->get_undo_redo().add_undo_method(node, "remove_child", root);
			editor_data->get_undo_redo().add_undo_method(editor, "set_edited_scene", root);
			editor_data->get_undo_redo().add_undo_method(node->get_parent(), "add_child", node);
			editor_data->get_undo_redo().add_undo_method(node->get_parent(), "move_child", node, node->get_index());
			editor_data->get_undo_redo().add_undo_method(root, "set_owner", (Object *)nullptr);
			editor_data->get_undo_redo().add_undo_method(node, "set_owner", root);
			_node_replace_owner(root, root, root, MODE_UNDO);

			editor_data->get_undo_redo().add_do_method(scene_tree, "update_tree");
			editor_data->get_undo_redo().add_undo_method(scene_tree, "update_tree");
			editor_data->get_undo_redo().commit_action();
		} break;
		case TOOL_MULTI_EDIT: {
			if (!profile_allow_editing) {
				break;
			}

			Node *root = EditorNode::get_singleton()->get_edited_scene();
			if (!root) {
				break;
			}
			Ref<MultiNodeEdit> mne = memnew(MultiNodeEdit);
			for (const Map<Node *, Object *>::Element *E = editor_selection->get_selection().front(); E; E = E->next()) {
				mne->add_node(root->get_path_to(E->key()));
			}

			_push_item(mne.ptr());

		} break;

		case TOOL_ERASE: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> remove_list = editor_selection->get_selected_node_list();

			if (remove_list.empty()) {
				return;
			}

			if (!_validate_no_foreign()) {
				break;
			}

			if (p_confirm_override) {
				_delete_confirm();

			} else {
				String msg;
				if (remove_list.size() > 1) {
					bool any_children = false;
					for (int i = 0; !any_children && i < remove_list.size(); i++) {
						any_children = remove_list[i]->get_child_count() > 0;
					}

					msg = vformat(any_children ? TTR("Delete %d nodes and any children?") : TTR("Delete %d nodes?"), remove_list.size());
				} else {
					Node *node = remove_list[0];
					if (node == editor_data->get_edited_scene_root()) {
						msg = vformat(TTR("Delete the root node \"%s\"?"), node->get_name());
					} else if (node->get_filename() == "" && node->get_child_count() > 0) {
						// Display this message only for non-instanced scenes
						msg = vformat(TTR("Delete node \"%s\" and its children?"), node->get_name());
					} else {
						msg = vformat(TTR("Delete node \"%s\"?"), node->get_name());
					}
				}

				delete_dialog->set_text(msg);

				// Resize the dialog to its minimum size.
				// This prevents the dialog from being too wide after displaying
				// a deletion confirmation for a node with a long name.
				delete_dialog->set_size(Size2());
				delete_dialog->popup_centered_minsize();
			}

		} break;
		case TOOL_MERGE_FROM_SCENE: {
			if (!profile_allow_editing) {
				break;
			}

			EditorNode::get_singleton()->merge_from_scene();
		} break;
		case TOOL_NEW_SCENE_FROM: {
			if (!profile_allow_editing) {
				break;
			}

			Node *scene = editor_data->get_edited_scene_root();

			if (!scene) {
				accept->set_text(TTR("Saving the branch as a scene requires having a scene open in the editor."));
				accept->popup_centered_minsize();
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();

			if (selection.size() != 1) {
				accept->set_text(vformat(TTR("Saving the branch as a scene requires selecting only one node, but you have selected %d nodes."), selection.size()));
				accept->popup_centered_minsize();
				break;
			}

			Node *tocopy = selection.front()->get();

			if (tocopy == scene) {
				accept->set_text(TTR("Can't save the root node branch as an instanced scene.\nTo create an editable copy of the current scene, duplicate it using the FileSystem dock context menu\nor create an inherited scene using Scene > New Inherited Scene... instead."));
				accept->popup_centered_minsize();
				break;
			}

			if (tocopy != editor_data->get_edited_scene_root() && tocopy->get_filename() != "") {
				accept->set_text(TTR("Can't save the branch of an already instanced scene.\nTo create a variation of a scene, you can make an inherited scene based on the instanced scene using Scene > New Inherited Scene... instead."));
				accept->popup_centered_minsize();
				break;
			}

			if (tocopy->get_owner() != scene) {
				accept->set_text(TTR("Can't save a branch which is a child of an already instantiated scene.\nTo save this branch into its own scene, open the original scene, right click on this branch, and select \"Save Branch as Scene\"."));
				accept->popup_centered();
				break;
			}

			if (scene->get_scene_inherited_state().is_valid() && scene->get_scene_inherited_state()->find_node_by_path(scene->get_path_to(tocopy)) >= 0) {
				accept->set_text(TTR("Can't save a branch which is part of an inherited scene.\nTo save this branch into its own scene, open the original scene, right click on this branch, and select \"Save Branch as Scene\"."));
				accept->popup_centered();
				break;
			}

			new_scene_from_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			new_scene_from_dialog->clear_filters();
			for (int i = 0; i < extensions.size(); i++) {
				new_scene_from_dialog->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			}

			String existing;
			if (extensions.size()) {
				String root_name(tocopy->get_name());
				existing = root_name + "." + extensions.front()->get().to_lower();
			}
			new_scene_from_dialog->set_current_path(existing);

			new_scene_from_dialog->popup_centered_ratio();
			new_scene_from_dialog->set_title(TTR("Save New Scene As..."));
		} break;
		case TOOL_COPY_NODE_PATH: {
			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					Node *root = EditorNode::get_singleton()->get_edited_scene();
					NodePath path = root->get_path().rel_path_to(node->get_path());
					OS::get_singleton()->set_clipboard(path);
				}
			}
		} break;
		case TOOL_OPEN_DOCUMENTATION: {
			List<Node *> selection = editor_selection->get_selected_node_list();
			for (int i = 0; i < selection.size(); i++) {
				ScriptEditor::get_singleton()->goto_help("class_name:" + selection[i]->get_class());
			}
			EditorNode::get_singleton()->set_visible_editor(EditorNode::EDITOR_SCRIPT);
		} break;
		case TOOL_SCENE_EDITABLE_CHILDREN: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					bool editable = EditorNode::get_singleton()->get_edited_scene()->is_editable_instance(node);

					if (editable) {
						editable_instance_remove_dialog->set_text(TTR("Disabling \"editable_instance\" will cause all properties of the node to be reverted to their default."));
						editable_instance_remove_dialog->popup_centered_minsize();
						break;
					}
					_toggle_editable_children(node);
				}
			}
		} break;
		case TOOL_SCENE_USE_PLACEHOLDER: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					bool editable = EditorNode::get_singleton()->get_edited_scene()->is_editable_instance(node);
					bool placeholder = node->get_scene_instance_load_placeholder();

					// Fire confirmation dialog when children are editable.
					if (editable && !placeholder) {
						placeholder_editable_instance_remove_dialog->set_text(TTR("Enabling \"Load As Placeholder\" will disable \"Editable Children\" and cause all properties of the node to be reverted to their default."));
						placeholder_editable_instance_remove_dialog->popup_centered_minsize();
						break;
					}

					placeholder = !placeholder;

					if (placeholder) {
						EditorNode::get_singleton()->get_edited_scene()->set_editable_instance(node, false);
					}

					node->set_scene_instance_load_placeholder(placeholder);
					scene_tree->update_tree();
				}
			}
		} break;
		case TOOL_SCENE_MAKE_LOCAL: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					Node *root = EditorNode::get_singleton()->get_edited_scene();
					UndoRedo *undo_redo = &editor_data->get_undo_redo();
					if (!root) {
						break;
					}

					ERR_FAIL_COND(node->get_filename() == String());
					undo_redo->create_action(TTR("Make Local"));
					undo_redo->add_do_method(node, "set_filename", "");
					undo_redo->add_undo_method(node, "set_filename", node->get_filename());
					_node_replace_owner(node, node, root);
					undo_redo->add_do_method(scene_tree, "update_tree");
					undo_redo->add_undo_method(scene_tree, "update_tree");
					undo_redo->commit_action();
				}
			}
		} break;
		case TOOL_SCENE_OPEN: {
			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					scene_tree->emit_signal("open", node->get_filename());
				}
			}
		} break;
		case TOOL_SCENE_CLEAR_INHERITANCE: {
			if (!profile_allow_editing) {
				break;
			}

			clear_inherit_confirm->popup_centered_minsize();
		} break;
		case TOOL_SCENE_CLEAR_INHERITANCE_CONFIRM: {
			if (!profile_allow_editing) {
				break;
			}

			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node) {
					node->set_scene_inherited_state(Ref<SceneState>());
					scene_tree->update_tree();
					EditorNode::get_singleton()->get_inspector()->update_tree();
				}
			}
		} break;
		case TOOL_SCENE_OPEN_INHERITED: {
			List<Node *> selection = editor_selection->get_selected_node_list();
			List<Node *>::Element *e = selection.front();
			if (e) {
				Node *node = e->get();
				if (node && node->get_scene_inherited_state().is_valid()) {
					scene_tree->emit_signal("open", node->get_scene_inherited_state()->get_path());
				}
			}
		} break;
		case TOOL_TOGGLE_SCENE_UNIQUE_NAME: {
			// Enabling/disabling based on the same node based on which the checkbox in the menu is checked/unchecked.
			List<Node *>::Element *first_selected = editor_selection->get_selected_node_list().front();
			if (first_selected == nullptr) {
				return;
			}
			if (first_selected->get() == EditorNode::get_singleton()->get_edited_scene()) {
				// Exclude Root Node. It should never be unique name in its own scene!
				editor_selection->remove_node(first_selected->get());
				first_selected = editor_selection->get_selected_node_list().front();
				if (first_selected == nullptr) {
					return;
				}
			}
			bool enabling = !first_selected->get()->is_unique_name_in_owner();

			List<Node *> full_selection = editor_selection->get_full_selected_node_list();
			UndoRedo *undo_redo = &editor_data->get_undo_redo();

			if (enabling) {
				Vector<Node *> new_unique_nodes;
				Vector<StringName> new_unique_names;
				Vector<StringName> cant_be_set_unique_names;

				for (List<Node *>::Element *e = full_selection.front(); e; e = e->next()) {
					Node *node = e->get();
					if (node->is_unique_name_in_owner()) {
						continue;
					}
					StringName name = node->get_name();
					if (new_unique_names.find(name) != -1 || get_tree()->get_edited_scene_root()->get_node_or_null(UNIQUE_NODE_PREFIX + String(name)) != nullptr) {
						cant_be_set_unique_names.push_back(name);
					} else {
						new_unique_nodes.push_back(node);
						new_unique_names.push_back(name);
					}
				}

				if (new_unique_nodes.size()) {
					undo_redo->create_action(TTR("Enable Scene Unique Name(s)"));
					for (int i = 0; i < new_unique_nodes.size(); i++) {
						undo_redo->add_do_method(new_unique_nodes[i], "set_unique_name_in_owner", true);
						undo_redo->add_undo_method(new_unique_nodes[i], "set_unique_name_in_owner", false);
					}
					undo_redo->commit_action();
				}

				if (cant_be_set_unique_names.size()) {
					String popup_text = TTR("Unique names already used by another node in the scene:");
					popup_text += "\n";
					for (int i = 0; i < cant_be_set_unique_names.size(); i++) {
						popup_text += "\n" + String(cant_be_set_unique_names[i]);
					}
					accept->set_text(popup_text);
					accept->popup_centered();
				}
			} else { // Disabling.
				undo_redo->create_action(TTR("Disable Scene Unique Name(s)"));
				for (List<Node *>::Element *e = full_selection.front(); e; e = e->next()) {
					Node *node = e->get();
					if (!node->is_unique_name_in_owner()) {
						continue;
					}
					undo_redo->add_do_method(node, "set_unique_name_in_owner", false);
					undo_redo->add_undo_method(node, "set_unique_name_in_owner", true);
				}
				undo_redo->commit_action();
			}
		} break;
		case TOOL_CREATE_2D_SCENE:
		case TOOL_CREATE_3D_SCENE:
		case TOOL_CREATE_USER_INTERFACE:
		case TOOL_CREATE_FAVORITE: {
			Node *new_node = nullptr;

			if (TOOL_CREATE_FAVORITE == p_tool) {
				String name = selected_favorite_root.get_slicec(' ', 0);
				if (ScriptServer::is_global_class(name)) {
					new_node = Object::cast_to<Node>(ClassDB::instance(ScriptServer::get_global_class_native_base(name)));
					Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(name), "Script");
					if (new_node && script.is_valid()) {
						new_node->set_script(script.get_ref_ptr());
						new_node->set_name(name);
					}
				} else {
					new_node = Object::cast_to<Node>(ClassDB::instance(selected_favorite_root));
				}

				if (!new_node) {
					new_node = memnew(Node);
					ERR_PRINT("Creating root from favorite '" + selected_favorite_root + "' failed. Creating 'Node' instead.");
				}
			} else {
				switch (p_tool) {
					case TOOL_CREATE_2D_SCENE:
						new_node = memnew(Node2D);
						break;
					case TOOL_CREATE_3D_SCENE:
						new_node = memnew(Spatial);
						break;
					case TOOL_CREATE_USER_INTERFACE: {
						Control *node = memnew(Control);
						node->set_anchors_and_margins_preset(PRESET_WIDE); //more useful for resizable UIs.
						new_node = node;

					} break;
				}
			}

			add_root_node(new_node);

			editor->edit_node(new_node);
			editor_selection->clear();
			editor_selection->add_node(new_node);

			scene_tree->get_scene_tree()->grab_focus();
		} break;

		default: {
			// Context menu options.
			if (p_tool >= EditorContextMenu::CONTEXT_ITEM_ID_BASE) {
				List<Node *> selection = editor_selection->get_selected_node_list();
				EditorContextMenu::options_pressed(EditorContextMenu::CONTEXT_SLOT_SCENE_TREE, p_tool, selection);
				break;
			}

			_filter_option_selected(p_tool);

			if (p_tool >= EDIT_SUBRESOURCE_BASE) {
				int idx = p_tool - EDIT_SUBRESOURCE_BASE;

				ERR_FAIL_INDEX(idx, subresources.size());

				Object *obj = ObjectDB::get_instance(subresources[idx]);
				ERR_FAIL_COND(!obj);

				_push_item(obj);
			}
		}
	}
}

void SceneTreeDock::_property_selected(int p_idx) {
	ERR_FAIL_NULL(property_drop_node);
	_perform_property_drop(property_drop_node, menu_properties->get_item_metadata(p_idx), ResourceLoader::load(resource_drop_path));
	property_drop_node = nullptr;
}

void SceneTreeDock::_perform_property_drop(Node *p_node, String p_property, RES p_res) {
	editor_data->get_undo_redo().create_action(vformat(TTR("Set %s"), p_property));
	editor_data->get_undo_redo().add_do_property(p_node, p_property, p_res);
	editor_data->get_undo_redo().add_do_method(p_node, "property_list_changed_notify");
	editor_data->get_undo_redo().add_undo_property(p_node, p_property, p_node->get(p_property));
	editor_data->get_undo_redo().add_undo_method(p_node, "property_list_changed_notify");
	editor_data->get_undo_redo().commit_action();
}

void SceneTreeDock::add_root_node(Node *p_node) {
	editor_data->get_undo_redo().create_action(TTR("New Scene Root"));
	editor_data->get_undo_redo().add_do_method(editor, "set_edited_scene", p_node);
	editor_data->get_undo_redo().add_do_method(scene_tree, "update_tree");
	editor_data->get_undo_redo().add_do_reference(p_node);
	editor_data->get_undo_redo().add_undo_method(editor, "set_edited_scene", (Object *)nullptr);
	editor_data->get_undo_redo().commit_action();
}

void SceneTreeDock::_node_collapsed(Object *p_obj) {
	TreeItem *ti = Object::cast_to<TreeItem>(p_obj);
	if (!ti) {
		return;
	}

	if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
		_set_collapsed_recursive(ti, ti->is_collapsed());
	}
}

void SceneTreeDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!first_enter) {
				break;
			}
			first_enter = false;

			EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", this, "_feature_profile_changed");

			CanvasItemEditorPlugin *canvas_item_plugin = Object::cast_to<CanvasItemEditorPlugin>(editor_data->get_editor("2D"));
			if (canvas_item_plugin) {
				canvas_item_plugin->get_canvas_item_editor()->connect("item_lock_status_changed", scene_tree, "_update_tree");
				canvas_item_plugin->get_canvas_item_editor()->connect("item_group_status_changed", scene_tree, "_update_tree");
				scene_tree->connect("node_changed", canvas_item_plugin->get_canvas_item_editor()->get_viewport_control(), "update");
			}

			SpatialEditorPlugin *spatial_editor_plugin = Object::cast_to<SpatialEditorPlugin>(editor_data->get_editor("3D"));
			spatial_editor_plugin->get_spatial_editor()->connect("item_lock_status_changed", scene_tree, "_update_tree");
			spatial_editor_plugin->get_spatial_editor()->connect("item_group_status_changed", scene_tree, "_update_tree");

			button_add->set_icon(get_icon("Add", "EditorIcons"));
			button_instance->set_icon(get_icon("Instance", "EditorIcons"));
			button_create_script->set_icon(get_icon("ScriptCreate", "EditorIcons"));
			button_detach_script->set_icon(get_icon("ScriptRemove", "EditorIcons"));

			filter->set_right_icon(get_icon("Search", "EditorIcons"));
			filter->set_clear_button_enabled(true);

			EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed", this, "_selection_changed");
			scene_tree->get_scene_tree()->connect("item_collapsed", this, "_node_collapsed");

			// create_root_dialog
			HBoxContainer *top_row = memnew(HBoxContainer);
			top_row->set_name("NodeShortcutsTopRow");
			top_row->set_h_size_flags(SIZE_EXPAND_FILL);
			top_row->add_child(memnew(Label(TTR("Create Root Node:"))));
			top_row->add_spacer();

			ToolButton *node_shortcuts_toggle = memnew(ToolButton);
			node_shortcuts_toggle->set_name("NodeShortcutsToggle");
			node_shortcuts_toggle->set_icon(get_icon("Favorites", "EditorIcons"));
			node_shortcuts_toggle->set_toggle_mode(true);
			node_shortcuts_toggle->set_pressed(EDITOR_GET("_use_favorites_root_selection"));
			node_shortcuts_toggle->set_anchors_and_margins_preset(Control::PRESET_CENTER_RIGHT);
			node_shortcuts_toggle->connect("pressed", this, "_update_create_root_dialog");
			top_row->add_child(node_shortcuts_toggle);

			create_root_dialog->add_child(top_row);

			ScrollContainer *scroll_container = memnew(ScrollContainer);
			scroll_container->set_name("NodeShortcutsScrollContainer");
			create_root_dialog->add_child(scroll_container);
			scroll_container->set_v_size_flags(SIZE_EXPAND_FILL);
			scroll_container->set_enable_h_scroll(false);

			VBoxContainer *node_shortcuts = memnew(VBoxContainer);
			node_shortcuts->set_name("NodeShortcuts");
			scroll_container->add_child(node_shortcuts);
			node_shortcuts->set_h_size_flags(SIZE_EXPAND_FILL);

			VBoxContainer *beginner_node_shortcuts = memnew(VBoxContainer);
			beginner_node_shortcuts->set_name("BeginnerNodeShortcuts");
			node_shortcuts->add_child(beginner_node_shortcuts);

			button_2d = memnew(Button);
			beginner_node_shortcuts->add_child(button_2d);
			button_2d->set_text(TTR("2D Scene"));
			button_2d->set_icon(get_icon("Node2D", "EditorIcons"));
			button_2d->connect("pressed", this, "_tool_selected", make_binds(TOOL_CREATE_2D_SCENE, false));
			button_3d = memnew(Button);
			beginner_node_shortcuts->add_child(button_3d);
			button_3d->set_text(TTR("3D Scene"));
			button_3d->set_icon(get_icon("Spatial", "EditorIcons"));
			button_3d->connect("pressed", this, "_tool_selected", make_binds(TOOL_CREATE_3D_SCENE, false));

			button_ui = memnew(Button);
			beginner_node_shortcuts->add_child(button_ui);
			button_ui->set_text(TTR("User Interface"));
			button_ui->set_icon(get_icon("Control", "EditorIcons"));
			button_ui->connect("pressed", this, "_tool_selected", make_binds(TOOL_CREATE_USER_INTERFACE, false));

			VBoxContainer *favorite_node_shortcuts = memnew(VBoxContainer);
			favorite_node_shortcuts->set_name("FavoriteNodeShortcuts");
			node_shortcuts->add_child(favorite_node_shortcuts);

			button_custom = memnew(Button);
			node_shortcuts->add_child(button_custom);
			button_custom->set_text(TTR("Other Node"));
			button_custom->set_icon(get_icon("Add", "EditorIcons"));
			button_custom->connect("pressed", this, "_tool_selected", make_binds(TOOL_NEW, false));

			_update_create_root_dialog();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			clear_inherit_confirm->connect("confirmed", this, "_tool_selected", varray(TOOL_SCENE_CLEAR_INHERITANCE_CONFIRM));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			clear_inherit_confirm->disconnect("confirmed", this, "_tool_selected");
		} break;
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			button_add->set_icon(get_icon("Add", "EditorIcons"));
			button_instance->set_icon(get_icon("Instance", "EditorIcons"));
			button_create_script->set_icon(get_icon("ScriptCreate", "EditorIcons"));
			button_detach_script->set_icon(get_icon("ScriptRemove", "EditorIcons"));
			button_2d->set_icon(get_icon("Node2D", "EditorIcons"));
			button_3d->set_icon(get_icon("Spatial", "EditorIcons"));
			button_ui->set_icon(get_icon("Control", "EditorIcons"));
			button_custom->set_icon(get_icon("Add", "EditorIcons"));

			filter->set_right_icon(get_icon("Search", "EditorIcons"));
			filter->set_clear_button_enabled(true);
		} break;
		case NOTIFICATION_PROCESS: {
			bool show_create_root = bool(EDITOR_GET("interface/editors/show_scene_tree_root_selection")) && get_tree()->get_edited_scene_root() == nullptr;

			if (show_create_root != create_root_dialog->is_visible_in_tree() && !remote_tree->is_visible()) {
				if (show_create_root) {
					create_root_dialog->show();
					scene_tree->hide();
				} else {
					create_root_dialog->hide();
					scene_tree->show();
				}
			}

		} break;
	}
}

void SceneTreeDock::_node_replace_owner(Node *p_base, Node *p_node, Node *p_root, ReplaceOwnerMode p_mode) {
	if (p_node->get_owner() == p_base && p_node != p_root) {
		UndoRedo *undo_redo = &editor_data->get_undo_redo();
		switch (p_mode) {
			case MODE_BIDI: {
				bool is_unique = p_node->is_unique_name_in_owner() && p_base->get_node_or_null(UNIQUE_NODE_PREFIX + String(p_node->get_name())) != nullptr;
				if (is_unique) {
					// Will create a unique name conflict. Disable before setting owner.
					undo_redo->add_do_method(p_node, "set_unique_name_in_owner", false);
				}
				undo_redo->add_do_method(p_node, "set_owner", p_root);
				undo_redo->add_undo_method(p_node, "set_owner", p_base);
				if (is_unique) {
					// Will create a unique name conflict. Enable after setting owner.
					undo_redo->add_undo_method(p_node, "set_unique_name_in_owner", true);
				}

			} break;
			case MODE_DO: {
				undo_redo->add_do_method(p_node, "set_owner", p_root);

			} break;
			case MODE_UNDO: {
				undo_redo->add_undo_method(p_node, "set_owner", p_root);

			} break;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_node_replace_owner(p_base, p_node->get_child(i), p_root, p_mode);
	}
}

void SceneTreeDock::_load_request(const String &p_path) {
	editor->open_request(p_path);
}

void SceneTreeDock::_script_open_request(const Ref<Script> &p_script) {
	editor->edit_resource(p_script);
}

void SceneTreeDock::_push_item(Object *p_object) {
	editor->push_item(p_object);
}

void SceneTreeDock::_handle_select(Node *p_node) {
	if (tree_clicked) {
		pending_click_select = p_node;
	} else {
		_push_item(p_node);
	}
}

void SceneTreeDock::_node_selected() {
	Node *node = scene_tree->get_selected();

	if (!node) {
		return;
	}
	_handle_select(node);
}

void SceneTreeDock::_node_renamed() {
	_node_selected();
}

void SceneTreeDock::_set_owners(Node *p_owner, const Array &p_nodes) {
	for (int i = 0; i < p_nodes.size(); i++) {
		Node *n = Object::cast_to<Node>(p_nodes[i]);
		if (!n) {
			continue;
		}
		n->set_owner(p_owner);
	}
}

void SceneTreeDock::_fill_path_renames(Vector<StringName> base_path, Vector<StringName> new_base_path, Node *p_node, Map<Node *, NodePath> *p_renames) {
	base_path.push_back(p_node->get_name());
	if (new_base_path.size()) {
		new_base_path.push_back(p_node->get_name());
	}

	NodePath new_path;
	if (new_base_path.size()) {
		new_path = NodePath(new_base_path, true);
	}

	p_renames->insert(p_node, new_path);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_fill_path_renames(base_path, new_base_path, p_node->get_child(i), p_renames);
	}
}

void SceneTreeDock::fill_path_renames(Node *p_node, Node *p_new_parent, Map<Node *, NodePath> *p_renames) {
	Vector<StringName> base_path;
	Node *n = p_node->get_parent();
	while (n) {
		base_path.push_back(n->get_name());
		n = n->get_parent();
	}
	base_path.invert();

	Vector<StringName> new_base_path;
	if (p_new_parent) {
		n = p_new_parent;
		while (n) {
			new_base_path.push_back(n->get_name());
			n = n->get_parent();
		}

		new_base_path.invert();
	}

	_fill_path_renames(base_path, new_base_path, p_node, p_renames);
}

bool SceneTreeDock::_update_node_path(Node *p_root_node, NodePath &r_node_path, Map<Node *, NodePath> *p_renames) const {
	Node *target_node = p_root_node->get_node_or_null(r_node_path);
	ERR_FAIL_NULL_V_MSG(target_node, false, "Found invalid node path '" + String(r_node_path) + "' on node '" + String(scene_root->get_path_to(p_root_node)) + "'");

	// Try to find the target node in modified node paths.
	Map<Node *, NodePath>::Element *found_node_path = p_renames->find(target_node);
	if (found_node_path) {
		Map<Node *, NodePath>::Element *found_root_path = p_renames->find(p_root_node);
		NodePath root_path_new = found_root_path ? found_root_path->get() : p_root_node->get_path();
		r_node_path = root_path_new.rel_path_to(found_node_path->get());

		return true;
	}

	// Update the path if the base node has changed and has not been deleted.
	Map<Node *, NodePath>::Element *found_root_path = p_renames->find(p_root_node);
	if (found_root_path) {
		NodePath root_path_new = found_root_path->get();
		if (!root_path_new.is_empty()) {
			NodePath old_abs_path = NodePath(String(p_root_node->get_path()).plus_file(r_node_path));
			old_abs_path.simplify();
			r_node_path = root_path_new.rel_path_to(old_abs_path);
		}

		return true;
	}

	return false;
}

bool SceneTreeDock::_check_node_path_recursive(Node *p_root_node, Variant &r_variant, Map<Node *, NodePath> *p_renames) const {
	switch (r_variant.get_type()) {
		case Variant::NODE_PATH: {
			NodePath node_path = r_variant;
			if (!node_path.is_empty() && _update_node_path(p_root_node, node_path, p_renames)) {
				r_variant = node_path;
				return true;
			}
		} break;

		case Variant::ARRAY: {
			Array a = r_variant;
			bool updated = false;
			for (int i = 0; i < a.size(); i++) {
				Variant value = a[i];
				if (_check_node_path_recursive(p_root_node, value, p_renames)) {
					if (!updated) {
						a = a.duplicate(); // Need to duplicate for undo-redo to work.
						updated = true;
					}
					a[i] = value;
				}
			}
			if (updated) {
				r_variant = a;
				return true;
			}
		} break;

		case Variant::DICTIONARY: {
			Dictionary d = r_variant;
			bool updated = false;
			for (int i = 0; i < d.size(); i++) {
				Variant value = d.get_value_at_index(i);
				if (_check_node_path_recursive(p_root_node, value, p_renames)) {
					if (!updated) {
						d = d.duplicate(); // Need to duplicate for undo-redo to work.
						updated = true;
					}
					d[d.get_key_at_index(i)] = value;
				}
			}
			if (updated) {
				r_variant = d;
				return true;
			}
		} break;

		default: {
		}
	}

	return false;
}

void SceneTreeDock::perform_node_renames(Node *p_base, Map<Node *, NodePath> *p_renames, Map<Ref<Animation>, Set<int>> *r_rem_anims) {
	Map<Ref<Animation>, Set<int>> rem_anims;
	if (!r_rem_anims) {
		r_rem_anims = &rem_anims;
	}

	if (!p_base) {
		p_base = edited_scene;
	}

	if (!p_base) {
		return;
	}

	// No renaming if base node is deleted.
	Map<Node *, NodePath>::Element *found_base_path = p_renames->find(p_base);
	if (found_base_path && found_base_path->get().is_empty()) {
		return;
	}

	// Renaming node paths used in node properties.
	List<PropertyInfo> properties;
	p_base->get_property_list(&properties);

	for (List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
		if (!(E->get().usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR))) {
			continue;
		}
		String propertyname = E->get().name;
		Variant old_variant = p_base->get(propertyname);
		Variant updated_variant = old_variant;
		if (_check_node_path_recursive(p_base, updated_variant, p_renames)) {
			editor_data->get_undo_redo().add_do_property(p_base, propertyname, updated_variant);
			editor_data->get_undo_redo().add_undo_property(p_base, propertyname, old_variant);
			p_base->set(propertyname, updated_variant);
		}
	}

	bool autorename_animation_tracks = bool(EDITOR_DEF("editors/animation/autorename_animation_tracks", true));

	if (autorename_animation_tracks && Object::cast_to<AnimationPlayer>(p_base)) {
		AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(p_base);
		List<StringName> anims;
		ap->get_animation_list(&anims);
		Node *root = ap->get_node(ap->get_root());

		if (root) {
			Map<Node *, NodePath>::Element *found_root_path = p_renames->find(root);
			NodePath new_root_path = found_root_path ? found_root_path->get() : root->get_path();
			if (!new_root_path.is_empty()) { // No renaming if root node is deleted.
				for (List<StringName>::Element *E = anims.front(); E; E = E->next()) {
					Ref<Animation> anim = ap->get_animation(E->get());
					if (!r_rem_anims->has(anim)) {
						r_rem_anims->insert(anim, Set<int>());
						Set<int> &ran = r_rem_anims->find(anim)->get();
						for (int i = 0; i < anim->get_track_count(); i++) {
							ran.insert(i);
						}
					}

					Set<int> &ran = r_rem_anims->find(anim)->get();

					if (anim.is_null()) {
						continue;
					}

					for (int i = 0; i < anim->get_track_count(); i++) {
						NodePath track_np = anim->track_get_path(i);
						Node *n = root->get_node(track_np);
						if (!n) {
							continue;
						}

						if (!ran.has(i)) {
							continue; //channel was removed
						}

						Map<Node *, NodePath>::Element *found_path = p_renames->find(n);
						if (found_path) {
							if (found_path->get() == NodePath()) {
								//will be erased

								int idx = 0;
								Set<int>::Element *EI = ran.front();
								ERR_FAIL_COND(!EI); //bug
								while (EI->get() != i) {
									idx++;
									EI = EI->next();
									ERR_FAIL_COND(!EI); //another bug
								}

								editor_data->get_undo_redo().add_do_method(anim.ptr(), "remove_track", idx);
								editor_data->get_undo_redo().add_undo_method(anim.ptr(), "add_track", anim->track_get_type(i), idx);
								editor_data->get_undo_redo().add_undo_method(anim.ptr(), "track_set_path", idx, track_np);
								editor_data->get_undo_redo().add_undo_method(anim.ptr(), "track_set_interpolation_type", idx, anim->track_get_interpolation_type(i));
								for (int j = 0; j < anim->track_get_key_count(i); j++) {
									editor_data->get_undo_redo().add_undo_method(anim.ptr(), "track_insert_key", idx, anim->track_get_key_time(i, j), anim->track_get_key_value(i, j), anim->track_get_key_transition(i, j));
								}

								ran.erase(i); //byebye channel

							} else {
								//will be renamed
								NodePath rel_path = new_root_path.rel_path_to(found_path->get());

								NodePath new_path = NodePath(rel_path.get_names(), track_np.get_subnames(), false);
								if (new_path == track_np) {
									continue; //bleh
								}
								editor_data->get_undo_redo().add_do_method(anim.ptr(), "track_set_path", i, new_path);
								editor_data->get_undo_redo().add_undo_method(anim.ptr(), "track_set_path", i, track_np);
							}
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < p_base->get_child_count(); i++) {
		perform_node_renames(p_base->get_child(i), p_renames, r_rem_anims);
	}
}

void SceneTreeDock::_node_prerenamed(Node *p_node, const String &p_new_name) {
	Map<Node *, NodePath> path_renames;

	Vector<StringName> base_path;
	Node *n = p_node->get_parent();
	while (n) {
		base_path.push_back(n->get_name());
		n = n->get_parent();
	}
	base_path.invert();

	Vector<StringName> new_base_path = base_path;
	base_path.push_back(p_node->get_name());

	new_base_path.push_back(p_new_name);

	NodePath new_path(new_base_path, true);
	path_renames[p_node] = new_path;

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_fill_path_renames(base_path, new_base_path, p_node->get_child(i), &path_renames);
	}

	perform_node_renames(nullptr, &path_renames);
}

bool SceneTreeDock::_validate_no_foreign() {
	List<Node *> selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		if (E->get() != edited_scene && E->get()->get_owner() != edited_scene) {
			accept->set_text(TTR("Can't operate on nodes from a foreign scene!"));
			accept->popup_centered_minsize();
			return false;
		}

		// When edited_scene inherits from another one the root Node will be the parent Scene,
		// we don't want to consider that Node a foreign one otherwise we would not be able to
		// delete it.
		if (edited_scene->get_scene_inherited_state().is_valid() && edited_scene == E->get()) {
			continue;
		}

		if (edited_scene->get_scene_inherited_state().is_valid() && edited_scene->get_scene_inherited_state()->find_node_by_path(edited_scene->get_path_to(E->get())) >= 0) {
			accept->set_text(TTR("Can't operate on nodes the current scene inherits from!"));
			accept->popup_centered_minsize();
			return false;
		}
	}

	return true;
}

bool SceneTreeDock::_validate_no_instance() {
	List<Node *> selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		if (E->get() != edited_scene && E->get()->get_filename() != "") {
			accept->set_text(TTR("This operation can't be done on instanced scenes."));
			accept->popup_centered();
			return false;
		}
	}

	return true;
}

void SceneTreeDock::_node_reparent(NodePath p_path, bool p_keep_global_xform) {
	Node *new_parent = scene_root->get_node(p_path);
	ERR_FAIL_COND(!new_parent);

	List<Node *> selection = editor_selection->get_selected_node_list();

	if (selection.empty()) {
		return; // Nothing to reparent.
	}

	Vector<Node *> nodes;

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		nodes.push_back(E->get());
	}

	_do_reparent(new_parent, -1, nodes, p_keep_global_xform);
}

void SceneTreeDock::_do_reparent(Node *p_new_parent, int p_position_in_parent, Vector<Node *> p_nodes, bool p_keep_global_xform) {
	Node *new_parent = p_new_parent;
	ERR_FAIL_COND(!new_parent);

	if (p_nodes.size() == 0) {
		return; // Nothing to reparent.
	}

	p_nodes.sort_custom<Node::Comparator>(); //Makes result reliable.

	bool no_change = true;
	for (int ni = 0; ni < p_nodes.size(); ni++) {
		if (p_nodes[ni] == p_new_parent) {
			return; // Attempt to reparent to itself.
		}

		if (p_nodes[ni]->get_parent() != p_new_parent || p_position_in_parent + ni != p_nodes[ni]->get_position_in_parent()) {
			no_change = false;
		}
	}

	if (no_change) {
		return; // Position and parent didn't change.
	}

	Node *validate = new_parent;
	while (validate) {
		ERR_FAIL_COND_MSG(p_nodes.find(validate) != -1, "Selection changed at some point. Can't reparent.");
		validate = validate->get_parent();
	}

	// Sort by tree order, so re-adding is easy.
	p_nodes.sort_custom<Node::Comparator>();

	editor_data->get_undo_redo().create_action(TTR("Reparent Node"));

	Map<Node *, NodePath> path_renames;
	Vector<StringName> former_names;

	int inc = 0;

	for (int ni = 0; ni < p_nodes.size(); ni++) {
		// No undo implemented for this yet.
		Node *node = p_nodes[ni];

		fill_path_renames(node, new_parent, &path_renames);
		former_names.push_back(node->get_name());

		List<Node *> owned;
		node->get_owned_by(node->get_owner(), &owned);
		Array owners;
		for (List<Node *>::Element *E = owned.front(); E; E = E->next()) {
			owners.push_back(E->get());
		}

		if (new_parent == node->get_parent() && node->get_index() < p_position_in_parent + ni) {
			inc--; // If the child will generate a gap when moved, adjust.
		}

		editor_data->get_undo_redo().add_do_method(node->get_parent(), "remove_child", node);
		editor_data->get_undo_redo().add_do_method(new_parent, "add_child", node);

		if (p_position_in_parent >= 0) {
			editor_data->get_undo_redo().add_do_method(new_parent, "move_child", node, p_position_in_parent + inc);
		}

		ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();
		String old_name = former_names[ni];
		String new_name = new_parent->validate_child_name(node);

		// Name was modified, fix the path renames.
		if (old_name.casecmp_to(new_name) != 0) {
			// Fix the to name to have the new name.
			Map<Node *, NodePath>::Element *found_path = path_renames.find(node);
			if (found_path) {
				NodePath old_new_name = found_path->get();

				Vector<StringName> unfixed_new_names = old_new_name.get_names();
				Vector<StringName> fixed_new_names;

				// Get last name and replace with fixed new name.
				for (int a = 0; a < (unfixed_new_names.size() - 1); a++) {
					fixed_new_names.push_back(unfixed_new_names[a]);
				}
				fixed_new_names.push_back(new_name);

				NodePath fixed_node_path = NodePath(fixed_new_names, true);
				path_renames[node] = fixed_node_path;
			} else {
				ERR_PRINT("Internal error. Can't find renamed path for node '" + node->get_path() + "'");
			}
		}

		editor_data->get_undo_redo().add_do_method(sed, "live_debug_reparent_node", edited_scene->get_path_to(node), edited_scene->get_path_to(new_parent), new_name, p_position_in_parent + inc);
		editor_data->get_undo_redo().add_undo_method(sed, "live_debug_reparent_node", NodePath(String(edited_scene->get_path_to(new_parent)).plus_file(new_name)), edited_scene->get_path_to(node->get_parent()), node->get_name(), node->get_index());

		if (p_keep_global_xform) {
			if (Object::cast_to<Node2D>(node)) {
				editor_data->get_undo_redo().add_do_method(node, "set_global_transform", Object::cast_to<Node2D>(node)->get_global_transform());
			}
			if (Object::cast_to<Spatial>(node)) {
				editor_data->get_undo_redo().add_do_method(node, "set_global_transform", Object::cast_to<Spatial>(node)->get_global_transform());
			}
			if (Object::cast_to<Control>(node)) {
				editor_data->get_undo_redo().add_do_method(node, "set_global_position", Object::cast_to<Control>(node)->get_global_position());
			}
		}

		editor_data->get_undo_redo().add_do_method(this, "_set_owners", edited_scene, owners);

		if (AnimationPlayerEditor::singleton->get_track_editor()->get_root() == node) {
			editor_data->get_undo_redo().add_do_method(AnimationPlayerEditor::singleton->get_track_editor(), "set_root", node);
		}

		editor_data->get_undo_redo().add_undo_method(new_parent, "remove_child", node);
		editor_data->get_undo_redo().add_undo_method(node, "set_name", former_names[ni]);

		inc++;
	}

	// Add and move in a second step (so old order is preserved).
	for (int ni = 0; ni < p_nodes.size(); ni++) {
		Node *node = p_nodes[ni];

		List<Node *> owned;
		node->get_owned_by(node->get_owner(), &owned);
		Array owners;
		for (List<Node *>::Element *E = owned.front(); E; E = E->next()) {
			owners.push_back(E->get());
		}

		int child_pos = node->get_position_in_parent();

		editor_data->get_undo_redo().add_undo_method(node->get_parent(), "add_child", node);
		editor_data->get_undo_redo().add_undo_method(node->get_parent(), "move_child", node, child_pos);
		editor_data->get_undo_redo().add_undo_method(this, "_set_owners", edited_scene, owners);
		if (AnimationPlayerEditor::singleton->get_track_editor()->get_root() == node) {
			editor_data->get_undo_redo().add_undo_method(AnimationPlayerEditor::singleton->get_track_editor(), "set_root", node);
		}

		if (p_keep_global_xform) {
			if (Object::cast_to<Node2D>(node)) {
				editor_data->get_undo_redo().add_undo_method(node, "set_transform", Object::cast_to<Node2D>(node)->get_transform());
			}
			if (Object::cast_to<Spatial>(node)) {
				editor_data->get_undo_redo().add_undo_method(node, "set_transform", Object::cast_to<Spatial>(node)->get_transform());
			}
			if (Object::cast_to<Control>(node)) {
				editor_data->get_undo_redo().add_undo_method(node, "set_position", Object::cast_to<Control>(node)->get_position());
			}
		}
	}

	perform_node_renames(nullptr, &path_renames);

	editor_data->get_undo_redo().commit_action();
}

bool SceneTreeDock::_is_collapsed_recursive(TreeItem *p_item) const {
	bool is_branch_collapsed = false;

	List<TreeItem *> needs_check;
	needs_check.push_back(p_item);

	while (!needs_check.empty()) {
		TreeItem *item = needs_check.back()->get();
		needs_check.pop_back();

		TreeItem *child = item->get_children();
		is_branch_collapsed = item->is_collapsed() && child;

		if (is_branch_collapsed) {
			break;
		}
		while (child) {
			needs_check.push_back(child);
			child = child->get_next();
		}
	}
	return is_branch_collapsed;
}

void SceneTreeDock::_set_collapsed_recursive(TreeItem *p_item, bool p_collapsed) {
	List<TreeItem *> to_collapse;
	to_collapse.push_back(p_item);

	while (!to_collapse.empty()) {
		TreeItem *item = to_collapse.back()->get();
		to_collapse.pop_back();

		item->set_collapsed(p_collapsed);

		TreeItem *child = item->get_children();
		while (child) {
			to_collapse.push_back(child);
			child = child->get_next();
		}
	}
}

void SceneTreeDock::_script_created(Ref<Script> p_script) {
	List<Node *> selected = editor_selection->get_selected_node_list();

	if (selected.empty()) {
		return;
	}

	InspectorDock *inspector_dock = EditorNode::get_singleton()->get_inspector_dock();
	UndoRedo &undo_redo = editor_data->get_undo_redo();

	undo_redo.create_action(TTR("Attach Script"));
	for (List<Node *>::Element *E = selected.front(); E; E = E->next()) {
		Node *node = E->get();
		Ref<Script> existing = node->get_script();
		undo_redo.add_do_method(inspector_dock, "store_script_properties", node);
		undo_redo.add_undo_method(inspector_dock, "store_script_properties", node);
		undo_redo.add_do_method(node, "set_script", p_script.get_ref_ptr());
		undo_redo.add_undo_method(node, "set_script", existing);
		undo_redo.add_do_method(inspector_dock, "apply_script_properties", node);
		undo_redo.add_undo_method(inspector_dock, "apply_script_properties", node);
		undo_redo.add_do_method(this, "_update_script_button");
		undo_redo.add_undo_method(this, "_update_script_button");
	}

	undo_redo.commit_action();

	_push_item(p_script.operator->());
	_update_script_button();
}

void SceneTreeDock::_script_creation_closed() {
	script_create_dialog->disconnect("script_created", this, "_script_created");
}

void SceneTreeDock::_toggle_editable_children_from_selection() {
	List<Node *> selection = editor_selection->get_selected_node_list();
	List<Node *>::Element *e = selection.front();

	if (e) {
		_toggle_editable_children(e->get());
	}
}

void SceneTreeDock::_toggle_placeholder_from_selection() {
	List<Node *> selection = editor_selection->get_selected_node_list();
	List<Node *>::Element *e = selection.front();

	if (e) {
		Node *node = e->get();
		if (node) {
			_toggle_editable_children(node);

			bool placeholder = node->get_scene_instance_load_placeholder();
			placeholder = !placeholder;

			node->set_scene_instance_load_placeholder(placeholder);
			scene_tree->update_tree();
		}
	}
}

void SceneTreeDock::_toggle_editable_children(Node *p_node) {
	if (p_node) {
		bool editable = !EditorNode::get_singleton()->get_edited_scene()->is_editable_instance(p_node);
		EditorNode::get_singleton()->get_edited_scene()->set_editable_instance(p_node, editable);
		if (editable) {
			p_node->set_scene_instance_load_placeholder(false);
		}

		SpatialEditor::get_singleton()->update_all_gizmos(p_node);

		scene_tree->update_tree();
	}
}

void SceneTreeDock::_delete_confirm(bool p_cut) {
	List<Node *> remove_list = editor_selection->get_selected_node_list();

	if (remove_list.empty()) {
		return;
	}

	editor->get_editor_plugins_over()->make_visible(false);

	if (p_cut) {
		editor_data->get_undo_redo().create_action(TTR("Cut Node(s)"));
	} else {
		editor_data->get_undo_redo().create_action(TTR("Remove Node(s)"));
	}

	bool entire_scene = false;

	for (List<Node *>::Element *E = remove_list.front(); E; E = E->next()) {
		if (E->get() == edited_scene) {
			entire_scene = true;
		}
	}

	if (entire_scene) {
		editor_data->get_undo_redo().add_do_method(editor, "set_edited_scene", (Object *)nullptr);
		editor_data->get_undo_redo().add_undo_method(editor, "set_edited_scene", edited_scene);
		editor_data->get_undo_redo().add_undo_method(edited_scene, "set_owner", edited_scene->get_owner());
		editor_data->get_undo_redo().add_undo_method(scene_tree, "update_tree");
		editor_data->get_undo_redo().add_undo_reference(edited_scene);

	} else {
		remove_list.sort_custom<Node::Comparator>(); //sort nodes to keep positions
		Map<Node *, NodePath> path_renames;

		//delete from animation
		for (List<Node *>::Element *E = remove_list.front(); E; E = E->next()) {
			Node *n = E->get();
			if (!n->is_inside_tree() || !n->get_parent()) {
				continue;
			}

			fill_path_renames(n, nullptr, &path_renames);
		}

		perform_node_renames(nullptr, &path_renames);
		//delete for read
		for (List<Node *>::Element *E = remove_list.front(); E; E = E->next()) {
			Node *n = E->get();
			if (!n->is_inside_tree() || !n->get_parent()) {
				continue;
			}

			List<Node *> owned;
			n->get_owned_by(n->get_owner(), &owned);
			Array owners;
			for (List<Node *>::Element *F = owned.front(); F; F = F->next()) {
				owners.push_back(F->get());
			}

			editor_data->get_undo_redo().add_do_method(n->get_parent(), "remove_child", n);
			editor_data->get_undo_redo().add_undo_method(n->get_parent(), "add_child", n);
			editor_data->get_undo_redo().add_undo_method(n->get_parent(), "move_child", n, n->get_index());
			if (AnimationPlayerEditor::singleton->get_track_editor()->get_root() == n) {
				editor_data->get_undo_redo().add_undo_method(AnimationPlayerEditor::singleton->get_track_editor(), "set_root", n);
			}
			editor_data->get_undo_redo().add_undo_method(this, "_set_owners", edited_scene, owners);
			editor_data->get_undo_redo().add_undo_reference(n);

			ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();
			editor_data->get_undo_redo().add_do_method(sed, "live_debug_remove_and_keep_node", edited_scene->get_path_to(n), n->get_instance_id());
			editor_data->get_undo_redo().add_undo_method(sed, "live_debug_restore_node", n->get_instance_id(), edited_scene->get_path_to(n->get_parent()), n->get_index());
		}
	}
	editor_data->get_undo_redo().commit_action();

	// hack, force 2d editor viewport to refresh after deletion
	if (CanvasItemEditor *editor = CanvasItemEditor::get_singleton()) {
		editor->get_viewport_control()->update();
	}

	_push_item(nullptr);

	// Fixes the EditorHistory from still offering deleted notes
	EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
	editor_history->cleanup_history();
	EditorNode::get_singleton()->get_inspector_dock()->call("_prepare_history");
}

void SceneTreeDock::_update_script_button() {
	if (!profile_allow_script_editing) {
		button_create_script->hide();
		button_detach_script->hide();
	} else if (editor_selection->get_selection().size() == 0) {
		button_create_script->hide();
		button_detach_script->hide();
	} else if (editor_selection->get_selection().size() == 1) {
		Node *n = editor_selection->get_selected_node_list()[0];
		if (n->get_script().is_null()) {
			button_create_script->show();
			button_detach_script->hide();
		} else {
			button_create_script->hide();
			button_detach_script->show();
		}
	} else {
		button_create_script->hide();
		Array selection = editor_selection->get_selected_nodes();
		for (int i = 0; i < selection.size(); i++) {
			Node *n = Object::cast_to<Node>(selection[i]);
			if (!n->get_script().is_null()) {
				button_detach_script->show();
				return;
			}
		}
		button_detach_script->hide();
	}
}

void SceneTreeDock::_selection_changed() {
	int selection_size = editor_selection->get_selection().size();
	if (selection_size > 1) {
		//automatically turn on multi-edit
		_tool_selected(TOOL_MULTI_EDIT);
	} else if (selection_size == 1) {
		_handle_select(editor_selection->get_selection().front()->key());
	} else if (selection_size == 0) {
		_push_item(nullptr);
	}

	_update_script_button();
}

Node *SceneTreeDock::_get_selection_group_tail(Node *p_node, List<Node *> p_list) {
	Node *tail = p_node;
	Node *parent = tail->get_parent();

	for (int i = p_node->get_position_in_parent(); i < parent->get_child_count(); i++) {
		Node *sibling = parent->get_child(i);

		if (p_list.find(sibling)) {
			tail = sibling;
		} else {
			break;
		}
	}

	return tail;
}

void SceneTreeDock::_do_create(Node *p_parent) {
	Variant c = create_dialog->instance_selected();

	ERR_FAIL_COND(!c);
	Node *child = Object::cast_to<Node>(c);
	ERR_FAIL_COND(!child);

	editor_data->get_undo_redo().create_action(TTR("Create Node"));

	if (edited_scene) {
		editor_data->get_undo_redo().add_do_method(p_parent, "add_child", child);
		editor_data->get_undo_redo().add_do_method(child, "set_owner", edited_scene);
		editor_data->get_undo_redo().add_do_method(editor_selection, "clear");
		editor_data->get_undo_redo().add_do_method(editor_selection, "add_node", child);
		editor_data->get_undo_redo().add_do_reference(child);
		editor_data->get_undo_redo().add_undo_method(p_parent, "remove_child", child);

		String new_name = p_parent->validate_child_name(child);
		ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();
		editor_data->get_undo_redo().add_do_method(sed, "live_debug_create_node", edited_scene->get_path_to(p_parent), child->get_class(), new_name);
		editor_data->get_undo_redo().add_undo_method(sed, "live_debug_remove_node", NodePath(String(edited_scene->get_path_to(p_parent)).plus_file(new_name)));

	} else {
		editor_data->get_undo_redo().add_do_method(editor, "set_edited_scene", child);
		editor_data->get_undo_redo().add_do_method(scene_tree, "update_tree");
		editor_data->get_undo_redo().add_do_reference(child);
		editor_data->get_undo_redo().add_undo_method(editor, "set_edited_scene", (Object *)nullptr);
	}

	editor_data->get_undo_redo().commit_action();
	_push_item(c);
	editor_selection->clear();
	editor_selection->add_node(child);
	if (Object::cast_to<Control>(c)) {
		//make editor more comfortable, so some controls don't appear super shrunk
		Control *ct = Object::cast_to<Control>(c);

		Size2 ms = ct->get_minimum_size();
		if (ms.width < 4) {
			ms.width = 40;
		}
		if (ms.height < 4) {
			ms.height = 40;
		}
		ct->set_size(ms);
	}

	emit_signal("node_created", c);
}

void SceneTreeDock::_create() {
	if (current_option == TOOL_NEW) {
		Node *parent = nullptr;

		if (edited_scene) {
			// If root exists in edited scene
			parent = scene_tree->get_selected();
			if (!parent) {
				parent = edited_scene;
			}

		} else {
			// If no root exist in edited scene
			parent = scene_root;
			ERR_FAIL_COND(!parent);
		}

		_do_create(parent);

	} else if (current_option == TOOL_REPLACE) {
		List<Node *> selection = editor_selection->get_selected_node_list();
		ERR_FAIL_COND(selection.size() <= 0);

		UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change type of node(s)"));

		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			Node *n = E->get();
			ERR_FAIL_COND(!n);

			Variant c = create_dialog->instance_selected();

			ERR_FAIL_COND(!c);
			Node *newnode = Object::cast_to<Node>(c);
			ERR_FAIL_COND(!newnode);

			ur->add_do_method(this, "replace_node", n, newnode, true, false);
			ur->add_do_reference(newnode);
			ur->add_undo_method(this, "replace_node", newnode, n, false, false);
			ur->add_undo_reference(n);
		}

		ur->commit_action();
	} else if (current_option == TOOL_REPARENT_TO_NEW_NODE) {
		List<Node *> selection = editor_selection->get_selected_node_list();
		ERR_FAIL_COND(selection.size() <= 0);

		// Find top level node in selection
		bool only_one_top_node = true;

		Node *first = selection.front()->get();
		ERR_FAIL_COND(!first);
		int smaller_path_to_top = first->get_path_to(scene_root).get_name_count();
		Node *top_node = first;

		for (List<Node *>::Element *E = selection.front()->next(); E; E = E->next()) {
			Node *n = E->get();
			ERR_FAIL_COND(!n);

			int path_length = n->get_path_to(scene_root).get_name_count();

			if (top_node != n) {
				if (smaller_path_to_top > path_length) {
					top_node = n;
					smaller_path_to_top = path_length;
					only_one_top_node = true;
				} else if (smaller_path_to_top == path_length) {
					if (only_one_top_node && top_node->get_parent() != n->get_parent()) {
						only_one_top_node = false;
					}
				}
			}
		}

		Node *parent = nullptr;
		if (only_one_top_node) {
			parent = top_node->get_parent();
		} else {
			parent = top_node->get_parent()->get_parent();
		}

		_do_create(parent);

		Vector<Node *> nodes;
		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			nodes.push_back(E->get());
		}

		// This works because editor_selection was cleared and populated with last created node in _do_create()
		Node *last_created = editor_selection->get_selected_node_list().front()->get();
		_do_reparent(last_created, -1, nodes, true);
	}

	scene_tree->get_scene_tree()->call_deferred("grab_focus");
}

void SceneTreeDock::replace_node(Node *p_node, Node *p_by_node, bool p_keep_properties, bool p_remove_old) {
	Node *n = p_node;
	Node *newnode = p_by_node;

	if (p_keep_properties) {
		Node *default_oldnode = Object::cast_to<Node>(ClassDB::instance(n->get_class()));
		List<PropertyInfo> pinfo;
		n->get_property_list(&pinfo);

		for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
			if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}

			if (E->get().name == "__meta__") {
				Dictionary metadata = n->get(E->get().name);
				if (metadata.has("_editor_description_")) {
					newnode->set_meta("_editor_description_", metadata["_editor_description_"]);
				}

				if (Object::cast_to<CanvasItem>(newnode) || Object::cast_to<Spatial>(newnode)) {
					if (metadata.has("_edit_group_") && metadata["_edit_group_"]) {
						newnode->set_meta("_edit_group_", true);
					}
					if (metadata.has("_edit_lock_") && metadata["_edit_lock_"]) {
						newnode->set_meta("_edit_lock_", true);
					}
				}

				continue;
			}

			if (default_oldnode->get(E->get().name) != n->get(E->get().name)) {
				newnode->set(E->get().name, n->get(E->get().name));
			}
		}

		memdelete(default_oldnode);
	}

	_push_item(nullptr);

	//reconnect signals
	List<MethodInfo> sl;

	n->get_signal_list(&sl);
	for (List<MethodInfo>::Element *E = sl.front(); E; E = E->next()) {
		List<Object::Connection> cl;
		n->get_signal_connection_list(E->get().name, &cl);

		for (List<Object::Connection>::Element *F = cl.front(); F; F = F->next()) {
			Object::Connection &c = F->get();
			if (!(c.flags & Object::CONNECT_PERSIST)) {
				continue;
			}
			newnode->connect(c.signal, c.target, c.method, c.binds, Object::CONNECT_PERSIST);
		}
	}

	String newname = n->get_name();

	List<Node *> to_erase;
	for (int i = 0; i < n->get_child_count(); i++) {
		if (n->get_child(i)->get_owner() == nullptr && n->is_owned_by_parent()) {
			to_erase.push_back(n->get_child(i));
		}
	}
	n->replace_by(newnode, true);

	if (n == edited_scene) {
		edited_scene = newnode;
		editor->set_edited_scene(newnode);
	}

	//small hack to make collisionshapes and other kind of nodes to work
	for (int i = 0; i < newnode->get_child_count(); i++) {
		Node *c = newnode->get_child(i);
		c->call("set_transform", c->call("get_transform"));
	}
	//p_remove_old was added to support undo
	if (p_remove_old) {
		editor_data->get_undo_redo().clear_history();
	}
	newnode->set_name(newname);

	_push_item(newnode);

	if (p_remove_old) {
		memdelete(n);

		while (to_erase.front()) {
			memdelete(to_erase.front()->get());
			to_erase.pop_front();
		}
	}
}

void SceneTreeDock::set_edited_scene(Node *p_scene) {
	edited_scene = p_scene;
}

void SceneTreeDock::set_selected(Node *p_node, bool p_emit_selected) {
	scene_tree->set_selected(p_node, p_emit_selected);
}

void SceneTreeDock::import_subscene() {
	import_subscene_dialog->popup_centered_clamped(Size2(500, 800) * EDSCALE, 0.8);
}

void SceneTreeDock::_import_subscene() {
	Node *parent = scene_tree->get_selected();
	if (!parent) {
		parent = editor_data->get_edited_scene_root();
		ERR_FAIL_COND(!parent);
	}

	import_subscene_dialog->move(parent, edited_scene);
	editor_data->get_undo_redo().clear_history(); //no undo for now..
}

void SceneTreeDock::_new_scene_from(String p_file) {
	List<Node *> selection = editor_selection->get_selected_node_list();

	if (selection.size() != 1) {
		accept->set_text(TTR("This operation requires a single selected node."));
		accept->popup_centered_minsize();
		return;
	}

	if (EditorNode::get_singleton()->is_scene_open(p_file)) {
		accept->set_text(TTR("Can't overwrite scene that is still open!"));
		accept->popup_centered_minsize();
		return;
	}

	Node *base = selection.front()->get();

	Map<Node *, Node *> reown;
	reown[editor_data->get_edited_scene_root()] = base;
	Node *copy = base->duplicate_and_reown(reown);
	if (copy) {
		Ref<PackedScene> sdata = memnew(PackedScene);
		Error err = sdata->pack(copy);
		memdelete(copy);

		if (err != OK) {
			accept->set_text(TTR("Couldn't save new scene. Likely dependencies (instances) couldn't be satisfied."));
			accept->popup_centered_minsize();
			return;
		}

		int flg = 0;
		if (EditorSettings::get_singleton()->get("filesystem/on_save/compress_binary_resources")) {
			flg |= ResourceSaver::FLAG_COMPRESS;
		}

		err = ResourceSaver::save(p_file, sdata, flg);
		if (err != OK) {
			accept->set_text(TTR("Error saving scene."));
			accept->popup_centered_minsize();
			return;
		}
		_replace_with_branch_scene(p_file, base);
	} else {
		accept->set_text(TTR("Error duplicating scene to save it."));
		accept->popup_centered_minsize();
		return;
	}
}

static bool _is_node_visible(Node *p_node) {
	if (!p_node->get_owner()) {
		return false;
	}
	if (p_node->get_owner() != EditorNode::get_singleton()->get_edited_scene() && !EditorNode::get_singleton()->get_edited_scene()->is_editable_instance(p_node->get_owner())) {
		return false;
	}

	return true;
}

static bool _has_visible_children(Node *p_node) {
	bool collapsed = p_node->is_displayed_folded();
	if (collapsed) {
		return false;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (!_is_node_visible(child)) {
			continue;
		}

		return true;
	}

	return false;
}

void SceneTreeDock::_normalize_drop(Node *&to_node, int &to_pos, int p_type) {
	to_pos = -1;

	if (p_type == -1) {
		//drop at above selected node
		if (to_node == EditorNode::get_singleton()->get_edited_scene()) {
			to_node = nullptr;
			ERR_FAIL_MSG("Cannot perform drop above the root node!");
		}

		to_pos = to_node->get_index();
		to_node = to_node->get_parent();

	} else if (p_type == 1) {
		//drop at below selected node
		if (to_node == EditorNode::get_singleton()->get_edited_scene()) {
			//if at lower sibling of root node
			to_pos = 0; //just insert at beginning of root node
			return;
		}

		Node *lower_sibling = nullptr;

		if (_has_visible_children(to_node)) {
			to_pos = 0;
		} else {
			for (int i = to_node->get_index() + 1; i < to_node->get_parent()->get_child_count(); i++) {
				Node *c = to_node->get_parent()->get_child(i);
				if (_is_node_visible(c)) {
					lower_sibling = c;
					break;
				}
			}
			if (lower_sibling) {
				to_pos = lower_sibling->get_index();
			}

			to_node = to_node->get_parent();
		}
	}
}

void SceneTreeDock::_files_dropped(Vector<String> p_files, NodePath p_to, int p_type) {
	Node *node = get_node(p_to);
	ERR_FAIL_COND(!node);

	if (scene_tree->get_scene_tree()->get_drop_mode_flags() & Tree::DROP_MODE_INBETWEEN) {
		// Dropped PackedScene, instance it.
		int to_pos = -1;
		_normalize_drop(node, to_pos, p_type);
		_perform_instance_scenes(p_files, node, to_pos);
	} else {
		String res_path = p_files[0];
		StringName res_type = EditorFileSystem::get_singleton()->get_file_type(res_path);
		List<String> valid_properties;

		List<PropertyInfo> pinfo;
		node->get_property_list(&pinfo);

		for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
			PropertyInfo &p = E->get();

			if (!(p.usage & PROPERTY_USAGE_EDITOR) || !(p.usage & PROPERTY_USAGE_STORAGE) || p.hint != PROPERTY_HINT_RESOURCE_TYPE) {
				continue;
			}
			Vector<String> valid_types = p.hint_string.split(",");

			for (int i = 0; i < valid_types.size(); i++) {
				String prop_type = valid_types[i];
				if (res_type == prop_type || ClassDB::is_parent_class(res_type, prop_type) || EditorNode::get_editor_data().script_class_is_parent(res_type, prop_type)) {
					valid_properties.push_back(p.name);
					break;
				}
			}
		}

		if (valid_properties.size() > 1) {
			property_drop_node = node;
			resource_drop_path = res_path;

			const EditorPropertyNameProcessor::Style style = EditorNode::get_singleton()->get_inspector_dock()->get_property_name_style();
			menu_properties->clear();
			for (List<String>::Element *E = valid_properties.front(); E; E = E->next()) {
				String &p = E->get();
				menu_properties->add_item(EditorPropertyNameProcessor::get_singleton()->process_name(p, style));
				menu_properties->set_item_metadata(menu_properties->get_item_count() - 1, p);
			}

			menu_properties->set_size(Size2(1, 1));
			menu_properties->set_position(get_global_mouse_position());
			menu_properties->popup();
		} else if (!valid_properties.empty()) {
			_perform_property_drop(node, valid_properties[0], ResourceLoader::load(res_path));
		}
	}
}

void SceneTreeDock::_script_dropped(String p_file, NodePath p_to) {
	Ref<Script> scr = ResourceLoader::load(p_file);
	ERR_FAIL_COND(!scr.is_valid());
	Node *n = get_node(p_to);
	if (n) {
		InspectorDock *inspector_dock = EditorNode::get_singleton()->get_inspector_dock();
		UndoRedo &undo_redo = editor_data->get_undo_redo();

		undo_redo.create_action(TTR("Attach Script"));
		undo_redo.add_do_method(inspector_dock, "store_script_properties", n);
		undo_redo.add_undo_method(inspector_dock, "store_script_properties", n);
		undo_redo.add_do_method(n, "set_script", scr);
		undo_redo.add_undo_method(n, "set_script", n->get_script());
		undo_redo.add_do_method(inspector_dock, "apply_script_properties", n);
		undo_redo.add_undo_method(inspector_dock, "apply_script_properties", n);
		undo_redo.add_do_method(this, "_update_script_button");
		undo_redo.add_undo_method(this, "_update_script_button");
		undo_redo.commit_action();
	}
}

void SceneTreeDock::_nodes_dragged(Array p_nodes, NodePath p_to, int p_type) {
	List<Node *> selection = editor_selection->get_selected_node_list();

	if (selection.empty()) {
		return; //nothing to reparent
	}

	Node *to_node = get_node(p_to);
	if (!to_node) {
		return;
	}

	Vector<Node *> nodes;
	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		nodes.push_back(E->get());
	}

	int to_pos = -1;

	_normalize_drop(to_node, to_pos, p_type);
	_do_reparent(to_node, to_pos, nodes, !Input::get_singleton()->is_key_pressed(KEY_SHIFT));
}

void SceneTreeDock::_add_children_to_popup(Object *p_obj, int p_depth) {
	if (p_depth > 8) {
		return;
	}

	List<PropertyInfo> pinfo;
	p_obj->get_property_list(&pinfo);
	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_EDITOR)) {
			continue;
		}
		if (E->get().hint != PROPERTY_HINT_RESOURCE_TYPE) {
			continue;
		}

		Variant value = p_obj->get(E->get().name);
		if (value.get_type() != Variant::OBJECT) {
			continue;
		}
		Object *obj = value;
		if (!obj) {
			continue;
		}

		Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(obj);

		if (menu->get_item_count() == 0) {
			menu->add_submenu_item(TTR("Sub-Resources"), "Sub-Resources");
		}
		int index = menu_subresources->get_item_count();
		menu_subresources->add_icon_item(icon, E->get().name.capitalize(), EDIT_SUBRESOURCE_BASE + subresources.size());
		menu_subresources->set_item_h_offset(index, p_depth * 10 * EDSCALE);
		subresources.push_back(obj->get_instance_id());

		_add_children_to_popup(obj, p_depth + 1);
	}
}

void SceneTreeDock::_tree_rmb(const Vector2 &p_menu_pos) {
	if (!EditorNode::get_singleton()->get_edited_scene()) {
		menu->clear();
		if (profile_allow_editing) {
			menu->add_icon_shortcut(get_icon("Add", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/add_child_node"), TOOL_NEW);
			menu->add_icon_shortcut(get_icon("Instance", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/instance_scene"), TOOL_INSTANCE);
		}

		menu->set_size(Size2(1, 1));
		menu->set_position(p_menu_pos);
		menu->popup();
		return;
	}

	List<Node *> selection = editor_selection->get_selected_node_list();
	List<Node *> full_selection = editor_selection->get_full_selected_node_list(); // Above method only returns nodes with common parent.

	if (selection.size() == 0) {
		return;
	}

	menu->clear();

	Ref<Script> existing_script;
	bool exisiting_script_removable = true;
	if (selection.size() == 1) {
		Node *selected = selection[0];

		if (profile_allow_editing) {
			subresources.clear();
			menu_subresources->clear();
			menu_subresources->set_size(Size2(1, 1));
			_add_children_to_popup(selection.front()->get(), 0);
			if (menu->get_item_count() > 0) {
				menu->add_separator();
			}

			menu->add_icon_shortcut(get_icon("Add", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/add_child_node"), TOOL_NEW);
			menu->add_icon_shortcut(get_icon("Instance", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/instance_scene"), TOOL_INSTANCE);
		}
		menu->add_icon_shortcut(get_icon("Collapse", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/expand_collapse_all"), TOOL_EXPAND_COLLAPSE);
		menu->add_separator();

		existing_script = selected->get_script();

		if (EditorNode::get_singleton()->get_object_custom_type_base(selected) == existing_script) {
			exisiting_script_removable = false;
		}
	}

	if (profile_allow_editing) {
		menu->add_shortcut(ED_GET_SHORTCUT("scene_tree/cut_node"), TOOL_CUT);
		menu->add_shortcut(ED_GET_SHORTCUT("scene_tree/copy_node"), TOOL_COPY);
		if (selection.size() == 1 && !node_clipboard.empty()) {
			menu->add_shortcut(ED_GET_SHORTCUT("scene_tree/paste_node"), TOOL_PASTE);
		}
		menu->add_separator();
	}

	if (profile_allow_script_editing) {
		bool add_separator = false;

		if (full_selection.size() == 1) {
			add_separator = true;
			menu->add_icon_shortcut(get_icon("ScriptCreate", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/attach_script"), TOOL_ATTACH_SCRIPT);
			if (existing_script.is_valid()) {
				menu->add_icon_shortcut(get_icon("ScriptExtend", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/extend_script"), TOOL_EXTEND_SCRIPT);
			}
		}
		if (existing_script.is_valid() && exisiting_script_removable) {
			add_separator = true;
			menu->add_icon_shortcut(get_icon("ScriptRemove", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/detach_script"), TOOL_DETACH_SCRIPT);
		} else if (full_selection.size() > 1) {
			bool script_exists = false;
			for (List<Node *>::Element *E = full_selection.front(); E; E = E->next()) {
				if (!E->get()->get_script().is_null()) {
					script_exists = true;
					break;
				}
			}

			if (script_exists) {
				add_separator = true;
				menu->add_icon_shortcut(get_icon("ScriptRemove", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/detach_script"), TOOL_DETACH_SCRIPT);
			}
		}

		if (add_separator && profile_allow_editing) {
			menu->add_separator();
		}
	}

	if (profile_allow_editing) {
		// Allow multi-toggling scene unique names but only if all selected nodes are owned by the edited scene root.
		bool all_owned = true;
		for (List<Node *>::Element *e = full_selection.front(); e; e = e->next()) {
			Node *node = e->get();
			if (node->get_owner() != EditorNode::get_singleton()->get_edited_scene()) {
				all_owned = false;
				break;
			}
		}
		if (all_owned) {
			Node *node = full_selection[0];
			menu->add_icon_shortcut(get_icon("SceneUniqueName", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/toggle_unique_name"), TOOL_TOGGLE_SCENE_UNIQUE_NAME);
			menu->set_item_text(menu->get_item_index(TOOL_TOGGLE_SCENE_UNIQUE_NAME), node->is_unique_name_in_owner() ? TTR("Revoke Unique Name") : TTR("Access as Unique Name"));
		}
	}

	if (profile_allow_editing) {
		bool add_separator = false;

		if (full_selection.size() == 1) {
			add_separator = true;
			menu->add_icon_shortcut(get_icon("Rename", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/rename"), TOOL_RENAME);
		}

		bool can_replace = true;
		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			if (E->get() != edited_scene && (E->get()->get_owner() != edited_scene || E->get()->get_filename() != "")) {
				can_replace = false;
				break;
			}
		}

		if (can_replace) {
			add_separator = true;
			menu->add_icon_shortcut(get_icon("Reload", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/change_node_type"), TOOL_REPLACE);
		}

		if (scene_tree->get_selected() != edited_scene) {
			if (add_separator) {
				menu->add_separator();
			}
			menu->add_icon_shortcut(get_icon("MoveUp", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/move_up"), TOOL_MOVE_UP);
			menu->add_icon_shortcut(get_icon("MoveDown", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/move_down"), TOOL_MOVE_DOWN);
			menu->add_icon_shortcut(get_icon("Duplicate", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/duplicate"), TOOL_DUPLICATE);
			menu->add_icon_shortcut(get_icon("Reparent", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/reparent"), TOOL_REPARENT);
			menu->add_icon_shortcut(get_icon("ReparentToNewNode", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/reparent_to_new_node"), TOOL_REPARENT_TO_NEW_NODE);
			if (selection.size() == 1) {
				menu->add_icon_shortcut(get_icon("NewRoot", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/make_root"), TOOL_MAKE_ROOT);
			}
		}
	}
	if (selection.size() == 1) {
		if (profile_allow_editing) {
			menu->add_separator();
			menu->add_icon_shortcut(get_icon("Blend", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/merge_from_scene"), TOOL_MERGE_FROM_SCENE);
			menu->add_icon_shortcut(get_icon("CreateNewSceneFrom", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/save_branch_as_scene"), TOOL_NEW_SCENE_FROM);
		}
		if (full_selection.size() == 1) {
			menu->add_separator();
			menu->add_icon_shortcut(get_icon("CopyNodePath", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/copy_node_path"), TOOL_COPY_NODE_PATH);
		}

		bool is_external = (selection[0]->get_filename() != "");
		if (is_external) {
			bool is_inherited = selection[0]->get_scene_inherited_state() != nullptr;
			bool is_top_level = selection[0]->get_owner() == nullptr;
			if (is_inherited && is_top_level) {
				menu->add_separator();
				if (profile_allow_editing) {
					menu->add_item(TTR("Clear Inheritance"), TOOL_SCENE_CLEAR_INHERITANCE);
				}
				menu->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open in Editor"), TOOL_SCENE_OPEN_INHERITED);
			} else if (!is_top_level) {
				menu->add_separator();
				bool editable = EditorNode::get_singleton()->get_edited_scene()->is_editable_instance(selection[0]);
				bool placeholder = selection[0]->get_scene_instance_load_placeholder();
				if (profile_allow_editing) {
					menu->add_check_item(TTR("Editable Children"), TOOL_SCENE_EDITABLE_CHILDREN);
					menu->add_check_item(TTR("Load As Placeholder"), TOOL_SCENE_USE_PLACEHOLDER);
					menu->add_item(TTR("Make Local"), TOOL_SCENE_MAKE_LOCAL);
				}
				menu->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open in Editor"), TOOL_SCENE_OPEN);
				if (profile_allow_editing) {
					menu->set_item_checked(menu->get_item_idx_from_text(TTR("Editable Children")), editable);
					menu->set_item_checked(menu->get_item_idx_from_text(TTR("Load As Placeholder")), placeholder);
				}
			}
		}
	}

#ifdef MODULE_REGEX_ENABLED
	if (profile_allow_editing && selection.size() > 1) {
		//this is not a commonly used action, it makes no sense for it to be where it was nor always present.
		menu->add_separator();
		menu->add_icon_shortcut(get_icon("Rename", "EditorIcons"), ED_GET_SHORTCUT("scene_tree/batch_rename"), TOOL_BATCH_RENAME);
	}
#endif // MODULE_REGEX_ENABLED
	menu->add_separator();
	menu->add_icon_item(get_icon("Help", "EditorIcons"), TTR("Open Documentation"), TOOL_OPEN_DOCUMENTATION);

	if (profile_allow_editing) {
		menu->add_separator();
		menu->add_icon_shortcut(get_icon("Remove", "EditorIcons"), ED_SHORTCUT("scene_tree/delete", TTR("Delete Node(s)"), KEY_DELETE), TOOL_ERASE);
	}
	menu->set_size(Size2(1, 1));
	menu->set_position(p_menu_pos);
	menu->popup();

	// Add context menu plugins.
	Vector<String> paths;
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		String node_path = root->get_path().rel_path_to(E->get()->get_path());
		paths.push_back(node_path);
	}
	EditorContextMenu::handle_plugins(EditorContextMenu::CONTEXT_SLOT_SCENE_TREE, paths, menu);
}

void SceneTreeDock::_update_filter_menu() {
	_append_filter_options_to(filter->get_menu());
}

void SceneTreeDock::_filter_changed(const String &p_filter) {
	scene_tree->set_filter(p_filter);

	String warning = scene_tree->get_filter_term_warning();
	if (!warning.empty()) {
		filter->add_icon_override("clear", get_icon("NodeWarning", "EditorIcons"));
		filter->set_tooltip(warning);
	} else {
		filter->remove_icon_override("clear");
		filter->set_tooltip("");
	}
}

void SceneTreeDock::_filter_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_null()) {
		return;
	}

	if (mb->is_pressed() && mb->get_button_index() == BUTTON_MIDDLE) {
		filter_quick_menu->clear();

		_append_filter_options_to(filter_quick_menu, false);
		filter_quick_menu->set_position(get_global_position() + get_local_mouse_position());
		filter_quick_menu->popup();
		filter_quick_menu->grab_focus();
		accept_event();
	}
}

void SceneTreeDock::_filter_option_selected(int p_option) {
	String filter_parameter;
	switch (p_option) {
		case FILTER_BY_TYPE: {
			filter_parameter = "type";
		} break;
		case FILTER_BY_GROUP: {
			filter_parameter = "group";
		} break;
	}

	if (!filter_parameter.empty()) {
		set_filter((get_filter() + " " + filter_parameter + ":").strip_edges());
		filter->set_cursor_position(filter->get_text().length());
		filter->grab_focus();
	}
}

void SceneTreeDock::_append_filter_options_to(PopupMenu *p_menu, bool p_include_separator) {
	if (p_include_separator) {
		p_menu->add_separator();
	}

	p_menu->add_item(TTR("Filter by Type"), FILTER_BY_TYPE);
	p_menu->add_item(TTR("Filter by Group"), FILTER_BY_GROUP);
	p_menu->set_item_icon(p_menu->get_item_index(FILTER_BY_TYPE), get_icon("Node", "EditorIcons"));
	p_menu->set_item_icon(p_menu->get_item_index(FILTER_BY_GROUP), get_icon("Groups", "EditorIcons"));
	p_menu->set_item_tooltip(p_menu->get_item_index(FILTER_BY_TYPE), TTR("Selects all Nodes of the given type."));
	p_menu->set_item_tooltip(p_menu->get_item_index(FILTER_BY_GROUP), TTR("Selects all Nodes belonging to the given group.\nIf empty, selects any Node belonging to any group."));
}

String SceneTreeDock::get_filter() {
	return filter->get_text();
}

void SceneTreeDock::set_filter(const String &p_filter) {
	filter->set_text(p_filter);
	scene_tree->set_filter(p_filter);
}

void SceneTreeDock::save_branch_to_file(String p_directory) {
	new_scene_from_dialog->set_current_dir(p_directory);
	_tool_selected(TOOL_NEW_SCENE_FROM);
}

void SceneTreeDock::_focus_node() {
	Node *node = scene_tree->get_selected();
	ERR_FAIL_COND(!node);

	if (node->is_class("CanvasItem")) {
		CanvasItemEditorPlugin *editor = Object::cast_to<CanvasItemEditorPlugin>(editor_data->get_editor("2D"));
		editor->get_canvas_item_editor()->focus_selection();
	} else {
		SpatialEditorPlugin *editor = Object::cast_to<SpatialEditorPlugin>(editor_data->get_editor("3D"));
		editor->get_spatial_editor()->get_editor_viewport(0)->focus_selection();
	}
}

void SceneTreeDock::attach_script_to_selected(bool p_extend) {
	if (ScriptServer::get_language_count() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("Cannot attach a script: there are no languages registered.\nThis is probably because this editor was built with all language modules disabled."));
		return;
	}

	if (!profile_allow_script_editing) {
		return;
	}

	List<Node *> selection = editor_selection->get_selected_node_list();
	if (selection.empty()) {
		return;
	}

	Node *selected = scene_tree->get_selected();
	if (!selected) {
		selected = selection.front()->get();
	}

	Ref<Script> existing = selected->get_script();

	String path = selected->get_filename();
	if (path == "") {
		String root_path = editor_data->get_edited_scene_root()->get_filename();
		if (root_path == "") {
			path = String("res://").plus_file(selected->get_name());
		} else {
			path = root_path.get_base_dir().plus_file(selected->get_name());
		}
	}

	String inherits = selected->get_class();

	if (p_extend && existing.is_valid()) {
		for (int i = 0; i < ScriptServer::get_language_count(); i++) {
			ScriptLanguage *l = ScriptServer::get_language(i);
			if (l->get_type() == existing->get_class()) {
				String name = l->get_global_class_name(existing->get_path());
				if (ScriptServer::is_global_class(name) && EDITOR_GET("interface/editors/derive_script_globals_by_name").operator bool()) {
					inherits = name;
				} else if (l->can_inherit_from_file()) {
					inherits = "\"" + existing->get_path() + "\"";
				}
				break;
			}
		}
	}

	script_create_dialog->connect("script_created", this, "_script_created");
	script_create_dialog->connect("popup_hide", this, "_script_creation_closed", varray(), CONNECT_ONESHOT);
	script_create_dialog->set_inheritance_base_type("Node");
	script_create_dialog->config(inherits, path);
	script_create_dialog->popup_centered();
}

void SceneTreeDock::open_script_dialog(Node *p_for_node, bool p_extend) {
	scene_tree->set_selected(p_for_node, false);

	if (p_extend) {
		_tool_selected(TOOL_EXTEND_SCRIPT);
	} else {
		_tool_selected(TOOL_ATTACH_SCRIPT);
	}
}

void SceneTreeDock::open_add_child_dialog() {
	create_dialog->set_base_type("CanvasItem");
	_tool_selected(TOOL_NEW, true);
	reset_create_dialog = true;
}

void SceneTreeDock::open_instance_child_dialog() {
	_tool_selected(TOOL_INSTANCE, true);
}

void SceneTreeDock::add_remote_tree_editor(Control *p_remote) {
	ERR_FAIL_COND(remote_tree != nullptr);
	add_child(p_remote);
	remote_tree = p_remote;
	remote_tree->hide();
}

void SceneTreeDock::show_remote_tree() {
	_remote_tree_selected();
}

void SceneTreeDock::hide_remote_tree() {
	_local_tree_selected();
}

void SceneTreeDock::show_tab_buttons() {
	button_hb->show();
}

void SceneTreeDock::hide_tab_buttons() {
	button_hb->hide();
}

void SceneTreeDock::_remote_tree_selected() {
	scene_tree->hide();
	create_root_dialog->hide();
	if (remote_tree) {
		remote_tree->show();
	}
	edit_remote->set_pressed(true);
	edit_local->set_pressed(false);

	emit_signal("remote_tree_selected");
}

void SceneTreeDock::_local_tree_selected() {
	if (!bool(EDITOR_GET("interface/editors/show_scene_tree_root_selection")) || get_tree()->get_edited_scene_root() != nullptr) {
		scene_tree->show();
	}
	if (remote_tree) {
		remote_tree->hide();
	}
	edit_remote->set_pressed(false);
	edit_local->set_pressed(true);
}

void SceneTreeDock::_update_create_root_dialog() {
	BaseButton *toggle = Object::cast_to<BaseButton>(create_root_dialog->get_node(String("NodeShortcutsTopRow/NodeShortcutsToggle")));
	Node *node_shortcuts = create_root_dialog->get_node(String("NodeShortcutsScrollContainer/NodeShortcuts"));

	if (!toggle || !node_shortcuts) {
		return;
	}

	Control *beginner_nodes = Object::cast_to<Control>(node_shortcuts->get_node(String("BeginnerNodeShortcuts")));
	Control *favorite_nodes = Object::cast_to<Control>(node_shortcuts->get_node(String("FavoriteNodeShortcuts")));

	if (!beginner_nodes || !favorite_nodes) {
		return;
	}

	EditorSettings::get_singleton()->set_setting("_use_favorites_root_selection", toggle->is_pressed());
	EditorSettings::get_singleton()->save();
	if (toggle->is_pressed()) {
		for (int i = 0; i < favorite_nodes->get_child_count(); i++) {
			favorite_nodes->get_child(i)->queue_delete();
		}

		FileAccess *f = FileAccess::open(EditorSettings::get_singleton()->get_project_settings_dir().plus_file("favorites.Node"), FileAccess::READ);

		if (f) {
			while (!f->eof_reached()) {
				String l = f->get_line().strip_edges();

				if (l != String()) {
					Button *button = memnew(Button);
					favorite_nodes->add_child(button);
					button->set_text(TTR(l));
					button->set_clip_text(true);
					String name = l.get_slicec(' ', 0);
					if (ScriptServer::is_global_class(name)) {
						name = ScriptServer::get_global_class_native_base(name);
					}
					button->set_icon(EditorNode::get_singleton()->get_class_icon(name));
					button->connect("pressed", this, "_favorite_root_selected", make_binds(l));
				}
			}

			memdelete(f);
		}

		if (!favorite_nodes->is_visible_in_tree()) {
			favorite_nodes->show();
			beginner_nodes->hide();
		}
	} else {
		if (!beginner_nodes->is_visible_in_tree()) {
			beginner_nodes->show();
			favorite_nodes->hide();
		}
	}
}

void SceneTreeDock::_favorite_root_selected(const String &p_class) {
	selected_favorite_root = p_class;
	_tool_selected(TOOL_CREATE_FAVORITE, false);
}

void SceneTreeDock::_feature_profile_changed() {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();

	if (profile.is_valid()) {
		profile_allow_editing = !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCENE_TREE);
		profile_allow_script_editing = !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCRIPT);
		bool profile_allow_3d = !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D);

		button_3d->set_visible(profile_allow_3d);
		button_add->set_visible(profile_allow_editing);
		button_instance->set_visible(profile_allow_editing);
		scene_tree->set_can_rename(profile_allow_editing);

	} else {
		button_3d->set_visible(true);
		button_add->set_visible(true);
		button_instance->set_visible(true);
		scene_tree->set_can_rename(true);
		profile_allow_editing = true;
		profile_allow_script_editing = true;
	}

	_update_script_button();
}

void SceneTreeDock::_clear_clipboard() {
	for (List<Node *>::Element *E = node_clipboard.front(); E; E = E->next()) {
		memdelete(E->get());
	}
	node_clipboard.clear();
	clipboard_resource_remap.clear();
}

void SceneTreeDock::_create_remap_for_node(Node *p_node, Map<RES, RES> &r_remap) {
	List<PropertyInfo> props;
	p_node->get_property_list(&props);

	Vector<SceneState::PackState> states_stack;
	bool states_stack_ready = false;

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_node->get(E->get().name);
		if (v.is_ref()) {
			RES res = v;
			if (res.is_valid()) {
				if (!states_stack_ready) {
					states_stack = PropertyUtils::get_node_states_stack(p_node);
					states_stack_ready = true;
				}

				bool is_valid_default = false;
				Variant orig = PropertyUtils::get_property_default_value(p_node, E->get().name, &is_valid_default, &states_stack);
				if (is_valid_default && !PropertyUtils::is_property_value_different(v, orig)) {
					continue;
				}

				if ((res->get_path() == "" || res->get_path().find("::") > -1) && !r_remap.has(res)) {
					_create_remap_for_resource(res, r_remap);
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_create_remap_for_node(p_node->get_child(i), r_remap);
	}
}

void SceneTreeDock::_create_remap_for_resource(RES p_resource, Map<RES, RES> &r_remap) {
	r_remap[p_resource] = p_resource->duplicate();

	List<PropertyInfo> props;
	p_resource->get_property_list(&props);

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_resource->get(E->get().name);
		if (v.is_ref()) {
			RES res = v;
			if (res.is_valid()) {
				if ((res->get_path() == "" || res->get_path().find("::") > -1) && !r_remap.has(res)) {
					_create_remap_for_resource(res, r_remap);
				}
			}
		}
	}
}

void SceneTreeDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_tool_selected"), &SceneTreeDock::_tool_selected, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_create"), &SceneTreeDock::_create);
	ClassDB::bind_method(D_METHOD("_node_reparent"), &SceneTreeDock::_node_reparent);
	ClassDB::bind_method(D_METHOD("_set_owners"), &SceneTreeDock::_set_owners);
	ClassDB::bind_method(D_METHOD("_node_selected"), &SceneTreeDock::_node_selected);
	ClassDB::bind_method(D_METHOD("_node_renamed"), &SceneTreeDock::_node_renamed);
	ClassDB::bind_method(D_METHOD("_script_created"), &SceneTreeDock::_script_created);
	ClassDB::bind_method(D_METHOD("_script_creation_closed"), &SceneTreeDock::_script_creation_closed);
	ClassDB::bind_method(D_METHOD("_load_request"), &SceneTreeDock::_load_request);
	ClassDB::bind_method(D_METHOD("_script_open_request"), &SceneTreeDock::_script_open_request);
	ClassDB::bind_method(D_METHOD("_unhandled_key_input"), &SceneTreeDock::_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("_input"), &SceneTreeDock::_input);
	ClassDB::bind_method(D_METHOD("_nodes_drag_begin"), &SceneTreeDock::_nodes_drag_begin);
	ClassDB::bind_method(D_METHOD("_delete_confirm"), &SceneTreeDock::_delete_confirm);
	ClassDB::bind_method(D_METHOD("_toggle_editable_children_from_selection"), &SceneTreeDock::_toggle_editable_children_from_selection);
	ClassDB::bind_method(D_METHOD("_toggle_placeholder_from_selection"), &SceneTreeDock::_toggle_placeholder_from_selection);
	ClassDB::bind_method(D_METHOD("_node_prerenamed"), &SceneTreeDock::_node_prerenamed);
	ClassDB::bind_method(D_METHOD("_import_subscene"), &SceneTreeDock::_import_subscene);
	ClassDB::bind_method(D_METHOD("_selection_changed"), &SceneTreeDock::_selection_changed);
	ClassDB::bind_method(D_METHOD("_node_collapsed"), &SceneTreeDock::_node_collapsed);
	ClassDB::bind_method(D_METHOD("_new_scene_from"), &SceneTreeDock::_new_scene_from);
	ClassDB::bind_method(D_METHOD("_nodes_dragged"), &SceneTreeDock::_nodes_dragged);
	ClassDB::bind_method(D_METHOD("_files_dropped"), &SceneTreeDock::_files_dropped);
	ClassDB::bind_method(D_METHOD("_property_selected"), &SceneTreeDock::_property_selected);
	ClassDB::bind_method(D_METHOD("_quick_open"), &SceneTreeDock::_quick_open);
	ClassDB::bind_method(D_METHOD("_script_dropped"), &SceneTreeDock::_script_dropped);
	ClassDB::bind_method(D_METHOD("_tree_rmb"), &SceneTreeDock::_tree_rmb);
	ClassDB::bind_method(D_METHOD("_filter_changed"), &SceneTreeDock::_filter_changed);
	ClassDB::bind_method(D_METHOD("_filter_gui_input"), &SceneTreeDock::_filter_gui_input);
	ClassDB::bind_method(D_METHOD("_update_filter_menu"), &SceneTreeDock::_update_filter_menu);
	ClassDB::bind_method(D_METHOD("_filter_option_selected"), &SceneTreeDock::_filter_option_selected);
	ClassDB::bind_method(D_METHOD("_focus_node"), &SceneTreeDock::_focus_node);
	ClassDB::bind_method(D_METHOD("_remote_tree_selected"), &SceneTreeDock::_remote_tree_selected);
	ClassDB::bind_method(D_METHOD("_local_tree_selected"), &SceneTreeDock::_local_tree_selected);
	ClassDB::bind_method(D_METHOD("_update_script_button"), &SceneTreeDock::_update_script_button);
	ClassDB::bind_method(D_METHOD("_favorite_root_selected"), &SceneTreeDock::_favorite_root_selected);
	ClassDB::bind_method(D_METHOD("_update_create_root_dialog"), &SceneTreeDock::_update_create_root_dialog);
	ClassDB::bind_method(D_METHOD("_feature_profile_changed"), &SceneTreeDock::_feature_profile_changed);

	ClassDB::bind_method(D_METHOD("instance"), &SceneTreeDock::instance);
	ClassDB::bind_method(D_METHOD("get_tree_editor"), &SceneTreeDock::get_tree_editor);
	ClassDB::bind_method(D_METHOD("replace_node"), &SceneTreeDock::replace_node);

	ADD_SIGNAL(MethodInfo("remote_tree_selected"));
	ADD_SIGNAL(MethodInfo("add_node_used"));
	ADD_SIGNAL(MethodInfo("node_created", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

SceneTreeDock::SceneTreeDock(EditorNode *p_editor, Node *p_scene_root, EditorSelection *p_editor_selection, EditorData &p_editor_data) {
	set_name("Scene");
	editor = p_editor;
	edited_scene = nullptr;
	editor_data = &p_editor_data;
	editor_selection = p_editor_selection;
	scene_root = p_scene_root;
	pending_click_select = nullptr;
	tree_clicked = false;

	VBoxContainer *vbc = this;

	HBoxContainer *filter_hbc = memnew(HBoxContainer);
	filter_hbc->add_constant_override("separate", 0);

#ifdef OSX_ENABLED
	ED_SHORTCUT("scene_tree/rename", TTR("Rename"), KEY_ENTER);
	ED_SHORTCUT("scene_tree/batch_rename", TTR("Batch Rename"), KEY_MASK_SHIFT | KEY_ENTER);
#else
	ED_SHORTCUT("scene_tree/rename", TTR("Rename"), KEY_F2);
	ED_SHORTCUT("scene_tree/batch_rename", TTR("Batch Rename"), KEY_MASK_SHIFT | KEY_F2);
#endif
	ED_SHORTCUT("scene_tree/add_child_node", TTR("Add Child Node"), KEY_MASK_CMD | KEY_A);
	ED_SHORTCUT("scene_tree/instance_scene", TTR("Instance Child Scene"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_A);
	ED_SHORTCUT("scene_tree/expand_collapse_all", TTR("Expand/Collapse All"));
	ED_SHORTCUT("scene_tree/cut_node", TTR("Cut"), KEY_MASK_CMD | KEY_X);
	ED_SHORTCUT("scene_tree/copy_node", TTR("Copy"), KEY_MASK_CMD | KEY_C);
	ED_SHORTCUT("scene_tree/paste_node", TTR("Paste"), KEY_MASK_CMD | KEY_V);
	ED_SHORTCUT("scene_tree/change_node_type", TTR("Change Type"));
	ED_SHORTCUT("scene_tree/attach_script", TTR("Attach Script"));
	ED_SHORTCUT("scene_tree/extend_script", TTR("Extend Script"));
	ED_SHORTCUT("scene_tree/detach_script", TTR("Detach Script"));
	ED_SHORTCUT("scene_tree/move_up", TTR("Move Up"), KEY_MASK_CMD | KEY_UP);
	ED_SHORTCUT("scene_tree/move_down", TTR("Move Down"), KEY_MASK_CMD | KEY_DOWN);
	ED_SHORTCUT("scene_tree/duplicate", TTR("Duplicate"), KEY_MASK_CMD | KEY_D);
	ED_SHORTCUT("scene_tree/reparent", TTR("Reparent"));
	ED_SHORTCUT("scene_tree/reparent_to_new_node", TTR("Reparent to New Node"));
	ED_SHORTCUT("scene_tree/make_root", TTR("Make Scene Root"));
	ED_SHORTCUT("scene_tree/merge_from_scene", TTR("Merge From Scene"));
	ED_SHORTCUT("scene_tree/save_branch_as_scene", TTR("Save Branch as Scene"));
	ED_SHORTCUT("scene_tree/copy_node_path", TTR("Copy Node Path"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_C);
	ED_SHORTCUT("scene_tree/toggle_unique_name", TTR("Toggle Access as Unique Name"));
	ED_SHORTCUT("scene_tree/delete_no_confirm", TTR("Delete (No Confirm)"), KEY_MASK_SHIFT | KEY_DELETE);
	ED_SHORTCUT("scene_tree/delete", TTR("Delete"), KEY_DELETE);

	button_add = memnew(ToolButton);
	button_add->connect("pressed", this, "_tool_selected", make_binds(TOOL_NEW, false));
	button_add->set_tooltip(TTR("Add/Create a New Node."));
	button_add->set_shortcut(ED_GET_SHORTCUT("scene_tree/add_child_node"));
	filter_hbc->add_child(button_add);

	button_instance = memnew(ToolButton);
	button_instance->connect("pressed", this, "_tool_selected", make_binds(TOOL_INSTANCE, false));
	button_instance->set_tooltip(TTR("Instance a scene file as a Node. Creates an inherited scene if no root node exists."));
	button_instance->set_shortcut(ED_GET_SHORTCUT("scene_tree/instance_scene"));
	filter_hbc->add_child(button_instance);
	vbc->add_child(filter_hbc);

	// The "Filter Nodes" text input above the Scene Tree Editor.
	filter = memnew(LineEdit);
	filter->set_h_size_flags(SIZE_EXPAND_FILL);
	filter->set_placeholder(TTR("Filter nodes"));
	filter_hbc->add_child(filter);
	filter->add_constant_override("minimum_spaces", 0);
	filter->connect("text_changed", this, "_filter_changed");
	filter->connect("gui_input", this, "_filter_gui_input");
	filter->get_menu()->connect("id_pressed", this, "_filter_option_selected");
	call_deferred("_update_filter_menu");

	filter_quick_menu = memnew(PopupMenu);
	filter_quick_menu->connect("id_pressed", this, "_filter_option_selected");
	filter->add_child(filter_quick_menu);

	button_create_script = memnew(ToolButton);
	button_create_script->connect("pressed", this, "_tool_selected", make_binds(TOOL_ATTACH_SCRIPT, false));
	button_create_script->set_tooltip(TTR("Attach a new or existing script to the selected node."));
	button_create_script->set_shortcut(ED_GET_SHORTCUT("scene_tree/attach_script"));
	filter_hbc->add_child(button_create_script);
	button_create_script->hide();

	button_detach_script = memnew(ToolButton);
	button_detach_script->connect("pressed", this, "_tool_selected", make_binds(TOOL_DETACH_SCRIPT, false));
	button_detach_script->set_tooltip(TTR("Detach the script from the selected node."));
	button_detach_script->set_shortcut(ED_GET_SHORTCUT("scene_tree/detach_script"));
	filter_hbc->add_child(button_detach_script);
	button_detach_script->hide();

	button_hb = memnew(HBoxContainer);
	vbc->add_child(button_hb);

	edit_remote = memnew(ToolButton);
	button_hb->add_child(edit_remote);
	edit_remote->set_h_size_flags(SIZE_EXPAND_FILL);
	edit_remote->set_text(TTR("Remote"));
	edit_remote->set_toggle_mode(true);
	edit_remote->set_tooltip(TTR("If selected, the Remote scene tree dock will cause the project to stutter every time it updates.\nSwitch back to the Local scene tree dock to improve performance."));
	edit_remote->connect("pressed", this, "_remote_tree_selected");

	edit_local = memnew(ToolButton);
	button_hb->add_child(edit_local);
	edit_local->set_h_size_flags(SIZE_EXPAND_FILL);
	edit_local->set_text(TTR("Local"));
	edit_local->set_toggle_mode(true);
	edit_local->connect("pressed", this, "_local_tree_selected");

	remote_tree = nullptr;
	button_hb->hide();

	create_root_dialog = memnew(VBoxContainer);
	vbc->add_child(create_root_dialog);
	create_root_dialog->set_v_size_flags(SIZE_EXPAND_FILL);
	create_root_dialog->hide();

	scene_tree = memnew(SceneTreeEditor(false, true, true));

	vbc->add_child(scene_tree);
	scene_tree->set_v_size_flags(SIZE_EXPAND | SIZE_FILL);
	scene_tree->connect("rmb_pressed", this, "_tree_rmb");

	scene_tree->connect("node_selected", this, "_node_selected", varray(), CONNECT_DEFERRED);
	scene_tree->connect("node_renamed", this, "_node_renamed", varray(), CONNECT_DEFERRED);
	scene_tree->connect("node_prerename", this, "_node_prerenamed");
	scene_tree->connect("open", this, "_load_request");
	scene_tree->connect("open_script", this, "_script_open_request");
	scene_tree->connect("nodes_rearranged", this, "_nodes_dragged");
	scene_tree->connect("files_dropped", this, "_files_dropped");
	scene_tree->connect("script_dropped", this, "_script_dropped");
	scene_tree->connect("nodes_dragged", this, "_nodes_drag_begin");

	scene_tree->get_scene_tree()->connect("item_double_clicked", this, "_focus_node");

	scene_tree->set_undo_redo(&editor_data->get_undo_redo());
	scene_tree->set_editor_selection(editor_selection);

	create_dialog = memnew(CreateDialog);
	create_dialog->set_base_type("Node");
	add_child(create_dialog);
	create_dialog->connect("create", this, "_create");
	create_dialog->connect("favorites_updated", this, "_update_create_root_dialog");

#ifdef MODULE_REGEX_ENABLED
	rename_dialog = memnew(RenameDialog(scene_tree, &editor_data->get_undo_redo()));
	add_child(rename_dialog);
#endif // MODULE_REGEX_ENABLED

	script_create_dialog = memnew(ScriptCreateDialog);
	script_create_dialog->set_inheritance_base_type("Node");
	add_child(script_create_dialog);

	reparent_dialog = memnew(ReparentDialog);
	add_child(reparent_dialog);
	reparent_dialog->connect("reparent", this, "_node_reparent");

	accept = memnew(AcceptDialog);
	add_child(accept);

	quick_open = memnew(EditorQuickOpen);
	add_child(quick_open);
	quick_open->connect("quick_open", this, "_quick_open");
	set_process_unhandled_key_input(true);

	delete_dialog = memnew(ConfirmationDialog);
	add_child(delete_dialog);
	delete_dialog->connect("confirmed", this, "_delete_confirm", varray(false));

	editable_instance_remove_dialog = memnew(ConfirmationDialog);
	add_child(editable_instance_remove_dialog);
	editable_instance_remove_dialog->connect("confirmed", this, "_toggle_editable_children_from_selection");

	placeholder_editable_instance_remove_dialog = memnew(ConfirmationDialog);
	add_child(placeholder_editable_instance_remove_dialog);
	placeholder_editable_instance_remove_dialog->connect("confirmed", this, "_toggle_placeholder_from_selection");

	import_subscene_dialog = memnew(EditorSubScene);
	add_child(import_subscene_dialog);
	import_subscene_dialog->connect("subscene_selected", this, "_import_subscene");

	new_scene_from_dialog = memnew(EditorFileDialog);
	new_scene_from_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	add_child(new_scene_from_dialog);
	new_scene_from_dialog->connect("file_selected", this, "_new_scene_from");

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->connect("id_pressed", this, "_tool_selected");
	menu->set_hide_on_window_lose_focus(true);
	menu_subresources = memnew(PopupMenu);
	menu_subresources->set_name("Sub-Resources");
	menu_subresources->connect("id_pressed", this, "_tool_selected");
	menu->add_child(menu_subresources);
	first_enter = true;

	menu_properties = memnew(PopupMenu);
	add_child(menu_properties);
	menu_properties->connect("id_pressed", this, "_property_selected");

	clear_inherit_confirm = memnew(ConfirmationDialog);
	clear_inherit_confirm->set_text(TTR("Clear Inheritance? (No Undo!)"));
	clear_inherit_confirm->get_ok()->set_text(TTR("Clear"));
	add_child(clear_inherit_confirm);

	set_process_input(true);
	set_process(true);

	profile_allow_editing = true;
	profile_allow_script_editing = true;

	EDITOR_DEF("interface/editors/show_scene_tree_root_selection", true);
	EDITOR_DEF("interface/editors/derive_script_globals_by_name", true);
	EDITOR_DEF("_use_favorites_root_selection", false);
}

SceneTreeDock::~SceneTreeDock() {
	if (!node_clipboard.empty()) {
		_clear_clipboard();
	}
}
