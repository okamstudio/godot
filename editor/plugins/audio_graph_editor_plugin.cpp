/**************************************************************************/
/*  audio_graph_editor_plugin.cpp                                      */
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

#include "audio_graph_editor_plugin.h"

#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/inspector_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/tween.h"
#include "scene/gui/button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"

void AudioGraphNodePlugin::_bind_methods() {
	GDVIRTUAL_BIND(_create_editor, "parent_resource", "visual_shader_node");
}

Control *AudioGraphNodePlugin::create_editor(const Ref<Resource> &p_parent_resource, const Ref<AudioStreamGraphNode> &p_node) {
	Object *ret = nullptr;
	GDVIRTUAL_CALL(_create_editor, p_parent_resource, p_node, ret);
	return Object::cast_to<Control>(ret);
}

void AudioGraphNodePlugin::set_editor(AudioGraphEditor *p_editor) {
	ageditor = p_editor;
}

/////////////////

void AudioGraphEditedProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_edited_property", "value"), &AudioGraphEditedProperty::set_edited_property);
	ClassDB::bind_method(D_METHOD("get_edited_property"), &AudioGraphEditedProperty::get_edited_property);

	ADD_PROPERTY(PropertyInfo(Variant::NIL, "edited_property", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), "set_edited_property", "get_edited_property");
}

void AudioGraphEditedProperty::set_edited_property(const Variant &p_variant) {
	edited_property = p_variant;
}

Variant AudioGraphEditedProperty::get_edited_property() const {
	return edited_property;
}

/////////////////

Vector2 AudioGraphEditor::selection_center;
List<AudioGraphEditor::CopyItem> AudioGraphEditor::copy_items_buffer;
List<AudioGraphEditor::Connection> AudioGraphEditor::copy_connections_buffer;

void AudioGraphEditor::edit(AudioStreamGraph *p_audio_graph) {
	// if (p_tree && !p_tree->is_connected("animation_list_changed", callable_mp(this, &AudioGraphEditor::_animation_list_changed))) {
	// 	p_tree->connect("animation_list_changed", callable_mp(this, &AudioGraphEditor::_animation_list_changed), CONNECT_DEFERRED);
	// }

	if (audio_graph == p_audio_graph) {
		return;
	}

	audio_graph = p_audio_graph;

	_update_graph();

	// Vector<String> path;
	// if (tree) {
	// 	edit_path(path);
	// }
}

void AudioGraphEditor::_path_button_pressed(int p_path) {
	edited_path.clear();
	for (int i = 0; i <= p_path; i++) {
		edited_path.push_back(button_path[i]);
	}
}

void AudioGraphEditor::_update_path() {
}

void AudioGraphEditor::edit_path(const Vector<String> &p_path) {
	button_path.clear();

	// Ref<AnimationNode> node = tree->get_root_animation_node();

	// if (node.is_valid()) {
	// 	current_root = node->get_instance_id();

	// 	for (int i = 0; i < p_path.size(); i++) {
	// 		Ref<AnimationNode> child = node->get_child_by_name(p_path[i]);
	// 		ERR_BREAK(child.is_null());
	// 		node = child;
	// 		button_path.push_back(p_path[i]);
	// 	}

	// 	edited_path = button_path;

	// 	for (int i = 0; i < editors.size(); i++) {
	// 		if (editors[i]->can_edit(node)) {
	// 			editors[i]->edit(node);
	// 			editors[i]->show();
	// 		} else {
	// 			editors[i]->edit(Ref<AnimationNode>());
	// 			editors[i]->hide();
	// 		}
	// 	}
	// } else {
	// 	current_root = ObjectID();
	// 	edited_path = button_path;
	// 	for (int i = 0; i < editors.size(); i++) {
	// 		editors[i]->edit(Ref<AnimationNode>());
	// 		editors[i]->hide();
	// 	}
	// }

	// _update_path();
}

Vector<String> AudioGraphEditor::get_edited_path() const {
	return button_path;
}

void AudioGraphEditor::enter_editor(const String &p_path) {
	Vector<String> path = edited_path;
	path.push_back(p_path);
	edit_path(path);
}

void AudioGraphEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_update_options_menu();
		} break;
		case NOTIFICATION_ENTER_TREE: {
		} break;
		case NOTIFICATION_PROCESS: {
			ObjectID root;
			// if (tree && tree->get_root_animation_node().is_valid()) {
			// 	root = tree->get_root_animation_node()->get_instance_id();
			// }

			if (root != current_root) {
				edit_path(Vector<String>());
			}

			if (button_path.size() != edited_path.size()) {
				edit_path(edited_path);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
		} break;
	}
}

void AudioGraphEditor::add_plugin(const Ref<AudioGraphNodePlugin> &p_plugin) {
	if (plugins.has(p_plugin)) {
		return;
	}
	plugins.push_back(p_plugin);
}

void AudioGraphEditor::remove_plugin(const Ref<AudioGraphNodePlugin> &p_plugin) {
	plugins.erase(p_plugin);
}

Variant AudioGraphEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (p_from == members) {
		TreeItem *it = members->get_item_at_position(p_point);
		if (!it) {
			return Variant();
		}
		if (!it->has_meta("id")) {
			return Variant();
		}

		int id = it->get_meta("id");
		//AddOption op = add_options[id];

		Dictionary d;
		d["id"] = id;

		Label *label = memnew(Label);
		label->set_text(it->get_text(0));
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		set_drag_preview(label);
		return d;
	}
	return Variant();
}

bool AudioGraphEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (p_from == graph) {
		Dictionary d = p_data;

		if (d.has("id")) {
			return true;
		}
		if (d.has("files")) {
			return true;
		}
	}

	return false;
}

void AudioGraphEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	// if (p_from == graph) {
	// 	Dictionary d = p_data;

	// 	if (d.has("id")) {
	// 		int idx = d["id"];
	// 		saved_node_pos = p_point;
	// 		saved_node_pos_dirty = true;
	// 		_add_node(idx, add_options[idx].ops);
	// 	} else if (d.has("files")) {
	// 		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	// 		undo_redo->create_action(TTR("Add Node(s) to Visual Shader"));

	// 		if (d["files"].get_type() == Variant::PACKED_STRING_ARRAY) {
	// 			PackedStringArray arr = d["files"];
	// 			for (int i = 0; i < arr.size(); i++) {
	// 				String type = ResourceLoader::get_resource_type(arr[i]);
	// 				if (type == "GDScript") {
	// 					Ref<Script> scr = ResourceLoader::load(arr[i]);
	// 					if (scr->get_instance_base_type() == "VisualShaderNodeCustom") {
	// 						saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 						saved_node_pos_dirty = true;

	// 						int idx = -1;

	// 						for (int j = custom_node_option_idx; j < add_options.size(); j++) {
	// 							if (add_options[j].script.is_valid()) {
	// 								if (add_options[j].script->get_path() == arr[i]) {
	// 									idx = j;
	// 									break;
	// 								}
	// 							}
	// 						}
	// 						if (idx != -1) {
	// 							_add_node(idx, {}, arr[i], i);
	// 						}
	// 					}
	// 				} else if (type == "CurveTexture") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(curve_node_option_idx, {}, arr[i], i);
	// 				} else if (type == "CurveXYZTexture") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(curve_xyz_node_option_idx, {}, arr[i], i);
	// 				} else if (ClassDB::get_parent_class(type) == "Texture2D") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(texture2d_node_option_idx, {}, arr[i], i);
	// 				} else if (type == "Texture2DArray") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(texture2d_array_node_option_idx, {}, arr[i], i);
	// 				} else if (ClassDB::get_parent_class(type) == "Texture3D") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(texture3d_node_option_idx, {}, arr[i], i);
	// 				} else if (type == "Cubemap") {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(cubemap_node_option_idx, {}, arr[i], i);
	// 				} else if (type == "Mesh" && visual_shader->get_mode() == Shader::MODE_PARTICLES &&
	// 						(visual_shader->get_shader_type() == VisualShader::TYPE_START || visual_shader->get_shader_type() == VisualShader::TYPE_START_CUSTOM)) {
	// 					saved_node_pos = p_point + Vector2(0, i * 250 * EDSCALE);
	// 					saved_node_pos_dirty = true;
	// 					_add_node(mesh_emitter_option_idx, {}, arr[i], i);
	// 				}
	// 			}
	// 		}
	// 		undo_redo->commit_action();
	// 	}
	// }
}

void AudioGraphEditor::_node_dragged(const Vector2 &p_from, const Vector2 &p_to, int p_node) {
	drag_buffer.push_back({ p_node, p_from, p_to });
	if (!drag_dirty) {
		callable_mp(this, &AudioGraphEditor::_nodes_dragged).call_deferred();
	}
	drag_dirty = true;
}

void AudioGraphEditor::_nodes_dragged() {
	drag_dirty = false;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(TTR("Move AudioGraph Node(s)"));

	for (const DragOp &E : drag_buffer) {
		undo_redo->add_do_method(audio_graph, "set_node_position", E.node, E.to);
		undo_redo->add_undo_method(audio_graph, "set_node_position", E.node, E.from);
		undo_redo->add_do_method(graph_plugin, "set_node_position", E.node, E.to);
		undo_redo->add_undo_method(graph_plugin, "set_node_position", E.node, E.from);
	}

	undo_redo->commit_action();

	//_handle_node_drop_on_connection();

	drag_buffer.clear();
}

