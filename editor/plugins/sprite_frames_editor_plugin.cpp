/**************************************************************************/
/*  sprite_frames_editor_plugin.cpp                                       */
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

#include "sprite_frames_editor_plugin.h"

#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/gui/center_container.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/atlas_texture.h"

static void _draw_shadowed_line(Control *p_control, const Point2 &p_from, const Size2 &p_size, const Size2 &p_shadow_offset, Color p_color, Color p_shadow_color) {
	p_control->draw_line(p_from, p_from + p_size, p_color);
	p_control->draw_line(p_from + p_shadow_offset, p_from + p_size + p_shadow_offset, p_shadow_color);
}

void SpriteFramesEditor::_open_sprite_sheet() {
	file_split_sheet->clear_filters();
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("Texture2D", &extensions);
	for (const String &extension : extensions) {
		file_split_sheet->add_filter("*." + extension);
	}

	file_split_sheet->popup_file_dialog();
}

int SpriteFramesEditor::_sheet_preview_position_to_frame_index(const Point2 &p_position) {
	const Size2i offset = _get_offset();
	const Size2i frame_size = _get_frame_size();
	const Size2i separation = _get_separation();
	const Size2i block_size = frame_size + separation;
	const Point2i position = p_position / sheet_zoom - offset;

	if (position.x < 0 || position.y < 0) {
		return -1; // Out of bounds.
	}

	if (position.x % block_size.x >= frame_size.x || position.y % block_size.y >= frame_size.y) {
		return -1; // Gap between frames.
	}

	const Point2i frame = position / block_size;
	const Size2i frame_count = _get_frame_count();
	if (frame.x >= frame_count.x || frame.y >= frame_count.y) {
		return -1; // Out of bounds.
	}

	return frame_count.x * frame.y + frame.x;
}

void SpriteFramesEditor::_sheet_preview_draw() {
	const Size2i frame_count = _get_frame_count();
	const Size2i separation = _get_separation();

	const Size2 draw_offset = Size2(_get_offset()) * sheet_zoom;
	const Size2 draw_sep = Size2(separation) * sheet_zoom;
	const Size2 draw_frame_size = Size2(_get_frame_size()) * sheet_zoom;
	const Size2 draw_size = draw_frame_size * frame_count + draw_sep * (frame_count - Size2i(1, 1));

	const Color line_color = Color(1, 1, 1, 0.3);
	const Color shadow_color = Color(0, 0, 0, 0.3);

	// Vertical lines.
	_draw_shadowed_line(split_sheet_preview, draw_offset, Vector2(0, draw_size.y), Vector2(1, 0), line_color, shadow_color);
	for (int i = 0; i < frame_count.x - 1; i++) {
		const Point2 start = draw_offset + Vector2(i * draw_sep.x + (i + 1) * draw_frame_size.x, 0);
		if (separation.x == 0) {
			_draw_shadowed_line(split_sheet_preview, start, Vector2(0, draw_size.y), Vector2(1, 0), line_color, shadow_color);
		} else {
			const Size2 size = Size2(draw_sep.x, draw_size.y);
			split_sheet_preview->draw_rect(Rect2(start, size), line_color);
		}
	}
	_draw_shadowed_line(split_sheet_preview, draw_offset + Vector2(draw_size.x, 0), Vector2(0, draw_size.y), Vector2(1, 0), line_color, shadow_color);

	// Horizontal lines.
	_draw_shadowed_line(split_sheet_preview, draw_offset, Vector2(draw_size.x, 0), Vector2(0, 1), line_color, shadow_color);
	for (int i = 0; i < frame_count.y - 1; i++) {
		const Point2 start = draw_offset + Vector2(0, i * draw_sep.y + (i + 1) * draw_frame_size.y);
		if (separation.y == 0) {
			_draw_shadowed_line(split_sheet_preview, start, Vector2(draw_size.x, 0), Vector2(0, 1), line_color, shadow_color);
		} else {
			const Size2 size = Size2(draw_size.x, draw_sep.y);
			split_sheet_preview->draw_rect(Rect2(start, size), line_color);
		}
	}
	_draw_shadowed_line(split_sheet_preview, draw_offset + Vector2(0, draw_size.y), Vector2(draw_size.x, 0), Vector2(0, 1), line_color, shadow_color);

	if (frames_selected.size() == 0) {
		split_sheet_dialog->get_ok_button()->set_disabled(true);
		split_sheet_dialog->set_ok_button_text(TTR("No Frames Selected"));
		return;
	}

	Color accent = get_theme_color("accent_color", EditorStringName(Editor));

	_sheet_sort_frames();

	Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	int font_size = get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));

	for (int i = 0; i < frames_ordered.size(); ++i) {
		const int idx = frames_ordered[i].second;

		const int x = idx % frame_count.x;
		const int y = idx / frame_count.x;
		const Point2 pos = draw_offset + Point2(x, y) * (draw_frame_size + draw_sep);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(5, 5), draw_frame_size - Size2(10, 10)), Color(0, 0, 0, 0.35), true);
		split_sheet_preview->draw_rect(Rect2(pos, draw_frame_size), Color(0, 0, 0, 1), false);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(1, 1), draw_frame_size - Size2(2, 2)), Color(0, 0, 0, 1), false);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(2, 2), draw_frame_size - Size2(4, 4)), accent, false);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(3, 3), draw_frame_size - Size2(6, 6)), accent, false);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(4, 4), draw_frame_size - Size2(8, 8)), Color(0, 0, 0, 1), false);
		split_sheet_preview->draw_rect(Rect2(pos + Size2(5, 5), draw_frame_size - Size2(10, 10)), Color(0, 0, 0, 1), false);

		const String text = itos(i);
		const Vector2 string_size = font->get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);

		// Stop rendering text if too large.
		if (string_size.x + 6 < draw_frame_size.x && string_size.y / 2 + 10 < draw_frame_size.y) {
			split_sheet_preview->draw_string_outline(font, pos + Size2(5, 7) + Size2(0, string_size.y / 2), text, HORIZONTAL_ALIGNMENT_LEFT, string_size.x, font_size, 1, Color(0, 0, 0, 1));
			split_sheet_preview->draw_string(font, pos + Size2(5, 7) + Size2(0, string_size.y / 2), text, HORIZONTAL_ALIGNMENT_LEFT, string_size.x, font_size, Color(1, 1, 1));
		}
	}

	split_sheet_dialog->get_ok_button()->set_disabled(false);
	split_sheet_dialog->set_ok_button_text(vformat(TTR("Add %d Frame(s)"), frames_selected.size()));
}

void SpriteFramesEditor::_sheet_preview_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		const int idx = _sheet_preview_position_to_frame_index(mb->get_position());

		if (idx != -1) {
			if (mb->is_shift_pressed() && last_frame_selected >= 0) {
				// Select multiple frames.
				const int from = last_frame_selected;
				const int to = idx;

				const int diff = ABS(to - from);
				const int dir = SIGN(to - from);

				for (int i = 0; i <= diff; i++) {
					const int this_idx = from + i * dir;

					// Prevent double-toggling the same frame when moving the mouse when the mouse button is still held.
					frames_toggled_by_mouse_hover.insert(this_idx);

					if (mb->is_command_or_control_pressed()) {
						frames_selected.erase(this_idx);
					} else if (!frames_selected.has(this_idx)) {
						frames_selected.insert(this_idx, selected_count);
						selected_count++;
					}
				}
			} else {
				// Prevent double-toggling the same frame when moving the mouse when the mouse button is still held.
				frames_toggled_by_mouse_hover.insert(idx);

				if (frames_selected.has(idx)) {
					frames_selected.erase(idx);
				} else {
					frames_selected.insert(idx, selected_count);
					selected_count++;
				}
			}
		}

		if (last_frame_selected != idx || idx != -1) {
			last_frame_selected = idx;
			frames_need_sort = true;
			split_sheet_preview->queue_redraw();
		}
	}

	if (mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		frames_toggled_by_mouse_hover.clear();
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		// Select by holding down the mouse button on frames.
		const int idx = _sheet_preview_position_to_frame_index(mm->get_position());

		if (idx != -1 && !frames_toggled_by_mouse_hover.has(idx)) {
			// Only allow toggling each tile once per mouse hold.
			// Otherwise, the selection would constantly "flicker" in and out when moving the mouse cursor.
			// The mouse button must be released before it can be toggled again.
			frames_toggled_by_mouse_hover.insert(idx);

			if (frames_selected.has(idx)) {
				frames_selected.erase(idx);
			} else {
				frames_selected.insert(idx, selected_count);
				selected_count++;
			}

			last_frame_selected = idx;
			frames_need_sort = true;
			split_sheet_preview->queue_redraw();
		}
	}

	if (frames_selected.is_empty()) {
		selected_count = 0;
	}
}

void SpriteFramesEditor::_sheet_scroll_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		// Zoom in/out using Ctrl + mouse wheel. This is done on the ScrollContainer
		// to allow performing this action anywhere, even if the cursor isn't
		// hovering the texture in the workspace.
		// keep CTRL and not CMD_OR_CTRL as CTRL is expected even on MacOS.
		if (mb->get_button_index() == MouseButton::WHEEL_UP && mb->is_pressed() && mb->is_ctrl_pressed()) {
			_sheet_zoom_on_position(scale_ratio, mb->get_position());
			// Don't scroll up after zooming in.
			split_sheet_scroll->accept_event();
		} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN && mb->is_pressed() && mb->is_ctrl_pressed()) {
			_sheet_zoom_on_position(1 / scale_ratio, mb->get_position());
			// Don't scroll down after zooming out.
			split_sheet_scroll->accept_event();
		}
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && mm->get_button_mask().has_flag(MouseButtonMask::MIDDLE)) {
		const Vector2 dragged = Input::get_singleton()->warp_mouse_motion(mm, split_sheet_scroll->get_global_rect());
		split_sheet_scroll->set_h_scroll(split_sheet_scroll->get_h_scroll() - dragged.x);
		split_sheet_scroll->set_v_scroll(split_sheet_scroll->get_v_scroll() - dragged.y);
	}
}

void SpriteFramesEditor::_sheet_add_frames() {
	const Size2i frame_count = _get_frame_count();
	const Size2i frame_size = _get_frame_size();
	const Size2i offset = _get_offset();
	const Size2i separation = _get_separation();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());
	int fc = frames->get_frame_count(edited_anim);

	_sheet_sort_frames();

	for (const Pair<int, int> &pair : frames_ordered) {
		const int idx = pair.second;

		const Point2 frame_coords(idx % frame_count.x, idx / frame_count.x);

		Ref<AtlasTexture> at;
		at.instantiate();
		at->set_atlas(split_sheet_preview->get_texture());
		at->set_region(Rect2(offset + frame_coords * (frame_size + separation), frame_size));

		undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, at, 1.0, -1);
		undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, fc);
	}

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_sheet_zoom_on_position(float p_zoom, const Vector2 &p_position) {
	const float old_zoom = sheet_zoom;
	sheet_zoom = CLAMP(sheet_zoom * p_zoom, min_sheet_zoom, max_sheet_zoom);

	const Size2 texture_size = split_sheet_preview->get_texture()->get_size();
	split_sheet_preview->set_custom_minimum_size(texture_size * sheet_zoom);

	Vector2 offset = Vector2(split_sheet_scroll->get_h_scroll(), split_sheet_scroll->get_v_scroll());
	offset = (offset + p_position) / old_zoom * sheet_zoom - p_position;
	split_sheet_scroll->set_h_scroll(offset.x);
	split_sheet_scroll->set_v_scroll(offset.y);
}

