/**************************************************************************/
/*  animation_blend_space_1d_editor.cpp                                   */
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

#include "animation_blend_space_1d_editor.h"

#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"

StringName AnimationNodeBlendSpace1DEditor::get_blend_position_path() const {
	StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + "blend_position";
	return path;
}

void AnimationNodeBlendSpace1DEditor::_blend_space_gui_input(const Ref<InputEvent> &p_event) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	Ref<InputEventKey> k = p_event;

	if (tool_select->is_pressed() && k.is_valid() && k->is_pressed() && k->get_keycode() == Key::KEY_DELETE && !k->is_echo()) {
		if (selected_point != -1) {
			if (!read_only) {
				_erase_selected();
			}
			accept_event();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && ((tool_select->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) || (mb->get_button_index() == MouseButton::LEFT && tool_create->is_pressed()))) {
		if (!read_only) {
			menu->clear(false);
			animations_menu->clear();
			animations_to_add.clear();

			List<StringName> classes;
			ClassDB::get_inheriters_from_class("AnimationRootNode", &classes);
			classes.sort_custom<StringName::AlphCompare>();

			menu->add_submenu_node_item(TTR("Add Animation"), animations_menu);

			List<StringName> names;
			tree->get_animation_list(&names);

			for (const StringName &E : names) {
				animations_menu->add_icon_item(get_editor_theme_icon(SNAME("Animation")), E);
				animations_to_add.push_back(E);
			}

			for (const StringName &E : classes) {
				String name = String(E).replace_first("AnimationNode", "");
				if (name == "Animation" || name == "StartState" || name == "EndState") {
					continue;
				}

				int idx = menu->get_item_count();
				menu->add_item(vformat(TTR("Add %s"), name), idx);
				menu->set_item_metadata(idx, E);
			}

			Ref<AnimationNode> clipb = EditorSettings::get_singleton()->get_resource_clipboard();
			if (clipb.is_valid()) {
				menu->add_separator();
				menu->add_item(TTR("Paste"), MENU_PASTE);
			}
			menu->add_separator();
			menu->add_item(TTR("Load..."), MENU_LOAD_FILE);

			menu->set_position(blend_space_draw->get_screen_position() + mb->get_position());
			menu->reset_size();
			menu->popup();

			add_point_pos = (mb->get_position() / blend_space_draw->get_size()).x;
			add_point_pos *= (blend_space->get_max_space() - blend_space->get_min_space());
			add_point_pos += blend_space->get_min_space();

			if (snap->is_pressed()) {
				add_point_pos = Math::snapped(add_point_pos, blend_space->get_snap());
			}
		}
	}

	if (mb.is_valid() && mb->is_pressed() && tool_select->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		blend_space_draw->queue_redraw(); // why not

		// try to see if a point can be selected
		selected_point = -1;
		_update_tool_erase();

		for (int i = 0; i < points.size(); i++) {
			if (Math::abs(float(points[i] - mb->get_position().x)) < 10 * EDSCALE) {
				selected_point = i;

				Ref<AnimationNode> node = blend_space->get_blend_point_node(i);
				EditorNode::get_singleton()->push_item(node.ptr(), "", true);
				dragging_selected_attempt = true;
				drag_from = mb->get_position();
				_update_tool_erase();
				_update_edited_point_pos();
				return;
			}
		}
	}

	if (mb.is_valid() && !mb->is_pressed() && dragging_selected_attempt && mb->get_button_index() == MouseButton::LEFT) {
		if (!read_only) {
			if (dragging_selected) {
				// move
				float point = blend_space->get_blend_point_position(selected_point);
				point += drag_ofs.x;

				if (snap->is_pressed()) {
					point = Math::snapped(point, blend_space->get_snap());
				}

				updating = true;
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Move Node Point"));
				undo_redo->add_do_method(blend_space.ptr(), "set_blend_point_position", selected_point, point);
				undo_redo->add_undo_method(blend_space.ptr(), "set_blend_point_position", selected_point, blend_space->get_blend_point_position(selected_point));
				undo_redo->add_do_method(this, "_update_space");
				undo_redo->add_undo_method(this, "_update_space");
				undo_redo->add_do_method(this, "_update_edited_point_pos");
				undo_redo->add_undo_method(this, "_update_edited_point_pos");
				undo_redo->commit_action();
				updating = false;
				_update_edited_point_pos();
			}

			dragging_selected_attempt = false;
			dragging_selected = false;
			blend_space_draw->queue_redraw();
		}
	}

	// *set* the blend
	if (mb.is_valid() && !mb->is_pressed() && tool_blend->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		float blend_pos = mb->get_position().x / blend_space_draw->get_size().x;
		blend_pos *= blend_space->get_max_space() - blend_space->get_min_space();
		blend_pos += blend_space->get_min_space();

		tree->set(get_blend_position_path(), blend_pos);
		blend_space_draw->queue_redraw();
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid() && !blend_space_draw->has_focus()) {
		blend_space_draw->grab_focus();
		blend_space_draw->queue_redraw();
	}

	if (mm.is_valid() && dragging_selected_attempt) {
		dragging_selected = true;
		drag_ofs = ((mm->get_position() - drag_from) / blend_space_draw->get_size()) * ((blend_space->get_max_space() - blend_space->get_min_space()) * Vector2(1, 0));
		blend_space_draw->queue_redraw();
		_update_edited_point_pos();
	}

	if (mm.is_valid() && tool_blend->is_pressed() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		float blend_pos = mm->get_position().x / blend_space_draw->get_size().x;
		blend_pos *= blend_space->get_max_space() - blend_space->get_min_space();
		blend_pos += blend_space->get_min_space();

		tree->set(get_blend_position_path(), blend_pos);

		blend_space_draw->queue_redraw();
	}
}