void AudioGraphEditor::_connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	int from = p_from.to_int();
	int to = p_to.to_int();

	// if (!visual_shader->can_connect_nodes(type, from, p_from_index, to, p_to_index)) {
	// 	return;
	// }

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Nodes Connected"));

	List<AudioStreamGraph::Connection> conns;
	audio_graph->get_node_connections(&conns);

	for (const AudioStreamGraph::Connection &E : conns) {
		if (E.to_node == to && E.to_port == p_to_index) {
			undo_redo->add_do_method(audio_graph, "disconnect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
			undo_redo->add_undo_method(audio_graph, "connect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
			undo_redo->add_do_method(graph_plugin, "disconnect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
			undo_redo->add_undo_method(graph_plugin, "connect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
		}
	}

	undo_redo->add_do_method(audio_graph, "connect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_undo_method(audio_graph, "disconnect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_do_method(graph_plugin, "connect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_undo_method(graph_plugin, "disconnect_nodes", from, p_from_index, to, p_to_index);

	undo_redo->add_do_method(graph_plugin, "update_node", from);
	undo_redo->add_undo_method(graph_plugin, "update_node", from);
	undo_redo->add_do_method(graph_plugin, "update_node", to);
	undo_redo->add_undo_method(graph_plugin, "update_node", to);
	undo_redo->commit_action();
}

void AudioGraphEditor::_disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	graph->disconnect_node(p_from, p_from_index, p_to, p_to_index);

	int from = p_from.to_int();
	int to = p_to.to_int();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Nodes Disconnected"));
	undo_redo->add_do_method(audio_graph, "disconnect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_undo_method(audio_graph, "connect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_do_method(graph_plugin, "disconnect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_undo_method(graph_plugin, "connect_nodes", from, p_from_index, to, p_to_index);
	undo_redo->add_do_method(graph_plugin, "update_node", to);
	undo_redo->add_undo_method(graph_plugin, "update_node", to);
	undo_redo->commit_action();
}

void AudioGraphEditor::_add_node(int p_idx) {
	ERR_FAIL_NULL(audio_graph);
	ERR_FAIL_INDEX(p_idx, add_options.size());

	const AddOption &op = add_options[p_idx];

	Ref<AudioStreamGraphNode> agnode = Object::cast_to<AudioStreamGraphNode>(ClassDB::instantiate(op.type));
	Point2 position = graph->get_scroll_offset();
	if (saved_node_pos_dirty) {
		position += saved_node_pos;
	} else {
		position += graph->get_size() * 0.5;
		position /= EDSCALE;
	}
	position /= graph->get_zoom();
	saved_node_pos_dirty = false;

	int id_to_use = audio_graph->get_valid_node_id();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Node to Audio Graph"));
	undo_redo->add_do_method(audio_graph, "add_node", agnode, position, id_to_use);
	undo_redo->add_undo_method(audio_graph, "remove_node", id_to_use);
	undo_redo->add_do_method(graph_plugin, "add_node", id_to_use, false);
	undo_redo->add_undo_method(graph_plugin, "remove_node", id_to_use, false);
	undo_redo->commit_action();
}

void AudioGraphEditor::_member_create() {
	TreeItem *item = members->get_selected();
	if (item != nullptr && item->has_meta("id")) {
		int idx = members->get_selected()->get_meta("id");
		if (connection_node_insert_requested) {
			from_node = String(clicked_connection->from_node).to_int();
			from_slot = clicked_connection->from_port;
			to_node = String(clicked_connection->to_node).to_int();
			to_slot = clicked_connection->to_port;

			connection_node_insert_requested = false;

			saved_node_pos_dirty = true;

			// Find both graph nodes and get their positions.
			GraphNode *from_graph_element = Object::cast_to<GraphNode>(graph->get_node(itos(from_node)));
			GraphNode *to_graph_element = Object::cast_to<GraphNode>(graph->get_node(itos(to_node)));

			ERR_FAIL_NULL(from_graph_element);
			ERR_FAIL_NULL(to_graph_element);

			// Since the size of the node to add is not known yet, it's not possible to center it exactly.
			float zoom = graph->get_zoom();
			saved_node_pos = 0.5 * (from_graph_element->get_position() + zoom * from_graph_element->get_output_port_position(from_slot) + to_graph_element->get_position() + zoom * to_graph_element->get_input_port_position(to_slot));
		}
		_add_node(idx);
		members_dialog->hide();
	}
}

void AudioGraphEditor::_member_selected() {
	TreeItem *item = members->get_selected();

	if (item != nullptr && item->has_meta("id")) {
		int id = item->get_meta("id");
		const AddOption &option = add_options[id];
		members_dialog->get_ok_button()->set_disabled(false);
		node_desc->set_text(option.description);
	} else {
		members_dialog->get_ok_button()->set_disabled(true);
		node_desc->set_text("");
	}
}

void AudioGraphEditor::_member_cancel() {
	to_node = -1;
	to_slot = -1;
	from_node = -1;
	from_slot = -1;
	connection_node_insert_requested = false;
}

void AudioGraphEditor::_update_options_menu() {
	TreeItem *root = members->create_item();
	for (int i = 0; i < add_options.size(); i++) {
		const AddOption &op = add_options[i];
		TreeItem *item = members->create_item(root);
		item->set_text(0, op.name);
		item->set_meta("id", i);
	}
}

void AudioGraphEditor::_show_members_dialog(bool at_mouse_pos, AudioStreamGraphNode::PortType p_input_port_type, AudioStreamGraphNode::PortType p_output_port_type) {
	if (members_input_port_type != p_input_port_type || members_output_port_type != p_output_port_type) {
		members_input_port_type = p_input_port_type;
		members_output_port_type = p_output_port_type;
		_update_options_menu();
	}

	if (at_mouse_pos) {
		saved_node_pos_dirty = true;
		saved_node_pos = graph->get_local_mouse_position();

		Point2 gpos = get_screen_position() + get_local_mouse_position();
		members_dialog->set_position(gpos);
	} else {
		saved_node_pos_dirty = false;
		members_dialog->set_position(graph->get_screen_position() + Point2(5 * EDSCALE, 65 * EDSCALE));
	}

	if (members_dialog->is_visible()) {
		members_dialog->grab_focus();
		return;
	}

	members_dialog->popup();

	// Keep dialog within window bounds.
	Rect2 window_rect = Rect2(get_window()->get_position(), get_window()->get_size());
	Rect2 dialog_rect = Rect2(members_dialog->get_position(), members_dialog->get_size());
	Vector2 difference = (dialog_rect.get_end() - window_rect.get_end()).maxf(0);
	members_dialog->set_position(members_dialog->get_position() - difference);

	node_filter->grab_focus();
	node_filter->select_all();
}

void AudioGraphEditor::_sbox_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			members->gui_input(key);
			node_filter->accept_event();
		}
	}
}

void AudioGraphEditor::_graph_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	Ref<InputEventMouseButton> mb = p_event;

	// Highlight valid connection on which a node can be dropped.
	if (mm.is_valid() && mm->get_button_mask().has_flag(MouseButtonMask::LEFT)) {
		Ref<GraphEdit::Connection> closest_connection;
		graph->reset_all_connection_activity();
		// if (_check_node_drop_on_connection(graph->get_local_mouse_position(), &closest_connection)) {
		// 	graph->set_connection_activity(closest_connection->from_node, closest_connection->from_port, closest_connection->to_node, closest_connection->to_port, 1.0);
		// }
	}

	Ref<AudioStreamGraphNode> selected_agnode;
	// Right click actions.
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) {
		List<int> selected_deletable_graph_elements;
		List<GraphElement *> selected_graph_elements;
		for (int i = 0; i < graph->get_child_count(); i++) {
			GraphElement *graph_element = Object::cast_to<GraphElement>(graph->get_child(i));
			if (!graph_element) {
				continue;
			}
			int id = String(graph_element->get_name()).to_int();
			Ref<AudioStreamGraphNode> agnode = audio_graph->get_node(id);

			if (!graph_element->is_selected()) {
				continue;
			}

			selected_graph_elements.push_back(graph_element);

			if (!agnode->is_deletable()) {
				continue;
			}

			selected_deletable_graph_elements.push_back(id);

			selected_agnode = agnode;
		}

		bool copy_buffer_empty = true;
		for (const CopyItem &item : copy_items_buffer) {
			if (!item.disabled) {
				copy_buffer_empty = false;
				break;
			}
		}

		menu_point = graph->get_local_mouse_position();
		Point2 gpos = get_screen_position() + get_local_mouse_position();

		Ref<GraphEdit::Connection> closest_connection = graph->get_closest_connection_at_point(menu_point);
		if (closest_connection.is_valid()) {
			clicked_connection = closest_connection;
			saved_node_pos = graph->get_local_mouse_position();
			saved_node_pos_dirty = true;
			connection_popup_menu->set_position(gpos);
			connection_popup_menu->reset_size();
			connection_popup_menu->popup();
		} else if (selected_graph_elements.is_empty() && copy_buffer_empty) {
			_show_members_dialog(true);
		} else {
			popup_menu->set_item_disabled(NodeMenuOptions::CUT, selected_deletable_graph_elements.is_empty());
			popup_menu->set_item_disabled(NodeMenuOptions::COPY, selected_deletable_graph_elements.is_empty());
			popup_menu->set_item_disabled(NodeMenuOptions::PASTE, copy_buffer_empty);
			popup_menu->set_item_disabled(NodeMenuOptions::DELETE, selected_deletable_graph_elements.is_empty());
			popup_menu->set_item_disabled(NodeMenuOptions::DUPLICATE, selected_deletable_graph_elements.is_empty());
			popup_menu->set_item_disabled(NodeMenuOptions::CLEAR_COPY_BUFFER, copy_buffer_empty);

			popup_menu->set_position(gpos);
			popup_menu->reset_size();
			popup_menu->popup();
		}
	}
}

void AudioGraphEditor::_node_menu_id_pressed(int p_idx) {
	switch (p_idx) {
		case NodeMenuOptions::ADD:
			_show_members_dialog(true);
			break;
		case NodeMenuOptions::CUT:
			//_copy_nodes(true);
			break;
		case NodeMenuOptions::COPY:
			//_copy_nodes(false);
			break;
		case NodeMenuOptions::PASTE:
			//_paste_nodes(true, menu_point);
			break;
		case NodeMenuOptions::DELETE:
			_delete_nodes_request(TypedArray<StringName>());
			break;
		case NodeMenuOptions::DUPLICATE:
			//_duplicate_nodes();
			break;
		case NodeMenuOptions::CLEAR_COPY_BUFFER:
			//_clear_copy_buffer();
			break;
		default:
			break;
	}
}
void AudioGraphEditor::_connection_menu_id_pressed(int p_idx) {
	switch (p_idx) {
		case ConnectionMenuOptions::DISCONNECT: {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			// undo_redo->create_action(TTR("Disconnect"));
			// undo_redo->add_do_method(visual_shader.ptr(), "disconnect_nodes", get_current_shader_type(), String(clicked_connection->from_node).to_int(), clicked_connection->from_port, String(clicked_connection->to_node).to_int(), clicked_connection->to_port);
			// undo_redo->add_undo_method(visual_shader.ptr(), "connect_nodes", get_current_shader_type(), String(clicked_connection->from_node).to_int(), clicked_connection->from_port, String(clicked_connection->to_node).to_int(), clicked_connection->to_port);
			// undo_redo->add_do_method(graph_plugin.ptr(), "disconnect_nodes", get_current_shader_type(), String(clicked_connection->from_node).to_int(), clicked_connection->from_port, String(clicked_connection->to_node).to_int(), clicked_connection->to_port);
			// undo_redo->add_undo_method(graph_plugin.ptr(), "connect_nodes", get_current_shader_type(), String(clicked_connection->from_node).to_int(), clicked_connection->from_port, String(clicked_connection->to_node).to_int(), clicked_connection->to_port);
			// undo_redo->commit_action();
		} break;
		case ConnectionMenuOptions::INSERT_NEW_NODE: {
			// VisualShaderNode::PortType input_port_type = VisualShaderNode::PORT_TYPE_MAX;
			// VisualShaderNode::PortType output_port_type = VisualShaderNode::PORT_TYPE_MAX;
			// Ref<VisualShaderNode> node1 = visual_shader->get_node(get_current_shader_type(), String(clicked_connection->from_node).to_int());
			// if (node1.is_valid()) {
			// 	output_port_type = node1->get_output_port_type(from_slot);
			// }
			// Ref<VisualShaderNode> node2 = visual_shader->get_node(get_current_shader_type(), String(clicked_connection->to_node).to_int());
			// if (node2.is_valid()) {
			// 	input_port_type = node2->get_input_port_type(to_slot);
			// }

			// connection_node_insert_requested = true;
			// _show_members_dialog(true, input_port_type, output_port_type);
		} break;
		case ConnectionMenuOptions::INSERT_NEW_REROUTE: {
			from_node = String(clicked_connection->from_node).to_int();
			from_slot = clicked_connection->from_port;
			to_node = String(clicked_connection->to_node).to_int();
			to_slot = clicked_connection->to_port;

			// Manual offset to place the port exactly at the mouse position.
			saved_node_pos -= Vector2(11 * EDSCALE * graph->get_zoom(), 50 * EDSCALE * graph->get_zoom());

			// Find reroute addoptions.
			int idx = -1;
			for (int i = 0; i < add_options.size(); i++) {
				if (add_options[i].name == "Reroute") {
					idx = i;
					break;
				}
			}
			_add_node(idx);
		} break;
		default:
			break;
	}
}

void AudioGraphEditor::_update_graph() {
	if (updating) {
		return;
	}

	if (!audio_graph) {
		return;
	}

	//graph->set_scroll_offset(audio_graph->get_graph_offset() * EDSCALE);

	graph->clear_connections();
	// Remove all nodes.
	for (int i = 0; i < graph->get_child_count(); i++) {
		if (Object::cast_to<GraphElement>(graph->get_child(i))) {
			Node *node = graph->get_child(i);
			graph->remove_child(node);
			memdelete(node);
			i--;
		}
	}

	List<AudioStreamGraph::Connection> node_connections;
	audio_graph->get_node_connections(&node_connections);
	graph_plugin->set_connections(node_connections);

	Vector<int> nodes = audio_graph->get_node_list();

	graph_plugin->clear_links();
	//graph_plugin->update_theme();

	for (int n_i = 0; n_i < nodes.size(); n_i++) {
		// Update frame related stuff later since we need to have all nodes in the graph.
		graph_plugin->add_node(nodes[n_i], false);
	}

	for (const AudioStreamGraph::Connection &E : node_connections) {
		int from = E.from_node;
		int from_idx = E.from_port;
		int to = E.to_node;
		int to_idx = E.to_port;

		graph->connect_node(itos(from), from_idx, itos(to), to_idx);
	}

	// Attach nodes to frames.
	// for (int node_id : nodes) {
	// 	Ref<VisualShaderNode> vsnode = visual_shader->get_node(node_id);
	// 	ERR_CONTINUE_MSG(vsnode.is_null(), "Node is null.");

	// 	if (vsnode->get_frame() != -1) {
	// 		int frame_name = vsnode->get_frame();
	// 		graph->attach_graph_element_to_frame(itos(node_id), itos(frame_name));
	// 	}
	// }

	float graph_minimap_opacity = EDITOR_GET("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);
	float graph_lines_curvature = EDITOR_GET("editors/visual_editors/lines_curvature");
	graph->set_connection_lines_curvature(graph_lines_curvature);
}

void AudioGraphEditor::_delete_node_request(int p_node) {
	Ref<AudioStreamGraphNode> node = audio_graph->get_node(p_node);
	if (!node->is_deletable()) {
		return;
	}

	List<int> to_erase;
	to_erase.push_back(p_node);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete AudioGraph Node"));
	_delete_nodes(to_erase);
	undo_redo->commit_action();
}

void AudioGraphEditor::_delete_nodes_request(const TypedArray<StringName> &p_nodes) {
	List<int> to_erase;

	if (p_nodes.is_empty()) {
		// Called from context menu.
		for (int i = 0; i < graph->get_child_count(); i++) {
			GraphElement *graph_element = Object::cast_to<GraphElement>(graph->get_child(i));
			if (!graph_element) {
				continue;
			}

			int id = String(graph_element->get_name()).to_int();
			Ref<AudioStreamGraphNode> agnode = audio_graph->get_node(id);
			if (agnode->is_deletable() && graph_element->is_selected()) {
				to_erase.push_back(graph_element->get_name().operator String().to_int());
			}
		}
	} else {
		for (int i = 0; i < p_nodes.size(); i++) {
			int id = p_nodes[i].operator String().to_int();
			Ref<AudioStreamGraphNode> agnode = audio_graph->get_node(id);
			if (agnode->is_deletable()) {
				to_erase.push_back(id);
			}
		}
	}

	if (to_erase.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete AudioGraph Node(s)"));
	_delete_nodes(to_erase);
	undo_redo->commit_action();
}

void AudioGraphEditor::_delete_nodes(const List<int> &p_nodes) {
	List<AudioStreamGraph::Connection> conns;
	audio_graph->get_node_connections(&conns);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	for (const int &F : p_nodes) {
		for (const AudioStreamGraph::Connection &E : conns) {
			if (E.from_node == F || E.to_node == F) {
				undo_redo->add_do_method(graph_plugin, "disconnect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
			}
		}
	}

	for (const int &F : p_nodes) {
		Ref<AudioStreamGraphNode> node = audio_graph->get_node(F);
		undo_redo->add_undo_method(audio_graph, "add_node", node, audio_graph->get_node_position(F), F);
		undo_redo->add_undo_method(graph_plugin, "add_node", F, false);
	}

	for (const int &F : p_nodes) {
		Ref<AudioStreamGraphNode> node = audio_graph->get_node(F);

		undo_redo->add_do_method(audio_graph, "remove_node", F);
	}

	List<AudioStreamGraph::Connection> used_conns;
	for (const int &F : p_nodes) {
		for (const AudioStreamGraph::Connection &E : conns) {
			if (E.from_node == F || E.to_node == F) {
				bool cancel = false;
				for (List<AudioStreamGraph::Connection>::Element *R = used_conns.front(); R; R = R->next()) {
					if (R->get().from_node == E.from_node && R->get().from_port == E.from_port && R->get().to_node == E.to_node && R->get().to_port == E.to_port) {
						cancel = true; // to avoid ERR_ALREADY_EXISTS warning
						break;
					}
				}
				if (!cancel) {
					undo_redo->add_undo_method(audio_graph, "connect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
					undo_redo->add_undo_method(graph_plugin, "connect_nodes", E.from_node, E.from_port, E.to_node, E.to_port);
					used_conns.push_back(E);
				}
			}
		}
	}

	// Delete nodes from the graph.
	for (const int &F : p_nodes) {
		undo_redo->add_do_method(graph_plugin, "remove_node", F, false);
	}
}

void AudioGraphEditor::_edit_port_default_input(Object *p_button, int p_node, int p_port) {
	Ref<AudioStreamGraphNode> ag_node = audio_graph->get_node(p_node);
	Variant value = ag_node->get_input_port_default_value(p_port);

	edited_property_holder->set_edited_property(value);
	editing_node = p_node;
	editing_port = p_port;

	if (property_editor) {
		property_editor->disconnect("property_changed", callable_mp(this, &AudioGraphEditor::_port_edited));
		property_editor_popup->remove_child(property_editor);
	}

	// TODO: Define these properties with actual PropertyInfo and feed it to the property editor widget.
	property_editor = EditorInspector::instantiate_property_editor(edited_property_holder.ptr(), value.get_type(), "edited_property", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, true);
	ERR_FAIL_NULL_MSG(property_editor, "Failed to create property editor for type: " + Variant::get_type_name(value.get_type()));

	// Determine the best size for the popup based on the property type.
	// This is done here, since the property editors are also used in the inspector where they have different layout requirements, so we can't just change their default minimum size.
	Size2 popup_pref_size = Size2(180, 0.0);
	property_editor_popup->set_min_size(popup_pref_size);

	property_editor->set_object_and_property(edited_property_holder.ptr(), "edited_property");
	property_editor->update_property();
	property_editor->set_name_split_ratio(0);
	property_editor_popup->add_child(property_editor);

	property_editor->connect("property_changed", callable_mp(this, &AudioGraphEditor::_port_edited));

	Button *button = Object::cast_to<Button>(p_button);
	if (button) {
		property_editor_popup->set_position(button->get_screen_position() + Vector2(0, button->get_size().height) * graph->get_zoom());
	}
	property_editor_popup->reset_size();
	if (button) {
		property_editor_popup->popup();
	} else {
		property_editor_popup->popup_centered_ratio();
	}
	property_editor->select(0); // Focus the first focusable control.
}

void AudioGraphEditor::_port_edited(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing) {
	Ref<AudioStreamGraphNode> asn = audio_graph->get_node(editing_node);
	ERR_FAIL_COND(!asn.is_valid());

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Input Default Port"));

	undo_redo->add_do_method(asn.ptr(), "set_input_port_default_value", editing_port, p_value);
	undo_redo->add_undo_method(asn.ptr(), "set_input_port_default_value", editing_port, asn->get_input_port_default_value(editing_port));

	undo_redo->add_do_method(graph_plugin, "set_input_port_default_value", editing_node, editing_port, p_value);
	undo_redo->add_undo_method(graph_plugin, "set_input_port_default_value", editing_node, editing_port, asn->get_input_port_default_value(editing_port));
	undo_redo->commit_action();
}

void AudioGraphEditor::_add_input_port(int p_port) {
	Ref<AudioStreamGraphNode> asn = audio_graph->get_node(p_port);

	if (asn.is_null()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Input Port"));

	undo_redo->add_do_method(asn.ptr(), "add_input_port");
	undo_redo->add_undo_method(asn.ptr(), "remove_input_port");

	undo_redo->add_do_method(graph_plugin, "update_node", p_port);
	undo_redo->add_undo_method(graph_plugin, "update_node", p_port);
	undo_redo->commit_action();
}

void AudioGraphEditor::_parameter_line_edit_changed(const String &p_text, int p_node_id) {
	Ref<AudioStreamGraphNodeParameter> node = audio_graph->get_node(p_node_id);
	ERR_FAIL_COND(!node.is_valid());

	String validated_name = audio_graph->validate_parameter_name(p_text, node);

	if (validated_name == node->get_parameter_name()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Parameter Name"));
	undo_redo->add_do_method(node.ptr(), "set_parameter_name", validated_name);
	undo_redo->add_undo_method(node.ptr(), "set_parameter_name", node->get_parameter_name());
	undo_redo->add_do_method(graph_plugin, "set_parameter_name", p_node_id, validated_name);
	undo_redo->add_undo_method(graph_plugin, "set_parameter_name", p_node_id, node->get_parameter_name());
	undo_redo->add_do_method(graph_plugin, "update_node_deferred", p_node_id);
	undo_redo->add_undo_method(graph_plugin, "update_node_deferred", p_node_id);

	undo_redo->add_do_method(this, "_update_parameters", true);
	undo_redo->add_undo_method(this, "_update_parameters", true);

	HashSet<String> changed_names;
	changed_names.insert(node->get_parameter_name());
	_update_parameter_refs(changed_names);

	undo_redo->commit_action();
}

void AudioGraphEditor::_update_parameter_refs(HashSet<String> &p_deleted_names) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	Vector<int> nodes = audio_graph->get_node_list();
	for (int i = 0; i < nodes.size(); i++) {
		if (i > 0) {
			Ref<AudioStreamGraphNodeParameter> ref = audio_graph->get_node(nodes[i]);
			if (ref.is_valid()) {
				if (p_deleted_names.has(ref->get_parameter_name())) {
					undo_redo->add_do_method(ref.ptr(), "set_parameter_name", "[None]");
					undo_redo->add_undo_method(ref.ptr(), "set_parameter_name", ref->get_parameter_name());
					undo_redo->add_do_method(graph_plugin, "update_node", nodes[i]);
					undo_redo->add_undo_method(graph_plugin, "update_node", nodes[i]);
				}
			}
		}
	}
}

void AudioGraphEditor::_parameter_line_edit_focus_out(Object *line_edit, int p_node_id) {
	_parameter_line_edit_changed(Object::cast_to<LineEdit>(line_edit)->get_text(), p_node_id);
}

void AudioGraphEditor::_remove_input_port(int p_port) {
	Ref<AudioStreamGraphNode> asn = audio_graph->get_node(p_port);

	if (asn.is_null()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Input Port"));

	List<AudioStreamGraph::Connection> conns;
	audio_graph->get_node_connections(&conns);
	for (const AudioStreamGraph::Connection &E : conns) {
		int cn_from_node = E.from_node;
		int cn_from_port = E.from_port;
		int cn_to_node = E.to_node;
		int cn_to_port = E.to_port;

		if (cn_to_node == p_port) {
			if (cn_to_port == p_port) {
				undo_redo->add_do_method(audio_graph, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);
				undo_redo->add_undo_method(audio_graph, "connect_nodes_forced", cn_from_node, cn_from_port, cn_to_node, cn_to_port);

				undo_redo->add_do_method(graph_plugin, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);
				undo_redo->add_undo_method(graph_plugin, "connect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);
			} else if (cn_to_port > p_port) {
				undo_redo->add_do_method(audio_graph, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);
				undo_redo->add_undo_method(audio_graph, "connect_nodes_forced", cn_from_node, cn_from_port, cn_to_node, cn_to_port);

				undo_redo->add_do_method(graph_plugin, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);
				undo_redo->add_undo_method(graph_plugin, "connect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port);

				undo_redo->add_do_method(audio_graph, "connect_nodes_forced", cn_from_node, cn_from_port, cn_to_node, cn_to_port - 1);
				undo_redo->add_undo_method(audio_graph, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port - 1);

				undo_redo->add_do_method(graph_plugin, "connect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port - 1);
				undo_redo->add_undo_method(graph_plugin, "disconnect_nodes", cn_from_node, cn_from_port, cn_to_node, cn_to_port - 1);
			}
		}
	}

	undo_redo->add_do_method(asn.ptr(), "remove_input_port");
	undo_redo->add_undo_method(asn.ptr(), "add_input_port");

	undo_redo->add_do_method(graph_plugin, "update_node", p_port);
	undo_redo->add_undo_method(graph_plugin, "update_node", p_port);
	undo_redo->commit_action();
}

void AudioGraphEditor::_script_created(const Ref<Script> &p_script) {
	// if (p_script.is_null() || p_script->get_instance_base_type() != "AudioStreamGraphNode") {
	// 	return;
	// }

	// Ref<AudioStreamGraphNode> ref;
	// ref.instantiate();
	// ref->set_script(p_script);

	// add_options.push_back(AddOption(ref->get_caption(), "classes.get(i)", ref->get_description()));
	// add_options.write[add_options.size() - 1].script = p_script;
	// _update_options_menu();
}

void AudioGraphEditor::_resource_saved(const Ref<Resource> &p_resource) {
	// Ref<Script> script = Ref<Script>(p_resource.ptr());
	// if (script.is_null() || script->get_instance_base_type() != "AudioStreamGraphNode") {
	// 	return;
	// }
}

Ref<AudioStreamGraph> AudioGraphEditor::get_audio_graph() const {
	return audio_graph;
}

AudioGraphEditorPlugin *AudioGraphEditor::get_graph_plugin() const {
	return graph_plugin;
}

void AudioGraphEditor::_bind_methods() {
	ClassDB::bind_method("_update_graph", &AudioGraphEditor::_update_graph);
}

AudioGraphEditor *AudioGraphEditor::singleton = nullptr;
AudioGraphEditor::AudioGraphEditor() {
	singleton = this;
	MarginContainer *main_box = memnew(MarginContainer);
	// EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &AudioGraphEditor::_resource_saved));
	// FileSystemDock::get_singleton()->get_script_create_dialog()->connect("script_created", callable_mp(this, &AudioGraphEditor::_script_created));
	main_box->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(main_box);

	graph = memnew(GraphEdit);
	graph->set_v_size_flags(SIZE_EXPAND_FILL);
	graph->set_h_size_flags(SIZE_EXPAND_FILL);
	graph->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
	graph->set_grid_pattern(GraphEdit::GridPattern::GRID_PATTERN_DOTS);
	int grid_pattern = EDITOR_GET("editors/visual_editors/grid_pattern");
	graph->set_grid_pattern((GraphEdit::GridPattern)grid_pattern);
	graph->set_show_zoom_label(true);
	main_box->add_child(graph);
	SET_DRAG_FORWARDING_GCD(graph, AudioGraphEditor);
	float graph_minimap_opacity = EDITOR_GET("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);
	float graph_lines_curvature = EDITOR_GET("editors/visual_editors/lines_curvature");
	graph->set_connection_lines_curvature(graph_lines_curvature);

	graph->add_valid_right_disconnect_type(AudioStreamGraphNode::PORT_TYPE_STREAM);
	graph->add_valid_right_disconnect_type(AudioStreamGraphNode::PORT_TYPE_SCALAR);
	graph->add_valid_connection_type(AudioStreamGraphNode::PORT_TYPE_STREAM, AudioStreamGraphNode::PORT_TYPE_STREAM);
	graph->add_valid_connection_type(AudioStreamGraphNode::PORT_TYPE_SCALAR, AudioStreamGraphNode::PORT_TYPE_SCALAR);

	//graph signals
	graph->connect("connection_request", callable_mp(this, &AudioGraphEditor::_connection_request), CONNECT_DEFERRED);
	graph->connect("disconnection_request", callable_mp(this, &AudioGraphEditor::_disconnection_request), CONNECT_DEFERRED);
	// graph->connect("node_selected", callable_mp(this, &AudioGraphEditor::_node_selected));
	// graph->connect("scroll_offset_changed", callable_mp(this, &AudioGraphEditor::_scroll_changed));
	// graph->connect("duplicate_nodes_request", callable_mp(this, &AudioGraphEditor::_duplicate_nodes));
	// graph->connect("copy_nodes_request", callable_mp(this, &AudioGraphEditor::_copy_nodes).bind(false));
	// graph->connect("cut_nodes_request", callable_mp(this, &AudioGraphEditor::_copy_nodes).bind(true));
	// graph->connect("paste_nodes_request", callable_mp(this, &AudioGraphEditor::_paste_nodes).bind(false, Point2()));
	// graph->connect("delete_nodes_request", callable_mp(this, &AudioGraphEditor::_delete_nodes_request));
	graph->connect(SceneStringName(gui_input), callable_mp(this, &AudioGraphEditor::_graph_gui_input));
	// graph->connect("connection_to_empty", callable_mp(this, &AudioGraphEditor::_connection_to_empty));
	// graph->connect("connection_from_empty", callable_mp(this, &AudioGraphEditor::_connection_from_empty));
	// graph->connect(SceneStringName(visibility_changed), callable_mp(this, &AudioGraphEditor::_visibility_changed));

	PanelContainer *toolbar_panel = static_cast<PanelContainer *>(graph->get_menu_hbox()->get_parent());
	toolbar_panel->set_anchors_and_offsets_preset(Control::PRESET_TOP_WIDE, PRESET_MODE_MINSIZE, 10);
	toolbar_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	HFlowContainer *toolbar = memnew(HFlowContainer);
	{
		LocalVector<Node *> toolbar_nodes;
		for (int i = 0; i < graph->get_menu_hbox()->get_child_count(); i++) {
			Node *child = graph->get_menu_hbox()->get_child(i);
			toolbar_nodes.push_back(child);
		}

		for (Node *node : toolbar_nodes) {
			graph->get_menu_hbox()->remove_child(node);
			toolbar->add_child(node);
		}

		graph->get_menu_hbox()->hide();
		toolbar_panel->add_child(toolbar);
	}

	VSeparator *vs = memnew(VSeparator);
	toolbar->add_child(vs);
	toolbar->move_child(vs, 0);

	add_node = memnew(Button);
	add_node->set_flat(true);
	add_node->set_text(TTR("Add Node..."));
	add_node->connect(SceneStringName(pressed), callable_mp(this, &AudioGraphEditor::_show_members_dialog).bind(false, AudioStreamGraphNode::PORT_TYPE_MAX, AudioStreamGraphNode::PORT_TYPE_MAX));
	toolbar->add_child(add_node);
	toolbar->move_child(add_node, 0);

	VBoxContainer *members_vb = memnew(VBoxContainer);
	members_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *filter_hb = memnew(HBoxContainer);
	members_vb->add_child(filter_hb);

	node_filter = memnew(LineEdit);
	filter_hb->add_child(node_filter);
	// node_filter->connect(SceneStringName(text_changed), callable_mp(this, &AudioGraphEditor::_member_filter_changed));
	node_filter->connect(SceneStringName(gui_input), callable_mp(this, &AudioGraphEditor::_sbox_input));
	node_filter->set_h_size_flags(SIZE_EXPAND_FILL);
	node_filter->set_placeholder(TTR("Search"));

	tools = memnew(MenuButton);
	filter_hb->add_child(tools);
	tools->set_tooltip_text(TTR("Options"));
	//tools->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &AudioGraphEditor::_tools_menu_option));
	tools->get_popup()->add_item(TTR("Expand All"), EXPAND_ALL);
	tools->get_popup()->add_item(TTR("Collapse All"), COLLAPSE_ALL);

	members = memnew(Tree);
	members_vb->add_child(members);
	SET_DRAG_FORWARDING_GCD(members, AudioGraphEditor);
	members->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // TODO: Implement proper translation switch.
	members->set_h_size_flags(SIZE_EXPAND_FILL);
	members->set_v_size_flags(SIZE_EXPAND_FILL);
	members->set_hide_root(true);
	members->set_allow_reselect(true);
	members->set_hide_folding(false);
	members->set_custom_minimum_size(Size2(180 * EDSCALE, 200 * EDSCALE));
	members->connect("item_activated", callable_mp(this, &AudioGraphEditor::_member_create));
	members->connect(SceneStringName(item_selected), callable_mp(this, &AudioGraphEditor::_member_selected));

	HBoxContainer *desc_hbox = memnew(HBoxContainer);
	members_vb->add_child(desc_hbox);

	Label *desc_label = memnew(Label);
	desc_hbox->add_child(desc_label);
	desc_label->set_text(TTR("Description:"));

	desc_hbox->add_spacer();

	node_desc = memnew(RichTextLabel);
	members_vb->add_child(node_desc);
	node_desc->set_h_size_flags(SIZE_EXPAND_FILL);
	node_desc->set_v_size_flags(SIZE_FILL);
	node_desc->set_custom_minimum_size(Size2(0, 70 * EDSCALE));

	members_dialog = memnew(ConfirmationDialog);
	members_dialog->set_title(TTR("Create Audio Node"));
	members_dialog->add_child(members_vb);
	members_dialog->set_ok_button_text(TTR("Create"));
	members_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AudioGraphEditor::_member_create));
	members_dialog->get_ok_button()->set_disabled(true);
	members_dialog->connect("canceled", callable_mp(this, &AudioGraphEditor::_member_cancel));
	members_dialog->register_text_enter(node_filter);
	add_child(members_dialog);

	///////////////////////////////////////
	// POPUP MENU
	///////////////////////////////////////

	popup_menu = memnew(PopupMenu);
	add_child(popup_menu);
	popup_menu->set_hide_on_checkable_item_selection(false);
	popup_menu->add_item(TTR("Add Node"), NodeMenuOptions::ADD);
	popup_menu->add_separator();
	popup_menu->add_item(TTR("Cut"), NodeMenuOptions::CUT);
	popup_menu->add_item(TTR("Copy"), NodeMenuOptions::COPY);
	popup_menu->add_item(TTR("Paste"), NodeMenuOptions::PASTE);
	popup_menu->add_item(TTR("Delete"), NodeMenuOptions::DELETE);
	popup_menu->add_item(TTR("Duplicate"), NodeMenuOptions::DUPLICATE);
	popup_menu->add_item(TTR("Clear Copy Buffer"), NodeMenuOptions::CLEAR_COPY_BUFFER);
	popup_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AudioGraphEditor::_node_menu_id_pressed));

	connection_popup_menu = memnew(PopupMenu);
	add_child(connection_popup_menu);
	connection_popup_menu->add_item(TTR("Disconnect"), ConnectionMenuOptions::DISCONNECT);
	connection_popup_menu->add_item(TTR("Insert New Node"), ConnectionMenuOptions::INSERT_NEW_NODE);
	connection_popup_menu->add_item(TTR("Insert New Reroute"), ConnectionMenuOptions::INSERT_NEW_REROUTE);
	connection_popup_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AudioGraphEditor::_connection_menu_id_pressed));

	List<StringName> classes;
	ClassDB::get_inheriters_from_class("AudioStreamGraphNode", &classes);
	for (int i = 0; i < classes.size(); i++) {
		if (classes.get(i) != "AudioStreamGraphOutputNode") {
			if (!ClassDB::can_instantiate(classes.get(i))) {
				continue;
			}
			AudioStreamGraphNode *node = Object::cast_to<AudioStreamGraphNode>(ClassDB::instantiate(classes.get(i)));
			if (node) {
				add_options.push_back(AddOption(node->get_caption(), classes.get(i), node->get_description()));
				memdelete(node);
			}
		}
	}

	Ref<AudioGraphNodePluginDefault> default_plugin;
	default_plugin.instantiate();
	default_plugin->set_editor(this);
	add_plugin(default_plugin);

	property_editor_popup = memnew(PopupPanel);
	property_editor_popup->set_min_size(Size2(360, 0) * EDSCALE);
	add_child(property_editor_popup);
	edited_property_holder.instantiate();
}

class AudioGraphNodePluginDefaultEditor : public VBoxContainer {
	GDCLASS(AudioGraphNodePluginDefaultEditor, VBoxContainer);
	AudioGraphEditor *editor = nullptr;
	Ref<Resource> parent_resource;
	int node_id = 0;

public:
	void _property_changed(const String &p_property, const Variant &p_value, const String &p_field = "", bool p_changing = false) {
		if (p_changing) {
			return;
		}

		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

		updating = true;
		undo_redo->create_action(vformat(TTR("Edit Audio Property: %s"), p_property), UndoRedo::MERGE_ENDS);
		undo_redo->add_do_property(node.ptr(), p_property, p_value);
		undo_redo->add_undo_property(node.ptr(), p_property, node->get(p_property));

		Ref<AudioStreamGraphNode> agnode = editor->get_audio_graph()->get_node(node_id);
		ERR_FAIL_COND(agnode.is_null());

		// Check for invalid connections due to removed ports.
		// We need to know the new state of the node to generate the proper undo/redo instructions.
		// Quite hacky but the best way I could come up with for now.
		Ref<AudioStreamGraphNode> agnode_new = agnode->duplicate();
		agnode_new->set(p_property, p_value);
		const int input_port_count = agnode_new->get_input_port_count();
		const int output_port_count = agnode_new->get_output_port_count();

		List<AudioStreamGraph::Connection> conns;
		editor->get_audio_graph()->get_node_connections(&conns);
		AudioGraphEditorPlugin *graph_plugin = editor->get_graph_plugin();
		bool undo_node_already_updated = false;
		for (const AudioStreamGraph::Connection &c : conns) {
			if ((c.from_node == node_id && c.from_port >= output_port_count) || (c.to_node == node_id && c.to_port >= input_port_count)) {
				undo_redo->add_do_method(editor->get_audio_graph().ptr(), "disconnect_nodes", c.from_node, c.from_port, c.to_node, c.to_port);
				undo_redo->add_do_method(graph_plugin, "disconnect_nodes", c.from_node, c.from_port, c.to_node, c.to_port);
				// We need to update the node before reconnecting to avoid accessing a non-existing port.
				undo_redo->add_undo_method(graph_plugin, "update_node_deferred", node_id);
				undo_node_already_updated = true;
				undo_redo->add_undo_method(editor->get_audio_graph().ptr(), "connect_nodes", c.from_node, c.from_port, c.to_node, c.to_port);
				undo_redo->add_undo_method(graph_plugin, "connect_nodes", c.from_node, c.from_port, c.to_node, c.to_port);
			}
		}

		if (p_value.get_type() == Variant::OBJECT) {
			Ref<Resource> prev_res = agnode->get(p_property);
			Ref<Resource> curr_res = p_value;

			if (curr_res.is_null()) {
				undo_redo->add_do_method(this, "_open_inspector", (Ref<Resource>)parent_resource.ptr());
			} else {
				undo_redo->add_do_method(this, "_open_inspector", (Ref<Resource>)curr_res.ptr());
			}
			if (!prev_res.is_null()) {
				undo_redo->add_undo_method(this, "_open_inspector", (Ref<Resource>)prev_res.ptr());
			} else {
				undo_redo->add_undo_method(this, "_open_inspector", (Ref<Resource>)parent_resource.ptr());
			}
		}
		if (p_property != "constant") {
			if (graph_plugin) {
				undo_redo->add_do_method(graph_plugin, "update_node_deferred", node_id);
				if (!undo_node_already_updated) {
					undo_redo->add_undo_method(graph_plugin, "update_node_deferred", node_id);
				}
			}
		}

		undo_redo->commit_action();

		updating = false;
	}

	void _node_changed() {
		if (updating) {
			return;
		}
		for (int i = 0; i < properties.size(); i++) {
			properties[i]->update_property();
		}
	}

	void _resource_selected(const String &p_path, Ref<Resource> p_resource) {
		_open_inspector(p_resource);
	}

	void _open_inspector(Ref<Resource> p_resource) {
		InspectorDock::get_inspector_singleton()->edit(p_resource.ptr());
	}

	bool updating = false;
	Ref<AudioStreamGraphNode> node;
	Vector<EditorProperty *> properties;
	Vector<Label *> prop_names;

	void _show_prop_names(bool p_show) {
		for (int i = 0; i < prop_names.size(); i++) {
			prop_names[i]->set_visible(p_show);
		}
	}

	void setup(AudioGraphEditor *p_editor, Ref<Resource> p_parent_resource, const Vector<EditorProperty *> &p_properties, const Vector<StringName> &p_names, const HashMap<StringName, String> &p_overrided_names, Ref<AudioStreamGraphNode> p_node) {
		editor = p_editor;
		parent_resource = p_parent_resource;
		updating = false;
		node = p_node;
		properties = p_properties;

		node_id = (int)p_node->get_meta("id");

		for (int i = 0; i < p_properties.size(); i++) {
			HBoxContainer *hbox = memnew(HBoxContainer);
			hbox->set_h_size_flags(SIZE_EXPAND_FILL);
			add_child(hbox);

			Label *prop_name = memnew(Label);
			String prop_name_str = p_names[i];
			if (p_overrided_names.has(p_names[i])) {
				prop_name_str = p_overrided_names[p_names[i]] + ":";
			} else {
				prop_name_str = prop_name_str.capitalize() + ":";
			}
			prop_name->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // TODO: Implement proper translation switch.
			prop_name->set_text(prop_name_str);
			prop_name->set_visible(false);
			hbox->add_child(prop_name);
			prop_names.push_back(prop_name);

			p_properties[i]->set_h_size_flags(SIZE_EXPAND_FILL);
			hbox->add_child(p_properties[i]);

			bool res_prop = Object::cast_to<EditorPropertyResource>(p_properties[i]);
			if (res_prop) {
				p_properties[i]->connect("resource_selected", callable_mp(this, &AudioGraphNodePluginDefaultEditor::_resource_selected));
			}

			properties[i]->connect("property_changed", callable_mp(this, &AudioGraphNodePluginDefaultEditor::_property_changed));
			properties[i]->set_object_and_property(node.ptr(), p_names[i]);
			properties[i]->update_property();
			properties[i]->set_name_split_ratio(0);
		}
		node->connect_changed(callable_mp(this, &AudioGraphNodePluginDefaultEditor::_node_changed));
	}

	static void _bind_methods() {
		ClassDB::bind_method("_open_inspector", &AudioGraphNodePluginDefaultEditor::_open_inspector); // Used by UndoRedo.
		ClassDB::bind_method("_show_prop_names", &AudioGraphNodePluginDefaultEditor::_show_prop_names); // Used with call_deferred.
	}
};

Control *AudioGraphNodePluginDefault::create_editor(const Ref<Resource> &p_parent_resource, const Ref<AudioStreamGraphNode> &p_node) {
	Ref<AudioStreamGraph> p_graph = Ref<AudioStreamGraph>(p_parent_resource.ptr());

	Vector<StringName> properties = p_node->get_editable_properties();
	if (properties.size() == 0) {
		return nullptr;
	}

	List<PropertyInfo> props;
	p_node->get_property_list(&props);

	Vector<PropertyInfo> pinfo;

	for (const PropertyInfo &E : props) {
		for (int i = 0; i < properties.size(); i++) {
			if (E.name == String(properties[i])) {
				pinfo.push_back(E);
			}
		}
	}

	if (pinfo.size() == 0) {
		return nullptr;
	}

	properties.clear();

	Ref<AudioStreamGraphNode> node = p_node;
	Vector<EditorProperty *> editors;

	for (int i = 0; i < pinfo.size(); i++) {
		EditorProperty *prop = EditorInspector::instantiate_property_editor(node.ptr(), pinfo[i].type, pinfo[i].name, pinfo[i].hint, pinfo[i].hint_string, pinfo[i].usage);
		if (!prop) {
			return nullptr;
		}

		if (Object::cast_to<EditorPropertyResource>(prop)) {
			Object::cast_to<EditorPropertyResource>(prop)->set_use_sub_inspector(false);
			prop->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
		}

		editors.push_back(prop);
		properties.push_back(pinfo[i].name);
	}

	AudioGraphNodePluginDefaultEditor *editor = memnew(AudioGraphNodePluginDefaultEditor);
	editor->setup(ageditor, p_parent_resource, editors, properties, p_node->get_editable_properties_names(), p_node);
	return editor;
}

///////////////////

void AGGraphNode::_draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color, const Color &p_rim_color) {
	Ref<Texture2D> port_icon = p_left ? get_slot_custom_icon_left(p_slot_index) : get_slot_custom_icon_right(p_slot_index);

	Point2 icon_offset;
	if (!port_icon.is_valid()) {
		port_icon = get_theme_icon(SNAME("port"), SNAME("GraphNode"));
	}

	icon_offset = -port_icon->get_size() * 0.5;

	// Draw "shadow"/outline in the connection rim color.
	draw_texture_rect(port_icon, Rect2(p_pos + (icon_offset - Size2(2, 2)) * EDSCALE, (port_icon->get_size() + Size2(4, 4)) * EDSCALE), false, p_rim_color);
	draw_texture_rect(port_icon, Rect2(p_pos + icon_offset * EDSCALE, port_icon->get_size() * EDSCALE), false, p_color);
}

void AGGraphNode::draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) {
	Color rim_color = get_theme_color(SNAME("connection_rim_color"), SNAME("GraphEdit"));
	_draw_port(p_slot_index, p_pos, p_left, p_color, rim_color);
}

///////////////////

void AGRerouteNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			connect(SceneStringName(mouse_entered), callable_mp(this, &AGRerouteNode::_on_mouse_entered));
			connect(SceneStringName(mouse_exited), callable_mp(this, &AGRerouteNode::_on_mouse_exited));
		} break;
		case NOTIFICATION_DRAW: {
			Vector2 offset = Vector2(0, -16 * EDSCALE);
			Color drag_bg_color = get_theme_color(SNAME("drag_background"), SNAME("AGRerouteNode"));
			draw_circle(get_size() * 0.5 + offset, 16 * EDSCALE, Color(drag_bg_color, selected ? 1 : icon_opacity), true, -1, true);

			Ref<Texture2D> icon = get_editor_theme_icon(SNAME("ToolMove"));
			Point2 icon_offset = -icon->get_size() * 0.5 + get_size() * 0.5 + offset;
			draw_texture(icon, icon_offset, Color(1, 1, 1, selected ? 1 : icon_opacity));
		} break;
	}
}