void SpriteFramesEditor::_sheet_zoom_in() {
	_sheet_zoom_on_position(scale_ratio, Vector2());
}

void SpriteFramesEditor::_sheet_zoom_out() {
	_sheet_zoom_on_position(1 / scale_ratio, Vector2());
}

void SpriteFramesEditor::_sheet_zoom_reset() {
	// Default the zoom to match the editor scale, but don't dezoom on editor scales below 100% to prevent pixel art from looking bad.
	sheet_zoom = MAX(1.0f, EDSCALE);
	Size2 texture_size = split_sheet_preview->get_texture()->get_size();
	split_sheet_preview->set_custom_minimum_size(texture_size * sheet_zoom);
}

void SpriteFramesEditor::_sheet_order_selected(int p_option) {
	frames_need_sort = true;
	split_sheet_preview->queue_redraw();
}

void SpriteFramesEditor::_sheet_select_all_frames() {
	for (int i = 0; i < split_sheet_h->get_value() * split_sheet_v->get_value(); i++) {
		if (!frames_selected.has(i)) {
			frames_selected.insert(i, selected_count);
			selected_count++;
			frames_need_sort = true;
		}
	}

	split_sheet_preview->queue_redraw();
}

void SpriteFramesEditor::_sheet_clear_all_frames() {
	frames_selected.clear();
	selected_count = 0;

	split_sheet_preview->queue_redraw();
}

void SpriteFramesEditor::_sheet_sort_frames() {
	if (!frames_need_sort) {
		return;
	}
	frames_need_sort = false;
	frames_ordered.resize(frames_selected.size());
	if (frames_selected.is_empty()) {
		return;
	}

	const Size2i frame_count = _get_frame_count();
	const int frame_order = split_sheet_order->get_selected_id();
	int index = 0;

	// Fill based on order.
	for (const KeyValue<int, int> &from_pair : frames_selected) {
		const int idx = from_pair.key;

		const int selection_order = from_pair.value;

		// Default to using selection order.
		int order_by = selection_order;

		// Extract coordinates for sorting.
		const int pos_frame_x = idx % frame_count.x;
		const int pos_frame_y = idx / frame_count.x;

		const int neg_frame_x = frame_count.x - (pos_frame_x + 1);
		const int neg_frame_y = frame_count.y - (pos_frame_y + 1);

		switch (frame_order) {
			case FRAME_ORDER_LEFT_RIGHT_TOP_BOTTOM: {
				order_by = frame_count.x * pos_frame_y + pos_frame_x;
			} break;

			case FRAME_ORDER_LEFT_RIGHT_BOTTOM_TOP: {
				order_by = frame_count.x * neg_frame_y + pos_frame_x;
			} break;

			case FRAME_ORDER_RIGHT_LEFT_TOP_BOTTOM: {
				order_by = frame_count.x * pos_frame_y + neg_frame_x;
			} break;

			case FRAME_ORDER_RIGHT_LEFT_BOTTOM_TOP: {
				order_by = frame_count.x * neg_frame_y + neg_frame_x;
			} break;

			case FRAME_ORDER_TOP_BOTTOM_LEFT_RIGHT: {
				order_by = pos_frame_y + frame_count.y * pos_frame_x;
			} break;

			case FRAME_ORDER_TOP_BOTTOM_RIGHT_LEFT: {
				order_by = pos_frame_y + frame_count.y * neg_frame_x;
			} break;

			case FRAME_ORDER_BOTTOM_TOP_LEFT_RIGHT: {
				order_by = neg_frame_y + frame_count.y * pos_frame_x;
			} break;

			case FRAME_ORDER_BOTTOM_TOP_RIGHT_LEFT: {
				order_by = neg_frame_y + frame_count.y * neg_frame_x;
			} break;
		}

		// Assign in vector.
		frames_ordered.set(index, Pair<int, int>(order_by, idx));
		index++;
	}

	// Sort frames.
	frames_ordered.sort_custom<PairSort<int, int>>();
}

void SpriteFramesEditor::_sheet_spin_changed(double p_value, int p_dominant_param) {
	if (updating_split_settings) {
		return;
	}
	updating_split_settings = true;

	if (p_dominant_param != PARAM_USE_CURRENT) {
		dominant_param = p_dominant_param;
	}

	const Size2i texture_size = split_sheet_preview->get_texture()->get_size();
	const Size2i size = texture_size - _get_offset();

	switch (dominant_param) {
		case PARAM_SIZE: {
			const Size2i frame_size = _get_frame_size();

			const Size2i offset_max = texture_size - frame_size;
			split_sheet_offset_x->set_max(offset_max.x);
			split_sheet_offset_y->set_max(offset_max.y);

			const Size2i sep_max = size - frame_size * 2;
			split_sheet_sep_x->set_max(sep_max.x);
			split_sheet_sep_y->set_max(sep_max.y);

			const Size2i separation = _get_separation();
			const Size2i count = (size + separation) / (frame_size + separation);
			split_sheet_h->set_value(count.x);
			split_sheet_v->set_value(count.y);
		} break;

		case PARAM_FRAME_COUNT: {
			const Size2i count = _get_frame_count();

			const Size2i offset_max = texture_size - count;
			split_sheet_offset_x->set_max(offset_max.x);
			split_sheet_offset_y->set_max(offset_max.y);

			const Size2i gap_count = count - Size2i(1, 1);
			split_sheet_sep_x->set_max(gap_count.x == 0 ? size.x : (size.x - count.x) / gap_count.x);
			split_sheet_sep_y->set_max(gap_count.y == 0 ? size.y : (size.y - count.y) / gap_count.y);

			const Size2i separation = _get_separation();
			const Size2i frame_size = (size - separation * gap_count) / count;
			split_sheet_size_x->set_value(frame_size.x);
			split_sheet_size_y->set_value(frame_size.y);
		} break;
	}

	updating_split_settings = false;

	frames_selected.clear();
	selected_count = 0;
	last_frame_selected = -1;
	split_sheet_preview->queue_redraw();
}

void SpriteFramesEditor::_toggle_show_settings() {
	split_sheet_settings_vb->set_visible(!split_sheet_settings_vb->is_visible());

	_update_show_settings();
}

void SpriteFramesEditor::_update_show_settings() {
	if (is_layout_rtl()) {
		toggle_settings_button->set_button_icon(get_editor_theme_icon(split_sheet_settings_vb->is_visible() ? SNAME("Back") : SNAME("Forward")));
	} else {
		toggle_settings_button->set_button_icon(get_editor_theme_icon(split_sheet_settings_vb->is_visible() ? SNAME("Forward") : SNAME("Back")));
	}
}

void SpriteFramesEditor::_auto_slice_sprite_sheet() {
	if (updating_split_settings) {
		return;
	}
	updating_split_settings = true;

	const Size2i size = split_sheet_preview->get_texture()->get_size();

	const Size2i split_sheet = _estimate_sprite_sheet_size(split_sheet_preview->get_texture());
	split_sheet_h->set_value(split_sheet.x);
	split_sheet_v->set_value(split_sheet.y);
	split_sheet_size_x->set_value(size.x / split_sheet.x);
	split_sheet_size_y->set_value(size.y / split_sheet.y);
	split_sheet_sep_x->set_value(0);
	split_sheet_sep_y->set_value(0);
	split_sheet_offset_x->set_value(0);
	split_sheet_offset_y->set_value(0);

	updating_split_settings = false;

	frames_selected.clear();
	selected_count = 0;
	last_frame_selected = -1;
	split_sheet_preview->queue_redraw();
}

bool SpriteFramesEditor::_matches_background_color(const Color &p_background_color, const Color &p_pixel_color) {
	if ((p_background_color.a == 0 && p_pixel_color.a == 0) || p_background_color.is_equal_approx(p_pixel_color)) {
		return true;
	}

	Color d = p_background_color - p_pixel_color;
	// 0.04f is the threshold for how much a colour can deviate from background colour and still be considered a match. Arrived at through experimentation, can be tweaked.
	return (d.r * d.r) + (d.g * d.g) + (d.b * d.b) + (d.a * d.a) < 0.04f;
}

Size2i SpriteFramesEditor::_estimate_sprite_sheet_size(const Ref<Texture2D> p_texture) {
	Ref<Image> image = p_texture->get_image();
	Size2i size = p_texture->get_size();

	Color assumed_background_color = image->get_pixel(0, 0);
	Size2i sheet_size;

	bool previous_line_background = true;
	for (int x = 0; x < size.x; x++) {
		int y = 0;
		while (y < size.y && _matches_background_color(assumed_background_color, image->get_pixel(x, y))) {
			y++;
		}
		bool current_line_background = (y == size.y);
		if (previous_line_background && !current_line_background) {
			sheet_size.x++;
		}
		previous_line_background = current_line_background;
	}

	previous_line_background = true;
	for (int y = 0; y < size.y; y++) {
		int x = 0;
		while (x < size.x && _matches_background_color(assumed_background_color, image->get_pixel(x, y))) {
			x++;
		}
		bool current_line_background = (x == size.x);
		if (previous_line_background && !current_line_background) {
			sheet_size.y++;
		}
		previous_line_background = current_line_background;
	}

	if (sheet_size == Size2i(0, 0) || sheet_size == Size2i(1, 1)) {
		sheet_size = Size2i(4, 4);
	}

	return sheet_size;
}

void SpriteFramesEditor::_prepare_sprite_sheet(const String &p_file) {
	Ref<Texture2D> texture = ResourceLoader::load(p_file);
	if (texture.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Unable to load images"));
		ERR_FAIL_COND(texture.is_null());
	}
	frames_selected.clear();
	selected_count = 0;
	last_frame_selected = -1;

	bool new_texture = texture != split_sheet_preview->get_texture();
	split_sheet_preview->set_texture(texture);
	if (new_texture) {
		// Reset spin max.
		const Size2i size = texture->get_size();
		split_sheet_size_x->set_max(size.x);
		split_sheet_size_y->set_max(size.y);
		split_sheet_sep_x->set_max(size.x);
		split_sheet_sep_y->set_max(size.y);
		split_sheet_offset_x->set_max(size.x);
		split_sheet_offset_y->set_max(size.y);

		if (size != previous_texture_size) {
			// Different texture, reset to 4x4.
			dominant_param = PARAM_FRAME_COUNT;
			updating_split_settings = true;
			const Size2i split_sheet = Size2i(4, 4);
			split_sheet_h->set_value(split_sheet.x);
			split_sheet_v->set_value(split_sheet.y);
			split_sheet_size_x->set_value(size.x / split_sheet.x);
			split_sheet_size_y->set_value(size.y / split_sheet.y);
			split_sheet_sep_x->set_value(0);
			split_sheet_sep_y->set_value(0);
			split_sheet_offset_x->set_value(0);
			split_sheet_offset_y->set_value(0);
			updating_split_settings = false;
		}
		previous_texture_size = size;

		// Reset zoom.
		_sheet_zoom_reset();
	}

	split_sheet_dialog->popup_centered_ratio(0.65);
}

void SpriteFramesEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &SpriteFramesEditor::_node_removed));

			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			autoplay_icon = get_editor_theme_icon(SNAME("AutoPlay"));
			stop_icon = get_editor_theme_icon(SNAME("Stop"));
			pause_icon = get_editor_theme_icon(SNAME("Pause"));
			_update_stop_icon();

			autoplay->set_button_icon(get_editor_theme_icon(SNAME("AutoPlay")));
			anim_loop->set_button_icon(get_editor_theme_icon(SNAME("Loop")));
			play->set_button_icon(get_editor_theme_icon(SNAME("PlayStart")));
			play_from->set_button_icon(get_editor_theme_icon(SNAME("Play")));
			play_bw->set_button_icon(get_editor_theme_icon(SNAME("PlayStartBackwards")));
			play_bw_from->set_button_icon(get_editor_theme_icon(SNAME("PlayBackwards")));

			load->set_button_icon(get_editor_theme_icon(SNAME("Load")));
			load_sheet->set_button_icon(get_editor_theme_icon(SNAME("SpriteSheet")));
			copy->set_button_icon(get_editor_theme_icon(SNAME("ActionCopy")));
			paste->set_button_icon(get_editor_theme_icon(SNAME("ActionPaste")));
			empty_before->set_button_icon(get_editor_theme_icon(SNAME("InsertBefore")));
			empty_after->set_button_icon(get_editor_theme_icon(SNAME("InsertAfter")));
			move_up->set_button_icon(get_editor_theme_icon(SNAME("MoveLeft")));
			move_down->set_button_icon(get_editor_theme_icon(SNAME("MoveRight")));
			delete_frame->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			zoom_out->set_button_icon(get_editor_theme_icon(SNAME("ZoomLess")));
			zoom_reset->set_button_icon(get_editor_theme_icon(SNAME("ZoomReset")));
			zoom_in->set_button_icon(get_editor_theme_icon(SNAME("ZoomMore")));
			add_anim->set_button_icon(get_editor_theme_icon(SNAME("New")));
			duplicate_anim->set_button_icon(get_editor_theme_icon(SNAME("Duplicate")));
			delete_anim->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			anim_search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			split_sheet_zoom_out->set_button_icon(get_editor_theme_icon(SNAME("ZoomLess")));
			split_sheet_zoom_reset->set_button_icon(get_editor_theme_icon(SNAME("ZoomReset")));
			split_sheet_zoom_in->set_button_icon(get_editor_theme_icon(SNAME("ZoomMore")));
			split_sheet_scroll->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));

			_update_show_settings();
		} break;

		case NOTIFICATION_READY: {
			add_theme_constant_override("autohide", 1); // Fixes the dragger always showing up.
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("node_removed", callable_mp(this, &SpriteFramesEditor::_node_removed));
		} break;
	}
}

void SpriteFramesEditor::_file_load_request(const Vector<String> &p_path, int p_at_pos) {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	List<Ref<Texture2D>> resources;

	for (int i = 0; i < p_path.size(); i++) {
		Ref<Texture2D> resource;
		resource = ResourceLoader::load(p_path[i]);

		if (resource.is_null()) {
			dialog->set_text(TTR("ERROR: Couldn't load frame resource!"));
			dialog->set_title(TTR("Error!"));

			//dialog->get_cancel()->set_text("Close");
			dialog->set_ok_button_text(TTR("Close"));
			dialog->popup_centered();
			return; ///beh should show an error i guess
		}

		resources.push_back(resource);
	}

	if (resources.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());
	int fc = frames->get_frame_count(edited_anim);

	int count = 0;

	for (const Ref<Texture2D> &E : resources) {
		undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, E, 1.0, p_at_pos == -1 ? -1 : p_at_pos + count);
		undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, p_at_pos == -1 ? fc : p_at_pos);
		count++;
	}
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");

	undo_redo->commit_action();
}

Size2i SpriteFramesEditor::_get_frame_count() const {
	return Size2i(split_sheet_h->get_value(), split_sheet_v->get_value());
}

Size2i SpriteFramesEditor::_get_frame_size() const {
	return Size2i(split_sheet_size_x->get_value(), split_sheet_size_y->get_value());
}

Size2i SpriteFramesEditor::_get_offset() const {
	return Size2i(split_sheet_offset_x->get_value(), split_sheet_offset_y->get_value());
}

Size2i SpriteFramesEditor::_get_separation() const {
	return Size2i(split_sheet_sep_x->get_value(), split_sheet_sep_y->get_value());
}

void SpriteFramesEditor::_load_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));
	loading_scene = false;

	file->clear_filters();
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("Texture2D", &extensions);
	for (const String &extension : extensions) {
		file->add_filter("*." + extension);
	}

	file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
	file->popup_file_dialog();
}

void SpriteFramesEditor::_paste_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	Ref<ClipboardSpriteFrames> clipboard_frames = EditorSettings::get_singleton()->get_resource_clipboard();
	if (clipboard_frames.is_valid()) {
		_paste_frame_array(clipboard_frames);
		return;
	}

	Ref<Texture2D> texture = EditorSettings::get_singleton()->get_resource_clipboard();
	if (texture.is_valid()) {
		_paste_texture(texture);
		return;
	}
}

void SpriteFramesEditor::_paste_frame_array(const Ref<ClipboardSpriteFrames> &p_clipboard_frames) {
	if (p_clipboard_frames->frames.is_empty()) {
		return;
	}

	Ref<Texture2D> texture;
	float duration = 1.0;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Paste Frame(s)"), UndoRedo::MERGE_DISABLE, frames.ptr());

	int undo_index = frames->get_frame_count(edited_anim);

	for (int index = 0; index < p_clipboard_frames->frames.size(); index++) {
		const ClipboardSpriteFrames::Frame &frame = p_clipboard_frames->frames[index];
		texture = frame.texture;
		duration = frame.duration;

		undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, texture, duration);
		undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, undo_index);
	}

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_paste_texture(const Ref<Texture2D> &p_texture) {
	float duration = 1.0;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Paste Texture"), UndoRedo::MERGE_DISABLE, frames.ptr());

	int undo_index = frames->get_frame_count(edited_anim);

	undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, p_texture, duration);
	undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, undo_index);

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_copy_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	Vector<int> selected_items = frame_list->get_selected_items();

	if (selected_items.is_empty()) {
		return;
	}

	Ref<ClipboardSpriteFrames> clipboard_frames = memnew(ClipboardSpriteFrames);

	for (const int &frame_index : selected_items) {
		Ref<Texture2D> texture = frames->get_frame_texture(edited_anim, frame_index);
		if (texture.is_null()) {
			continue;
		}

		ClipboardSpriteFrames::Frame frame;
		frame.texture = texture;
		frame.duration = frames->get_frame_duration(edited_anim, frame_index);

		clipboard_frames->frames.push_back(frame);
	}
	EditorSettings::get_singleton()->set_resource_clipboard(clipboard_frames);
}

