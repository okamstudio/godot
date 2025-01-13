/**************************************************************************/
/*  editor_spin_slider.cpp                                                */
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

#include "editor_spin_slider.h"

#include "core/input/input.h"
#include "core/math/expression.h"
#include "core/os/keyboard.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/theme/theme_db.h"

String EditorSpinSlider::get_tooltip(const Point2 &p_pos) const {
	if (!read_only && grabber->is_visible()) {
		Key key = (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) ? Key::META : Key::CTRL;
		return TS->format_number(rtos(get_value())) + "\n\n" + vformat(TTR("Hold %s to round to integers.\nHold Shift for more precise changes."), find_keycode_name(key));
	}
	return TS->format_number(rtos(get_value()));
}

String EditorSpinSlider::get_text_value() const {
	return TS->format_number(String::num(get_value(), Math::range_step_decimals(get_step())));
}

void EditorSpinSlider::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (read_only) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				if (updown_offset != -1 && ((!is_layout_rtl() && mb->get_position().x > updown_offset) || (is_layout_rtl() && mb->get_position().x < updown_offset))) {
					// Updown pressed.
					if (mb->get_position().y < get_size().height / 2) {
						set_value(get_value() + get_step());
					} else {
						set_value(get_value() - get_step());
					}
					emit_signal("updown_pressed");
					return;
				}
				_grab_start();
			} else {
				_grab_end();
			}
		} else if (mb->get_button_index() == MouseButton::RIGHT) {
			if (mb->is_pressed() && is_grabbing()) {
				_grab_end();
				set_value(pre_grab_value);
			}
		} else if (mb->get_button_index() == MouseButton::WHEEL_UP || mb->get_button_index() == MouseButton::WHEEL_DOWN) {
			if (grabber->is_visible()) {
				callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw).call_deferred();
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (grabbing_spinner_attempt) {
			double diff_x = mm->get_relative().x;
			if (mm->is_shift_pressed() && grabbing_spinner) {
				diff_x *= 0.1;
			}
			grabbing_spinner_dist_cache += diff_x * grabbing_spinner_speed;

			if (!grabbing_spinner && ABS(grabbing_spinner_dist_cache) > 4 * grabbing_spinner_speed * EDSCALE) {
				Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
				grabbing_spinner = true;
			}

			if (grabbing_spinner) {
				// Don't make the user scroll all the way back to 'in range' if they went off the end.
				if (pre_grab_value < get_min() && !is_lesser_allowed()) {
					pre_grab_value = get_min();
				}
				if (pre_grab_value > get_max() && !is_greater_allowed()) {
					pre_grab_value = get_max();
				}

				if (mm->is_command_or_control_pressed()) {
					// If control was just pressed, don't make the value do a huge jump in magnitude.
					if (grabbing_spinner_dist_cache != 0) {
						pre_grab_value += grabbing_spinner_dist_cache * get_step();
						grabbing_spinner_dist_cache = 0;
					}

					set_value(Math::round(pre_grab_value + get_step() * grabbing_spinner_dist_cache * 10));
				} else {
					set_value(pre_grab_value + get_step() * grabbing_spinner_dist_cache);
				}
			}
		} else if (updown_offset != -1) {
			bool new_hover = (!is_layout_rtl() && mm->get_position().x > updown_offset) || (is_layout_rtl() && mm->get_position().x < updown_offset);
			if (new_hover != hover_updown) {
				hover_updown = new_hover;
				queue_redraw();
			}
		}
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed()) {
		if (k->is_action("ui_accept", true)) {
			_focus_entered();
		} else if (is_grabbing()) {
			if (k->is_action("ui_cancel", true)) {
				_grab_end();
				set_value(pre_grab_value);
			}
			accept_event();
		}
	}
}

void EditorSpinSlider::_grab_start() {
	grabbing_spinner_attempt = true;
	grabbing_spinner_dist_cache = 0;
	pre_grab_value = get_value();
	grabbing_spinner = false;
	grabbing_spinner_mouse_pos = get_global_mouse_position();
	emit_signal("grabbed");
}