void AGRerouteNode::draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) {
	Color rim_color = selected ? get_theme_color("selected_rim_color", "AGRerouteNode") : get_theme_color("connection_rim_color", "GraphEdit");
	_draw_port(p_slot_index, p_pos, p_left, p_color, rim_color);
}

AGRerouteNode::AGRerouteNode() {
	Label *title_lbl = Object::cast_to<Label>(get_titlebar_hbox()->get_child(0));
	title_lbl->hide();

	const Size2 size = Size2(32, 32) * EDSCALE;

	Control *slot_area = memnew(Control);
	slot_area->set_custom_minimum_size(size);
	slot_area->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(slot_area);

	// Lay the input and output ports on top of each other to create the illusion of a single port.
	add_theme_constant_override("port_h_offset", size.width / 2);
}

void AGRerouteNode::set_icon_opacity(float p_opacity) {
	icon_opacity = p_opacity;
	queue_redraw();
}

void AGRerouteNode::_on_mouse_entered() {
	Ref<Tween> tween = create_tween();
	tween->tween_method(callable_mp(this, &AGRerouteNode::set_icon_opacity), 0.0, 1.0, FADE_ANIMATION_LENGTH_SEC);
}

void AGRerouteNode::_on_mouse_exited() {
	Ref<Tween> tween = create_tween();
	tween->tween_method(callable_mp(this, &AGRerouteNode::set_icon_opacity), 1.0, 0.0, FADE_ANIMATION_LENGTH_SEC);
}

