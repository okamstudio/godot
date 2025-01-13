/**************************************************************************/
/*  animation_state_machine_editor.cpp                                    */
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

#include "animation_state_machine_editor.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/resource_loader.h"
#include "core/math/geometry_2d.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "scene/theme/theme_db.h"

bool AnimationNodeStateMachineEditor::can_edit(const Ref<AnimationNode> &p_node) {
	Ref<AnimationNodeStateMachine> ansm = p_node;
	return ansm.is_valid();
}

void AnimationNodeStateMachineEditor::edit(const Ref<AnimationNode> &p_node) {
	state_machine = p_node;

	read_only = false;

	if (state_machine.is_valid()) {
		read_only = EditorNode::get_singleton()->is_resource_read_only(state_machine);

		selected_transition_from = StringName();
		selected_transition_to = StringName();
		selected_transition_index = -1;
		selected_node = StringName();
		selected_nodes.clear();
		connected_nodes.clear();
		_update_mode();
		_update_graph();
	}

	if (read_only) {
		tool_create->set_pressed(false);
		tool_connect->set_pressed(false);
	}

	tool_create->set_disabled(read_only);
	tool_connect->set_disabled(read_only);
}

String AnimationNodeStateMachineEditor::_get_root_playback_path(String &r_node_directory) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	Vector<String> edited_path = AnimationTreeEditor::get_singleton()->get_edited_path();

	String base_path;
	Vector<String> node_directory_path;

	bool is_playable_anodesm_found = false;

	if (edited_path.size()) {
		while (!is_playable_anodesm_found) {
			base_path = String("/").join(edited_path);
			Ref<AnimationNodeStateMachine> anodesm = !edited_path.size() ? Ref<AnimationNode>(tree->get_root_animation_node().ptr()) : tree->get_root_animation_node()->find_node_by_path(base_path);
			if (!anodesm.is_valid()) {
				break;
			} else {
				if (anodesm->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
					is_playable_anodesm_found = true;
				} else {
					int idx = edited_path.size() - 1;
					node_directory_path.push_back(edited_path[idx]);
					edited_path.remove_at(idx);
				}
			}
		}
	}

	if (is_playable_anodesm_found) {
		// Return Root/Nested state machine playback.
		node_directory_path.reverse();
		r_node_directory = String("/").join(node_directory_path);
		if (node_directory_path.size()) {
			r_node_directory += "/";
		}
		base_path = !edited_path.size() ? Animation::PARAMETERS_BASE_PATH + "playback" : Animation::PARAMETERS_BASE_PATH + base_path + "/playback";
	} else {
		// Hmmm, we have to return Grouped state machine playback...
		// It will give the user the error that Root/Nested state machine should be retrieved, that would be kind :-)
		r_node_directory = String();
		base_path = AnimationTreeEditor::get_singleton()->get_base_path() + "playback";
	}

	return base_path;
}