void EditorSpinSlider::_grab_end() {
	if (grabbing_spinner_attempt) {
		if (grabbing_spinner) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			Input::get_singleton()->warp_mouse(grabbing_spinner_mouse_pos);
			queue_redraw();
			grabbing_spinner = false;
			emit_signal("ungrabbed");
		} else {
			_focus_entered();
		}

		grabbing_spinner_attempt = false;
	}

	if (grabbing_grabber) {
		grabbing_grabber = false;
		mousewheel_over_grabber = false;
		emit_signal("ungrabbed");
	}
}

void EditorSpinSlider::_grabber_gui_input(const Ref<InputEvent> &p_event) {
	if (read_only) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (grabbing_grabber) {
		if (mb.is_valid()) {
			if (mb->get_button_index() == MouseButton::WHEEL_UP) {
				set_value(get_value() + get_step());
				mousewheel_over_grabber = true;
			} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN) {
				set_value(get_value() - get_step());
				mousewheel_over_grabber = true;
			}
		}
	}

	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			grabbing_grabber = true;
			pre_grab_value = get_value();
			if (!mousewheel_over_grabber) {
				grabbing_ratio = get_as_ratio();
				grabbing_from = grabber->get_transform().xform(mb->get_position()).x;
			}
			grab_focus();
			emit_signal("grabbed");
		} else {
			grabbing_grabber = false;
			mousewheel_over_grabber = false;
			emit_signal("ungrabbed");
		}
	} else if (mb.is_valid() && mb->get_button_index() == MouseButton::RIGHT) {
		if (mb->is_pressed() && grabbing_grabber) {
			grabbing_grabber = false;
			mousewheel_over_grabber = false;
			set_value(pre_grab_value);
			emit_signal("ungrabbed");
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && grabbing_grabber) {
		if (mousewheel_over_grabber) {
			return;
		}

		float scale_x = get_global_transform_with_canvas().get_scale().x;
		ERR_FAIL_COND(Math::is_zero_approx(scale_x));
		float grabbing_ofs = (grabber->get_transform().xform(mm->get_position()).x - grabbing_from) / float(grabber_range) / scale_x;
		set_as_ratio(grabbing_ratio + grabbing_ofs);
		queue_redraw();
	}
}

void EditorSpinSlider::_value_input_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !read_only) {
		Key code = k->get_keycode();

		switch (code) {
			case Key::UP:
			case Key::DOWN: {
				double step = get_step();
				if (step < 1) {
					double divisor = 1.0 / step;

					if (trunc(divisor) == divisor) {
						step = 1.0;
					}
				}

				if (k->is_command_or_control_pressed()) {
					step *= 100.0;
				} else if (k->is_shift_pressed()) {
					step *= 10.0;
				} else if (k->is_alt_pressed()) {
					step *= 0.1;
				}

				_evaluate_input_text();

				double last_value = get_value();
				if (code == Key::DOWN) {
					step *= -1;
				}
				set_value(last_value + step);

				value_input_dirty = true;
				set_process_internal(true);
			} break;
			case Key::ESCAPE: {
				value_input_closed_frame = Engine::get_singleton()->get_frames_drawn();
				if (value_input_popup) {
					value_input_popup->hide();
				}
			} break;
			default:
				break;
		}
	}
}

void EditorSpinSlider::_update_value_input_stylebox() {
	if (!value_input) {
		return;
	}

	// Add a left margin to the stylebox to make the number align with the Label
	// when it's edited. The LineEdit "focus" stylebox uses the "normal" stylebox's
	// default margins.
	Ref<StyleBox> stylebox = get_theme_stylebox(CoreStringName(normal), SNAME("LineEdit"))->duplicate();
	// EditorSpinSliders with a label have more space on the left, so add an
	// higher margin to match the location where the text begins.
	// The margin values below were determined by empirical testing.
	if (is_layout_rtl()) {
		stylebox->set_content_margin(SIDE_RIGHT, (!get_label().is_empty() ? 23 : 16) * EDSCALE);
	} else {
		stylebox->set_content_margin(SIDE_LEFT, (!get_label().is_empty() ? 23 : 16) * EDSCALE);
	}

	value_input->add_theme_style_override(CoreStringName(normal), stylebox);
}