void AudioGraphEditorPlugin::edit(Object *p_object) {
	audio_graph_editor->edit(Object::cast_to<AudioStreamGraph>(p_object));
}

bool AudioGraphEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("AudioStreamGraph");
}

void AudioGraphEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		editor_button->show();
		EditorNode::get_bottom_panel()->make_item_visible(audio_graph_editor);
		audio_graph_editor->set_process(true);
	} else {
		if (audio_graph_editor->is_visible_in_tree()) {
			EditorNode::get_bottom_panel()->hide_bottom_panel();
		}
		editor_button->hide();
		audio_graph_editor->set_process(false);
	}
}

void AudioGraphEditorPlugin::clear_links() {
	links.clear();
}

void AudioGraphEditorPlugin::register_link(int p_id, AudioStreamGraphNode *p_audio_node, GraphElement *p_graph_element) {
	links.insert(p_id, {
							   p_audio_node,
							   p_graph_element,
					   });
}

void AudioGraphEditorPlugin::register_default_input_button(int p_node_id, int p_port_id, Button *p_button) {
	links[p_node_id].input_ports.insert(p_port_id, { p_button });
}

void AudioGraphEditorPlugin::register_parameter_name(int p_node_id, LineEdit *p_parameter_name) {
	links[p_node_id].parameter_name = p_parameter_name;
}