void SpriteFramesEditor::_empty_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	int from = -1;
	Vector<int> selected_items = frame_list->get_selected_items();

	if (!selected_items.is_empty()) {
		from = selected_items[0];
		selection.clear();
		selection.push_back(from + 1);
	} else {
		from = frames->get_frame_count(edited_anim);
	}

	Ref<Texture2D> texture;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Empty"), UndoRedo::MERGE_DISABLE, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, texture, 1.0, from);
	undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, from);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_empty2_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	int from = -1;
	Vector<int> selected_items = frame_list->get_selected_items();

	if (!selected_items.is_empty()) {
		from = selected_items[selected_items.size() - 1];
		selection.clear();
		selection.push_back(from);
	} else {
		from = frames->get_frame_count(edited_anim);
	}

	Ref<Texture2D> texture;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Empty"), UndoRedo::MERGE_DISABLE, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, texture, 1.0, from + 1);
	undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, from + 1);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_up_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	Vector<int> selected_items = frame_list->get_selected_items();

	int nb_selected_items = selected_items.size();
	if (nb_selected_items <= 0) {
		return;
	}

	int first_selected_frame_index = selected_items[0];
	if (first_selected_frame_index < 1) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Move Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());

	int last_overwritten_frame = -1;

	for (int selected_index = 0; selected_index < nb_selected_items; selected_index++) {
		int to_move = selected_items[selected_index];
		int new_index = to_move - 1;
		selected_items.set(selected_index, new_index);

		undo_redo->add_do_method(frames.ptr(), "set_frame", edited_anim, new_index, frames->get_frame_texture(edited_anim, to_move), frames->get_frame_duration(edited_anim, to_move));
		undo_redo->add_undo_method(frames.ptr(), "set_frame", edited_anim, new_index, frames->get_frame_texture(edited_anim, new_index), frames->get_frame_duration(edited_anim, new_index));

		bool is_next_item_in_selection = selected_index + 1 < nb_selected_items && selected_items[selected_index + 1] == to_move + 1;
		if (last_overwritten_frame == -1) {
			last_overwritten_frame = new_index;
		}

		if (!is_next_item_in_selection) {
			undo_redo->add_do_method(frames.ptr(), "set_frame", edited_anim, to_move, frames->get_frame_texture(edited_anim, last_overwritten_frame), frames->get_frame_duration(edited_anim, last_overwritten_frame));
			undo_redo->add_undo_method(frames.ptr(), "set_frame", edited_anim, to_move, frames->get_frame_texture(edited_anim, to_move), frames->get_frame_duration(edited_anim, to_move));
			last_overwritten_frame = -1;
		}
	}
	selection = selected_items;

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_down_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	Vector<int> selected_items = frame_list->get_selected_items();

	int nb_selected_items = selected_items.size();
	if (nb_selected_items <= 0) {
		return;
	}

	int last_selected_frame_index = selected_items[nb_selected_items - 1];
	if (last_selected_frame_index >= frames->get_frame_count(edited_anim) - 1) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Move Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());

	int first_moved_frame = -1;

	for (int selected_index = 0; selected_index < nb_selected_items; selected_index++) {
		int to_move = selected_items[selected_index];
		int new_index = to_move + 1;
		selected_items.set(selected_index, new_index);

		undo_redo->add_do_method(frames.ptr(), "set_frame", edited_anim, new_index, frames->get_frame_texture(edited_anim, to_move), frames->get_frame_duration(edited_anim, to_move));
		undo_redo->add_undo_method(frames.ptr(), "set_frame", edited_anim, new_index, frames->get_frame_texture(edited_anim, new_index), frames->get_frame_duration(edited_anim, new_index));

		bool is_next_item_in_selection = selected_index + 1 < nb_selected_items && selected_items[selected_index + 1] == new_index;
		if (first_moved_frame == -1) {
			first_moved_frame = to_move;
		}

		if (!is_next_item_in_selection) {
			undo_redo->add_do_method(frames.ptr(), "set_frame", edited_anim, first_moved_frame, frames->get_frame_texture(edited_anim, new_index), frames->get_frame_duration(edited_anim, new_index));
			undo_redo->add_undo_method(frames.ptr(), "set_frame", edited_anim, first_moved_frame, frames->get_frame_texture(edited_anim, first_moved_frame), frames->get_frame_duration(edited_anim, first_moved_frame));
			first_moved_frame = -1;
		}
	}
	selection = selected_items;

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_delete_pressed() {
	ERR_FAIL_COND(!frames->has_animation(edited_anim));

	Vector<int> selected_items = frame_list->get_selected_items();

	int nb_selected_items = selected_items.size();
	if (nb_selected_items <= 0) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete Resource"), UndoRedo::MERGE_DISABLE, frames.ptr());
	for (int selected_index = 0; selected_index < nb_selected_items; selected_index++) {
		int to_delete = selected_items[selected_index];
		undo_redo->add_do_method(frames.ptr(), "remove_frame", edited_anim, to_delete - selected_index);
		undo_redo->add_undo_method(frames.ptr(), "add_frame", edited_anim, frames->get_frame_texture(edited_anim, to_delete), frames->get_frame_duration(edited_anim, to_delete), to_delete);
	}

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_animation_selected() {
	if (updating) {
		return;
	}

	TreeItem *selected = animations->get_selected();
	ERR_FAIL_NULL(selected);
	edited_anim = selected->get_text(0);

	if (animated_sprite) {
		sprite_node_updating = true;
		animated_sprite->call("set_animation", edited_anim);
		sprite_node_updating = false;
	}

	_update_library(true);
}

void SpriteFramesEditor::_sync_animation() {
	if (!animated_sprite || sprite_node_updating) {
		return;
	}
	_select_animation(animated_sprite->call("get_animation"), false);
	_update_stop_icon();
}

void SpriteFramesEditor::_select_animation(const String &p_name, bool p_update_node) {
	if (frames.is_null() || !frames->has_animation(p_name)) {
		return;
	}
	edited_anim = p_name;

	if (animated_sprite) {
		if (p_update_node) {
			animated_sprite->call("set_animation", edited_anim);
		}
	}

	_update_library();
}

static void _find_anim_sprites(Node *p_node, List<Node *> *r_nodes, Ref<SpriteFrames> p_sfames) {
	Node *edited = EditorNode::get_singleton()->get_edited_scene();
	if (!edited) {
		return;
	}
	if (p_node != edited && p_node->get_owner() != edited) {
		return;
	}

	{
		AnimatedSprite2D *as = Object::cast_to<AnimatedSprite2D>(p_node);
		if (as && as->get_sprite_frames() == p_sfames) {
			r_nodes->push_back(p_node);
		}
	}

	{
		AnimatedSprite3D *as = Object::cast_to<AnimatedSprite3D>(p_node);
		if (as && as->get_sprite_frames() == p_sfames) {
			r_nodes->push_back(p_node);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_anim_sprites(p_node->get_child(i), r_nodes, p_sfames);
	}
}

void SpriteFramesEditor::_animation_name_edited() {
	if (updating) {
		return;
	}

	if (!frames->has_animation(edited_anim)) {
		return;
	}

	TreeItem *edited = animations->get_edited();
	if (!edited) {
		return;
	}

	String new_name = edited->get_text(0);

	if (new_name == String(edited_anim)) {
		return;
	}

	if (new_name.is_empty()) {
		new_name = "new_animation";
	}

	new_name = new_name.replace("/", "_").replace(",", " ");

	String name = new_name;
	int counter = 0;
	while (frames->has_animation(name)) {
		if (name == String(edited_anim)) {
			edited->set_text(0, name); // The name didn't change, just updated the column text to name.
			return;
		}
		counter++;
		name = new_name + "_" + itos(counter);
	}
	edited->set_text(0, name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Rename Animation"), UndoRedo::MERGE_DISABLE, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "rename_animation", edited_anim, name);
	undo_redo->add_undo_method(frames.ptr(), "rename_animation", name, edited_anim);
	_rename_node_animation(undo_redo, false, edited_anim, name, name);
	_rename_node_animation(undo_redo, true, edited_anim, edited_anim, edited_anim);
	undo_redo->add_do_method(this, "_select_animation", name);
	undo_redo->add_undo_method(this, "_select_animation", edited_anim);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();

	animations->grab_focus();
}

void SpriteFramesEditor::_rename_node_animation(EditorUndoRedoManager *undo_redo, bool is_undo, const String &p_filter, const String &p_new_animation, const String &p_new_autoplay) {
	List<Node *> nodes;
	_find_anim_sprites(EditorNode::get_singleton()->get_edited_scene(), &nodes, Ref<SpriteFrames>(frames));

	if (is_undo) {
		for (Node *E : nodes) {
			String current_name = E->call("get_animation");
			if (current_name == p_filter) {
				undo_redo->force_fixed_history(); // Fixes corner-case when editing SpriteFrames stored as separate file.
				undo_redo->add_undo_method(E, "set_animation", p_new_animation);
			}
			String autoplay_name = E->call("get_autoplay");
			if (autoplay_name == p_filter) {
				undo_redo->force_fixed_history();
				undo_redo->add_undo_method(E, "set_autoplay", p_new_autoplay);
			}
		}
	} else {
		for (Node *E : nodes) {
			String current_name = E->call("get_animation");
			if (current_name == p_filter) {
				undo_redo->force_fixed_history();
				undo_redo->add_do_method(E, "set_animation", p_new_animation);
			}
			String autoplay_name = E->call("get_autoplay");
			if (autoplay_name == p_filter) {
				undo_redo->force_fixed_history();
				undo_redo->add_do_method(E, "set_autoplay", p_new_autoplay);
			}
		}
	}
}

void SpriteFramesEditor::_animation_add() {
	String name = "new_animation";
	int counter = 0;
	while (frames->has_animation(name)) {
		counter++;
		name = vformat("new_animation_%d", counter);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Animation"), UndoRedo::MERGE_DISABLE, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "add_animation", name);
	undo_redo->add_undo_method(frames.ptr(), "remove_animation", name);
	undo_redo->add_do_method(this, "_select_animation", name);
	undo_redo->add_undo_method(this, "_select_animation", edited_anim);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();

	animations->grab_focus();
}

void SpriteFramesEditor::_animation_duplicate() {
	if (updating) {
		return;
	}

	if (!frames->has_animation(edited_anim)) {
		return;
	}

	int counter = 1;
	String new_name = edited_anim;
	PackedStringArray name_component = new_name.rsplit("_", true, 1);
	String base_name = name_component[0];
	if (name_component.size() > 1 && name_component[1].is_valid_int() && name_component[1].to_int() >= 0) {
		counter = name_component[1].to_int();
	}
	new_name = base_name + "_" + itos(counter);
	while (frames->has_animation(new_name)) {
		counter++;
		new_name = base_name + "_" + itos(counter);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Duplicate Animation"), UndoRedo::MERGE_DISABLE, EditorNode::get_singleton()->get_edited_scene());
	undo_redo->add_do_method(frames.ptr(), "duplicate_animation", edited_anim, new_name);
	undo_redo->add_undo_method(frames.ptr(), "remove_animation", new_name);
	undo_redo->add_do_method(this, "_select_animation", new_name);
	undo_redo->add_undo_method(this, "_select_animation", edited_anim);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();

	animations->grab_focus();
}

void SpriteFramesEditor::_animation_remove() {
	if (updating) {
		return;
	}

	if (!frames->has_animation(edited_anim)) {
		return;
	}

	delete_dialog->set_text(TTR("Delete Animation?"));
	delete_dialog->popup_centered();
}

void SpriteFramesEditor::_animation_remove_confirmed() {
	StringName new_edited;
	List<StringName> anim_names;
	frames->get_animation_list(&anim_names);
	anim_names.sort_custom<StringName::AlphCompare>();
	if (anim_names.size() >= 2) {
		if (edited_anim == anim_names.get(0)) {
			new_edited = anim_names.get(1);
		} else {
			new_edited = anim_names.get(0);
		}
	} else {
		new_edited = StringName();
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Animation"), UndoRedo::MERGE_DISABLE, frames.ptr());
	_rename_node_animation(undo_redo, false, edited_anim, new_edited, "");
	undo_redo->add_do_method(frames.ptr(), "remove_animation", edited_anim);
	undo_redo->add_undo_method(frames.ptr(), "add_animation", edited_anim);
	_rename_node_animation(undo_redo, true, edited_anim, edited_anim, edited_anim);
	undo_redo->add_undo_method(frames.ptr(), "set_animation_speed", edited_anim, frames->get_animation_speed(edited_anim));
	undo_redo->add_undo_method(frames.ptr(), "set_animation_loop", edited_anim, frames->get_animation_loop(edited_anim));
	int fc = frames->get_frame_count(edited_anim);
	for (int i = 0; i < fc; i++) {
		Ref<Texture2D> texture = frames->get_frame_texture(edited_anim, i);
		float duration = frames->get_frame_duration(edited_anim, i);
		undo_redo->add_undo_method(frames.ptr(), "add_frame", edited_anim, texture, duration);
	}
	undo_redo->add_do_method(this, "_select_animation", new_edited);
	undo_redo->add_undo_method(this, "_select_animation", edited_anim);
	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_animation_search_text_changed(const String &p_text) {
	_update_library();
}

void SpriteFramesEditor::_animation_loop_changed() {
	if (updating) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Animation Loop"), UndoRedo::MERGE_DISABLE, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "set_animation_loop", edited_anim, anim_loop->is_pressed());
	undo_redo->add_undo_method(frames.ptr(), "set_animation_loop", edited_anim, frames->get_animation_loop(edited_anim));
	undo_redo->add_do_method(this, "_update_library", true);
	undo_redo->add_undo_method(this, "_update_library", true);
	undo_redo->commit_action();
}

void SpriteFramesEditor::_animation_speed_changed(double p_value) {
	if (updating) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Animation FPS"), UndoRedo::MERGE_ENDS, frames.ptr());
	undo_redo->add_do_method(frames.ptr(), "set_animation_speed", edited_anim, p_value);
	undo_redo->add_undo_method(frames.ptr(), "set_animation_speed", edited_anim, frames->get_animation_speed(edited_anim));
	undo_redo->add_do_method(this, "_update_library", true);
	undo_redo->add_undo_method(this, "_update_library", true);
	undo_redo->commit_action();
}

void SpriteFramesEditor::_frame_list_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::WHEEL_UP && mb->is_pressed() && mb->is_ctrl_pressed()) {
			_zoom_in();
			// Don't scroll up after zooming in.
			accept_event();
		} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN && mb->is_pressed() && mb->is_ctrl_pressed()) {
			_zoom_out();
			// Don't scroll down after zooming out.
			accept_event();
		}
	}
}

void SpriteFramesEditor::_frame_list_item_selected(int p_index, bool p_selected) {
	if (updating) {
		return;
	}

	selection = frame_list->get_selected_items();
	if (selection.is_empty() || !p_selected) {
		return;
	}

	updating = true;
	frame_duration->set_value(frames->get_frame_duration(edited_anim, selection[0]));
	updating = false;
}

void SpriteFramesEditor::_frame_duration_changed(double p_value) {
	if (updating) {
		return;
	}

	if (selection.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Frame Duration"), UndoRedo::MERGE_ENDS, frames.ptr());

	for (const int &index : selection) {
		Ref<Texture2D> texture = frames->get_frame_texture(edited_anim, index);
		float old_duration = frames->get_frame_duration(edited_anim, index);

		undo_redo->add_do_method(frames.ptr(), "set_frame", edited_anim, index, texture, p_value);
		undo_redo->add_undo_method(frames.ptr(), "set_frame", edited_anim, index, texture, old_duration);
	}

	undo_redo->add_do_method(this, "_update_library");
	undo_redo->add_undo_method(this, "_update_library");
	undo_redo->commit_action();
}

void SpriteFramesEditor::_zoom_in() {
	// Do not zoom in or out with no visible frames
	if (frames->get_frame_count(edited_anim) <= 0) {
		return;
	}
	if (thumbnail_zoom < max_thumbnail_zoom) {
		thumbnail_zoom *= scale_ratio;
		int thumbnail_size = (int)(thumbnail_default_size * thumbnail_zoom);
		frame_list->set_fixed_column_width(thumbnail_size * 3 / 2);
		frame_list->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));
	}
}

void SpriteFramesEditor::_zoom_out() {
	// Do not zoom in or out with no visible frames
	if (frames->get_frame_count(edited_anim) <= 0) {
		return;
	}
	if (thumbnail_zoom > min_thumbnail_zoom) {
		thumbnail_zoom /= scale_ratio;
		int thumbnail_size = (int)(thumbnail_default_size * thumbnail_zoom);
		frame_list->set_fixed_column_width(thumbnail_size * 3 / 2);
		frame_list->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));
	}
}