void AnimationNodeStateMachineEditor::_state_machine_gui_input(const Ref<InputEvent> &p_event) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	String node_directory;
	Ref<AnimationNodeStateMachinePlayback> playback = tree->get(_get_root_playback_path(node_directory));
	if (!playback.is_valid()) {
		return;
	}

	Ref<InputEventKey> k = p_event;
	if (tool_select->is_pressed() && k.is_valid() && k->is_pressed() && k->get_keycode() == Key::KEY_DELETE && !k->is_echo()) {
		if (selected_node != StringName() || !selected_nodes.is_empty() || selected_transition_to != StringName() || selected_transition_from != StringName()) {
			if (!read_only) {
				_erase_selected();
			}
			accept_event();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	// Add new node
	if (!read_only) {
		if (mb.is_valid() && mb->is_pressed() && !box_selecting && !connecting && ((tool_select->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) || (tool_create->is_pressed() && mb->get_button_index() == MouseButton::LEFT))) {
			connecting_from = StringName();
			_open_menu(mb->get_position());
		}
	}

	// Select node or push a field inside
	if (mb.is_valid() && !mb->is_shift_pressed() && !mb->is_command_or_control_pressed() && mb->is_pressed() && tool_select->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		selected_transition_from = StringName();
		selected_transition_to = StringName();
		selected_transition_index = -1;
		selected_node = StringName();

		for (int i = node_rects.size() - 1; i >= 0; i--) { //inverse to draw order
			if (node_rects[i].play.has_point(mb->get_position())) { //edit name
				if (play_mode->get_selected() == 1 || !playback->is_playing()) {
					// Start
					playback->start(node_directory + String(node_rects[i].node_name));
				} else {
					// Travel
					playback->travel(node_directory + String(node_rects[i].node_name));
				}
				state_machine_draw->queue_redraw();
				return;
			}

			if (!read_only) {
				if (node_rects[i].name.has_point(mb->get_position()) && state_machine->can_edit_node(node_rects[i].node_name)) { // edit name
					// TODO: Avoid using strings, expose a method on LineEdit.
					Ref<StyleBox> line_sb = name_edit->get_theme_stylebox(CoreStringName(normal));
					Rect2 edit_rect = node_rects[i].name;
					edit_rect.position -= line_sb->get_offset();
					edit_rect.size += line_sb->get_minimum_size();

					name_edit_popup->set_position(state_machine_draw->get_screen_position() + edit_rect.position);
					name_edit_popup->set_size(edit_rect.size);
					name_edit->set_text(node_rects[i].node_name);
					name_edit_popup->popup();
					name_edit->grab_focus();
					name_edit->select_all();

					prev_name = node_rects[i].node_name;
					return;
				}
			}

			if (node_rects[i].edit.has_point(mb->get_position())) { //edit name
				callable_mp(this, &AnimationNodeStateMachineEditor::_open_editor).call_deferred(node_rects[i].node_name);
				return;
			}

			if (node_rects[i].node.has_point(mb->get_position())) { //select node since nothing else was selected
				selected_node = node_rects[i].node_name;

				if (!selected_nodes.has(selected_node)) {
					selected_nodes.clear();
				}

				selected_nodes.insert(selected_node);
				_update_connected_nodes(selected_node);

				Ref<AnimationNode> anode = state_machine->get_node(selected_node);
				EditorNode::get_singleton()->push_item(anode.ptr(), "", true);
				state_machine_draw->queue_redraw();
				dragging_selected_attempt = true;
				dragging_selected = false;
				drag_from = mb->get_position();
				snap_x = StringName();
				snap_y = StringName();
				_update_mode();
				return;
			}
		}

		// Test the transition lines.
		int closest = -1;
		float closest_d = 1e20;
		Vector<int> close_candidates;

		// First find closest lines using point-to-segment distance.
		for (int i = 0; i < transition_lines.size(); i++) {
			Vector2 s[2] = {
				transition_lines[i].from,
				transition_lines[i].to
			};
			Vector2 cpoint = Geometry2D::get_closest_point_to_segment(mb->get_position(), s);
			float d = cpoint.distance_to(mb->get_position());

			if (d > transition_lines[i].width) {
				continue;
			}

			// If this is very close to our current closest distance, add it to candidates.
			if (Math::abs(d - closest_d) < 2.0) { // Within 2 pixels.
				close_candidates.push_back(i);
			} else if (d < closest_d) {
				closest_d = d;
				closest = i;
				close_candidates.clear();
				close_candidates.push_back(i);
			}
		}

		// Use midpoint distance as bias.
		if (close_candidates.size() > 1) {
			float best_midpoint_dist = 1e20;

			for (int idx : close_candidates) {
				Vector2 midpoint = (transition_lines[idx].from + transition_lines[idx].to) / 2.0;
				float midpoint_dist = midpoint.distance_to(mb->get_position());

				if (midpoint_dist < best_midpoint_dist) {
					best_midpoint_dist = midpoint_dist;
					closest = idx;
				}
			}
		}

		if (closest >= 0) {
			selected_transition_from = transition_lines[closest].from_node;
			selected_transition_to = transition_lines[closest].to_node;
			selected_transition_index = closest;

			// Update connected_nodes for the selected transition.
			connected_nodes.clear();
			connected_nodes.insert(selected_transition_from);
			connected_nodes.insert(selected_transition_to);

			Ref<AnimationNodeStateMachineTransition> tr = state_machine->get_transition(closest);
			if (!state_machine->is_transition_across_group(closest)) {
				EditorNode::get_singleton()->push_item(tr.ptr(), "", true);
			} else {
				EditorNode::get_singleton()->push_item(tr.ptr(), "", true);
				EditorNode::get_singleton()->push_item(nullptr, "", true);
			}
		}

		state_machine_draw->queue_redraw();
		_update_mode();
	}

	// End moving node
	if (mb.is_valid() && dragging_selected_attempt && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed()) {
		if (dragging_selected) {
			Ref<AnimationNode> an = state_machine->get_node(selected_node);
			updating = true;

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Move Node"));

			for (int i = 0; i < node_rects.size(); i++) {
				if (!selected_nodes.has(node_rects[i].node_name)) {
					continue;
				}

				undo_redo->add_do_method(state_machine.ptr(), "set_node_position", node_rects[i].node_name, state_machine->get_node_position(node_rects[i].node_name) + drag_ofs / EDSCALE);
				undo_redo->add_undo_method(state_machine.ptr(), "set_node_position", node_rects[i].node_name, state_machine->get_node_position(node_rects[i].node_name));
			}

			undo_redo->add_do_method(this, "_update_graph");
			undo_redo->add_undo_method(this, "_update_graph");
			undo_redo->commit_action();
			updating = false;
		}
		snap_x = StringName();
		snap_y = StringName();

		dragging_selected_attempt = false;
		dragging_selected = false;
		state_machine_draw->queue_redraw();
	}

	// Connect nodes
	if (mb.is_valid() && ((tool_select->is_pressed() && mb->is_shift_pressed()) || tool_connect->is_pressed()) && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
		for (int i = node_rects.size() - 1; i >= 0; i--) { //inverse to draw order
			if (node_rects[i].node.has_point(mb->get_position())) { //select node since nothing else was selected
				connecting = true;
				connection_follows_cursor = true;
				connecting_from = node_rects[i].node_name;
				connecting_to = mb->get_position();
				connecting_to_node = StringName();
				return;
			}
		}
	}

	// End connecting nodes
	if (mb.is_valid() && connecting && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed()) {
		if (connecting_to_node != StringName()) {
			Ref<AnimationNode> node = state_machine->get_node(connecting_to_node);
			Ref<AnimationNodeStateMachine> anodesm = node;
			Ref<AnimationNodeEndState> end_node = node;

			if (state_machine->has_transition(connecting_from, connecting_to_node) && state_machine->can_edit_node(connecting_to_node) && !anodesm.is_valid()) {
				EditorNode::get_singleton()->show_warning(TTR("Transition exists!"));
				connecting = false;
			} else {
				_add_transition();
			}
		} else {
			_open_menu(mb->get_position());
		}
		connecting_to_node = StringName();
		connection_follows_cursor = false;
		state_machine_draw->queue_redraw();
	}

	// Start box selecting
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT && tool_select->is_pressed()) {
		box_selecting = true;
		box_selecting_from = box_selecting_to = state_machine_draw->get_local_mouse_position();
		box_selecting_rect = Rect2(box_selecting_from.min(box_selecting_to), (box_selecting_from - box_selecting_to).abs());

		if (mb->is_command_or_control_pressed() || mb->is_shift_pressed()) {
			previous_selected = selected_nodes;
		} else {
			selected_nodes.clear();
			previous_selected.clear();
		}
	}

	// End box selecting
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed() && box_selecting) {
		box_selecting = false;
		state_machine_draw->queue_redraw();
		_update_mode();
	}

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		StringName clicked_node;
		for (int i = node_rects.size() - 1; i >= 0; i--) {
			if (node_rects[i].node.has_point(mb->get_position())) {
				clicked_node = node_rects[i].node_name;
				break;
			}
		}

		if (clicked_node != StringName()) {
			if (selected_nodes.has(clicked_node) && mb->is_shift_pressed()) {
				selected_nodes.erase(clicked_node);
			} else {
				if (!mb->is_shift_pressed()) {
					selected_nodes.clear();
				}
				selected_nodes.insert(clicked_node);
			}
			selected_node = clicked_node;
		} else {
			// Clicked on empty space.
			selected_nodes.clear();
			selected_node = StringName();
		}

		_update_connected_nodes(selected_node);
		state_machine_draw->queue_redraw();
		_update_mode();

		if (clicked_node != StringName()) {
			Ref<AnimationNode> anode = state_machine->get_node(clicked_node);
			EditorNode::get_singleton()->push_item(anode.ptr(), "", true);
			dragging_selected_attempt = true;
			dragging_selected = false;
			drag_from = mb->get_position();
			snap_x = StringName();
			snap_y = StringName();
		}

		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;

	// Pan window
	if (mm.is_valid() && mm->get_button_mask().has_flag(MouseButtonMask::MIDDLE)) {
		h_scroll->set_value(h_scroll->get_value() - mm->get_relative().x);
		v_scroll->set_value(v_scroll->get_value() - mm->get_relative().y);
	}

	// Move mouse while connecting
	if (mm.is_valid() && connecting && connection_follows_cursor && !read_only) {
		connecting_to = mm->get_position();
		connecting_to_node = StringName();
		state_machine_draw->queue_redraw();

		for (int i = node_rects.size() - 1; i >= 0; i--) { //inverse to draw order
			if (node_rects[i].node_name != connecting_from && node_rects[i].node.has_point(connecting_to)) { //select node since nothing else was selected
				connecting_to_node = node_rects[i].node_name;
				return;
			}
		}
	}

	// Move mouse while moving a node
	if (mm.is_valid() && dragging_selected_attempt && !read_only) {
		dragging_selected = true;
		drag_ofs = mm->get_position() - drag_from;
		snap_x = StringName();
		snap_y = StringName();
		{
			//snap
			Vector2 cpos = state_machine->get_node_position(selected_node) + drag_ofs / EDSCALE;
			List<StringName> nodes;
			state_machine->get_node_list(&nodes);

			float best_d_x = 1e20;
			float best_d_y = 1e20;

			for (const StringName &E : nodes) {
				if (E == selected_node) {
					continue;
				}
				Vector2 npos = state_machine->get_node_position(E);

				float d_x = ABS(npos.x - cpos.x);
				if (d_x < MIN(5, best_d_x)) {
					drag_ofs.x -= cpos.x - npos.x;
					best_d_x = d_x;
					snap_x = E;
				}

				float d_y = ABS(npos.y - cpos.y);
				if (d_y < MIN(5, best_d_y)) {
					drag_ofs.y -= cpos.y - npos.y;
					best_d_y = d_y;
					snap_y = E;
				}
			}
		}

		state_machine_draw->queue_redraw();
	}

	// Move mouse while moving box select
	if (mm.is_valid() && box_selecting) {
		box_selecting_to = state_machine_draw->get_local_mouse_position();

		box_selecting_rect = Rect2(box_selecting_from.min(box_selecting_to), (box_selecting_from - box_selecting_to).abs());

		for (int i = 0; i < node_rects.size(); i++) {
			bool in_box = node_rects[i].node.intersects(box_selecting_rect);

			if (in_box) {
				if (previous_selected.has(node_rects[i].node_name)) {
					selected_nodes.erase(node_rects[i].node_name);
				} else {
					selected_nodes.insert(node_rects[i].node_name);
				}
			} else {
				if (previous_selected.has(node_rects[i].node_name)) {
					selected_nodes.insert(node_rects[i].node_name);
				} else {
					selected_nodes.erase(node_rects[i].node_name);
				}
			}
		}

		state_machine_draw->queue_redraw();
	}

	if (mm.is_valid()) {
		state_machine_draw->grab_focus();

		String new_hovered_node_name;
		HoveredNodeArea new_hovered_node_area = HOVER_NODE_NONE;
		if (tool_select->is_pressed()) {
			for (int i = node_rects.size() - 1; i >= 0; i--) { // Inverse to draw order.

				if (!state_machine->can_edit_node(node_rects[i].node_name)) {
					continue; // start/end node can't be edited
				}

				if (node_rects[i].node.has_point(mm->get_position())) {
					new_hovered_node_name = node_rects[i].node_name;
					if (node_rects[i].play.has_point(mm->get_position())) {
						new_hovered_node_area = HOVER_NODE_PLAY;
					} else if (node_rects[i].edit.has_point(mm->get_position())) {
						new_hovered_node_area = HOVER_NODE_EDIT;
					}
					break;
				}
			}
		}

		if (new_hovered_node_name != hovered_node_name || new_hovered_node_area != hovered_node_area) {
			hovered_node_name = new_hovered_node_name;
			hovered_node_area = new_hovered_node_area;
			state_machine_draw->queue_redraw();
		}

		// set tooltip for transition
		if (tool_select->is_pressed()) {
			int closest = -1;
			float closest_d = 1e20;
			for (int i = 0; i < transition_lines.size(); i++) {
				Vector2 s[2] = {
					transition_lines[i].from,
					transition_lines[i].to
				};
				Vector2 cpoint = Geometry2D::get_closest_point_to_segment(mm->get_position(), s);
				float d = cpoint.distance_to(mm->get_position());
				if (d > transition_lines[i].width) {
					continue;
				}

				if (d < closest_d) {
					closest = i;
					closest_d = d;
				}
			}

			if (closest >= 0) {
				String from = String(transition_lines[closest].from_node);
				String to = String(transition_lines[closest].to_node);
				String tooltip = from + " -> " + to;
				state_machine_draw->set_tooltip_text(tooltip);
			} else {
				state_machine_draw->set_tooltip_text("");
			}
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);
		v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);
	}
}