void AnimationNodeBlendSpace1DEditor::_blend_space_draw() {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	Color linecolor = get_theme_color(SceneStringName(font_color), SNAME("Label"));
	Color linecolor_soft = linecolor;
	linecolor_soft.a *= 0.5;

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	Ref<Texture2D> icon = get_editor_theme_icon(SNAME("KeyValue"));
	Ref<Texture2D> icon_selected = get_editor_theme_icon(SNAME("KeySelected"));

	Size2 s = blend_space_draw->get_size();

	if (blend_space_draw->has_focus()) {
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		blend_space_draw->draw_rect(Rect2(Point2(), s), color, false);
	}

	blend_space_draw->draw_line(Point2(1, s.height - 1), Point2(s.width - 1, s.height - 1), linecolor, Math::round(EDSCALE));

	if (blend_space->get_min_space() < 0) {
		float point = 0.0;
		point = (point - blend_space->get_min_space()) / (blend_space->get_max_space() - blend_space->get_min_space());
		point *= s.width;

		float x = point;

		blend_space_draw->draw_line(Point2(x, s.height - 1), Point2(x, s.height - 5 * EDSCALE), linecolor, Math::round(EDSCALE));
		blend_space_draw->draw_string(font, Point2(x + 2 * EDSCALE, s.height - 2 * EDSCALE - font->get_height(font_size) + font->get_ascent(font_size)), "0", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, linecolor);
		blend_space_draw->draw_line(Point2(x, s.height - 5 * EDSCALE), Point2(x, 0), linecolor_soft, Math::round(EDSCALE));
	}

	if (snap->is_pressed()) {
		linecolor_soft.a = linecolor.a * 0.1;

		if (blend_space->get_snap() > 0) {
			int prev_idx = -1;

			for (int i = 0; i < s.x; i++) {
				float v = blend_space->get_min_space() + i * (blend_space->get_max_space() - blend_space->get_min_space()) / s.x;
				int idx = int(v / blend_space->get_snap());

				if (i > 0 && prev_idx != idx) {
					blend_space_draw->draw_line(Point2(i, 0), Point2(i, s.height), linecolor_soft, Math::round(EDSCALE));
				}

				prev_idx = idx;
			}
		}
	}

	points.clear();

	for (int i = 0; i < blend_space->get_blend_point_count(); i++) {
		float point = blend_space->get_blend_point_position(i);

		if (!read_only) {
			if (dragging_selected && selected_point == i) {
				point += drag_ofs.x;
				if (snap->is_pressed()) {
					point = Math::snapped(point, blend_space->get_snap());
				}
			}
		}

		point = (point - blend_space->get_min_space()) / (blend_space->get_max_space() - blend_space->get_min_space());
		point *= s.width;

		points.push_back(point);

		Vector2 gui_point = Vector2(point, s.height / 2.0);

		gui_point -= (icon->get_size() / 2.0);

		gui_point = gui_point.floor();

		if (i == selected_point) {
			blend_space_draw->draw_texture(icon_selected, gui_point);
		} else {
			blend_space_draw->draw_texture(icon, gui_point);
		}
	}

	// blend position
	{
		Color color;
		if (tool_blend->is_pressed()) {
			color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		} else {
			color = linecolor;
			color.a *= 0.5;
		}

		float point = tree->get(get_blend_position_path());

		point = (point - blend_space->get_min_space()) / (blend_space->get_max_space() - blend_space->get_min_space());
		point *= s.width;

		Vector2 gui_point = Vector2(point, s.height / 2.0);

		float mind = 5 * EDSCALE;
		float maxd = 15 * EDSCALE;
		blend_space_draw->draw_line(gui_point + Vector2(mind, 0), gui_point + Vector2(maxd, 0), color, Math::round(2 * EDSCALE));
		blend_space_draw->draw_line(gui_point + Vector2(-mind, 0), gui_point + Vector2(-maxd, 0), color, Math::round(2 * EDSCALE));
		blend_space_draw->draw_line(gui_point + Vector2(0, mind), gui_point + Vector2(0, maxd), color, Math::round(2 * EDSCALE));
		blend_space_draw->draw_line(gui_point + Vector2(0, -mind), gui_point + Vector2(0, -maxd), color, Math::round(2 * EDSCALE));
	}
}