void SpriteFramesEditor::_zoom_reset() {
	thumbnail_zoom = MAX(1.0f, EDSCALE);
	frame_list->set_fixed_column_width(thumbnail_default_size * 3 / 2);
	frame_list->set_fixed_icon_size(Size2(thumbnail_default_size, thumbnail_default_size));
}

void SpriteFramesEditor::_update_library(bool p_skip_selector) {
	if (!p_skip_selector) {
		animations_dirty = true;
	}

	if (pending_update) {
		return;
	}
	pending_update = true;
	callable_mp(this, &SpriteFramesEditor::_update_library_impl).call_deferred();
}

void SpriteFramesEditor::_update_library_impl() {
	pending_update = false;

	if (frames.is_null()) {
		return;
	}

	updating = true;

	frame_duration->set_value_no_signal(1.0); // Default.

	if (animations_dirty) {
		animations_dirty = false;
		animations->clear();

		TreeItem *anim_root = animations->create_item();

		List<StringName> anim_names;
		frames->get_animation_list(&anim_names);
		anim_names.sort_custom<StringName::AlphCompare>();
		if (!anim_names.size()) {
			missing_anim_label->show();
			anim_frames_vb->hide();
			return;
		}
		missing_anim_label->hide();
		anim_frames_vb->show();
		bool searching = anim_search_box->get_text().size();
		String searched_string = searching ? anim_search_box->get_text().to_lower() : String();

		TreeItem *selected = nullptr;
		for (const StringName &E : anim_names) {
			String name = E;
			if (searching && !name.to_lower().contains(searched_string)) {
				continue;
			}
			TreeItem *it = animations->create_item(anim_root);
			it->set_metadata(0, name);
			it->set_text(0, name);
			it->set_editable(0, true);
			if (animated_sprite) {
				if (name == String(animated_sprite->call("get_autoplay"))) {
					it->set_icon(0, autoplay_icon);
				}
			}
			if (E == edited_anim) {
				it->select(0);
				selected = it;
			}
		}
		if (selected) {
			animations->scroll_to_item(selected);
		}
	}

	if (animated_sprite) {
		String autoplay_name = animated_sprite->call("get_autoplay");
		if (autoplay_name.is_empty()) {
			autoplay->set_pressed_no_signal(false);
		} else {
			autoplay->set_pressed_no_signal(String(edited_anim) == autoplay_name);
		}
	}

	frame_list->clear();

	if (!frames->has_animation(edited_anim)) {
		updating = false;
		return;
	}

	int anim_frame_count = frames->get_frame_count(edited_anim);
	if (anim_frame_count == 0) {
		selection.clear();
	}

	for (int index = 0; index < selection.size(); index++) {
		int sel = selection[index];
		if (sel == -1) {
			selection.remove_at(index);
			index--;
		}
		if (sel >= anim_frame_count) {
			selection.set(index, anim_frame_count - 1);
			// Since selection is ordered, if we get a frame that is outside of the range
			// we can clip all the other one.
			selection.resize(index + 1);
			break;
		}
	}

	if (selection.is_empty() && frames->get_frame_count(edited_anim)) {
		selection.push_back(0);
	}

	bool is_first_selection = true;
	for (int i = 0; i < frames->get_frame_count(edited_anim); i++) {
		String name = itos(i);
		Ref<Texture2D> texture = frames->get_frame_texture(edited_anim, i);
		float duration = frames->get_frame_duration(edited_anim, i);

		if (texture.is_null()) {
			texture = empty_icon;
			name += ": " + TTR("(empty)");
		} else if (!texture->get_name().is_empty()) {
			name += ": " + texture->get_name();
		}

		if (duration != 1.0f) {
			name += String::utf8(" [× ") + String::num(duration, 2) + "]";
		}

		frame_list->add_item(name, texture);
		if (texture.is_valid()) {
			String tooltip = texture->get_path();

			// Frame is often saved as an AtlasTexture subresource within a scene/resource file,
			// thus its path might be not what the user is looking for. So we're also showing
			// subsequent source texture paths.
			String prefix = U"┖╴";
			Ref<AtlasTexture> at = texture;
			while (at.is_valid() && at->get_atlas().is_valid()) {
				tooltip += "\n" + prefix + at->get_atlas()->get_path();
				prefix = "    " + prefix;
				at = at->get_atlas();
			}

			frame_list->set_item_tooltip(-1, tooltip);
		}
		if (selection.has(i)) {
			frame_list->select(frame_list->get_item_count() - 1, is_first_selection);
			if (is_first_selection) {
				frame_duration->set_value_no_signal(frames->get_frame_duration(edited_anim, i));
			}
			is_first_selection = false;
		}
	}

	anim_speed->set_value_no_signal(frames->get_animation_speed(edited_anim));
	anim_loop->set_pressed_no_signal(frames->get_animation_loop(edited_anim));

	updating = false;
}

void SpriteFramesEditor::_edit() {
	if (!animated_sprite) {
		return;
	}
	edit(animated_sprite->call("get_sprite_frames"));
}

void SpriteFramesEditor::edit(Ref<SpriteFrames> p_frames) {
	_update_stop_icon();

	if (p_frames.is_null()) {
		frames.unref();
		_remove_sprite_node();
		hide();
		return;
	}

	frames = p_frames;
	read_only = EditorNode::get_singleton()->is_resource_read_only(p_frames);

	if (!p_frames->has_animation(edited_anim)) {
		List<StringName> anim_names;
		frames->get_animation_list(&anim_names);
		anim_names.sort_custom<StringName::AlphCompare>();
		if (anim_names.size()) {
			edited_anim = anim_names.front()->get();
		} else {
			edited_anim = StringName();
		}
	}

	_update_library();
	// Clear zoom and split sheet texture
	split_sheet_preview->set_texture(Ref<Texture2D>());
	_zoom_reset();

	add_anim->set_disabled(read_only);
	duplicate_anim->set_disabled(read_only);
	delete_anim->set_disabled(read_only);
	anim_speed->set_editable(!read_only);
	anim_loop->set_disabled(read_only);
	load->set_disabled(read_only);
	load_sheet->set_disabled(read_only);
	copy->set_disabled(read_only);
	paste->set_disabled(read_only);
	empty_before->set_disabled(read_only);
	empty_after->set_disabled(read_only);
	move_up->set_disabled(read_only);
	move_down->set_disabled(read_only);
	delete_frame->set_disabled(read_only);

	_fetch_sprite_node(); // Fetch node after set frames.
}

Ref<SpriteFrames> SpriteFramesEditor::get_sprite_frames() const {
	return frames;
}

Variant SpriteFramesEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (read_only) {
		return false;
	}

	if (!frames->has_animation(edited_anim)) {
		return false;
	}

	int idx = frame_list->get_item_at_position(p_point, true);

	if (idx < 0 || idx >= frames->get_frame_count(edited_anim)) {
		return Variant();
	}

	Ref<Resource> frame = frames->get_frame_texture(edited_anim, idx);

	if (frame.is_null()) {
		return Variant();
	}

	Dictionary drag_data = EditorNode::get_singleton()->drag_resource(frame, p_from);
	drag_data["frame"] = idx; // store the frame, in case we want to reorder frames inside 'drop_data_fw'
	return drag_data;
}

bool SpriteFramesEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (read_only) {
		return false;
	}

	Dictionary d = p_data;

	if (!d.has("type")) {
		return false;
	}

	// reordering frames
	if (d.has("from") && (Object *)(d["from"]) == frame_list) {
		return true;
	}

	if (String(d["type"]) == "resource" && d.has("resource")) {
		Ref<Resource> r = d["resource"];

		Ref<Texture2D> texture = r;

		if (texture.is_valid()) {
			return true;
		}
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false;
		}

		for (int i = 0; i < files.size(); i++) {
			const String &f = files[i];
			String ftype = EditorFileSystem::get_singleton()->get_file_type(f);

			if (!ClassDB::is_parent_class(ftype, "Texture2D")) {
				return false;
			}
		}

		return true;
	}
	return false;
}

void SpriteFramesEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	Dictionary d = p_data;

	if (!d.has("type")) {
		return;
	}

	int at_pos = frame_list->get_item_at_position(p_point, true);

	if (String(d["type"]) == "resource" && d.has("resource")) {
		Ref<Resource> r = d["resource"];

		Ref<Texture2D> texture = r;

		if (texture.is_valid()) {
			bool reorder = false;
			if (d.has("from") && (Object *)(d["from"]) == frame_list) {
				reorder = true;
			}

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			if (reorder) { //drop is from reordering frames
				int from_frame = -1;
				float duration = 1.0;
				if (d.has("frame")) {
					from_frame = d["frame"];
					duration = frames->get_frame_duration(edited_anim, from_frame);
				}

				undo_redo->create_action(TTR("Move Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());
				undo_redo->add_do_method(frames.ptr(), "remove_frame", edited_anim, from_frame == -1 ? frames->get_frame_count(edited_anim) : from_frame);
				undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, texture, duration, at_pos == -1 ? -1 : at_pos);
				undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, at_pos == -1 ? frames->get_frame_count(edited_anim) - 1 : at_pos);
				undo_redo->add_undo_method(frames.ptr(), "add_frame", edited_anim, texture, duration, from_frame);
				undo_redo->add_do_method(this, "_update_library");
				undo_redo->add_undo_method(this, "_update_library");
				undo_redo->commit_action();
			} else {
				undo_redo->create_action(TTR("Add Frame"), UndoRedo::MERGE_DISABLE, frames.ptr());
				undo_redo->add_do_method(frames.ptr(), "add_frame", edited_anim, texture, 1.0, at_pos == -1 ? -1 : at_pos);
				undo_redo->add_undo_method(frames.ptr(), "remove_frame", edited_anim, at_pos == -1 ? frames->get_frame_count(edited_anim) : at_pos);
				undo_redo->add_do_method(this, "_update_library");
				undo_redo->add_undo_method(this, "_update_library");
				undo_redo->commit_action();
			}
		}
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL)) {
			_prepare_sprite_sheet(files[0]);
		} else {
			_file_load_request(files, at_pos);
		}
	}
}

void SpriteFramesEditor::_update_stop_icon() {
	bool is_playing = false;
	if (animated_sprite) {
		is_playing = animated_sprite->call("is_playing");
	}
	if (is_playing) {
		stop->set_button_icon(pause_icon);
	} else {
		stop->set_button_icon(stop_icon);
	}
}

void SpriteFramesEditor::_remove_sprite_node() {
	if (!animated_sprite) {
		return;
	}
	if (animated_sprite->is_connected("sprite_frames_changed", callable_mp(this, &SpriteFramesEditor::_edit))) {
		animated_sprite->disconnect("sprite_frames_changed", callable_mp(this, &SpriteFramesEditor::_edit));
	}
	if (animated_sprite->is_connected(SceneStringName(animation_changed), callable_mp(this, &SpriteFramesEditor::_sync_animation))) {
		animated_sprite->disconnect(SceneStringName(animation_changed), callable_mp(this, &SpriteFramesEditor::_sync_animation));
	}
	if (animated_sprite->is_connected(SceneStringName(animation_finished), callable_mp(this, &SpriteFramesEditor::_update_stop_icon))) {
		animated_sprite->disconnect(SceneStringName(animation_finished), callable_mp(this, &SpriteFramesEditor::_update_stop_icon));
	}
	animated_sprite = nullptr;
}