Control::CursorShape AnimationNodeStateMachineEditor::get_cursor_shape(const Point2 &p_pos) const {
	Control::CursorShape cursor_shape = get_default_cursor_shape();
	if (!read_only) {
		// Put ibeam (text cursor) over names to make it clearer that they are editable.
		Transform2D xform = panel->get_transform() * state_machine_draw->get_transform();
		Point2 pos = xform.xform_inv(p_pos);

		for (int i = node_rects.size() - 1; i >= 0; i--) { // Inverse to draw order.
			if (node_rects[i].node.has_point(pos)) {
				if (node_rects[i].name.has_point(pos)) {
					if (state_machine->can_edit_node(node_rects[i].node_name)) {
						cursor_shape = Control::CURSOR_IBEAM;
					}
				}
				break;
			}
		}
	}
	return cursor_shape;
}

String AnimationNodeStateMachineEditor::get_tooltip(const Point2 &p_pos) const {
	if (hovered_node_name == StringName()) {
		return AnimationTreeNodeEditorPlugin::get_tooltip(p_pos);
	}

	String tooltip_text;
	if (hovered_node_area == HOVER_NODE_PLAY) {
		tooltip_text = vformat(TTR("Play/Travel to %s"), hovered_node_name);
	} else if (hovered_node_area == HOVER_NODE_EDIT) {
		tooltip_text = vformat(TTR("Edit %s"), hovered_node_name);
	} else {
		tooltip_text = hovered_node_name;
	}

	return tooltip_text;
}

void AnimationNodeStateMachineEditor::_open_menu(const Vector2 &p_position) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	menu->clear(false);
	animations_menu->clear();
	animations_to_add.clear();

	List<StringName> animation_names;
	tree->get_animation_list(&animation_names);
	menu->add_submenu_node_item(TTR("Add Animation"), animations_menu);
	if (animation_names.is_empty()) {
		menu->set_item_disabled(menu->get_item_idx_from_text(TTR("Add Animation")), true);
	} else {
		for (const StringName &name : animation_names) {
			animations_menu->add_icon_item(theme_cache.animation_icon, name);
			animations_to_add.push_back(name);
		}
	}

	List<StringName> classes;
	ClassDB::get_inheriters_from_class("AnimationRootNode", &classes);
	classes.sort_custom<StringName::AlphCompare>();

	for (List<StringName>::Element *E = classes.front(); E; E = E->next()) {
		String name = String(E->get()).replace_first("AnimationNode", "");
		if (name == "Animation" || name == "StartState" || name == "EndState") {
			continue; // nope
		}
		int idx = menu->get_item_count();
		menu->add_item(vformat(TTR("Add %s"), name), idx);
		menu->set_item_metadata(idx, E->get());
	}
	Ref<AnimationNode> clipb = EditorSettings::get_singleton()->get_resource_clipboard();

	if (clipb.is_valid()) {
		menu->add_separator();
		menu->add_item(TTR("Paste"), MENU_PASTE);
	}
	menu->add_separator();
	menu->add_item(TTR("Load..."), MENU_LOAD_FILE);

	menu->set_position(state_machine_draw->get_screen_transform().xform(p_position));
	menu->popup();
	add_node_pos = p_position / EDSCALE + state_machine->get_graph_offset();
}

bool AnimationNodeStateMachineEditor::_create_submenu(PopupMenu *p_menu, Ref<AnimationNodeStateMachine> p_nodesm, const StringName &p_name, const StringName &p_path) {
	String prev_path;

	List<StringName> nodes;
	p_nodesm->get_node_list(&nodes);

	PopupMenu *nodes_menu = memnew(PopupMenu);
	nodes_menu->set_name(p_name);
	nodes_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_connect_to));
	p_menu->add_child(nodes_menu);

	bool node_added = false;
	for (const StringName &E : nodes) {
		if (p_nodesm->can_edit_node(E)) {
			Ref<AnimationNodeStateMachine> ansm = p_nodesm->get_node(E);

			String path = String(p_path) + "/" + E;

			if (ansm == state_machine) {
				end_menu->add_item(E, nodes_to_connect.size());
				nodes_to_connect.push_back(SceneStringName(End));
				continue;
			}

			if (ansm.is_valid()) {
				state_machine_menu->add_item(E, nodes_to_connect.size());
				nodes_to_connect.push_back(path);

				if (_create_submenu(nodes_menu, ansm, E, path)) {
					nodes_menu->add_submenu_item(E, E);
					node_added = true;
				}
			} else {
				nodes_menu->add_item(E, nodes_to_connect.size());
				nodes_to_connect.push_back(path);
				node_added = true;
			}
		}
	}

	return node_added;
}