void AudioGraphEditorPlugin::add_node(int p_id, bool p_just_update) {
	if (!audio_graph_editor->audio_graph) {
		return;
	}
	AudioStreamGraph *agraph = audio_graph_editor->audio_graph;
	Ref<AudioStreamGraphNode> agnode = agraph->get_node(p_id);
	ERR_FAIL_COND(agnode.is_null());
	GraphElement *node;
	// if (is_frame) {
	// 	GraphFrame *frame = memnew(GraphFrame);
	// 	frame->set_title(vsnode->get_caption());
	// 	node = frame;
	// } else if (is_reroute) {
	// 	VSRerouteNode *reroute_gnode = memnew(VSRerouteNode);
	// 	reroute_gnode->set_ignore_invalid_connection_type(true);
	// 	node = reroute_gnode;
	// } else {

	// }

	const Color type_color[AudioStreamGraphNode::PortType::PORT_TYPE_MAX] = {
		Color(1.0, 1.0, 1.0),
		Color(0.0, 1.0, 0.0)
	};

	AGGraphNode *gnode = memnew(AGGraphNode);
	gnode->set_title(agnode->get_caption());
	node = gnode;
	node->set_name(itos(p_id));
	node->set_position_offset(agraph->get_node_position(p_id));
	node->connect("dragged", callable_mp(audio_graph_editor, &AudioGraphEditor::_node_dragged).bind(p_id));
	Control *content_offset = memnew(Control);
	content_offset->set_custom_minimum_size(Size2(0, 5 * EDSCALE));
	node->add_child(content_offset);

	if (p_id >= 2) {
		agnode->set_deletable(true);
		node->connect("delete_request", callable_mp(audio_graph_editor, &AudioGraphEditor::_delete_node_request).bind(p_id), CONNECT_DEFERRED);
	}

	if (p_just_update) {
		Link &link = links[p_id];

		link.audio_node = agnode.ptr();
		link.graph_element = node;
		// link.output_ports.clear();
		link.input_ports.clear();
	} else {
		register_link(p_id, agnode.ptr(), node);
	}

	int port_offset = 1;

	Ref<AudioStreamGraphNodeParameter> parameter = agnode;

	if (parameter.is_valid()) {
		LineEdit *parameter_name = memnew(LineEdit);
		register_parameter_name(p_id, parameter_name);
		parameter_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		parameter_name->set_text(parameter->get_parameter_name());
		parameter_name->connect("text_submitted", callable_mp(audio_graph_editor, &AudioGraphEditor::_parameter_line_edit_changed).bind(p_id));
		parameter_name->connect(SceneStringName(focus_exited), callable_mp(audio_graph_editor, &AudioGraphEditor::_parameter_line_edit_focus_out).bind(parameter_name, p_id));

		if (agnode->get_output_port_count() == 1 && agnode->get_output_port_name(0) == "") {
			HBoxContainer *hb = nullptr;
			hb = memnew(HBoxContainer);
			hb->add_child(parameter_name);
			node->add_child(hb);
		} else {
			node->add_child(parameter_name);
		}
		port_offset++;
	}

	Control *custom_editor = nullptr;
	for (int i = 0; i < audio_graph_editor->plugins.size(); i++) {
		agnode->set_meta("id", p_id);
		custom_editor = audio_graph_editor->plugins.write[i]->create_editor(agraph, agnode);
		agnode->remove_meta("id");
		if (custom_editor) {
			if (agnode->is_show_prop_names()) {
				custom_editor->call_deferred(SNAME("_show_prop_names"), true);
			}
			break;
		}
	}

	if (custom_editor) {
		port_offset++;
		node->add_child(custom_editor);
		custom_editor = nullptr;
	}

	GraphNode *graph_node = Object::cast_to<GraphNode>(node);
	int max_ports = MAX(agnode->get_input_port_count(), agnode->get_output_port_count());

	HBoxContainer *hb = nullptr;

	for (int i = 0; i < max_ports; i++) {
		bool is_first_hbox = false;
		if (i == 0 && hb != nullptr) {
			is_first_hbox = true;
		} else {
			hb = memnew(HBoxContainer);
		}

		hb->add_theme_constant_override("separation", 7 * EDSCALE);

		bool valid_left = i < agnode->get_input_port_count();
		bool port_left_used = false;
		AudioStreamGraphNode::PortType port_left = AudioStreamGraphNode::PORT_TYPE_STREAM;
		String name_left;
		if (valid_left) {
			name_left = agnode->get_input_port_name(i);
			port_left = agnode->get_input_port_type(i);
			for (const AudioStreamGraph::Connection &E : connections) {
				if (E.to_node == p_id && E.to_port == i) {
					port_left_used = true;
					break;
				}
			}
		}

		if (valid_left) {
			String name = name_left;
			Label *label = memnew(Label);
			label->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED); // TODO: Implement proper translation switch.
			label->set_text(name);
			//label->add_theme_style_override(CoreStringName(normal), editor->get_theme_stylebox(SNAME("label_style"), SNAME("VShaderEditor")));
			hb->add_child(label);
		}

		bool valid_right = i < agnode->get_output_port_count();
		AudioStreamGraphNode::PortType port_right = AudioStreamGraphNode::PORT_TYPE_STREAM;
		String name_right;
		if (valid_right) {
			name_right = agnode->get_output_port_name(i);
			port_right = agnode->get_output_port_type(i);
			String name = agnode->get_output_port_name(i);
			Label *label = memnew(Label);
			label->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED); // TODO: Implement proper translation switch.
			label->set_text(name);
			//label->add_theme_style_override(CoreStringName(normal), editor->get_theme_stylebox(SNAME("label_style"), SNAME("VShaderEditor")));
			hb->add_child(label);
		}

		Variant default_value;

		if (valid_left && !port_left_used) {
			default_value = agnode->get_input_port_default_value(i);
		}

		Button *default_input_btn = memnew(Button);
		hb->add_child(default_input_btn);
		register_default_input_button(p_id, i, default_input_btn);
		default_input_btn->connect(SceneStringName(pressed), callable_mp(audio_graph_editor, &AudioGraphEditor::_edit_port_default_input).bind(default_input_btn, p_id, i));
		if (default_value.get_type() != Variant::NIL) { // only a label
			set_input_port_default_value(p_id, i, default_value);
		} else {
			default_input_btn->hide();
		}

		if (!is_first_hbox) {
			hb->add_spacer();
		}

		int idx = i + port_offset;

		graph_node->set_slot(idx, valid_left, port_left, type_color[port_left], valid_right, port_right, type_color[port_right]);

		node->add_child(hb);
	}

	if (agnode->get_port_group_count() > 1) {
		HBoxContainer *button_container = memnew(HBoxContainer);
		Button *add_input_btn = memnew(Button);
		add_input_btn->set_text(TTR("Add Input"));
		add_input_btn->connect(SceneStringName(pressed), callable_mp(audio_graph_editor, &AudioGraphEditor::_add_input_port).bind(p_id), CONNECT_DEFERRED);

		Button *remove_input_btn = memnew(Button);
		remove_input_btn->set_text(TTR("Remove Input"));
		remove_input_btn->set_disabled(agnode->get_port_group_count() == 2);
		remove_input_btn->connect(SceneStringName(pressed), callable_mp(audio_graph_editor, &AudioGraphEditor::_remove_input_port).bind(p_id), CONNECT_DEFERRED);

		button_container->add_child(add_input_btn);
		button_container->add_child(remove_input_btn);
		node->add_child(button_container);
	}

	audio_graph_editor->graph->add_child(node);
}