void SpriteFramesEditor::_fetch_sprite_node() {
	Node *selected = nullptr;
	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	if (editor_selection->get_selected_node_list().size() == 1) {
		selected = editor_selection->get_selected_node_list().front()->get();
	}

	bool show_node_edit = false;
	AnimatedSprite2D *as2d = Object::cast_to<AnimatedSprite2D>(selected);
	AnimatedSprite3D *as3d = Object::cast_to<AnimatedSprite3D>(selected);
	if (as2d || as3d) {
		if (frames != selected->call("get_sprite_frames")) {
			_remove_sprite_node();
		} else {
			animated_sprite = selected;
			if (!animated_sprite->is_connected("sprite_frames_changed", callable_mp(this, &SpriteFramesEditor::_edit))) {
				animated_sprite->connect("sprite_frames_changed", callable_mp(this, &SpriteFramesEditor::_edit));
			}
			if (!animated_sprite->is_connected(SceneStringName(animation_changed), callable_mp(this, &SpriteFramesEditor::_sync_animation))) {
				animated_sprite->connect(SceneStringName(animation_changed), callable_mp(this, &SpriteFramesEditor::_sync_animation), CONNECT_DEFERRED);
			}
			if (!animated_sprite->is_connected(SceneStringName(animation_finished), callable_mp(this, &SpriteFramesEditor::_update_stop_icon))) {
				animated_sprite->connect(SceneStringName(animation_finished), callable_mp(this, &SpriteFramesEditor::_update_stop_icon));
			}
			show_node_edit = true;
		}
	} else {
		_remove_sprite_node();
	}

	if (show_node_edit) {
		_sync_animation();
		autoplay_container->show();
		playback_container->show();
	} else {
		_update_library(); // To init autoplay icon.
		autoplay_container->hide();
		playback_container->hide();
	}
}

void SpriteFramesEditor::_play_pressed() {
	if (animated_sprite) {
		animated_sprite->call("stop");
		animated_sprite->call("play", animated_sprite->call("get_animation"));
	}
	_update_stop_icon();
}

void SpriteFramesEditor::_play_from_pressed() {
	if (animated_sprite) {
		animated_sprite->call("play", animated_sprite->call("get_animation"));
	}
	_update_stop_icon();
}

void SpriteFramesEditor::_play_bw_pressed() {
	if (animated_sprite) {
		animated_sprite->call("stop");
		animated_sprite->call("play_backwards", animated_sprite->call("get_animation"));
	}
	_update_stop_icon();
}

void SpriteFramesEditor::_play_bw_from_pressed() {
	if (animated_sprite) {
		animated_sprite->call("play_backwards", animated_sprite->call("get_animation"));
	}
	_update_stop_icon();
}

void SpriteFramesEditor::_stop_pressed() {
	if (animated_sprite) {
		if (animated_sprite->call("is_playing")) {
			animated_sprite->call("pause");
		} else {
			animated_sprite->call("stop");
		}
	}
	_update_stop_icon();
}

void SpriteFramesEditor::_autoplay_pressed() {
	if (updating) {
		return;
	}

	if (animated_sprite) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Toggle Autoplay"), UndoRedo::MERGE_DISABLE, animated_sprite);
		String current = animated_sprite->call("get_animation");
		String current_auto = animated_sprite->call("get_autoplay");
		if (current == current_auto) {
			//unset
			undo_redo->add_do_method(animated_sprite, "set_autoplay", "");
			undo_redo->add_undo_method(animated_sprite, "set_autoplay", current_auto);
		} else {
			//set
			undo_redo->add_do_method(animated_sprite, "set_autoplay", current);
			undo_redo->add_undo_method(animated_sprite, "set_autoplay", current_auto);
		}
		undo_redo->add_do_method(this, "_update_library");
		undo_redo->add_undo_method(this, "_update_library");
		undo_redo->commit_action();
	}

	_update_library();
}

void SpriteFramesEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_library", "skipsel"), &SpriteFramesEditor::_update_library, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_select_animation", "name", "update_node"), &SpriteFramesEditor::_select_animation, DEFVAL(true));
}

void SpriteFramesEditor::_node_removed(Node *p_node) {
	if (animated_sprite) {
		if (animated_sprite != p_node) {
			return;
		}
		_remove_sprite_node();
	}
}