void AnimationNodeBlendSpace1DEditor::_update_space() {
	if (updating) {
		return;
	}

	updating = true;

	max_value->set_value(blend_space->get_max_space());
	min_value->set_value(blend_space->get_min_space());

	sync->set_pressed(blend_space->is_using_sync());
	interpolation->select(blend_space->get_blend_mode());

	label_value->set_text(blend_space->get_value_label());

	snap_value->set_value(blend_space->get_snap());

	blend_space_draw->queue_redraw();

	updating = false;
}

void AnimationNodeBlendSpace1DEditor::_config_changed(double) {
	if (updating) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change BlendSpace1D Config"));
	undo_redo->add_do_method(blend_space.ptr(), "set_max_space", max_value->get_value());
	undo_redo->add_undo_method(blend_space.ptr(), "set_max_space", blend_space->get_max_space());
	undo_redo->add_do_method(blend_space.ptr(), "set_min_space", min_value->get_value());
	undo_redo->add_undo_method(blend_space.ptr(), "set_min_space", blend_space->get_min_space());
	undo_redo->add_do_method(blend_space.ptr(), "set_snap", snap_value->get_value());
	undo_redo->add_undo_method(blend_space.ptr(), "set_snap", blend_space->get_snap());
	undo_redo->add_do_method(blend_space.ptr(), "set_use_sync", sync->is_pressed());
	undo_redo->add_undo_method(blend_space.ptr(), "set_use_sync", blend_space->is_using_sync());
	undo_redo->add_do_method(blend_space.ptr(), "set_blend_mode", interpolation->get_selected());
	undo_redo->add_undo_method(blend_space.ptr(), "set_blend_mode", blend_space->get_blend_mode());
	undo_redo->add_do_method(this, "_update_space");
	undo_redo->add_undo_method(this, "_update_space");
	undo_redo->commit_action();
	updating = false;

	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_labels_changed(String) {
	if (updating) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change BlendSpace1D Labels"), UndoRedo::MERGE_ENDS);
	undo_redo->add_do_method(blend_space.ptr(), "set_value_label", label_value->get_text());
	undo_redo->add_undo_method(blend_space.ptr(), "set_value_label", blend_space->get_value_label());
	undo_redo->add_do_method(this, "_update_space");
	undo_redo->add_undo_method(this, "_update_space");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendSpace1DEditor::_snap_toggled() {
	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_file_opened(const String &p_file) {
	file_loaded = ResourceLoader::load(p_file);
	if (file_loaded.is_valid()) {
		_add_menu_type(MENU_LOAD_FILE_CONFIRM);
	} else {
		EditorNode::get_singleton()->show_warning(TTR("This type of node can't be used. Only animation nodes are allowed."));
	}
}

void AnimationNodeBlendSpace1DEditor::_add_menu_type(int p_index) {
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
	}

	if (!node.is_valid()) {
		EditorNode::get_singleton()->show_warning(TTR("This type of node can't be used. Only root nodes are allowed."));
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Node Point"));
	undo_redo->add_do_method(blend_space.ptr(), "add_blend_point", node, add_point_pos);
	undo_redo->add_undo_method(blend_space.ptr(), "remove_blend_point", blend_space->get_blend_point_count());
	undo_redo->add_do_method(this, "_update_space");
	undo_redo->add_undo_method(this, "_update_space");
	undo_redo->commit_action();
	updating = false;

	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_add_animation_type(int p_index) {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	anim->set_animation(animations_to_add[p_index]);

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Animation Point"));
	undo_redo->add_do_method(blend_space.ptr(), "add_blend_point", anim, add_point_pos);
	undo_redo->add_undo_method(blend_space.ptr(), "remove_blend_point", blend_space->get_blend_point_count());
	undo_redo->add_do_method(this, "_update_space");
	undo_redo->add_undo_method(this, "_update_space");
	undo_redo->commit_action();
	updating = false;

	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_tool_switch(int p_tool) {
	if (p_tool == 0) {
		tool_erase->show();
		tool_erase_sep->show();
	} else {
		tool_erase->hide();
		tool_erase_sep->hide();
	}

	_update_tool_erase();
	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_update_edited_point_pos() {
	if (updating) {
		return;
	}

	if (selected_point >= 0 && selected_point < blend_space->get_blend_point_count()) {
		float pos = blend_space->get_blend_point_position(selected_point);

		if (dragging_selected) {
			pos += drag_ofs.x;

			if (snap->is_pressed()) {
				pos = Math::snapped(pos, blend_space->get_snap());
			}
		}

		updating = true;
		edit_value->set_value(pos);
		updating = false;
	}
}

void AnimationNodeBlendSpace1DEditor::_update_tool_erase() {
	bool point_valid = selected_point >= 0 && selected_point < blend_space->get_blend_point_count();
	tool_erase->set_disabled(!point_valid || read_only);

	if (point_valid) {
		Ref<AnimationNode> an = blend_space->get_blend_point_node(selected_point);

		if (AnimationTreeEditor::get_singleton()->can_edit(an)) {
			open_editor->show();
		} else {
			open_editor->hide();
		}

		if (!read_only) {
			edit_hb->show();
		} else {
			edit_hb->hide();
		}
	} else {
		edit_hb->hide();
	}
}

void AnimationNodeBlendSpace1DEditor::_erase_selected() {
	if (selected_point != -1) {
		updating = true;

		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Remove BlendSpace1D Point"));
		undo_redo->add_do_method(blend_space.ptr(), "remove_blend_point", selected_point);
		undo_redo->add_undo_method(blend_space.ptr(), "add_blend_point", blend_space->get_blend_point_node(selected_point), blend_space->get_blend_point_position(selected_point), selected_point);
		undo_redo->add_do_method(this, "_update_space");
		undo_redo->add_undo_method(this, "_update_space");
		undo_redo->commit_action();

		updating = false;

		blend_space_draw->queue_redraw();
	}
}

void AnimationNodeBlendSpace1DEditor::_edit_point_pos(double) {
	if (updating) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Move BlendSpace1D Node Point"));
	undo_redo->add_do_method(blend_space.ptr(), "set_blend_point_position", selected_point, edit_value->get_value());
	undo_redo->add_undo_method(blend_space.ptr(), "set_blend_point_position", selected_point, blend_space->get_blend_point_position(selected_point));
	undo_redo->add_do_method(this, "_update_space");
	undo_redo->add_undo_method(this, "_update_space");
	undo_redo->add_do_method(this, "_update_edited_point_pos");
	undo_redo->add_undo_method(this, "_update_edited_point_pos");
	undo_redo->commit_action();
	updating = false;

	blend_space_draw->queue_redraw();
}

void AnimationNodeBlendSpace1DEditor::_open_editor() {
	if (selected_point >= 0 && selected_point < blend_space->get_blend_point_count()) {
		Ref<AnimationNode> an = blend_space->get_blend_point_node(selected_point);
		ERR_FAIL_COND(an.is_null());
		AnimationTreeEditor::get_singleton()->enter_editor(itos(selected_point));
	}
}

void AnimationNodeBlendSpace1DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			error_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
			panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			tool_blend->set_button_icon(get_editor_theme_icon(SNAME("EditPivot")));
			tool_select->set_button_icon(get_editor_theme_icon(SNAME("ToolSelect")));
			tool_create->set_button_icon(get_editor_theme_icon(SNAME("EditKey")));
			tool_erase->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			snap->set_button_icon(get_editor_theme_icon(SNAME("SnapGrid")));
			open_editor->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
			interpolation->clear();
			interpolation->add_icon_item(get_editor_theme_icon(SNAME("TrackContinuous")), TTR("Continuous"), 0);
			interpolation->add_icon_item(get_editor_theme_icon(SNAME("TrackDiscrete")), TTR("Discrete"), 1);
			interpolation->add_icon_item(get_editor_theme_icon(SNAME("TrackCapture")), TTR("Capture"), 2);
		} break;

		case NOTIFICATION_PROCESS: {
			AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
			if (!tree) {
				return;
			}

			String error;

			if (!tree->is_active()) {
				error = TTR("AnimationTree is inactive.\nActivate to enable playback, check node warnings if activation fails.");
			} else if (tree->is_state_invalid()) {
				error = tree->get_invalid_state_reason();
			}

			if (error != error_label->get_text()) {
				error_label->set_text(error);
				if (!error.is_empty()) {
					error_panel->show();
				} else {
					error_panel->hide();
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process(is_visible_in_tree());
		} break;
	}
}

void AnimationNodeBlendSpace1DEditor::_bind_methods() {
	ClassDB::bind_method("_update_space", &AnimationNodeBlendSpace1DEditor::_update_space);
	ClassDB::bind_method("_update_tool_erase", &AnimationNodeBlendSpace1DEditor::_update_tool_erase);

	ClassDB::bind_method("_update_edited_point_pos", &AnimationNodeBlendSpace1DEditor::_update_edited_point_pos);
}

bool AnimationNodeBlendSpace1DEditor::can_edit(const Ref<AnimationNode> &p_node) {
	Ref<AnimationNodeBlendSpace1D> b1d = p_node;
	return b1d.is_valid();
}

void AnimationNodeBlendSpace1DEditor::edit(const Ref<AnimationNode> &p_node) {
	blend_space = p_node;
	read_only = false;

	if (!blend_space.is_null()) {
		read_only = EditorNode::get_singleton()->is_resource_read_only(blend_space);

		_update_space();
	}

	tool_create->set_disabled(read_only);
	edit_value->set_editable(!read_only);
	label_value->set_editable(!read_only);
	min_value->set_editable(!read_only);
	max_value->set_editable(!read_only);
	sync->set_disabled(read_only);
	interpolation->set_disabled(read_only);
}

AnimationNodeBlendSpace1DEditor *AnimationNodeBlendSpace1DEditor::singleton = nullptr;

AnimationNodeBlendSpace1DEditor::AnimationNodeBlendSpace1DEditor() {
	singleton = this;

	HBoxContainer *top_hb = memnew(HBoxContainer);
	add_child(top_hb);

	Ref<ButtonGroup> bg;
	bg.instantiate();

	tool_blend = memnew(Button);
	tool_blend->set_theme_type_variation(SceneStringName(FlatButton));
	tool_blend->set_toggle_mode(true);
	tool_blend->set_button_group(bg);
	top_hb->add_child(tool_blend);
	tool_blend->set_pressed(true);
	tool_blend->set_tooltip_text(TTR("Set the blending position within the space"));
	tool_blend->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_tool_switch).bind(3));

	tool_select = memnew(Button);
	tool_select->set_theme_type_variation(SceneStringName(FlatButton));
	tool_select->set_toggle_mode(true);
	tool_select->set_button_group(bg);
	top_hb->add_child(tool_select);
	tool_select->set_tooltip_text(TTR("Select and move points, create points with RMB."));
	tool_select->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_tool_switch).bind(0));

	tool_create = memnew(Button);
	tool_create->set_theme_type_variation(SceneStringName(FlatButton));
	tool_create->set_toggle_mode(true);
	tool_create->set_button_group(bg);
	top_hb->add_child(tool_create);
	tool_create->set_tooltip_text(TTR("Create points."));
	tool_create->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_tool_switch).bind(1));

	tool_erase_sep = memnew(VSeparator);
	top_hb->add_child(tool_erase_sep);
	tool_erase = memnew(Button);
	tool_erase->set_theme_type_variation(SceneStringName(FlatButton));
	top_hb->add_child(tool_erase);
	tool_erase->set_tooltip_text(TTR("Erase points."));
	tool_erase->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_erase_selected));

	top_hb->add_child(memnew(VSeparator));

	snap = memnew(Button);
	snap->set_theme_type_variation(SceneStringName(FlatButton));
	snap->set_toggle_mode(true);
	top_hb->add_child(snap);
	snap->set_pressed(true);
	snap->set_tooltip_text(TTR("Enable snap and show grid."));
	snap->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_snap_toggled));

	snap_value = memnew(SpinBox);
	top_hb->add_child(snap_value);
	snap_value->set_min(0.01);
	snap_value->set_step(0.01);
	snap_value->set_max(1000);

	top_hb->add_child(memnew(VSeparator));
	top_hb->add_child(memnew(Label(TTR("Sync:"))));
	sync = memnew(CheckBox);
	top_hb->add_child(sync);
	sync->connect(SceneStringName(toggled), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_config_changed));

	top_hb->add_child(memnew(VSeparator));

	top_hb->add_child(memnew(Label(TTR("Blend:"))));
	interpolation = memnew(OptionButton);
	top_hb->add_child(interpolation);
	interpolation->connect(SceneStringName(item_selected), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_config_changed));

	edit_hb = memnew(HBoxContainer);
	top_hb->add_child(edit_hb);
	edit_hb->add_child(memnew(VSeparator));
	edit_hb->add_child(memnew(Label(TTR("Point"))));

	edit_value = memnew(SpinBox);
	edit_hb->add_child(edit_value);
	edit_value->set_min(-1000);
	edit_value->set_max(1000);
	edit_value->set_step(0.01);
	edit_value->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_edit_point_pos));

	open_editor = memnew(Button);
	edit_hb->add_child(open_editor);
	open_editor->set_text(TTR("Open Editor"));
	open_editor->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_open_editor), CONNECT_DEFERRED);

	edit_hb->hide();
	open_editor->hide();

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);
	main_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	panel = memnew(PanelContainer);
	panel->set_clip_contents(true);
	main_vb->add_child(panel);
	panel->set_h_size_flags(SIZE_EXPAND_FILL);
	panel->set_v_size_flags(SIZE_EXPAND_FILL);

	blend_space_draw = memnew(Control);
	blend_space_draw->connect(SceneStringName(gui_input), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_blend_space_gui_input));
	blend_space_draw->connect(SceneStringName(draw), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_blend_space_draw));
	blend_space_draw->set_focus_mode(FOCUS_ALL);

	panel->add_child(blend_space_draw);

	{
		HBoxContainer *bottom_hb = memnew(HBoxContainer);
		main_vb->add_child(bottom_hb);
		bottom_hb->set_h_size_flags(SIZE_EXPAND_FILL);

		min_value = memnew(SpinBox);
		min_value->set_min(-10000);
		min_value->set_max(0);
		min_value->set_step(0.01);

		max_value = memnew(SpinBox);
		max_value->set_min(0.01);
		max_value->set_max(10000);
		max_value->set_step(0.01);

		label_value = memnew(LineEdit);
		label_value->set_expand_to_text_length_enabled(true);

		// now add

		bottom_hb->add_child(min_value);
		bottom_hb->add_spacer();
		bottom_hb->add_child(label_value);
		bottom_hb->add_spacer();
		bottom_hb->add_child(max_value);
	}

	snap_value->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_config_changed));
	min_value->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_config_changed));
	max_value->connect(SceneStringName(value_changed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_config_changed));
	label_value->connect(SceneStringName(text_changed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_labels_changed));

	error_panel = memnew(PanelContainer);
	add_child(error_panel);

	error_label = memnew(Label);
	error_panel->add_child(error_label);

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeBlendSpace1DEditor::_add_menu_type));

	animations_menu = memnew(PopupMenu);
	animations_menu->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	menu->add_child(animations_menu);
	animations_menu->connect("index_pressed", callable_mp(this, &AnimationNodeBlendSpace1DEditor::_add_animation_type));

	open_file = memnew(EditorFileDialog);
	add_child(open_file);
	open_file->set_title(TTR("Open Animation Node"));
	open_file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	open_file->connect("file_selected", callable_mp(this, &AnimationNodeBlendSpace1DEditor::_file_opened));

	set_custom_minimum_size(Size2(0, 150 * EDSCALE));
}