void AnimationNodeStateMachineEditor::_stop_connecting() {
	connecting = false;
	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_delete_selected() {
	TreeItem *item = delete_tree->get_next_selected(nullptr);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	while (item) {
		if (!updating) {
			updating = true;
			undo_redo->create_action("Transition(s) Removed");
		}

		Vector<String> path = item->get_text(0).split(" -> ");

		selected_transition_from = path[0];
		selected_transition_to = path[1];
		_erase_selected(true);

		item = delete_tree->get_next_selected(item);
	}

	if (updating) {
		undo_redo->commit_action();
		updating = false;
	}
}

void AnimationNodeStateMachineEditor::_delete_all() {
	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Transition(s) Removed");
	_erase_selected(true);
	undo_redo->commit_action();
	updating = false;

	delete_window->hide();
}

void AnimationNodeStateMachineEditor::_delete_tree_draw() {
	TreeItem *item = delete_tree->get_next_selected(nullptr);
	while (item) {
		delete_window->get_cancel_button()->set_disabled(false);
		return;
	}
	delete_window->get_cancel_button()->set_disabled(true);
}

void AnimationNodeStateMachineEditor::_file_opened(const String &p_file) {
	file_loaded = ResourceLoader::load(p_file);
	if (file_loaded.is_valid()) {
		_add_menu_type(MENU_LOAD_FILE_CONFIRM);
	} else {
		EditorNode::get_singleton()->show_warning(TTR("This type of node can't be used. Only animation nodes are allowed."));
	}
}

void AnimationNodeStateMachineEditor::_add_menu_type(int p_index) {
	String base_name;
	Ref<AnimationRootNode> node;

	if (p_index == MENU_LOAD_FILE) {
		open_file->clear_filters();
		List<String> filters;
		ResourceLoader::get_recognized_extensions_for_type("AnimationRootNode", &filters);
		for (const String &E : filters) {
			open_file->add_filter("*." + E);
		}
		open_file->popup_file_dialog();
		return;
	} else if (p_index == MENU_LOAD_FILE_CONFIRM) {
		node = file_loaded;
		file_loaded.unref();
	} else if (p_index == MENU_PASTE) {
		node = EditorSettings::get_singleton()->get_resource_clipboard();

	} else {
		String type = menu->get_item_metadata(p_index);

		Object *obj = ClassDB::instantiate(type);
		ERR_FAIL_NULL(obj);
		AnimationNode *an = Object::cast_to<AnimationNode>(obj);
		ERR_FAIL_NULL(an);

		node = Ref<AnimationNode>(an);
		base_name = type.replace_first("AnimationNode", "");
	}

	if (!node.is_valid()) {
		EditorNode::get_singleton()->show_warning(TTR("This type of node can't be used. Only root nodes are allowed."));
		return;
	}

	if (base_name.is_empty()) {
		base_name = node->get_class().replace_first("AnimationNode", "");
	}

	int base = 1;
	String name = base_name;
	while (state_machine->has_node(name)) {
		base++;
		name = base_name + " " + itos(base);
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Node and Transition"));
	undo_redo->add_do_method(state_machine.ptr(), "add_node", name, node, add_node_pos);
	undo_redo->add_undo_method(state_machine.ptr(), "remove_node", name);
	connecting_to_node = name;
	_add_transition(true);
	undo_redo->commit_action();
	updating = false;

	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_add_animation_type(int p_index) {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	anim->set_animation(animations_to_add[p_index]);

	String base_name = animations_to_add[p_index].validate_node_name();
	int base = 1;
	String name = base_name;
	while (state_machine->has_node(name)) {
		base++;
		name = base_name + " " + itos(base);
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Node and Transition"));
	undo_redo->add_do_method(state_machine.ptr(), "add_node", name, anim, add_node_pos);
	undo_redo->add_undo_method(state_machine.ptr(), "remove_node", name);
	connecting_to_node = name;
	_add_transition(true);
	undo_redo->commit_action();
	updating = false;

	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_connect_to(int p_index) {
	connecting_to_node = nodes_to_connect[p_index];
	_add_transition();
}

void AnimationNodeStateMachineEditor::_add_transition(const bool p_nested_action) {
	if (connecting_from != StringName() && connecting_to_node != StringName()) {
		if (state_machine->has_transition(connecting_from, connecting_to_node)) {
			EditorNode::get_singleton()->show_warning("Transition exists!");
			connecting = false;
			return;
		}

		Ref<AnimationNodeStateMachineTransition> tr;
		tr.instantiate();
		tr->set_advance_mode(auto_advance->is_pressed() ? AnimationNodeStateMachineTransition::AdvanceMode::ADVANCE_MODE_AUTO : AnimationNodeStateMachineTransition::AdvanceMode::ADVANCE_MODE_ENABLED);
		tr->set_switch_mode(AnimationNodeStateMachineTransition::SwitchMode(switch_mode->get_selected()));

		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		if (!p_nested_action) {
			updating = true;
			undo_redo->create_action(TTR("Add Transition"));
		}

		undo_redo->add_do_method(state_machine.ptr(), "add_transition", connecting_from, connecting_to_node, tr);
		undo_redo->add_undo_method(state_machine.ptr(), "remove_transition", connecting_from, connecting_to_node);
		undo_redo->add_do_method(this, "_update_graph");
		undo_redo->add_undo_method(this, "_update_graph");

		if (!p_nested_action) {
			undo_redo->commit_action();
			updating = false;
		}

		selected_transition_from = connecting_from;
		selected_transition_to = connecting_to_node;
		selected_transition_index = transition_lines.size();

		if (!state_machine->is_transition_across_group(selected_transition_index)) {
			EditorNode::get_singleton()->push_item(tr.ptr(), "", true);
		} else {
			EditorNode::get_singleton()->push_item(tr.ptr(), "", true);
			EditorNode::get_singleton()->push_item(nullptr, "", true);
		}
		_update_mode();
	}

	connecting = false;
}

void AnimationNodeStateMachineEditor::_connection_draw(const Vector2 &p_from, const Vector2 &p_to, AnimationNodeStateMachineTransition::SwitchMode p_mode, bool p_enabled, bool p_selected, bool p_travel, float p_fade_ratio, bool p_auto_advance, bool p_is_across_group, float p_opacity) {
	Color line_color = p_enabled ? theme_cache.transition_color : theme_cache.transition_disabled_color;
	Color icon_color = p_enabled ? theme_cache.transition_icon_color : theme_cache.transition_icon_disabled_color;
	Color highlight_color = p_enabled ? theme_cache.highlight_color : theme_cache.highlight_disabled_color;

	line_color.a *= p_opacity;
	icon_color.a *= p_opacity;
	highlight_color.a *= p_opacity;

	if (p_travel) {
		line_color = highlight_color;
	}

	if (p_selected) {
		state_machine_draw->draw_line(p_from, p_to, highlight_color, 6, true);
	}
	state_machine_draw->draw_line(p_from, p_to, line_color, 2, true);

	if (p_fade_ratio > 0.0) {
		Color fade_line_color = highlight_color;
		fade_line_color.set_hsv(1.0, fade_line_color.get_s(), fade_line_color.get_v());
		fade_line_color.a *= p_opacity;
		state_machine_draw->draw_line(p_from, p_from.lerp(p_to, p_fade_ratio), fade_line_color, 2);
	}

	const int ICON_COUNT = sizeof(theme_cache.transition_icons) / sizeof(*theme_cache.transition_icons);
	int icon_index = p_mode + (p_auto_advance ? ICON_COUNT / 2 : 0);
	ERR_FAIL_COND(icon_index >= ICON_COUNT);
	Ref<Texture2D> icon = theme_cache.transition_icons[icon_index];

	Transform2D xf;
	xf.columns[0] = (p_to - p_from).normalized();
	xf.columns[1] = xf.columns[0].orthogonal();
	xf.columns[2] = (p_from + p_to) * 0.5 - xf.columns[1] * icon->get_height() * 0.5 - xf.columns[0] * icon->get_height() * 0.5;

	state_machine_draw->draw_set_transform_matrix(xf);
	if (!p_is_across_group) {
		state_machine_draw->draw_texture(icon, Vector2(), icon_color);
	}
	state_machine_draw->draw_set_transform_matrix(Transform2D());
}

void AnimationNodeStateMachineEditor::_clip_src_line_to_rect(Vector2 &r_from, const Vector2 &p_to, const Rect2 &p_rect) {
	if (p_to == r_from) {
		return;
	}

	//this could be optimized...
	Vector2 n = (p_to - r_from).normalized();
	while (p_rect.has_point(r_from)) {
		r_from += n;
	}
}

void AnimationNodeStateMachineEditor::_clip_dst_line_to_rect(const Vector2 &p_from, Vector2 &r_to, const Rect2 &p_rect) {
	if (r_to == p_from) {
		return;
	}

	//this could be optimized...
	Vector2 n = (r_to - p_from).normalized();
	while (p_rect.has_point(r_to)) {
		r_to -= n;
	}
}

Ref<StyleBox> AnimationNodeStateMachineEditor::_adjust_stylebox_opacity(Ref<StyleBox> p_style, float p_opacity) {
	Ref<StyleBox> style = p_style->duplicate();
	if (style->is_class("StyleBoxFlat")) {
		Ref<StyleBoxFlat> flat_style = style;
		Color bg_color = flat_style->get_bg_color();
		Color border_color = flat_style->get_border_color();
		Color shadow_color = flat_style->get_shadow_color();

		bg_color.a *= p_opacity;
		border_color.a *= p_opacity;
		shadow_color.a *= p_opacity;

		flat_style->set_bg_color(bg_color);
		flat_style->set_border_color(border_color);
		flat_style->set_shadow_color(shadow_color);
	}
	return style;
}

void AnimationNodeStateMachineEditor::_state_machine_draw() {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	bool playing = false;
	StringName current;
	StringName blend_from;
	Vector<StringName> travel_path;

	Ref<AnimationNodeStateMachinePlayback> playback = tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + "playback");
	if (playback.is_valid()) {
		playing = playback->is_playing();
		current = playback->get_current_node();
		blend_from = playback->get_fading_from_node();
		travel_path = playback->get_travel_path();
	}

	if (state_machine_draw->has_focus()) {
		state_machine_draw->draw_rect(Rect2(Point2(), state_machine_draw->get_size()), theme_cache.focus_color, false);
	}
	int sep = 3 * EDSCALE;

	List<StringName> nodes;
	state_machine->get_node_list(&nodes);

	node_rects.clear();
	Rect2 scroll_range;

	//snap lines
	if (dragging_selected) {
		Vector2 from = (state_machine->get_node_position(selected_node) * EDSCALE) + drag_ofs - state_machine->get_graph_offset() * EDSCALE;
		if (snap_x != StringName()) {
			Vector2 to = (state_machine->get_node_position(snap_x) * EDSCALE) - state_machine->get_graph_offset() * EDSCALE;
			state_machine_draw->draw_line(from, to, theme_cache.guideline_color, 2);
		}
		if (snap_y != StringName()) {
			Vector2 to = (state_machine->get_node_position(snap_y) * EDSCALE) - state_machine->get_graph_offset() * EDSCALE;
			state_machine_draw->draw_line(from, to, theme_cache.guideline_color, 2);
		}
	}

	//pre pass nodes so we know the rectangles
	for (const StringName &E : nodes) {
		String name = E;
		int name_string_size = theme_cache.node_title_font->get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.node_title_font_size).width;

		Ref<AnimationNode> anode = state_machine->get_node(name);
		bool needs_editor = AnimationTreeEditor::get_singleton()->can_edit(anode);
		bool is_selected = selected_nodes.has(name);

		Size2 s = (is_selected ? theme_cache.node_frame_selected : theme_cache.node_frame)->get_minimum_size();
		s.width += name_string_size;
		s.height += MAX(theme_cache.node_title_font->get_height(theme_cache.node_title_font_size), theme_cache.play_node->get_height());
		s.width += sep + theme_cache.play_node->get_width();

		if (needs_editor) {
			s.width += sep + theme_cache.edit_node->get_width();
		}

		Vector2 offset;
		offset += state_machine->get_node_position(E) * EDSCALE;

		if (selected_nodes.has(E) && dragging_selected) {
			offset += drag_ofs;
		}

		offset -= s / 2;
		offset = offset.floor();

		//prepre rect

		NodeRect nr;
		nr.node = Rect2(offset, s);
		nr.node_name = E;

		scroll_range = scroll_range.merge(nr.node); //merge with range

		//now scroll it to draw
		nr.node.position -= state_machine->get_graph_offset() * EDSCALE;

		node_rects.push_back(nr);
	}

	transition_lines.clear();

	//draw connecting line for potential new transition
	if (connecting) {
		Vector2 from = (state_machine->get_node_position(connecting_from) * EDSCALE) - state_machine->get_graph_offset() * EDSCALE;
		Vector2 to;
		if (connecting_to_node != StringName()) {
			to = (state_machine->get_node_position(connecting_to_node) * EDSCALE) - state_machine->get_graph_offset() * EDSCALE;
		} else {
			to = connecting_to;
		}

		for (int i = 0; i < node_rects.size(); i++) {
			if (node_rects[i].node_name == connecting_from) {
				_clip_src_line_to_rect(from, to, node_rects[i].node);
			}
			if (node_rects[i].node_name == connecting_to_node) {
				_clip_dst_line_to_rect(from, to, node_rects[i].node);
			}
		}

		_connection_draw(from, to, AnimationNodeStateMachineTransition::SwitchMode(switch_mode->get_selected()), true, false, false, 0.0, false, false);
	}

	// TransitionImmediateBig
	float tr_bidi_offset = int(theme_cache.transition_icons[0]->get_height() * 0.8);

	//draw transition lines
	for (int i = 0; i < state_machine->get_transition_count(); i++) {
		TransitionLine tl;
		tl.transition_index = i;

		tl.from_node = state_machine->get_transition_from(i);
		Vector2 ofs_from = (dragging_selected && selected_nodes.has(tl.from_node)) ? drag_ofs : Vector2();
		tl.from = (state_machine->get_node_position(tl.from_node) * EDSCALE) + ofs_from - state_machine->get_graph_offset() * EDSCALE;

		tl.to_node = state_machine->get_transition_to(i);
		Vector2 ofs_to = (dragging_selected && selected_nodes.has(tl.to_node)) ? drag_ofs : Vector2();
		tl.to = (state_machine->get_node_position(tl.to_node) * EDSCALE) + ofs_to - state_machine->get_graph_offset() * EDSCALE;

		Ref<AnimationNodeStateMachineTransition> tr = state_machine->get_transition(i);
		tl.disabled = bool(tr->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED);
		tl.auto_advance = bool(tr->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
		tl.advance_condition_name = tr->get_advance_condition_name();
		tl.advance_condition_state = false;
		tl.mode = tr->get_switch_mode();
		tl.width = tr_bidi_offset;
		tl.travel = false;
		tl.fade_ratio = 0.0;
		tl.hidden = false;
		tl.is_across_group = state_machine->is_transition_across_group(i);

		if (state_machine->has_transition(tl.to_node, tl.from_node)) { //offset if same exists
			Vector2 offset = -(tl.from - tl.to).normalized().orthogonal() * tr_bidi_offset;
			tl.from += offset;
			tl.to += offset;
		}

		for (int j = 0; j < node_rects.size(); j++) {
			if (node_rects[j].node_name == tl.from_node) {
				_clip_src_line_to_rect(tl.from, tl.to, node_rects[j].node);
			}
			if (node_rects[j].node_name == tl.to_node) {
				_clip_dst_line_to_rect(tl.from, tl.to, node_rects[j].node);
			}
		}

		tl.selected = selected_transition_from == tl.from_node && selected_transition_to == tl.to_node;

		if (blend_from == tl.from_node && current == tl.to_node) {
			tl.travel = true;
			tl.fade_ratio = MIN(1.0, fading_pos / fading_time);
		}

		if (travel_path.size()) {
			if (current == tl.from_node && travel_path[0] == tl.to_node) {
				tl.travel = true;
			} else {
				for (int j = 0; j < travel_path.size() - 1; j++) {
					if (travel_path[j] == tl.from_node && travel_path[j + 1] == tl.to_node) {
						tl.travel = true;
						break;
					}
				}
			}
		}

		StringName fullpath = AnimationTreeEditor::get_singleton()->get_base_path() + String(tl.advance_condition_name);
		if (tl.advance_condition_name != StringName() && bool(tree->get(fullpath))) {
			tl.advance_condition_state = true;
			tl.auto_advance = true;
		}

		// check if already have this transition
		for (int j = 0; j < transition_lines.size(); j++) {
			if (transition_lines[j].from_node == tl.from_node && transition_lines[j].to_node == tl.to_node) {
				tl.hidden = true;
				transition_lines.write[j].disabled = transition_lines[j].disabled && tl.disabled;
			}
		}
		transition_lines.push_back(tl);
	}

	for (int i = 0; i < transition_lines.size(); i++) {
		TransitionLine tl = transition_lines[i];
		if (!tl.hidden) {
			float opacity = 0.2; // Default to reduced opacity.

			if (selected_transition_from != StringName() && selected_transition_to != StringName()) {
				// A transition is selected.
				if ((tl.from_node == selected_transition_from && tl.to_node == selected_transition_to) || (tl.from_node == selected_transition_to && tl.to_node == selected_transition_from)) {
					opacity = 1.0; // Full opacity for the selected transition pair.
				}
			} else if (!connected_nodes.is_empty()) {
				// A node is selected.
				if (connected_nodes.has(selected_node)) {
					// Only keep full opacity for transitions directly connected to the selected node.
					if (tl.from_node == selected_node || tl.to_node == selected_node) {
						opacity = 1.0;
					}
				} else {
					// If no node is selected, all transitions are at full opacity.
					opacity = 1.0;
				}
			} else {
				// If nothing is selected, all transitions are at full opacity.
				opacity = 1.0;
			}

			_connection_draw(tl.from, tl.to, tl.mode, !tl.disabled, tl.selected, tl.travel, tl.fade_ratio, tl.auto_advance, tl.is_across_group, opacity);
		}
	}

	//draw actual nodes
	for (int i = 0; i < node_rects.size(); i++) {
		String name = node_rects[i].node_name;
		int name_string_size = theme_cache.node_title_font->get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.node_title_font_size).width;

		Ref<AnimationNode> anode = state_machine->get_node(name);
		bool needs_editor = AnimationTreeEditor::get_singleton()->can_edit(anode);
		bool is_selected = selected_nodes.has(name);

		NodeRect &nr = node_rects.write[i];
		Vector2 offset = nr.node.position;
		int h = nr.node.size.height;

		float opacity = 1.0;
		if (selected_transition_from != StringName() && selected_transition_to != StringName()) {
			// A transition is selected.
			if (name != selected_transition_from && name != selected_transition_to) {
				opacity = 0.2;
			}
		} else if (!connected_nodes.is_empty() && !connected_nodes.has(name)) {
			// A node is selected.
			opacity = 0.2;
		}

		Ref<StyleBox> original_style = is_selected ? theme_cache.node_frame_selected : theme_cache.node_frame;
		Ref<StyleBox> node_style = _adjust_stylebox_opacity(original_style, opacity);

		state_machine_draw->draw_style_box(node_style, nr.node);

		if (!is_selected && SceneStringName(Start) == name) {
			Ref<StyleBox> start_style = _adjust_stylebox_opacity(theme_cache.node_frame_start, opacity);
			state_machine_draw->draw_style_box(start_style, nr.node);
		}
		if (!is_selected && SceneStringName(End) == name) {
			Ref<StyleBox> end_style = _adjust_stylebox_opacity(theme_cache.node_frame_end, opacity);
			state_machine_draw->draw_style_box(end_style, nr.node);
		}
		if (playing && (blend_from == name || current == name || travel_path.has(name))) {
			Ref<StyleBox> playing_style = _adjust_stylebox_opacity(theme_cache.node_frame_playing, opacity);
			state_machine_draw->draw_style_box(playing_style, nr.node);
		}

		offset.x += original_style->get_offset().x;

		nr.play.position = offset + Vector2(0, (h - theme_cache.play_node->get_height()) / 2).floor();
		nr.play.size = theme_cache.play_node->get_size();

		Color color_mod = Color(1, 1, 1, opacity);
		if (hovered_node_name == name && hovered_node_area == HOVER_NODE_PLAY) {
			state_machine_draw->draw_texture(theme_cache.play_node, nr.play.position, theme_cache.highlight_color * color_mod);
		} else {
			state_machine_draw->draw_texture(theme_cache.play_node, nr.play.position, color_mod);
		}

		offset.x += sep + theme_cache.play_node->get_width();

		nr.name.position = offset + Vector2(0, (h - theme_cache.node_title_font->get_height(theme_cache.node_title_font_size)) / 2).floor();
		nr.name.size = Vector2(name_string_size, theme_cache.node_title_font->get_height(theme_cache.node_title_font_size));

		Color font_color = theme_cache.node_title_font_color;
		font_color.a *= opacity;
		state_machine_draw->draw_string(theme_cache.node_title_font, nr.name.position + Vector2(0, theme_cache.node_title_font->get_ascent(theme_cache.node_title_font_size)), name, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.node_title_font_size, font_color);
		offset.x += name_string_size + sep;

		nr.can_edit = needs_editor;
		if (needs_editor) {
			nr.edit.position = offset + Vector2(0, (h - theme_cache.edit_node->get_height()) / 2).floor();
			nr.edit.size = theme_cache.edit_node->get_size();

			if (hovered_node_name == name && hovered_node_area == HOVER_NODE_EDIT) {
				state_machine_draw->draw_texture(theme_cache.edit_node, nr.edit.position, theme_cache.highlight_color * color_mod);
			} else {
				state_machine_draw->draw_texture(theme_cache.edit_node, nr.edit.position, color_mod);
			}
		}
	}

	//draw box select
	if (box_selecting) {
		state_machine_draw->draw_rect(box_selecting_rect, Color(0.7, 0.7, 1.0, 0.3));
	}

	scroll_range.position -= state_machine_draw->get_size();
	scroll_range.size += state_machine_draw->get_size() * 2.0;

	//adjust scrollbars
	updating = true;
	h_scroll->set_min(scroll_range.position.x);
	h_scroll->set_max(scroll_range.position.x + scroll_range.size.x);
	h_scroll->set_page(state_machine_draw->get_size().x);
	h_scroll->set_value(state_machine->get_graph_offset().x);

	v_scroll->set_min(scroll_range.position.y);
	v_scroll->set_max(scroll_range.position.y + scroll_range.size.y);
	v_scroll->set_page(state_machine_draw->get_size().y);
	v_scroll->set_value(state_machine->get_graph_offset().y);
	updating = false;

	state_machine_play_pos->queue_redraw();
}

void AnimationNodeStateMachineEditor::_update_connected_nodes(const StringName &p_node) {
	connected_nodes.clear();
	if (p_node != StringName()) {
		connected_nodes.insert(p_node);

		Vector<StringName> nodes_to = state_machine->get_nodes_with_transitions_to(p_node);
		for (const StringName &node_to : nodes_to) {
			connected_nodes.insert(node_to);
		}

		Vector<StringName> nodes_from = state_machine->get_nodes_with_transitions_from(p_node);
		for (const StringName &node_from : nodes_from) {
			connected_nodes.insert(node_from);
		}
	}
}

void AnimationNodeStateMachineEditor::_state_machine_pos_draw_individual(const String &p_name, float p_ratio) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	Ref<AnimationNodeStateMachinePlayback> playback = tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + "playback");
	if (!playback.is_valid() || !playback->is_playing()) {
		return;
	}

	if (p_name == SceneStringName(Start) || p_name == SceneStringName(End) || p_name.is_empty()) {
		return;
	}

	int idx = -1;
	for (int i = 0; i < node_rects.size(); i++) {
		if (node_rects[i].node_name == p_name) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		return;
	}

	const NodeRect &nr = node_rects[idx];
	if (nr.can_edit) {
		return; // It is not AnimationNodeAnimation.
	}

	Vector2 from;
	from.x = nr.play.position.x;
	from.y = (nr.play.position.y + nr.play.size.y + nr.node.position.y + nr.node.size.y) * 0.5;

	Vector2 to;
	if (nr.edit.size.x) {
		to.x = nr.edit.position.x + nr.edit.size.x;
	} else {
		to.x = nr.name.position.x + nr.name.size.x;
	}
	to.y = from.y;

	state_machine_play_pos->draw_line(from, to, theme_cache.playback_background_color, 2);
	to = from.lerp(to, p_ratio);
	state_machine_play_pos->draw_line(from, to, theme_cache.playback_color, 2);
}

void AnimationNodeStateMachineEditor::_state_machine_pos_draw_all() {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	Ref<AnimationNodeStateMachinePlayback> playback = tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + "playback");
	if (!playback.is_valid() || !playback->is_playing()) {
		return;
	}

	{
		float len = MAX(0.0001, current_length);
		float pos = CLAMP(current_play_pos, 0, len);
		float c = pos / len;
		_state_machine_pos_draw_individual(playback->get_current_node(), c);
	}

	{
		float len = MAX(0.0001, fade_from_length);
		float pos = CLAMP(fade_from_current_play_pos, 0, len);
		float c = pos / len;
		_state_machine_pos_draw_individual(playback->get_fading_from_node(), c);
	}
}

