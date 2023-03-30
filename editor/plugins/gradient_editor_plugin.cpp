/**************************************************************************/
/*  gradient_editor_plugin.cpp                                            */
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

#include "gradient_editor_plugin.h"

#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/separator.h"
#include "scene/resources/gradient_texture.h"

int GradientEdit::_get_point_at(int p_xpos) const {
	int result = -1;
	int total_w = _get_gradient_rect_width();
	float min_distance = handle_width * 0.8; // Allow the cursor to be more than half a handle width away for ease of use.
	for (int i = 0; i < gradient->get_point_count(); i++) {
		// Ignore points outside of [0, 1].
		if (gradient->get_offset(i) < 0) {
			continue;
		} else if (gradient->get_offset(i) > 1) {
			break;
		}
		// Check if we clicked at point.
		float distance = ABS(p_xpos - gradient->get_offset(i) * total_w);
		if (distance < min_distance) {
			result = i;
			min_distance = distance;
		}
	}
	return result;
}

int GradientEdit::_predict_insertion_index(float p_offset) {
	int result = 0;
	while (result < gradient->get_point_count() && gradient->get_offset(result) < p_offset) {
		result++;
	}
	return result;
}

int GradientEdit::_get_gradient_rect_width() const {
	return get_size().width - get_size().height - draw_spacing - handle_width;
}

void GradientEdit::_show_color_picker() {
	if (selected_index == -1) {
		return;
	}

	picker->set_pick_color(gradient->get_color(selected_index));
	Size2 minsize = popup->get_contents_minimum_size();
	float viewport_height = get_viewport_rect().size.y;

	// Determine in which direction to show the popup. By default popup below.
	// But if the popup doesn't fit below and the Gradient Editor is in the bottom half of the viewport, show above.
	bool show_above = get_global_position().y + get_size().y + minsize.y > viewport_height && get_global_position().y * 2 + get_size().y > viewport_height;

	float v_offset = show_above ? -minsize.y : get_size().y;
	popup->set_position(get_screen_position() + Vector2(0, v_offset));
	popup->popup();
}

void GradientEdit::_color_changed(const Color &p_color) {
	set_color(selected_index, p_color);
}

void GradientEdit::set_gradient(const Ref<Gradient> &p_gradient) {
	gradient = p_gradient;
	gradient->connect(CoreStringName(changed), callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw));
}

const Ref<Gradient> &GradientEdit::get_gradient() const {
	return gradient;
}

void GradientEdit::add_point(float p_offset, const Color &p_color) {
	int new_idx = _predict_insertion_index(p_offset);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Gradient Point"));
	undo_redo->add_do_method(*gradient, "add_point", p_offset, p_color);
	undo_redo->add_do_method(this, "set_selected_index", new_idx);
	undo_redo->add_undo_method(*gradient, "remove_point", new_idx);
	undo_redo->add_undo_method(this, "set_selected_index", -1);
	undo_redo->commit_action();
}