void AudioGraphEditorPlugin::remove_node(int p_id, bool p_just_update) {
	if (links.has(p_id)) {
		GraphEdit *graph_edit = audio_graph_editor->graph;
		if (!graph_edit) {
			return;
		}

		graph_edit->remove_child(links[p_id].graph_element);
		memdelete(links[p_id].graph_element);
		if (!p_just_update) {
			links.erase(p_id);
		}
	}
}

void AudioGraphEditorPlugin::update_node(int p_node_id) {
	if (!links.has(p_node_id)) {
		return;
	}
	remove_node(p_node_id, true);
	add_node(p_node_id, true);

	// TODO: Restore focus here?
}

void AudioGraphEditorPlugin::update_node_deferred(int p_node_id) {
	callable_mp(this, &AudioGraphEditorPlugin::update_node).call_deferred(p_node_id);
}

void AudioGraphEditorPlugin::connect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	GraphEdit *graph = audio_graph_editor->graph;
	if (!graph) {
		return;
	}

	if (audio_graph_editor->audio_graph) {
		graph->connect_node(itos(p_from_node), p_from_port, itos(p_to_node), p_to_port);

		connections.push_back({ p_from_node, p_from_port, p_to_node, p_to_port });
		if (links[p_to_node].input_ports.has(p_to_port) && links[p_to_node].input_ports[p_to_port].default_input_button != nullptr) {
			links[p_to_node].input_ports[p_to_port].default_input_button->hide();
		}
	}
}