void AnimationNodeStateMachineEditor::_update_graph() {
	if (updating) {
		return;
	}

	updating = true;

	state_machine_draw->queue_redraw();

	updating = false;
}

void AnimationNodeStateMachineEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			panel->add_theme_style_override(SceneStringName(panel), theme_cache.panel_style);
			error_panel->add_theme_style_override(SceneStringName(panel), theme_cache.error_panel_style);
			error_label->add_theme_color_override(SceneStringName(font_color), theme_cache.error_color);

			tool_select->set_button_icon(theme_cache.tool_icon_select);
			tool_create->set_button_icon(theme_cache.tool_icon_create);
			tool_connect->set_button_icon(theme_cache.tool_icon_connect);

			switch_mode->clear();
			switch_mode->add_icon_item(theme_cache.transition_icon_immediate, TTR("Immediate"));
			switch_mode->add_icon_item(theme_cache.transition_icon_sync, TTR("Sync"));
			switch_mode->add_icon_item(theme_cache.transition_icon_end, TTR("At End"));

			auto_advance->set_button_icon(theme_cache.play_icon_auto);

			tool_erase->set_button_icon(theme_cache.tool_icon_erase);

			play_mode->clear();
			play_mode->add_icon_item(theme_cache.play_icon_travel, TTR("Travel"));
			play_mode->add_icon_item(theme_cache.play_icon_start, TTR("Immediate"));
		} break;

		case NOTIFICATION_PROCESS: {
			AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
			if (!tree) {
				return;
			}

			String error;

			Ref<AnimationNodeStateMachinePlayback> playback = tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + "playback");

			if (error_time > 0) {
				error = error_text;
				error_time -= get_process_delta_time();
			} else if (!tree->is_active()) {
				error = TTR("AnimationTree is inactive.\nActivate to enable playback, check node warnings if activation fails.");
			} else if (tree->is_state_invalid()) {
				error = tree->get_invalid_state_reason();
			} else if (playback.is_null()) {
				error = vformat(TTR("No playback resource set at path: %s."), AnimationTreeEditor::get_singleton()->get_base_path() + "playback");
			}

			if (error != error_label->get_text()) {
				error_label->set_text(error);
				if (!error.is_empty()) {
					error_panel->show();
				} else {
					error_panel->hide();
				}
			}

			for (int i = 0; i < transition_lines.size(); i++) {
				int tidx = -1;
				for (int j = 0; j < state_machine->get_transition_count(); j++) {
					if (transition_lines[i].from_node == state_machine->get_transition_from(j) && transition_lines[i].to_node == state_machine->get_transition_to(j)) {
						tidx = j;
						break;
					}
				}

				if (tidx == -1) { //missing transition, should redraw
					state_machine_draw->queue_redraw();
					break;
				}

				if (transition_lines[i].disabled != bool(state_machine->get_transition(tidx)->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED)) {
					state_machine_draw->queue_redraw();
					break;
				}

				if (transition_lines[i].auto_advance != bool(state_machine->get_transition(tidx)->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO)) {
					state_machine_draw->queue_redraw();
					break;
				}

				if (transition_lines[i].advance_condition_name != state_machine->get_transition(tidx)->get_advance_condition_name()) {
					state_machine_draw->queue_redraw();
					break;
				}

				if (transition_lines[i].mode != state_machine->get_transition(tidx)->get_switch_mode()) {
					state_machine_draw->queue_redraw();
					break;
				}

				bool acstate = transition_lines[i].advance_condition_name != StringName() && bool(tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + String(transition_lines[i].advance_condition_name)));

				if (transition_lines[i].advance_condition_state != acstate) {
					state_machine_draw->queue_redraw();
					break;
				}
			}

			bool same_travel_path = true;
			Vector<StringName> tp;
			bool is_playing = false;
			StringName current_node;
			StringName fading_from_node;

			current_play_pos = 0;
			current_length = 0;

			fade_from_current_play_pos = 0;
			fade_from_length = 0;

			fading_time = 0;
			fading_pos = 0;

			if (playback.is_valid()) {
				tp = playback->get_travel_path();
				is_playing = playback->is_playing();
				current_node = playback->get_current_node();
				fading_from_node = playback->get_fading_from_node();
				current_play_pos = playback->get_current_play_pos();
				current_length = playback->get_current_length();
				fade_from_current_play_pos = playback->get_fade_from_play_pos();
				fade_from_length = playback->get_fade_from_length();
				fading_time = playback->get_fading_time();
				fading_pos = playback->get_fading_pos();
			}

			{
				if (last_travel_path.size() != tp.size()) {
					same_travel_path = false;
				} else {
					for (int i = 0; i < last_travel_path.size(); i++) {
						if (last_travel_path[i] != tp[i]) {
							same_travel_path = false;
							break;
						}
					}
				}
			}

			//redraw if travel state changed
			if (!same_travel_path ||
					last_active != is_playing ||
					last_current_node != current_node ||
					last_fading_from_node != fading_from_node ||
					last_fading_time != fading_time ||
					last_fading_pos != fading_pos) {
				state_machine_draw->queue_redraw();
				last_travel_path = tp;
				last_current_node = current_node;
				last_active = is_playing;
				last_fading_from_node = fading_from_node;
				last_fading_time = fading_time;
				last_fading_pos = fading_pos;
				state_machine_play_pos->queue_redraw();
			}

			{
				if (current_node != StringName() && state_machine->has_node(current_node)) {
					String next = current_node;
					Ref<AnimationNodeStateMachine> anodesm = state_machine->get_node(next);
					Ref<AnimationNodeStateMachinePlayback> current_node_playback;

					while (anodesm.is_valid()) {
						current_node_playback = tree->get(AnimationTreeEditor::get_singleton()->get_base_path() + next + "/playback");
						StringName cnode = current_node_playback->get_current_node();
						next += "/" + cnode;
						if (!anodesm->has_node(cnode)) {
							break;
						}
						anodesm = anodesm->get_node(cnode);
					}

					// when current_node is a state machine, use playback of current_node to set play_pos
					if (current_node_playback.is_valid()) {
						current_play_pos = current_node_playback->get_current_play_pos();
						current_length = current_node_playback->get_current_length();
					}
				}
			}

			if (last_play_pos != current_play_pos || fade_from_last_play_pos != fade_from_current_play_pos) {
				last_play_pos = current_play_pos;
				fade_from_last_play_pos = fade_from_current_play_pos;
				state_machine_play_pos->queue_redraw();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			hovered_node_name = StringName();
			hovered_node_area = HOVER_NODE_NONE;
			set_process(is_visible_in_tree());
		} break;
	}
}