SpriteFramesEditor::SpriteFramesEditor() {
	VBoxContainer *vbc_animlist = memnew(VBoxContainer);
	add_child(vbc_animlist);
	vbc_animlist->set_custom_minimum_size(Size2(150, 0) * EDSCALE);

	VBoxContainer *sub_vb = memnew(VBoxContainer);
	vbc_animlist->add_margin_child(TTR("Animations:"), sub_vb, true);
	sub_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *hbc_animlist = memnew(HBoxContainer);
	sub_vb->add_child(hbc_animlist);

	add_anim = memnew(Button);
	add_anim->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_animlist->add_child(add_anim);
	add_anim->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_animation_add));

	duplicate_anim = memnew(Button);
	duplicate_anim->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_animlist->add_child(duplicate_anim);
	duplicate_anim->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_animation_duplicate));

	delete_anim = memnew(Button);
	delete_anim->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_animlist->add_child(delete_anim);
	delete_anim->set_disabled(true);
	delete_anim->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_animation_remove));

	autoplay_container = memnew(HBoxContainer);
	hbc_animlist->add_child(autoplay_container);

	autoplay_container->add_child(memnew(VSeparator));

	autoplay = memnew(Button);
	autoplay->set_theme_type_variation(SceneStringName(FlatButton));
	autoplay->set_tooltip_text(TTR("Autoplay on Load"));
	autoplay_container->add_child(autoplay);

	hbc_animlist->add_child(memnew(VSeparator));

	anim_loop = memnew(Button);
	anim_loop->set_toggle_mode(true);
	anim_loop->set_theme_type_variation(SceneStringName(FlatButton));
	anim_loop->set_tooltip_text(TTR("Animation Looping"));
	anim_loop->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_animation_loop_changed));
	hbc_animlist->add_child(anim_loop);

	anim_speed = memnew(SpinBox);
	anim_speed->set_suffix(TTR("FPS"));
	anim_speed->set_min(0);
	anim_speed->set_max(120);
	anim_speed->set_step(0.01);
	anim_speed->set_custom_arrow_step(1);
	anim_speed->set_tooltip_text(TTR("Animation Speed"));
	anim_speed->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_animation_speed_changed));
	hbc_animlist->add_child(anim_speed);

	anim_search_box = memnew(LineEdit);
	sub_vb->add_child(anim_search_box);
	anim_search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	anim_search_box->set_placeholder(TTR("Filter Animations"));
	anim_search_box->set_clear_button_enabled(true);
	anim_search_box->connect(SceneStringName(text_changed), callable_mp(this, &SpriteFramesEditor::_animation_search_text_changed));

	animations = memnew(Tree);
	sub_vb->add_child(animations);
	animations->set_v_size_flags(SIZE_EXPAND_FILL);
	animations->set_hide_root(true);
	// HACK: The cell_selected signal is emitted before the FPS spinbox loses focus and applies the change.
	animations->connect("cell_selected", callable_mp(this, &SpriteFramesEditor::_animation_selected), CONNECT_DEFERRED);
	animations->connect("item_edited", callable_mp(this, &SpriteFramesEditor::_animation_name_edited));
	animations->set_theme_type_variation("TreeSecondary");
	animations->set_allow_reselect(true);

	add_anim->set_shortcut_context(animations);
	add_anim->set_shortcut(ED_SHORTCUT("sprite_frames/new_animation", TTRC("Add Animation"), KeyModifierMask::CMD_OR_CTRL | Key::N));
	duplicate_anim->set_shortcut_context(animations);
	duplicate_anim->set_shortcut(ED_SHORTCUT("sprite_frames/duplicate_animation", TTRC("Duplicate Animation"), KeyModifierMask::CMD_OR_CTRL | Key::D));
	delete_anim->set_shortcut_context(animations);
	delete_anim->set_shortcut(ED_SHORTCUT("sprite_frames/delete_animation", TTRC("Delete Animation"), Key::KEY_DELETE));

	missing_anim_label = memnew(Label);
	missing_anim_label->set_text(TTR("This resource does not have any animations."));
	missing_anim_label->set_h_size_flags(SIZE_EXPAND_FILL);
	missing_anim_label->set_v_size_flags(SIZE_EXPAND_FILL);
	missing_anim_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	missing_anim_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	missing_anim_label->hide();
	add_child(missing_anim_label);

	anim_frames_vb = memnew(VBoxContainer);
	add_child(anim_frames_vb);
	anim_frames_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	anim_frames_vb->hide();

	sub_vb = memnew(VBoxContainer);
	anim_frames_vb->add_margin_child(TTR("Animation Frames:"), sub_vb, true);

	HFlowContainer *hfc = memnew(HFlowContainer);
	sub_vb->add_child(hfc);

	playback_container = memnew(HBoxContainer);
	playback_container->set_layout_direction(LAYOUT_DIRECTION_LTR);
	hfc->add_child(playback_container);

	play_bw_from = memnew(Button);
	play_bw_from->set_theme_type_variation(SceneStringName(FlatButton));
	play_bw_from->set_tooltip_text(TTR("Play selected animation backwards from current pos. (A)"));
	playback_container->add_child(play_bw_from);

	play_bw = memnew(Button);
	play_bw->set_theme_type_variation(SceneStringName(FlatButton));
	play_bw->set_tooltip_text(TTR("Play selected animation backwards from end. (Shift+A)"));
	playback_container->add_child(play_bw);

	stop = memnew(Button);
	stop->set_theme_type_variation(SceneStringName(FlatButton));
	stop->set_tooltip_text(TTR("Pause/stop animation playback. (S)"));
	playback_container->add_child(stop);

	play = memnew(Button);
	play->set_theme_type_variation(SceneStringName(FlatButton));
	play->set_tooltip_text(TTR("Play selected animation from start. (Shift+D)"));
	playback_container->add_child(play);

	play_from = memnew(Button);
	play_from->set_theme_type_variation(SceneStringName(FlatButton));
	play_from->set_tooltip_text(TTR("Play selected animation from current pos. (D)"));
	playback_container->add_child(play_from);

	hfc->add_child(memnew(VSeparator));

	autoplay->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_autoplay_pressed));
	autoplay->set_toggle_mode(true);
	play->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_play_pressed));
	play_from->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_play_from_pressed));
	play_bw->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_play_bw_pressed));
	play_bw_from->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_play_bw_from_pressed));
	stop->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_stop_pressed));

	HBoxContainer *hbc_actions = memnew(HBoxContainer);
	hfc->add_child(hbc_actions);

	load = memnew(Button);
	load->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(load);

	load_sheet = memnew(Button);
	load_sheet->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(load_sheet);

	hbc_actions->add_child(memnew(VSeparator));

	copy = memnew(Button);
	copy->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(copy);

	paste = memnew(Button);
	paste->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(paste);

	hbc_actions->add_child(memnew(VSeparator));

	empty_before = memnew(Button);
	empty_before->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(empty_before);

	empty_after = memnew(Button);
	empty_after->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(empty_after);

	hbc_actions->add_child(memnew(VSeparator));

	move_up = memnew(Button);
	move_up->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(move_up);

	move_down = memnew(Button);
	move_down->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(move_down);

	delete_frame = memnew(Button);
	delete_frame->set_theme_type_variation(SceneStringName(FlatButton));
	hbc_actions->add_child(delete_frame);

	hbc_actions->add_child(memnew(VSeparator));

	HBoxContainer *hbc_frame_duration = memnew(HBoxContainer);
	hfc->add_child(hbc_frame_duration);

	Label *label = memnew(Label);
	label->set_text(TTR("Frame Duration:"));
	hbc_frame_duration->add_child(label);

	frame_duration = memnew(SpinBox);
	frame_duration->set_prefix(String::utf8("×"));
	frame_duration->set_min(SPRITE_FRAME_MINIMUM_DURATION); // Avoid zero div.
	frame_duration->set_max(10);
	frame_duration->set_step(0.01);
	frame_duration->set_custom_arrow_step(0.1);
	frame_duration->set_allow_lesser(false);
	frame_duration->set_allow_greater(true);
	frame_duration->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_frame_duration_changed));
	hbc_frame_duration->add_child(frame_duration);

	// Wide empty separation control. (like BoxContainer::add_spacer())
	Control *c = memnew(Control);
	c->set_mouse_filter(MOUSE_FILTER_PASS);
	c->set_h_size_flags(SIZE_EXPAND_FILL);
	hfc->add_child(c);

	HBoxContainer *hbc_zoom = memnew(HBoxContainer);
	hfc->add_child(hbc_zoom);

	zoom_out = memnew(Button);
	zoom_out->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_zoom_out));
	zoom_out->set_flat(true);
	zoom_out->set_tooltip_text(TTRC("Zoom Out"));
	hbc_zoom->add_child(zoom_out);

	zoom_reset = memnew(Button);
	zoom_reset->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_zoom_reset));
	zoom_reset->set_flat(true);
	zoom_reset->set_tooltip_text(TTRC("Zoom Reset"));
	hbc_zoom->add_child(zoom_reset);

	zoom_in = memnew(Button);
	zoom_in->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_zoom_in));
	zoom_in->set_flat(true);
	zoom_in->set_tooltip_text(TTRC("Zoom In"));
	hbc_zoom->add_child(zoom_in);

	file = memnew(EditorFileDialog);
	file->connect("files_selected", callable_mp(this, &SpriteFramesEditor::_file_load_request).bind(-1));
	add_child(file);

	frame_list = memnew(ItemList);
	frame_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	frame_list->set_v_size_flags(SIZE_EXPAND_FILL);
	frame_list->set_icon_mode(ItemList::ICON_MODE_TOP);
	frame_list->set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	frame_list->set_select_mode(ItemList::SELECT_MULTI);

	frame_list->set_max_columns(0);
	frame_list->set_max_text_lines(2);
	SET_DRAG_FORWARDING_GCD(frame_list, SpriteFramesEditor);
	frame_list->connect(SceneStringName(gui_input), callable_mp(this, &SpriteFramesEditor::_frame_list_gui_input));
	// HACK: The item_selected signal is emitted before the Frame Duration spinbox loses focus and applies the change.
	frame_list->connect("multi_selected", callable_mp(this, &SpriteFramesEditor::_frame_list_item_selected), CONNECT_DEFERRED);

	sub_vb->add_child(frame_list);

	dialog = memnew(AcceptDialog);
	add_child(dialog);

	load->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_load_pressed));
	load_sheet->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_open_sprite_sheet));
	delete_frame->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_delete_pressed));
	copy->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_copy_pressed));
	paste->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_paste_pressed));
	empty_before->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_empty_pressed));
	empty_after->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_empty2_pressed));
	move_up->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_up_pressed));
	move_down->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_down_pressed));

	load->set_shortcut_context(frame_list);
	load->set_shortcut(ED_SHORTCUT("sprite_frames/load_from_file", TTRC("Add frame from file"), KeyModifierMask::CMD_OR_CTRL | Key::O));
	load_sheet->set_shortcut_context(frame_list);
	load_sheet->set_shortcut(ED_SHORTCUT("sprite_frames/load_from_sheet", TTRC("Add frames from sprite sheet"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::O));
	delete_frame->set_shortcut_context(frame_list);
	delete_frame->set_shortcut(ED_SHORTCUT("sprite_frames/delete", TTRC("Delete Frame"), Key::KEY_DELETE));
	copy->set_shortcut_context(frame_list);
	copy->set_shortcut(ED_SHORTCUT("sprite_frames/copy", TTRC("Copy Frame(s)"), KeyModifierMask::CMD_OR_CTRL | Key::C));
	paste->set_shortcut_context(frame_list);
	paste->set_shortcut(ED_SHORTCUT("sprite_frames/paste", TTRC("Paste Frame(s)"), KeyModifierMask::CMD_OR_CTRL | Key::V));
	empty_before->set_shortcut_context(frame_list);
	empty_before->set_shortcut(ED_SHORTCUT("sprite_frames/empty_before", TTRC("Insert Empty (Before Selected)"), KeyModifierMask::ALT | Key::LEFT));
	empty_after->set_shortcut_context(frame_list);
	empty_after->set_shortcut(ED_SHORTCUT("sprite_frames/empty_after", TTRC("Insert Empty (After Selected)"), KeyModifierMask::ALT | Key::RIGHT));
	move_up->set_shortcut_context(frame_list);
	move_up->set_shortcut(ED_SHORTCUT("sprite_frames/move_left", TTRC("Move Frame Left"), KeyModifierMask::CMD_OR_CTRL | Key::LEFT));
	move_down->set_shortcut_context(frame_list);
	move_down->set_shortcut(ED_SHORTCUT("sprite_frames/move_right", TTRC("Move Frame Right"), KeyModifierMask::CMD_OR_CTRL | Key::RIGHT));

	zoom_out->set_shortcut_context(frame_list);
	zoom_out->set_shortcut(ED_SHORTCUT_ARRAY("sprite_frames/zoom_out", TTRC("Zoom Out"),
			{ int32_t(KeyModifierMask::CMD_OR_CTRL | Key::MINUS), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::KP_SUBTRACT) }));
	zoom_in->set_shortcut_context(frame_list);
	zoom_in->set_shortcut(ED_SHORTCUT_ARRAY("sprite_frames/zoom_in", TTRC("Zoom In"),
			{ int32_t(KeyModifierMask::CMD_OR_CTRL | Key::EQUAL), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::KP_ADD) }));

	loading_scene = false;

	updating = false;

	edited_anim = SceneStringName(default_);

	delete_dialog = memnew(ConfirmationDialog);
	add_child(delete_dialog);
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &SpriteFramesEditor::_animation_remove_confirmed));

	split_sheet_dialog = memnew(ConfirmationDialog);
	add_child(split_sheet_dialog);
	split_sheet_dialog->set_title(TTR("Select Frames"));
	split_sheet_dialog->connect(SceneStringName(confirmed), callable_mp(this, &SpriteFramesEditor::_sheet_add_frames));

	HBoxContainer *split_sheet_hb = memnew(HBoxContainer);
	split_sheet_dialog->add_child(split_sheet_hb);
	split_sheet_hb->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_hb->set_v_size_flags(SIZE_EXPAND_FILL);

	VBoxContainer *split_sheet_vb = memnew(VBoxContainer);
	split_sheet_hb->add_child(split_sheet_vb);
	split_sheet_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *split_sheet_menu_hb = memnew(HBoxContainer);

	split_sheet_menu_hb->add_child(memnew(Label(TTR("Frame Order"))));

	split_sheet_order = memnew(OptionButton);
	split_sheet_order->add_item(TTR("As Selected"), FRAME_ORDER_SELECTION);
	split_sheet_order->add_separator(TTR("By Row"));
	split_sheet_order->add_item(TTR("Left to Right, Top to Bottom"), FRAME_ORDER_LEFT_RIGHT_TOP_BOTTOM);
	split_sheet_order->add_item(TTR("Left to Right, Bottom to Top"), FRAME_ORDER_LEFT_RIGHT_BOTTOM_TOP);
	split_sheet_order->add_item(TTR("Right to Left, Top to Bottom"), FRAME_ORDER_RIGHT_LEFT_TOP_BOTTOM);
	split_sheet_order->add_item(TTR("Right to Left, Bottom to Top"), FRAME_ORDER_RIGHT_LEFT_BOTTOM_TOP);
	split_sheet_order->add_separator(TTR("By Column"));
	split_sheet_order->add_item(TTR("Top to Bottom, Left to Right"), FRAME_ORDER_TOP_BOTTOM_LEFT_RIGHT);
	split_sheet_order->add_item(TTR("Top to Bottom, Right to Left"), FRAME_ORDER_TOP_BOTTOM_RIGHT_LEFT);
	split_sheet_order->add_item(TTR("Bottom to Top, Left to Right"), FRAME_ORDER_BOTTOM_TOP_LEFT_RIGHT);
	split_sheet_order->add_item(TTR("Bottom to Top, Right to Left"), FRAME_ORDER_BOTTOM_TOP_RIGHT_LEFT);
	split_sheet_order->connect(SceneStringName(item_selected), callable_mp(this, &SpriteFramesEditor::_sheet_order_selected));
	split_sheet_menu_hb->add_child(split_sheet_order);

	Button *select_all = memnew(Button);
	select_all->set_text(TTR("Select All"));
	select_all->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_sheet_select_all_frames));
	split_sheet_menu_hb->add_child(select_all);

	Button *clear_all = memnew(Button);
	clear_all->set_text(TTR("Select None"));
	clear_all->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_sheet_clear_all_frames));
	split_sheet_menu_hb->add_child(clear_all);

	split_sheet_menu_hb->add_spacer();

	toggle_settings_button = memnew(Button);
	toggle_settings_button->set_h_size_flags(SIZE_SHRINK_END);
	toggle_settings_button->set_theme_type_variation(SceneStringName(FlatButton));
	toggle_settings_button->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_toggle_show_settings));
	toggle_settings_button->set_tooltip_text(TTR("Toggle Settings Panel"));
	split_sheet_menu_hb->add_child(toggle_settings_button);

	split_sheet_vb->add_child(split_sheet_menu_hb);

	PanelContainer *split_sheet_panel = memnew(PanelContainer);
	split_sheet_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_panel->set_v_size_flags(SIZE_EXPAND_FILL);
	split_sheet_vb->add_child(split_sheet_panel);

	split_sheet_preview = memnew(TextureRect);
	split_sheet_preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	split_sheet_preview->set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	split_sheet_preview->set_mouse_filter(MOUSE_FILTER_PASS);
	split_sheet_preview->connect(SceneStringName(draw), callable_mp(this, &SpriteFramesEditor::_sheet_preview_draw));
	split_sheet_preview->connect(SceneStringName(gui_input), callable_mp(this, &SpriteFramesEditor::_sheet_preview_input));

	split_sheet_scroll = memnew(ScrollContainer);
	split_sheet_scroll->connect(SceneStringName(gui_input), callable_mp(this, &SpriteFramesEditor::_sheet_scroll_input));
	split_sheet_panel->add_child(split_sheet_scroll);
	CenterContainer *cc = memnew(CenterContainer);
	cc->add_child(split_sheet_preview);
	cc->set_h_size_flags(SIZE_EXPAND_FILL);
	cc->set_v_size_flags(SIZE_EXPAND_FILL);
	split_sheet_scroll->add_child(cc);

	MarginContainer *split_sheet_zoom_margin = memnew(MarginContainer);
	split_sheet_panel->add_child(split_sheet_zoom_margin);
	split_sheet_zoom_margin->set_h_size_flags(0);
	split_sheet_zoom_margin->set_v_size_flags(0);
	split_sheet_zoom_margin->add_theme_constant_override("margin_top", 5);
	split_sheet_zoom_margin->add_theme_constant_override("margin_left", 5);
	HBoxContainer *split_sheet_zoom_hb = memnew(HBoxContainer);
	split_sheet_zoom_margin->add_child(split_sheet_zoom_hb);

	split_sheet_zoom_out = memnew(Button);
	split_sheet_zoom_out->set_theme_type_variation(SceneStringName(FlatButton));
	split_sheet_zoom_out->set_focus_mode(FOCUS_NONE);
	split_sheet_zoom_out->set_tooltip_text(TTR("Zoom Out"));
	split_sheet_zoom_out->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_sheet_zoom_out));
	split_sheet_zoom_hb->add_child(split_sheet_zoom_out);

	split_sheet_zoom_reset = memnew(Button);
	split_sheet_zoom_reset->set_theme_type_variation(SceneStringName(FlatButton));
	split_sheet_zoom_reset->set_focus_mode(FOCUS_NONE);
	split_sheet_zoom_reset->set_tooltip_text(TTR("Zoom Reset"));
	split_sheet_zoom_reset->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_sheet_zoom_reset));
	split_sheet_zoom_hb->add_child(split_sheet_zoom_reset);

	split_sheet_zoom_in = memnew(Button);
	split_sheet_zoom_in->set_theme_type_variation(SceneStringName(FlatButton));
	split_sheet_zoom_in->set_focus_mode(FOCUS_NONE);
	split_sheet_zoom_in->set_tooltip_text(TTR("Zoom In"));
	split_sheet_zoom_in->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_sheet_zoom_in));
	split_sheet_zoom_hb->add_child(split_sheet_zoom_in);

	split_sheet_settings_vb = memnew(VBoxContainer);
	split_sheet_settings_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *split_sheet_h_hb = memnew(HBoxContainer);
	split_sheet_h_hb->set_h_size_flags(SIZE_EXPAND_FILL);

	Label *split_sheet_h_label = memnew(Label(TTR("Horizontal")));
	split_sheet_h_label->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_h_hb->add_child(split_sheet_h_label);

	split_sheet_h = memnew(SpinBox);
	split_sheet_h->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_h->set_min(1);
	split_sheet_h->set_max(128);
	split_sheet_h->set_step(1);
	split_sheet_h->set_select_all_on_focus(true);
	split_sheet_h_hb->add_child(split_sheet_h);
	split_sheet_h->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_FRAME_COUNT));
	split_sheet_settings_vb->add_child(split_sheet_h_hb);

	HBoxContainer *split_sheet_v_hb = memnew(HBoxContainer);
	split_sheet_v_hb->set_h_size_flags(SIZE_EXPAND_FILL);

	Label *split_sheet_v_label = memnew(Label(TTR("Vertical")));
	split_sheet_v_label->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_v_hb->add_child(split_sheet_v_label);

	split_sheet_v = memnew(SpinBox);
	split_sheet_v->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_v->set_min(1);
	split_sheet_v->set_max(128);
	split_sheet_v->set_step(1);
	split_sheet_v->set_select_all_on_focus(true);
	split_sheet_v_hb->add_child(split_sheet_v);
	split_sheet_v->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_FRAME_COUNT));
	split_sheet_settings_vb->add_child(split_sheet_v_hb);

	HBoxContainer *split_sheet_size_hb = memnew(HBoxContainer);
	split_sheet_size_hb->set_h_size_flags(SIZE_EXPAND_FILL);

	Label *split_sheet_size_label = memnew(Label(TTR("Size")));
	split_sheet_size_label->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_size_label->set_v_size_flags(SIZE_SHRINK_BEGIN);
	split_sheet_size_hb->add_child(split_sheet_size_label);

	VBoxContainer *split_sheet_size_vb = memnew(VBoxContainer);
	split_sheet_size_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_size_x = memnew(SpinBox);
	split_sheet_size_x->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_size_x->set_min(1);
	split_sheet_size_x->set_step(1);
	split_sheet_size_x->set_suffix("px");
	split_sheet_size_x->set_select_all_on_focus(true);
	split_sheet_size_x->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_SIZE));
	split_sheet_size_vb->add_child(split_sheet_size_x);
	split_sheet_size_y = memnew(SpinBox);
	split_sheet_size_y->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_size_y->set_min(1);
	split_sheet_size_y->set_step(1);
	split_sheet_size_y->set_suffix("px");
	split_sheet_size_y->set_select_all_on_focus(true);
	split_sheet_size_y->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_SIZE));
	split_sheet_size_vb->add_child(split_sheet_size_y);
	split_sheet_size_hb->add_child(split_sheet_size_vb);
	split_sheet_settings_vb->add_child(split_sheet_size_hb);

	HBoxContainer *split_sheet_sep_hb = memnew(HBoxContainer);
	split_sheet_sep_hb->set_h_size_flags(SIZE_EXPAND_FILL);

	Label *split_sheet_sep_label = memnew(Label(TTR("Separation")));
	split_sheet_sep_label->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_sep_label->set_v_size_flags(SIZE_SHRINK_BEGIN);
	split_sheet_sep_hb->add_child(split_sheet_sep_label);

	VBoxContainer *split_sheet_sep_vb = memnew(VBoxContainer);
	split_sheet_sep_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_sep_x = memnew(SpinBox);
	split_sheet_sep_x->set_min(0);
	split_sheet_sep_x->set_step(1);
	split_sheet_sep_x->set_suffix("px");
	split_sheet_sep_x->set_select_all_on_focus(true);
	split_sheet_sep_x->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_USE_CURRENT));
	split_sheet_sep_vb->add_child(split_sheet_sep_x);
	split_sheet_sep_y = memnew(SpinBox);
	split_sheet_sep_y->set_min(0);
	split_sheet_sep_y->set_step(1);
	split_sheet_sep_y->set_suffix("px");
	split_sheet_sep_y->set_select_all_on_focus(true);
	split_sheet_sep_y->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_USE_CURRENT));
	split_sheet_sep_vb->add_child(split_sheet_sep_y);
	split_sheet_sep_hb->add_child(split_sheet_sep_vb);
	split_sheet_settings_vb->add_child(split_sheet_sep_hb);

	HBoxContainer *split_sheet_offset_hb = memnew(HBoxContainer);
	split_sheet_offset_hb->set_h_size_flags(SIZE_EXPAND_FILL);

	Label *split_sheet_offset_label = memnew(Label(TTR("Offset")));
	split_sheet_offset_label->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_offset_label->set_v_size_flags(SIZE_SHRINK_BEGIN);
	split_sheet_offset_hb->add_child(split_sheet_offset_label);

	VBoxContainer *split_sheet_offset_vb = memnew(VBoxContainer);
	split_sheet_offset_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	split_sheet_offset_x = memnew(SpinBox);
	split_sheet_offset_x->set_min(0);
	split_sheet_offset_x->set_step(1);
	split_sheet_offset_x->set_suffix("px");
	split_sheet_offset_x->set_select_all_on_focus(true);
	split_sheet_offset_x->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_USE_CURRENT));
	split_sheet_offset_vb->add_child(split_sheet_offset_x);
	split_sheet_offset_y = memnew(SpinBox);
	split_sheet_offset_y->set_min(0);
	split_sheet_offset_y->set_step(1);
	split_sheet_offset_y->set_suffix("px");
	split_sheet_offset_y->set_select_all_on_focus(true);
	split_sheet_offset_y->connect(SceneStringName(value_changed), callable_mp(this, &SpriteFramesEditor::_sheet_spin_changed).bind(PARAM_USE_CURRENT));
	split_sheet_offset_vb->add_child(split_sheet_offset_y);
	split_sheet_offset_hb->add_child(split_sheet_offset_vb);
	split_sheet_settings_vb->add_child(split_sheet_offset_hb);

	Button *auto_slice = memnew(Button);
	auto_slice->set_text(TTR("Auto Slice"));
	auto_slice->connect(SceneStringName(pressed), callable_mp(this, &SpriteFramesEditor::_auto_slice_sprite_sheet));
	split_sheet_settings_vb->add_child(auto_slice);

	split_sheet_hb->add_child(split_sheet_settings_vb);

	file_split_sheet = memnew(EditorFileDialog);
	file_split_sheet->set_title(TTR("Create Frames from Sprite Sheet"));
	file_split_sheet->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	add_child(file_split_sheet);
	file_split_sheet->connect("file_selected", callable_mp(this, &SpriteFramesEditor::_prepare_sprite_sheet));

	// Config scale.
	scale_ratio = 1.2f;
	thumbnail_default_size = 96 * MAX(1, EDSCALE);
	thumbnail_zoom = MAX(1.0f, EDSCALE);
	max_thumbnail_zoom = 8.0f * MAX(1.0f, EDSCALE);
	min_thumbnail_zoom = 0.1f * MAX(1.0f, EDSCALE);
	// Default the zoom to match the editor scale, but don't dezoom on editor scales below 100% to prevent pixel art from looking bad.
	sheet_zoom = MAX(1.0f, EDSCALE);
	max_sheet_zoom = 128.0f * MAX(1.0f, EDSCALE);
	min_sheet_zoom = 0.01f * MAX(1.0f, EDSCALE);
	_zoom_reset();

	// Ensure the anim search box is wide enough by default.
	// Not by setting its minimum size so it can still be shrunk if desired.
	set_split_offset(56 * EDSCALE);
}