void AudioGraphEditorPlugin::disconnect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	GraphEdit *graph = audio_graph_editor->graph;
	if (!graph) {
		return;
	}

	if (audio_graph_editor->audio_graph) {
		graph->disconnect_node(itos(p_from_node), p_from_port, itos(p_to_node), p_to_port);

		for (List<AudioStreamGraph::Connection>::Element *E = connections.front(); E; E = E->next()) {
			if (E->get().from_node == p_from_node && E->get().from_port == p_from_port && E->get().to_node == p_to_node && E->get().to_port == p_to_port) {
				connections.erase(E);
				break;
			}
		}
		if (links[p_to_node].input_ports.has(p_to_port) && links[p_to_node].input_ports[p_to_port].default_input_button != nullptr && links[p_to_node].audio_node->get_input_port_default_value(p_to_port).get_type() != Variant::NIL) {
			links[p_to_node].input_ports[p_to_port].default_input_button->show();
			set_input_port_default_value(p_to_node, p_to_port, links[p_to_node].audio_node->get_input_port_default_value(p_to_port));
		}
	}
}

void AudioGraphEditorPlugin::set_node_position(int p_id, const Vector2 &p_position) {
	if (links.has(p_id)) {
		links[p_id].graph_element->set_position_offset(p_position);
	}
}