void EditorSpinSlider::_draw_spin_slider() {
	updown_offset = -1;

	RID ci = get_canvas_item();
	bool rtl = is_layout_rtl();
	Vector2 size = get_size();

	Ref<StyleBox> sb = get_theme_stylebox(read_only ? SNAME("read_only") : CoreStringName(normal), SNAME("LineEdit"));
	if (!flat) {
		draw_style_box(sb, Rect2(Vector2(), size));
	}
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("LineEdit"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("LineEdit"));
	int sep_base = 4 * EDSCALE;
	int sep = sep_base + sb->get_offset().x; //make it have the same margin on both sides, looks better

	int label_width = font->get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).width;
	int number_width = size.width - sb->get_minimum_size().width - label_width - sep;

	Ref<Texture2D> updown = get_theme_icon(read_only ? SNAME("updown_disabled") : SNAME("updown"), SNAME("SpinBox"));

	String numstr = get_text_value();

	int vofs = (size.height - font->get_height(font_size)) / 2 + font->get_ascent(font_size);

	Color fc = get_theme_color(read_only ? SNAME("font_uneditable_color") : SceneStringName(font_color), SNAME("LineEdit"));
	Color lc = get_theme_color(read_only ? SNAME("read_only_label_color") : SNAME("label_color"));

	if (flat && !label.is_empty()) {
		Ref<StyleBox> label_bg = get_theme_stylebox(SNAME("label_bg"), SNAME("EditorSpinSlider"));
		if (rtl) {
			draw_style_box(label_bg, Rect2(Vector2(size.width - (sb->get_offset().x * 2 + label_width), 0), Vector2(sb->get_offset().x * 2 + label_width, size.height)));
		} else {
			draw_style_box(label_bg, Rect2(Vector2(), Vector2(sb->get_offset().x * 2 + label_width, size.height)));
		}
	}

	if (has_focus()) {
		Ref<StyleBox> focus = get_theme_stylebox(SNAME("focus"), SNAME("LineEdit"));
		draw_style_box(focus, Rect2(Vector2(), size));
	}

	if (rtl) {
		draw_string(font, Vector2(Math::round(size.width - sb->get_offset().x - label_width), vofs), label, HORIZONTAL_ALIGNMENT_RIGHT, -1, font_size, lc * Color(1, 1, 1, 0.5));
	} else {
		draw_string(font, Vector2(Math::round(sb->get_offset().x), vofs), label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, lc * Color(1, 1, 1, 0.5));
	}

	int suffix_start = numstr.length();
	RID num_rid = TS->create_shaped_text();
	TS->shaped_text_add_string(num_rid, numstr + U"\u2009" + suffix, font->get_rids(), font_size, font->get_opentype_features());

	float text_start = rtl ? Math::round(sb->get_offset().x) : Math::round(sb->get_offset().x + label_width + sep);
	Vector2 text_ofs = rtl ? Vector2(text_start + (number_width - TS->shaped_text_get_width(num_rid)), vofs) : Vector2(text_start, vofs);
	int v_size = TS->shaped_text_get_glyph_count(num_rid);
	const Glyph *glyphs = TS->shaped_text_get_glyphs(num_rid);
	for (int i = 0; i < v_size; i++) {
		for (int j = 0; j < glyphs[i].repeat; j++) {
			if (text_ofs.x >= text_start && (text_ofs.x + glyphs[i].advance) <= (text_start + number_width)) {
				Color color = fc;
				if (glyphs[i].start >= suffix_start) {
					color.a *= 0.4;
				}
				if (glyphs[i].font_rid != RID()) {
					TS->font_draw_glyph(glyphs[i].font_rid, ci, glyphs[i].font_size, text_ofs + Vector2(glyphs[i].x_off, glyphs[i].y_off), glyphs[i].index, color);
				} else if ((glyphs[i].flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) {
					TS->draw_hex_code_box(ci, glyphs[i].font_size, text_ofs + Vector2(glyphs[i].x_off, glyphs[i].y_off), glyphs[i].index, color);
				}
			}
			text_ofs.x += glyphs[i].advance;
		}
	}
	TS->free_rid(num_rid);

	if (!hide_slider) {
		if (get_step() == 1) {
			Ref<Texture2D> updown2 = read_only ? theme_cache.updown_disabled_icon : theme_cache.updown_icon;
			int updown_vofs = (size.height - updown2->get_height()) / 2;
			if (rtl) {
				updown_offset = sb->get_margin(SIDE_LEFT);
			} else {
				updown_offset = size.width - sb->get_margin(SIDE_RIGHT) - updown2->get_width();
			}
			Color c(1, 1, 1);
			if (hover_updown) {
				c *= Color(1.2, 1.2, 1.2);
			}
			draw_texture(updown2, Vector2(updown_offset, updown_vofs), c);
			if (rtl) {
				updown_offset += updown2->get_width();
			}
			if (grabber->is_visible()) {
				grabber->hide();
			}
		} else {
			const int grabber_w = 4 * EDSCALE;
			const int width = size.width - sb->get_minimum_size().width - grabber_w;
			const int ofs = sb->get_offset().x;
			const int svofs = (size.height + vofs) / 2 - 1;
			Color c = fc;

			// Draw the horizontal slider's background.
			c.a = 0.2;
			draw_rect(Rect2(ofs, svofs + 1, width, 2 * EDSCALE), c);

			// Draw the horizontal slider's filled part on the left.
			const int gofs = get_as_ratio() * width;
			c.a = 0.45;
			draw_rect(Rect2(ofs, svofs + 1, gofs, 2 * EDSCALE), c);

			// Draw the horizontal slider's grabber.
			c.a = 0.9;
			const Rect2 grabber_rect = Rect2(ofs + gofs, svofs, grabber_w, 4 * EDSCALE);
			draw_rect(grabber_rect, c);

			grabbing_spinner_mouse_pos = get_global_position() + grabber_rect.get_center();

			bool display_grabber = !read_only && (grabbing_grabber || mouse_over_spin || mouse_over_grabber) && !grabbing_spinner && !(value_input_popup && value_input_popup->is_visible());
			if (grabber->is_visible() != display_grabber) {
				grabber->set_visible(display_grabber);
			}

			if (display_grabber) {
				Ref<Texture2D> grabber_tex;
				if (mouse_over_grabber) {
					grabber_tex = get_theme_icon(SNAME("grabber_highlight"), SNAME("HSlider"));
				} else {
					grabber_tex = get_theme_icon(SNAME("grabber"), SNAME("HSlider"));
				}

				if (grabber->get_texture() != grabber_tex) {
					grabber->set_texture(grabber_tex);
				}

				Vector2 scale = get_global_transform_with_canvas().get_scale();
				grabber->set_scale(scale);
				grabber->reset_size();
				grabber->set_position((grabber_rect.get_center() - grabber->get_size() * 0.5) * scale);

				if (mousewheel_over_grabber) {
					Input::get_singleton()->warp_mouse(grabber->get_position() + grabber_rect.size);
				}

				grabber_range = width;
			}
		}
	}
}