void GradientEdit::remove_point(int p_index) {
	ERR_FAIL_INDEX_MSG(p_index, gradient->get_point_count(), "Gradient point is out of bounds.");

	if (gradient->get_point_count() <= 1) {
		return;
	}

	// If the point is removed while it's being moved, remember its old offset.
	float old_offset = (grabbing == GRAB_MOVE) ? pre_grab_offset : gradient->get_offset(p_index);
	Color old_color = gradient->get_color(p_index);

	int new_selected_index = selected_index;
	// Reselect the old selected point if it's not the deleted one.
	if (new_selected_index > p_index) {
		new_selected_index -= 1;
	} else if (new_selected_index == p_index) {
		new_selected_index = -1;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Gradient Point"));
	undo_redo->add_do_method(*gradient, "remove_point", p_index);
	undo_redo->add_do_method(this, "set_selected_index", new_selected_index);
	undo_redo->add_undo_method(*gradient, "add_point", old_offset, old_color);
	undo_redo->add_undo_method(this, "set_selected_index", selected_index);
	undo_redo->commit_action();
}

void GradientEdit::set_offset(int p_index, float p_offset) {
	ERR_FAIL_INDEX_MSG(p_index, gradient->get_point_count(), "Gradient point is out of bounds.");

	// Use pre_grab_offset to determine things for the undo/redo.
	if (Math::is_equal_approx(pre_grab_offset, p_offset)) {
		return;
	}

	int new_idx = _predict_insertion_index(p_offset);

	gradient->set_offset(p_index, pre_grab_offset); // Pretend the point started from its old place.
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Move Gradient Point"));
	undo_redo->add_do_method(*gradient, "set_offset", pre_grab_index, p_offset);
	undo_redo->add_do_method(this, "set_selected_index", new_idx);
	undo_redo->add_undo_method(*gradient, "set_offset", new_idx, pre_grab_offset);
	undo_redo->add_undo_method(this, "set_selected_index", pre_grab_index);
	undo_redo->commit_action();
	queue_redraw();
}

void GradientEdit::set_color(int p_index, const Color &p_color) {
	ERR_FAIL_INDEX_MSG(p_index, gradient->get_point_count(), "Gradient point is out of bounds.");

	Color old_color = gradient->get_color(p_index);
	if (old_color == p_color) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Recolor Gradient Point"), UndoRedo::MERGE_ENDS);
	undo_redo->add_do_method(*gradient, "set_color", p_index, p_color);
	undo_redo->add_undo_method(*gradient, "set_color", p_index, old_color);
	undo_redo->commit_action();
	queue_redraw();
}

void GradientEdit::reverse_gradient() {
	int new_selected_idx = (selected_index == -1) ? -1 : (gradient->get_point_count() - selected_index - 1);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Reverse Gradient"), UndoRedo::MERGE_DISABLE, *gradient);
	undo_redo->add_do_method(*gradient, "reverse");
	undo_redo->add_do_method(this, "set_selected_index", new_selected_idx);
	undo_redo->add_undo_method(*gradient, "reverse");
	undo_redo->add_undo_method(this, "set_selected_index", selected_index);
	undo_redo->commit_action();
}

void GradientEdit::set_selected_index(int p_index) {
	selected_index = p_index;
	queue_redraw();
}

void GradientEdit::set_snap_enabled(bool p_enabled) {
	snap_enabled = p_enabled;
	queue_redraw();
	if (gradient.is_valid()) {
		if (snap_enabled) {
			gradient->set_meta(SNAME("_snap_enabled"), true);
		} else {
			gradient->remove_meta(SNAME("_snap_enabled"));
		}
	}
}

void GradientEdit::set_snap_count(int p_count) {
	snap_count = p_count;
	queue_redraw();
	if (gradient.is_valid()) {
		if (snap_count != GradientEditor::DEFAULT_SNAP) {
			gradient->set_meta(SNAME("_snap_count"), snap_count);
		} else {
			gradient->remove_meta(SNAME("_snap_count"));
		}
	}
}

ColorPicker *GradientEdit::get_picker() const {
	return picker;
}

PopupPanel *GradientEdit::get_popup() const {
	return popup;
}

void GradientEdit::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed() && k->get_keycode() == Key::KEY_DELETE && selected_index != -1) {
		if (grabbing == GRAB_ADD) {
			gradient->remove_point(selected_index); // Point is temporary, so remove directly from gradient.
			set_selected_index(-1);
		} else {
			remove_point(selected_index);
		}
		grabbing = GRAB_NONE;
		hovered_index = -1;
		accept_event();
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed()) {
		float adjusted_mb_x = mb->get_position().x - handle_width / 2;
		bool should_snap = snap_enabled || mb->is_ctrl_pressed();

		// Delete point or move it to old position on middle or right click.
		if (mb->get_button_index() == MouseButton::RIGHT || mb->get_button_index() == MouseButton::MIDDLE) {
			if (grabbing == GRAB_MOVE && mb->get_button_index() == MouseButton::RIGHT) {
				gradient->set_offset(selected_index, pre_grab_offset);
				set_selected_index(pre_grab_index);
			} else {
				int point_to_remove = _get_point_at(adjusted_mb_x);
				if (point_to_remove == -1) {
					set_selected_index(-1); // Nothing on the place of the click, just deselect any handle.
				} else {
					if (grabbing == GRAB_ADD) {
						gradient->remove_point(point_to_remove); // Point is temporary, so remove directly from gradient.
						set_selected_index(-1);
					} else {
						remove_point(point_to_remove);
					}
					hovered_index = -1;
				}
			}
			grabbing = GRAB_NONE;
			accept_event();
		}

		// Select point.
		if (mb->get_button_index() == MouseButton::LEFT) {
			int total_w = _get_gradient_rect_width();

			// Check if color picker was clicked or gradient was double-clicked.
			if (adjusted_mb_x > total_w + draw_spacing) {
				if (!mb->is_double_click()) {
					_show_color_picker();
				}
				accept_event();
				return;
			} else if (mb->is_double_click()) {
				set_selected_index(_get_point_at(adjusted_mb_x));
				_show_color_picker();
				accept_event();
				return;
			}

			if (grabbing == GRAB_NONE) {
				set_selected_index(_get_point_at(adjusted_mb_x));
			}

			if (selected_index != -1 && !mb->is_alt_pressed()) {
				// An existing point was grabbed.
				grabbing = GRAB_MOVE;
				pre_grab_offset = gradient->get_offset(selected_index);
				pre_grab_index = selected_index;
			} else if (grabbing == GRAB_NONE) {
				// Adding a new point. Insert a temporary point for the user to adjust, so it's not in the undo/redo.
				float new_offset = CLAMP(adjusted_mb_x / float(total_w), 0, 1);
				if (should_snap) {
					new_offset = Math::snapped(new_offset, 1.0 / snap_count);
				}

				for (int i = 0; i < gradient->get_point_count(); i++) {
					if (gradient->get_offset(i) == new_offset) {
						// If another point with the same offset is found, then
						// tweak it if Alt was pressed, otherwise something has gone wrong, so stop the operation.
						if (mb->is_alt_pressed()) {
							new_offset = MIN(gradient->get_offset(i) + 0.00001, 1);
						} else {
							return;
						}
					}
				}

				Color new_color = gradient->get_color_at_offset(new_offset);
				if (mb->is_alt_pressed()) {
					// Alt + Click on a point duplicates it. So copy its color.
					int point_to_copy = _get_point_at(adjusted_mb_x);
					if (point_to_copy != -1) {
						new_color = gradient->get_color(point_to_copy);
					}
				}
				// Add a temporary point for the user to adjust before adding it permanently.
				gradient->add_point(new_offset, new_color);
				set_selected_index(_predict_insertion_index(new_offset));
				grabbing = GRAB_ADD;
			}
		}
	}

	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed()) {
		if (grabbing == GRAB_MOVE) {
			// Finish moving a point.
			set_offset(selected_index, gradient->get_offset(selected_index));
			grabbing = GRAB_NONE;
		} else if (grabbing == GRAB_ADD) {
			// Finish inserting a new point. Remove the temporary point and insert the permanent one in its place.
			float new_offset = gradient->get_offset(selected_index);
			Color new_color = gradient->get_color(selected_index);
			gradient->remove_point(selected_index);
			add_point(new_offset, new_color);
			grabbing = GRAB_NONE;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		int total_w = _get_gradient_rect_width();
		float adjusted_mm_x = mm->get_position().x - handle_width / 2;
		bool should_snap = snap_enabled || mm->is_ctrl_pressed();

		// Hovering logic.
		if (grabbing == GRAB_NONE) {
			int nearest_point = _get_point_at(adjusted_mm_x);
			if (hovered_index != nearest_point) {
				hovered_index = nearest_point;
				queue_redraw();
			}
			return;
		} else {
			hovered_index = -1;
		}

		// Grabbing logic.
		float new_offset = CLAMP(adjusted_mm_x / float(total_w), 0, 1);

		// Give the ability to snap right next to a point when using Shift.
		if (mm->is_shift_pressed()) {
			float smallest_offset = should_snap ? (0.5 / snap_count) : 0.01;
			int nearest_idx = -1;
			// Only check the two adjacent points to find which one is the nearest.
			if (selected_index > 0) {
				float temp_offset = ABS(gradient->get_offset(selected_index - 1) - new_offset);
				if (temp_offset < smallest_offset) {
					smallest_offset = temp_offset;
					nearest_idx = selected_index - 1;
				}
			}
			if (selected_index < gradient->get_point_count() - 1) {
				float temp_offset = ABS(gradient->get_offset(selected_index + 1) - new_offset);
				if (temp_offset < smallest_offset) {
					smallest_offset = temp_offset;
					nearest_idx = selected_index + 1;
				}
			}
			if (nearest_idx != -1) {
				// Snap to the point with a slight adjustment to the left or right.
				float adjustment = gradient->get_offset(nearest_idx) < new_offset ? 0.00001 : -0.00001;
				new_offset = CLAMP(gradient->get_offset(nearest_idx) + adjustment, 0, 1);
			} else if (should_snap) {
				new_offset = Math::snapped(new_offset, 1.0 / snap_count);
			}
		} else if (should_snap) {
			// Shift is not pressed, so snap fully without adjustments.
			new_offset = Math::snapped(new_offset, 1.0 / snap_count);
		}

		// Don't move the point if its new offset would be the same as another point's.
		for (int i = 0; i < gradient->get_point_count(); i++) {
			if (gradient->get_offset(i) == new_offset && i != selected_index) {
				return;
			}
		}

		if (selected_index == -1) {
			return;
		}

		// We want to only save this action for undo/redo when released, so don't use set_offset() yet.
		gradient->set_offset(selected_index, new_offset);

		// Update selected_index after the gradient updates its indices, so you keep holding the same color.
		for (int i = 0; i < gradient->get_point_count(); i++) {
			if (gradient->get_offset(i) == new_offset) {
				set_selected_index(i);
				break;
			}
		}
	}
}