void AnimationNodeStateMachineEditor::_open_editor(const String &p_name) {
	AnimationTreeEditor::get_singleton()->enter_editor(p_name);
}

void AnimationNodeStateMachineEditor::_name_edited(const String &p_text) {
	const String &new_name = p_text;

	ERR_FAIL_COND(new_name.is_empty() || new_name.contains_char('.') || new_name.contains_char('/'));

	if (new_name == prev_name) {
		return; // Nothing to do.
	}

	const String &base_name = new_name;
	int base = 1;
	String name = base_name;
	while (state_machine->has_node(name)) {
		if (name == prev_name) {
			name_edit_popup->hide(); // The old name wins, the name doesn't change, just hide the popup.
			return;
		}
		base++;
		name = base_name + " " + itos(base);
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Node Renamed"));
	undo_redo->add_do_method(state_machine.ptr(), "rename_node", prev_name, name);
	undo_redo->add_undo_method(state_machine.ptr(), "rename_node", name, prev_name);
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
	name_edit_popup->hide();
	updating = false;

	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_name_edited_focus_out() {
	if (updating) {
		return;
	}

	_name_edited(name_edit->get_text());
}

void AnimationNodeStateMachineEditor::_scroll_changed(double) {
	if (updating) {
		return;
	}

	state_machine->set_graph_offset(Vector2(h_scroll->get_value(), v_scroll->get_value()));
	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_erase_selected(const bool p_nested_action) {
	if (!selected_nodes.is_empty()) {
		if (!p_nested_action) {
			updating = true;
		}
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Node Removed"));

		for (int i = 0; i < node_rects.size(); i++) {
			if (node_rects[i].node_name == SceneStringName(Start) || node_rects[i].node_name == SceneStringName(End)) {
				continue;
			}

			if (!selected_nodes.has(node_rects[i].node_name)) {
				continue;
			}

			undo_redo->add_do_method(state_machine.ptr(), "remove_node", node_rects[i].node_name);
			undo_redo->add_undo_method(state_machine.ptr(), "add_node", node_rects[i].node_name,
					state_machine->get_node(node_rects[i].node_name),
					state_machine->get_node_position(node_rects[i].node_name));

			for (int j = 0; j < state_machine->get_transition_count(); j++) {
				String from = state_machine->get_transition_from(j);
				String to = state_machine->get_transition_to(j);
				if (from == node_rects[i].node_name || to == node_rects[i].node_name) {
					undo_redo->add_undo_method(state_machine.ptr(), "add_transition", from, to, state_machine->get_transition(j));
				}
			}
		}

		undo_redo->add_do_method(this, "_update_graph");
		undo_redo->add_undo_method(this, "_update_graph");
		undo_redo->commit_action();

		if (!p_nested_action) {
			updating = false;
		}

		selected_nodes.clear();
	}

	if (selected_transition_to != StringName() && selected_transition_from != StringName() && state_machine->has_transition(selected_transition_from, selected_transition_to)) {
		Ref<AnimationNodeStateMachineTransition> tr = state_machine->get_transition(state_machine->find_transition(selected_transition_from, selected_transition_to));
		if (!p_nested_action) {
			updating = true;
		}
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Transition Removed"));
		undo_redo->add_do_method(state_machine.ptr(), "remove_transition", selected_transition_from, selected_transition_to);
		undo_redo->add_undo_method(state_machine.ptr(), "add_transition", selected_transition_from, selected_transition_to, tr);
		undo_redo->add_do_method(this, "_update_graph");
		undo_redo->add_undo_method(this, "_update_graph");
		undo_redo->commit_action();
		if (!p_nested_action) {
			updating = false;
		}
		selected_transition_from = StringName();
		selected_transition_to = StringName();
		selected_transition_index = -1;
	}

	state_machine_draw->queue_redraw();
}

void AnimationNodeStateMachineEditor::_update_mode() {
	if (tool_select->is_pressed()) {
		selection_tools_hb->show();
		bool nothing_selected = selected_nodes.is_empty() && selected_transition_from == StringName() && selected_transition_to == StringName();
		bool start_end_selected = selected_nodes.size() == 1 && (*selected_nodes.begin() == SceneStringName(Start) || *selected_nodes.begin() == SceneStringName(End));
		tool_erase->set_disabled(nothing_selected || start_end_selected || read_only);
	} else {
		selection_tools_hb->hide();
	}

	if (read_only) {
		tool_create->set_pressed(false);
		tool_connect->set_pressed(false);
	}

	if (tool_connect->is_pressed()) {
		transition_tools_hb->show();
	} else {
		transition_tools_hb->hide();
	}
}

void AnimationNodeStateMachineEditor::_bind_methods() {
	ClassDB::bind_method("_update_graph", &AnimationNodeStateMachineEditor::_update_graph);

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, panel_style, "panel", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, error_panel_style, "error_panel", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, error_color, "error_color", "GraphStateMachine");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, tool_icon_select, "ToolSelect", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, tool_icon_create, "ToolAddNode", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, tool_icon_connect, "ToolConnect", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, tool_icon_erase, "Remove", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icon_immediate, "TransitionImmediate", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icon_sync, "TransitionSync", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icon_end, "TransitionEnd", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, play_icon_start, "Play", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, play_icon_travel, "PlayTravel", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, play_icon_auto, "AutoPlay", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, animation_icon, "Animation", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, node_frame, "node_frame", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, node_frame_selected, "node_frame_selected", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, node_frame_playing, "node_frame_playing", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, node_frame_start, "node_frame_start", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, AnimationNodeStateMachineEditor, node_frame_end, "node_frame_end", "GraphStateMachine");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_FONT, AnimationNodeStateMachineEditor, node_title_font, "node_title_font", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_FONT_SIZE, AnimationNodeStateMachineEditor, node_title_font_size, "node_title_font_size", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, node_title_font_color, "node_title_font_color", "GraphStateMachine");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, play_node, "Play", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, edit_node, "Edit", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, transition_color, "transition_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, transition_disabled_color, "transition_disabled_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, transition_icon_color, "transition_icon_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, transition_icon_disabled_color, "transition_icon_disabled_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, highlight_color, "highlight_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, highlight_disabled_color, "highlight_disabled_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, focus_color, "focus_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, guideline_color, "guideline_color", "GraphStateMachine");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[0], "TransitionImmediateBig", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[1], "TransitionSyncBig", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[2], "TransitionEndBig", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[3], "TransitionImmediateAutoBig", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[4], "TransitionSyncAutoBig", "EditorIcons");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, AnimationNodeStateMachineEditor, transition_icons[5], "TransitionEndAutoBig", "EditorIcons");

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, playback_color, "playback_color", "GraphStateMachine");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, AnimationNodeStateMachineEditor, playback_background_color, "playback_background_color", "GraphStateMachine");
}