void EditorSpinSlider::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			grabbing_spinner_speed = EditorSettings::get_singleton()->get("interface/inspector/float_drag_speed");
			_update_value_input_stylebox();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_value_input_stylebox();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (value_input_dirty) {
				value_input_dirty = false;
				value_input->set_text(get_text_value());
			}
			set_process_internal(false);
		} break;

		case NOTIFICATION_DRAW: {
			_draw_spin_slider();
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN:
		case NOTIFICATION_WM_WINDOW_FOCUS_OUT:
		case NOTIFICATION_WM_CLOSE_REQUEST:
		case NOTIFICATION_EXIT_TREE: {
			if (grabbing_spinner) {
				grabber->hide();
				Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
				Input::get_singleton()->warp_mouse(grabbing_spinner_mouse_pos);
				grabbing_spinner = false;
				grabbing_spinner_attempt = false;
			}
		} break;

		case NOTIFICATION_MOUSE_ENTER: {
			mouse_over_spin = true;
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			mouse_over_spin = false;
			queue_redraw();
		} break;

		case NOTIFICATION_FOCUS_ENTER: {
			if ((Input::get_singleton()->is_action_pressed("ui_focus_next") || Input::get_singleton()->is_action_pressed("ui_focus_prev")) && value_input_closed_frame != Engine::get_singleton()->get_frames_drawn()) {
				_focus_entered();
			}
			value_input_closed_frame = 0;
		} break;
	}
}