void GradientEdit::_redraw() {
	int w = get_size().x;
	int h = get_size().y - draw_spacing; // A bit of spacing below the gradient too.

	if (w == 0 || h == 0) {
		return; // Safety check as there is nothing to draw with such size.
	}

	int total_w = _get_gradient_rect_width();
	int half_handle_width = handle_width * 0.5;

	// Draw gradient.
	draw_texture_rect(get_editor_theme_icon(SNAME("GuiMiniCheckerboard")), Rect2(half_handle_width, 0, total_w, h), true);
	preview_texture->set_gradient(gradient);
	draw_texture_rect(preview_texture, Rect2(half_handle_width, 0, total_w, h));

	// Draw vertical snap lines.
	if (snap_enabled || (Input::get_singleton()->is_key_pressed(Key::CTRL) && grabbing != GRAB_NONE)) {
		const Color line_color = Color(0.5, 0.5, 0.5, 0.5);
		for (int idx = 1; idx < snap_count; idx++) {
			float offset_x = idx * total_w / (float)snap_count + half_handle_width;
			draw_line(Point2(offset_x, 0), Point2(offset_x, h), line_color);
		}
	}

	// Draw handles.
	for (int i = 0; i < gradient->get_point_count(); i++) {
		// Only draw handles for points in [0, 1]. If there are points before or after, draw a little indicator.
		if (gradient->get_offset(i) < 0.0) {
			continue;
		} else if (gradient->get_offset(i) > 1.0) {
			break;
		}
		// White or black handle color, to contrast with the selected color's brightness.
		// Also consider the fact that the color may be translucent.
		// The checkerboard pattern in the background has an average luminance of 0.75.
		Color inside_col = gradient->get_color(i);
		Color border_col = Math::lerp(0.75f, inside_col.get_luminance(), inside_col.a) > 0.455 ? Color(0, 0, 0) : Color(1, 1, 1);

		int handle_thickness = MAX(1, Math::round(EDSCALE));
		float handle_x_pos = gradient->get_offset(i) * total_w + half_handle_width;
		float handle_start_x = handle_x_pos - half_handle_width;
		Rect2 rect = Rect2(handle_start_x, h / 2, handle_width, h / 2);

		if (inside_col.a < 1) {
			// If the color is translucent, draw a little opaque rectangle at the bottom to more easily see it.
			draw_texture_rect(get_editor_theme_icon(SNAME("GuiMiniCheckerboard")), rect, true);
			draw_rect(rect, inside_col, true);
			Color inside_col_opaque = inside_col;
			inside_col_opaque.a = 1.0;
			draw_rect(Rect2(handle_start_x + handle_thickness / 2.0, h * 0.9 - handle_thickness / 2.0, handle_width - handle_thickness, h * 0.1), inside_col_opaque, true);
		} else {
			draw_rect(rect, inside_col, true);
		}

		if (selected_index == i) {
			// Handle is selected.
			draw_rect(rect, border_col, false, handle_thickness);
			draw_line(Vector2(handle_x_pos, 0), Vector2(handle_x_pos, h / 2 - handle_thickness), border_col, handle_thickness);
			if (inside_col.a < 1) {
				draw_line(Vector2(handle_start_x + handle_thickness / 2.0, h * 0.9 - handle_thickness), Vector2(handle_start_x + handle_width - handle_thickness / 2.0, h * 0.9 - handle_thickness), border_col, handle_thickness);
			}
			rect = rect.grow(-handle_thickness);
			const Color focus_col = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect(rect, has_focus() ? focus_col : focus_col.darkened(0.4), false, handle_thickness);
			rect = rect.grow(-handle_thickness);
			draw_rect(rect, border_col, false, handle_thickness);
		} else {
			// Handle isn't selected.
			border_col.a = 0.9;
			draw_rect(rect, border_col, false, handle_thickness);
			draw_line(Vector2(handle_x_pos, 0), Vector2(handle_x_pos, h / 2 - handle_thickness), border_col, handle_thickness);
			if (inside_col.a < 1) {
				draw_line(Vector2(handle_start_x + handle_thickness / 2.0, h * 0.9 - handle_thickness), Vector2(handle_start_x + handle_width - handle_thickness / 2.0, h * 0.9 - handle_thickness), border_col, handle_thickness);
			}
			if (hovered_index == i) {
				// Draw a subtle translucent rect inside the handle if it's being hovered.
				rect = rect.grow(-handle_thickness);
				border_col.a = 0.54;
				draw_rect(rect, border_col, false, handle_thickness);
			}
		}
	}

	// Draw "button" for color selector.
	int button_offset = total_w + handle_width + draw_spacing;
	if (selected_index != -1) {
		Color grabbed_col = gradient->get_color(selected_index);
		if (grabbed_col.a < 1) {
			draw_texture_rect(get_editor_theme_icon(SNAME("GuiMiniCheckerboard")), Rect2(button_offset, 0, h, h), true);
		}
		draw_rect(Rect2(button_offset, 0, h, h), grabbed_col);
		if (grabbed_col.r > 1 || grabbed_col.g > 1 || grabbed_col.b > 1) {
			// Draw an indicator to denote that the currently selected color is "overbright".
			draw_texture(get_theme_icon(SNAME("overbright_indicator"), SNAME("ColorPicker")), Point2(button_offset, 0));
		}
	} else {
		// If no color is selected, draw gray color with 'X' on top.
		draw_rect(Rect2(button_offset, 0, h, h), Color(0.5, 0.5, 0.5, 1));
		draw_line(Vector2(button_offset, 0), Vector2(button_offset + h, h), Color(0.8, 0.8, 0.8));
		draw_line(Vector2(button_offset, h), Vector2(button_offset + h, 0), Color(0.8, 0.8, 0.8));
	}
}

void GradientEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			draw_spacing = BASE_SPACING * get_theme_default_base_scale();
			handle_width = BASE_HANDLE_WIDTH * get_theme_default_base_scale();
		} break;
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			RID ae = get_accessibility_element();
			ERR_FAIL_COND(ae.is_null());

			//TODO
			DisplayServer::get_singleton()->accessibility_update_set_role(ae, DisplayServer::AccessibilityRole::ROLE_STATIC_TEXT);
			DisplayServer::get_singleton()->accessibility_update_set_value(ae, TTR(vformat("The %s is not accessible at this time.", "Gradient editor")));
		} break;
		case NOTIFICATION_DRAW: {
			_redraw();
		} break;
		case NOTIFICATION_MOUSE_EXIT: {
			if (hovered_index != -1) {
				hovered_index = -1;
				queue_redraw();
			}
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				grabbing = GRAB_NONE;
			}
		} break;
	}
}

void GradientEdit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_selected_index", "index"), &GradientEdit::set_selected_index);
}

GradientEdit::GradientEdit() {
	set_focus_mode(FOCUS_ALL);
	set_custom_minimum_size(Size2(0, 60) * EDSCALE);

	picker = memnew(ColorPicker);
	int picker_shape = EDITOR_GET("interface/inspector/default_color_picker_shape");
	picker->set_picker_shape((ColorPicker::PickerShapeType)picker_shape);
	picker->connect("color_changed", callable_mp(this, &GradientEdit::_color_changed));

	popup = memnew(PopupPanel);
	popup->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(picker));

	add_child(popup, false, INTERNAL_MODE_FRONT);
	popup->add_child(picker);

	preview_texture.instantiate();
	preview_texture->set_width(1024);
}