AnimationNodeStateMachineEditor *AnimationNodeStateMachineEditor::singleton = nullptr;

AnimationNodeStateMachineEditor::AnimationNodeStateMachineEditor() {
	singleton = this;

	HBoxContainer *top_hb = memnew(HBoxContainer);
	add_child(top_hb);

	Ref<ButtonGroup> bg;
	bg.instantiate();

	tool_select = memnew(Button);
	tool_select->set_theme_type_variation(SceneStringName(FlatButton));
	top_hb->add_child(tool_select);
	tool_select->set_toggle_mode(true);
	tool_select->set_button_group(bg);
	tool_select->set_pressed(true);
	tool_select->set_tooltip_text(TTR("Select and move nodes.\nRMB: Add node at position clicked.\nShift+LMB+Drag: Connects the selected node with another node or creates a new node if you select an area without nodes."));
	tool_select->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_update_mode), CONNECT_DEFERRED);

	tool_create = memnew(Button);
	tool_create->set_theme_type_variation(SceneStringName(FlatButton));
	top_hb->add_child(tool_create);
	tool_create->set_toggle_mode(true);
	tool_create->set_button_group(bg);
	tool_create->set_tooltip_text(TTR("Create new nodes."));
	tool_create->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_update_mode), CONNECT_DEFERRED);

	tool_connect = memnew(Button);
	tool_connect->set_theme_type_variation(SceneStringName(FlatButton));
	top_hb->add_child(tool_connect);
	tool_connect->set_toggle_mode(true);
	tool_connect->set_button_group(bg);
	tool_connect->set_tooltip_text(TTR("Connect nodes."));
	tool_connect->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_update_mode), CONNECT_DEFERRED);

	// Context-sensitive selection tools:
	selection_tools_hb = memnew(HBoxContainer);
	top_hb->add_child(selection_tools_hb);
	selection_tools_hb->add_child(memnew(VSeparator));

	tool_erase = memnew(Button);
	tool_erase->set_theme_type_variation(SceneStringName(FlatButton));
	tool_erase->set_tooltip_text(TTR("Remove selected node or transition."));
	tool_erase->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_erase_selected).bind(false));
	tool_erase->set_disabled(true);
	selection_tools_hb->add_child(tool_erase);

	transition_tools_hb = memnew(HBoxContainer);
	top_hb->add_child(transition_tools_hb);
	transition_tools_hb->add_child(memnew(VSeparator));

	transition_tools_hb->add_child(memnew(Label(TTR("Transition:"))));
	switch_mode = memnew(OptionButton);
	transition_tools_hb->add_child(switch_mode);

	auto_advance = memnew(Button);
	auto_advance->set_theme_type_variation(SceneStringName(FlatButton));
	auto_advance->set_tooltip_text(TTR("New Transitions Should Auto Advance"));
	auto_advance->set_toggle_mode(true);
	auto_advance->set_pressed(true);
	transition_tools_hb->add_child(auto_advance);

	//

	top_hb->add_spacer();

	top_hb->add_child(memnew(Label(TTR("Play Mode:"))));
	play_mode = memnew(OptionButton);
	top_hb->add_child(play_mode);

	panel = memnew(PanelContainer);
	panel->set_clip_contents(true);
	panel->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	add_child(panel);
	panel->set_v_size_flags(SIZE_EXPAND_FILL);

	state_machine_draw = memnew(Control);
	panel->add_child(state_machine_draw);
	state_machine_draw->connect(SceneStringName(gui_input), callable_mp(this, &AnimationNodeStateMachineEditor::_state_machine_gui_input));
	state_machine_draw->connect(SceneStringName(draw), callable_mp(this, &AnimationNodeStateMachineEditor::_state_machine_draw));
	state_machine_draw->set_focus_mode(FOCUS_ALL);
	state_machine_draw->set_mouse_filter(Control::MOUSE_FILTER_PASS);

	state_machine_play_pos = memnew(Control);
	state_machine_draw->add_child(state_machine_play_pos);
	state_machine_play_pos->set_mouse_filter(MOUSE_FILTER_PASS); //pass all to parent
	state_machine_play_pos->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	state_machine_play_pos->connect(SceneStringName(draw), callable_mp(this, &AnimationNodeStateMachineEditor::_state_machine_pos_draw_all));

	v_scroll = memnew(VScrollBar);
	state_machine_draw->add_child(v_scroll);
	v_scroll->set_anchors_and_offsets_preset(PRESET_RIGHT_WIDE);
	v_scroll->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeStateMachineEditor::_scroll_changed));

	h_scroll = memnew(HScrollBar);
	state_machine_draw->add_child(h_scroll);
	h_scroll->set_anchors_and_offsets_preset(PRESET_BOTTOM_WIDE);
	h_scroll->set_offset(SIDE_RIGHT, -v_scroll->get_size().x * EDSCALE);
	h_scroll->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeStateMachineEditor::_scroll_changed));

	error_panel = memnew(PanelContainer);
	add_child(error_panel);
	error_label = memnew(Label);
	error_panel->add_child(error_label);
	error_panel->hide();

	set_custom_minimum_size(Size2(0, 300 * EDSCALE));

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_add_menu_type));
	menu->connect("popup_hide", callable_mp(this, &AnimationNodeStateMachineEditor::_stop_connecting));

	animations_menu = memnew(PopupMenu);
	animations_menu->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	menu->add_child(animations_menu);
	animations_menu->connect("index_pressed", callable_mp(this, &AnimationNodeStateMachineEditor::_add_animation_type));

	connect_menu = memnew(PopupMenu);
	add_child(connect_menu);
	connect_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_connect_to));
	connect_menu->connect("popup_hide", callable_mp(this, &AnimationNodeStateMachineEditor::_stop_connecting));

	state_machine_menu = memnew(PopupMenu);
	state_machine_menu->set_name("state_machines");
	state_machine_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_connect_to));
	connect_menu->add_child(state_machine_menu);

	end_menu = memnew(PopupMenu);
	end_menu->set_name("end_nodes");
	end_menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_connect_to));
	connect_menu->add_child(end_menu);

	name_edit_popup = memnew(Popup);
	add_child(name_edit_popup);
	name_edit = memnew(LineEdit);
	name_edit_popup->add_child(name_edit);
	name_edit->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	name_edit->connect(SceneStringName(text_submitted), callable_mp(this, &AnimationNodeStateMachineEditor::_name_edited));
	name_edit->connect(SceneStringName(focus_exited), callable_mp(this, &AnimationNodeStateMachineEditor::_name_edited_focus_out));

	open_file = memnew(EditorFileDialog);
	add_child(open_file);
	open_file->set_title(TTR("Open Animation Node"));
	open_file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	open_file->connect("file_selected", callable_mp(this, &AnimationNodeStateMachineEditor::_file_opened));

	delete_window = memnew(ConfirmationDialog);
	delete_window->set_flag(Window::FLAG_RESIZE_DISABLED, true);
	add_child(delete_window);

	delete_tree = memnew(Tree);
	delete_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	delete_tree->set_hide_root(true);
	delete_tree->connect(SceneStringName(draw), callable_mp(this, &AnimationNodeStateMachineEditor::_delete_tree_draw));
	delete_window->add_child(delete_tree);

	Button *ok = delete_window->get_cancel_button();
	ok->set_text(TTR("Delete Selected"));
	ok->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_delete_selected));

	Button *delete_all = delete_window->add_button(TTR("Delete All"), true);
	delete_all->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeStateMachineEditor::_delete_all));
}