LineEdit *EditorSpinSlider::get_line_edit() {
	_ensure_input_popup();
	return value_input;
}

Size2 EditorSpinSlider::get_minimum_size() const {
	Ref<StyleBox> sb = get_theme_stylebox(CoreStringName(normal), SNAME("LineEdit"));
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("LineEdit"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("LineEdit"));

	Size2 ms = sb->get_minimum_size();
	ms.height += font->get_height(font_size);

	return ms;
}

void EditorSpinSlider::set_hide_slider(bool p_hide) {
	hide_slider = p_hide;
	queue_redraw();
}

bool EditorSpinSlider::is_hiding_slider() const {
	return hide_slider;
}

void EditorSpinSlider::set_label(const String &p_label) {
	label = p_label;
	queue_redraw();
}

String EditorSpinSlider::get_label() const {
	return label;
}

void EditorSpinSlider::set_suffix(const String &p_suffix) {
	suffix = p_suffix;
	queue_redraw();
}

String EditorSpinSlider::get_suffix() const {
	return suffix;
}

void EditorSpinSlider::_evaluate_input_text() {
	Ref<Expression> expr;
	expr.instantiate();

	// Convert commas ',' to dots '.' for French/German etc. keyboard layouts.
	String text = value_input->get_text().replace(",", ".");
	text = text.replace(";", ",");
	text = TS->parse_number(text);

	Error err = expr->parse(text);
	if (err != OK) {
		// If the expression failed try without converting commas to dots - they might have been for parameter separation.
		text = value_input->get_text();
		text = TS->parse_number(text);

		err = expr->parse(text);
		if (err != OK) {
			return;
		}
	}

	Variant v = expr->execute(Array(), nullptr, false, true);
	if (v.get_type() == Variant::NIL) {
		return;
	}
	set_value(v);
}

//text_submitted signal
void EditorSpinSlider::_value_input_submitted(const String &p_text) {
	value_input_closed_frame = Engine::get_singleton()->get_frames_drawn();
	if (value_input_popup) {
		value_input_popup->hide();
	}
}

//modal_closed signal
void EditorSpinSlider::_value_input_closed() {
	_evaluate_input_text();
	value_input_closed_frame = Engine::get_singleton()->get_frames_drawn();
}

//focus_exited signal
void EditorSpinSlider::_value_focus_exited() {
	// discontinue because the focus_exit was caused by right-click context menu
	if (value_input->is_menu_visible()) {
		return;
	}

	if (read_only) {
		// Spin slider has become read only while it was being edited.
		return;
	}

	_evaluate_input_text();
	// focus is not on the same element after the value_input was exited
	// -> focus is on next element
	// -> TAB was pressed
	// -> modal_close was not called
	// -> need to close/hide manually
	if (!is_visible_in_tree() || value_input_closed_frame != Engine::get_singleton()->get_frames_drawn()) {
		// Hidden or something else took focus.
		if (value_input_popup) {
			value_input_popup->hide();
		}
	} else {
		// Enter or Esc was pressed.
		grab_focus();
	}

	emit_signal("value_focus_exited");
}

void EditorSpinSlider::_grabber_mouse_entered() {
	mouse_over_grabber = true;
	queue_redraw();
}

void EditorSpinSlider::_grabber_mouse_exited() {
	mouse_over_grabber = false;
	queue_redraw();
}

void EditorSpinSlider::set_read_only(bool p_enable) {
	read_only = p_enable;
	if (read_only && value_input && value_input->is_inside_tree()) {
		value_input->release_focus();
	}

	queue_redraw();
}

bool EditorSpinSlider::is_read_only() const {
	return read_only;
}

void EditorSpinSlider::set_flat(bool p_enable) {
	flat = p_enable;
	queue_redraw();
}

bool EditorSpinSlider::is_flat() const {
	return flat;
}