///////////////////////

const int GradientEditor::DEFAULT_SNAP = 10;

void GradientEditor::_set_snap_enabled(bool p_enabled) {
	gradient_editor_rect->set_snap_enabled(p_enabled);
	snap_count_edit->set_visible(p_enabled);
}

void GradientEditor::_set_snap_count(int p_count) {
	gradient_editor_rect->set_snap_count(CLAMP(p_count, 2, 100));
}

void GradientEditor::set_gradient(const Ref<Gradient> &p_gradient) {
	gradient_editor_rect->set_gradient(p_gradient);
}

void GradientEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			reverse_button->set_button_icon(get_editor_theme_icon(SNAME("ReverseGradient")));
			snap_button->set_button_icon(get_editor_theme_icon(SNAME("SnapGrid")));
		} break;
		case NOTIFICATION_READY: {
			Ref<Gradient> gradient = gradient_editor_rect->get_gradient();
			if (gradient.is_valid()) {
				// Set snapping settings based on the gradient's meta.
				snap_button->set_pressed(gradient->get_meta("_snap_enabled", false));
				snap_count_edit->set_value(gradient->get_meta("_snap_count", DEFAULT_SNAP));
			}
		} break;
	}
}

GradientEditor::GradientEditor() {
	HFlowContainer *toolbar = memnew(HFlowContainer);
	add_child(toolbar);

	reverse_button = memnew(Button);
	reverse_button->set_tooltip_text(TTR("Reverse/Mirror Gradient"));
	toolbar->add_child(reverse_button);

	toolbar->add_child(memnew(VSeparator));

	snap_button = memnew(Button);
	snap_button->set_tooltip_text(TTR("Toggle Grid Snap"));
	snap_button->set_toggle_mode(true);
	toolbar->add_child(snap_button);
	snap_button->connect(SceneStringName(toggled), callable_mp(this, &GradientEditor::_set_snap_enabled));

	snap_count_edit = memnew(EditorSpinSlider);
	snap_count_edit->set_min(2);
	snap_count_edit->set_max(100);
	snap_count_edit->set_value(DEFAULT_SNAP);
	snap_count_edit->set_custom_minimum_size(Size2(65 * EDSCALE, 0));
	toolbar->add_child(snap_count_edit);
	snap_count_edit->connect(SceneStringName(value_changed), callable_mp(this, &GradientEditor::_set_snap_count));

	gradient_editor_rect = memnew(GradientEdit);
	add_child(gradient_editor_rect);
	reverse_button->connect(SceneStringName(pressed), callable_mp(gradient_editor_rect, &GradientEdit::reverse_gradient));

	set_mouse_filter(MOUSE_FILTER_STOP);
	_set_snap_enabled(snap_button->is_pressed());
	_set_snap_count(snap_count_edit->get_value());
}

///////////////////////

bool EditorInspectorPluginGradient::can_handle(Object *p_object) {
	return Object::cast_to<Gradient>(p_object) != nullptr;
}

void EditorInspectorPluginGradient::parse_begin(Object *p_object) {
	Gradient *gradient = Object::cast_to<Gradient>(p_object);
	ERR_FAIL_NULL(gradient);
	Ref<Gradient> g(gradient);

	GradientEditor *editor = memnew(GradientEditor);
	editor->set_gradient(g);
	add_custom_control(editor);
}

///////////////////////

GradientEditorPlugin::GradientEditorPlugin() {
	Ref<EditorInspectorPluginGradient> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