void SpriteFramesEditorPlugin::edit(Object *p_object) {
	Ref<SpriteFrames> s;
	AnimatedSprite2D *animated_sprite = Object::cast_to<AnimatedSprite2D>(p_object);
	if (animated_sprite) {
		s = animated_sprite->get_sprite_frames();
	} else {
		AnimatedSprite3D *animated_sprite_3d = Object::cast_to<AnimatedSprite3D>(p_object);
		if (animated_sprite_3d) {
			s = animated_sprite_3d->get_sprite_frames();
		} else {
			s = p_object;
		}
	}

	frames_editor->edit(s);
}

bool SpriteFramesEditorPlugin::handles(Object *p_object) const {
	AnimatedSprite2D *animated_sprite_2d = Object::cast_to<AnimatedSprite2D>(p_object);
	if (animated_sprite_2d && *animated_sprite_2d->get_sprite_frames()) {
		return true;
	}
	AnimatedSprite3D *animated_sprite_3d = Object::cast_to<AnimatedSprite3D>(p_object);
	if (animated_sprite_3d && *animated_sprite_3d->get_sprite_frames()) {
		return true;
	}
	SpriteFrames *frames = Object::cast_to<SpriteFrames>(p_object);
	if (frames && (frames_editor->get_sprite_frames().is_null() || frames_editor->get_sprite_frames() == frames)) {
		return true;
	}
	return false;
}

void SpriteFramesEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		EditorNode::get_bottom_panel()->make_item_visible(frames_editor);
	} else {
		button->hide();
		if (frames_editor->is_visible_in_tree()) {
			EditorNode::get_bottom_panel()->hide_bottom_panel();
		}
	}
}

SpriteFramesEditorPlugin::SpriteFramesEditorPlugin() {
	frames_editor = memnew(SpriteFramesEditor);
	frames_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	button = EditorNode::get_bottom_panel()->add_item(TTR("SpriteFrames"), frames_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_sprite_frames_bottom_panel", TTRC("Toggle SpriteFrames Bottom Panel")));
	button->hide();
}

SpriteFramesEditorPlugin::~SpriteFramesEditorPlugin() {
}