bool EditorSpinSlider::is_grabbing() const {
	return grabbing_grabber || grabbing_spinner;
}

void EditorSpinSlider::_focus_entered() {
	if (read_only) {
		return;
	}

	_ensure_input_popup();
	value_input->set_text(get_text_value());
	value_input_popup->set_size(get_size());
	value_input->set_focus_next(find_next_valid_focus()->get_path());
	value_input->set_focus_previous(find_prev_valid_focus()->get_path());
	callable_mp((CanvasItem *)value_input_popup, &CanvasItem::show).call_deferred();
	callable_mp((Control *)value_input, &Control::grab_focus).call_deferred();
	callable_mp(value_input, &LineEdit ::select_all).call_deferred();
	emit_signal("value_focus_entered");
}

void EditorSpinSlider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_label", "label"), &EditorSpinSlider::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &EditorSpinSlider::get_label);

	ClassDB::bind_method(D_METHOD("set_suffix", "suffix"), &EditorSpinSlider::set_suffix);
	ClassDB::bind_method(D_METHOD("get_suffix"), &EditorSpinSlider::get_suffix);

	ClassDB::bind_method(D_METHOD("set_read_only", "read_only"), &EditorSpinSlider::set_read_only);
	ClassDB::bind_method(D_METHOD("is_read_only"), &EditorSpinSlider::is_read_only);

	ClassDB::bind_method(D_METHOD("set_flat", "flat"), &EditorSpinSlider::set_flat);
	ClassDB::bind_method(D_METHOD("is_flat"), &EditorSpinSlider::is_flat);

	ClassDB::bind_method(D_METHOD("set_hide_slider", "hide_slider"), &EditorSpinSlider::set_hide_slider);
	ClassDB::bind_method(D_METHOD("is_hiding_slider"), &EditorSpinSlider::is_hiding_slider);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "suffix"), "set_suffix", "get_suffix");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "read_only"), "set_read_only", "is_read_only");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat"), "set_flat", "is_flat");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_slider"), "set_hide_slider", "is_hiding_slider");

	ADD_SIGNAL(MethodInfo("grabbed"));
	ADD_SIGNAL(MethodInfo("ungrabbed"));
	ADD_SIGNAL(MethodInfo("updown_pressed"));
	ADD_SIGNAL(MethodInfo("value_focus_entered"));
	ADD_SIGNAL(MethodInfo("value_focus_exited"));

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, EditorSpinSlider, updown_icon, "updown");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, EditorSpinSlider, updown_disabled_icon, "updown_disabled");
}

void EditorSpinSlider::_ensure_input_popup() {
	if (value_input_popup) {
		return;
	}

	value_input_popup = memnew(Control);
	value_input_popup->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	add_child(value_input_popup);

	value_input = memnew(LineEdit);
	value_input->set_focus_mode(FOCUS_CLICK);
	value_input_popup->add_child(value_input);
	value_input->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	value_input_popup->connect(SceneStringName(hidden), callable_mp(this, &EditorSpinSlider::_value_input_closed));
	value_input->connect(SceneStringName(text_submitted), callable_mp(this, &EditorSpinSlider::_value_input_submitted));
	value_input->connect(SceneStringName(focus_exited), callable_mp(this, &EditorSpinSlider::_value_focus_exited));
	value_input->connect(SceneStringName(gui_input), callable_mp(this, &EditorSpinSlider::_value_input_gui_input));

	if (is_inside_tree()) {
		_update_value_input_stylebox();
	}
}

EditorSpinSlider::EditorSpinSlider() {
	set_focus_mode(FOCUS_ALL);
	grabber = memnew(TextureRect);
	add_child(grabber);
	grabber->hide();
	grabber->set_z_index(1);
	grabber->set_mouse_filter(MOUSE_FILTER_STOP);
	grabber->connect(SceneStringName(mouse_entered), callable_mp(this, &EditorSpinSlider::_grabber_mouse_entered));
	grabber->connect(SceneStringName(mouse_exited), callable_mp(this, &EditorSpinSlider::_grabber_mouse_exited));
	grabber->connect(SceneStringName(gui_input), callable_mp(this, &EditorSpinSlider::_grabber_gui_input));
}