void EditorAnimationMultiTransitionEdit::add_transition(const StringName &p_from, const StringName &p_to, Ref<AnimationNodeStateMachineTransition> p_transition) {
	Transition tr;
	tr.from = p_from;
	tr.to = p_to;
	tr.transition = p_transition;
	transitions.push_back(tr);
}

bool EditorAnimationMultiTransitionEdit::_set(const StringName &p_name, const Variant &p_property) {
	int index = String(p_name).get_slicec('/', 0).to_int();
	StringName prop = String(p_name).get_slicec('/', 1);

	bool found;
	transitions.write[index].transition->set(prop, p_property, &found);
	if (found) {
		return true;
	}

	return false;
}

bool EditorAnimationMultiTransitionEdit::_get(const StringName &p_name, Variant &r_property) const {
	int index = String(p_name).get_slicec('/', 0).to_int();
	StringName prop = String(p_name).get_slicec('/', 1);

	if (prop == "transition_path") {
		r_property = String(transitions[index].from) + " -> " + transitions[index].to;
		return true;
	}

	bool found;
	r_property = transitions[index].transition->get(prop, &found);
	if (found) {
		return true;
	}

	return false;
}

void EditorAnimationMultiTransitionEdit::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < transitions.size(); i++) {
		List<PropertyInfo> plist;
		transitions[i].transition->get_property_list(&plist, true);

		PropertyInfo prop_transition_path;
		prop_transition_path.type = Variant::STRING;
		prop_transition_path.name = itos(i) + "/" + "transition_path";
		p_list->push_back(prop_transition_path);

		for (List<PropertyInfo>::Element *F = plist.front(); F; F = F->next()) {
			if (F->get().name == "script" || F->get().name == "resource_name" || F->get().name == "resource_path" || F->get().name == "resource_local_to_scene") {
				continue;
			}

			if (F->get().usage != PROPERTY_USAGE_DEFAULT) {
				continue;
			}

			PropertyInfo prop = F->get();
			prop.name = itos(i) + "/" + prop.name;

			p_list->push_back(prop);
		}
	}
}