void AudioGraphEditorPlugin::set_connections(const List<AudioStreamGraph::Connection> &p_connections) {
	connections = p_connections;
}

void AudioGraphEditorPlugin::set_input_port_default_value(int p_node_id, int p_port_id, const Variant &p_value) {
	if (!links.has(p_node_id)) {
		return;
	}

	Button *button = links[p_node_id].input_ports[p_port_id].default_input_button;

	switch (p_value.get_type()) {
		case Variant::BOOL: {
			button->set_text(((bool)p_value) ? "true" : "false");
		} break;
		case Variant::INT:
		case Variant::FLOAT: {
			button->set_text(String::num(p_value, 4));
		} break;
		default: {
		}
	}
}

void AudioGraphEditorPlugin::_bind_methods() {
	ClassDB::bind_method("add_node", &AudioGraphEditorPlugin::add_node);
	ClassDB::bind_method("update_node", &AudioGraphEditorPlugin::update_node);
	ClassDB::bind_method("update_node_deferred", &AudioGraphEditorPlugin::update_node_deferred);
	ClassDB::bind_method("remove_node", &AudioGraphEditorPlugin::remove_node);
	ClassDB::bind_method("connect_nodes", &AudioGraphEditorPlugin::connect_nodes);
	ClassDB::bind_method("disconnect_nodes", &AudioGraphEditorPlugin::disconnect_nodes);
	ClassDB::bind_method("set_node_position", &AudioGraphEditorPlugin::set_node_position);
	ClassDB::bind_method("set_input_port_default_value", &AudioGraphEditorPlugin::set_input_port_default_value);
}

AudioGraphEditorPlugin::AudioGraphEditorPlugin() {
	audio_graph_editor = memnew(AudioGraphEditor);
	audio_graph_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);

	audio_graph_editor->graph_plugin = this;

	editor_button = EditorNode::get_bottom_panel()->add_item(TTR("Audio Graph"), audio_graph_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_animation_tree_bottom_panel", TTR("Toggle AnimationTree Bottom Panel")));
	editor_button->hide();
}

AudioGraphEditorPlugin::~AudioGraphEditorPlugin() {
}