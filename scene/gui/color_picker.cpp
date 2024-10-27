/**************************************************************************/
/*  color_picker.cpp                                                      */
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

#include "color_picker.h"

#include "core/input/input.h"
#include "core/io/image.h"
#include "core/math/color.h"
#include "scene/gui/color_mode.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_texture.h"
#include "scene/theme/theme_db.h"
#include "servers/display_server.h"
#include "thirdparty/misc/ok_color_shader.h"

List<Color> ColorPicker::preset_cache;
List<Color> ColorPicker::recent_preset_cache;

void ColorPicker::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_color();
		} break;

		case NOTIFICATION_READY: {
			// FIXME: The embedding check is needed to fix a bug in single-window mode (GH-93718).
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_SCREEN_CAPTURE) && !get_tree()->get_root()->is_embedding_subwindows()) {
				btn_pick->set_tooltip_text(ETR("Pick a color from the screen."));
				btn_pick->connect(SceneStringName(pressed), callable_mp(this, &ColorPicker::_pick_button_pressed));
			} else {
				// On unsupported platforms, use a legacy method for color picking.
				btn_pick->set_tooltip_text(ETR("Pick a color from the application window."));
				btn_pick->connect(SceneStringName(pressed), callable_mp(this, &ColorPicker::_pick_button_pressed_legacy));
			}
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			List<BaseButton *> buttons;
			preset_group->get_buttons(&buttons);
			for (List<BaseButton *>::Element *E = buttons.front(); E; E = E->next()) {
				Color preset_color = ((ColorPresetButton *)E->get())->get_preset_color();
				E->get()->set_tooltip_text(vformat(atr(ETR("Color: #%s\nLMB: Apply color\nRMB: Remove preset")), preset_color.to_html(preset_color.a < 1)));
			}

			buttons.clear();
			recent_preset_group->get_buttons(&buttons);
			for (List<BaseButton *>::Element *E = buttons.front(); E; E = E->next()) {
				Color preset_color = ((ColorPresetButton *)E->get())->get_preset_color();
				E->get()->set_tooltip_text(vformat(atr(ETR("Color: #%s\nLMB: Apply color")), preset_color.to_html(preset_color.a < 1)));
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			btn_pick->set_icon(theme_cache.screen_picker);
			_update_drop_down_arrow(btn_preset->is_pressed(), btn_preset);
			_update_drop_down_arrow(btn_recent_preset->is_pressed(), btn_recent_preset);
			btn_add_preset->set_icon(theme_cache.add_preset);

			btn_pick->set_custom_minimum_size(Size2(28 * theme_cache.base_scale, 0));
			btn_shape->set_custom_minimum_size(Size2(28 * theme_cache.base_scale, 0));
			btn_mode->set_custom_minimum_size(Size2(28 * theme_cache.base_scale, 0));

			uv_edit->set_custom_minimum_size(Size2(theme_cache.sv_width, theme_cache.sv_height));
			w_edit->set_custom_minimum_size(Size2(theme_cache.h_width, 0));

			wheel_edit->set_custom_minimum_size(Size2(theme_cache.sv_width, theme_cache.sv_height));
			wheel_margin->add_theme_constant_override("margin_bottom", 8 * theme_cache.base_scale);

			for (int i = 0; i < SLIDER_COUNT; i++) {
				labels[i]->set_custom_minimum_size(Size2(theme_cache.label_width, 0));
				sliders[i]->add_theme_constant_override(SNAME("center_grabber"), theme_cache.center_slider_grabbers);
			}
			alpha_label->set_custom_minimum_size(Size2(theme_cache.label_width, 0));
			alpha_slider->add_theme_constant_override(SNAME("center_grabber"), theme_cache.center_slider_grabbers);

			for (int i = 0; i < MODE_BUTTON_COUNT; i++) {
				mode_btns[i]->begin_bulk_theme_override();
				mode_btns[i]->add_theme_style_override(SceneStringName(pressed), theme_cache.mode_button_pressed);
				mode_btns[i]->add_theme_style_override(CoreStringName(normal), theme_cache.mode_button_normal);
				mode_btns[i]->add_theme_style_override(SNAME("hover"), theme_cache.mode_button_hover);
				mode_btns[i]->end_bulk_theme_override();
			}

			shape_popup->set_item_icon(shape_popup->get_item_index(SHAPE_HSV_RECTANGLE), theme_cache.shape_rect);
			shape_popup->set_item_icon(shape_popup->get_item_index(SHAPE_HSV_WHEEL), theme_cache.shape_rect_wheel);
			shape_popup->set_item_icon(shape_popup->get_item_index(SHAPE_VHS_CIRCLE), theme_cache.shape_circle);
			shape_popup->set_item_icon(shape_popup->get_item_index(SHAPE_OKHSL_CIRCLE), theme_cache.shape_circle);

			if (current_shape != SHAPE_NONE) {
				btn_shape->set_icon(shape_popup->get_item_icon(current_shape));
			}

			internal_margin->begin_bulk_theme_override();
			internal_margin->add_theme_constant_override(SNAME("margin_bottom"), theme_cache.content_margin);
			internal_margin->add_theme_constant_override(SNAME("margin_left"), theme_cache.content_margin);
			internal_margin->add_theme_constant_override(SNAME("margin_right"), theme_cache.content_margin);
			internal_margin->add_theme_constant_override(SNAME("margin_top"), theme_cache.content_margin);
			internal_margin->end_bulk_theme_override();

			_reset_sliders_theme();

			if (Engine::get_singleton()->is_editor_hint()) {
				// Adjust for the width of the "Script" icon.
				text_type->set_custom_minimum_size(Size2(28 * theme_cache.base_scale, 0));
			}

			_update_presets();
			_update_recent_presets();
			_update_controls();
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			if (picker_window != nullptr && picker_window->is_visible()) {
				picker_window->hide();
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!is_picking_color) {
				return;
			}
			set_pick_color(DisplayServer::get_singleton()->screen_get_pixel(DisplayServer::get_singleton()->mouse_get_position()));
		}
	}
}

void ColorPicker::_update_theme_item_cache() {
	VBoxContainer::_update_theme_item_cache();

	theme_cache.base_scale = get_theme_default_base_scale();
}

Ref<Shader> ColorPicker::wheel_shader;
Ref<Shader> ColorPicker::circle_shader;
Ref<Shader> ColorPicker::circle_ok_color_shader;

void ColorPicker::init_shaders() {
	wheel_shader.instantiate();
	wheel_shader->set_code(R"(
// ColorPicker wheel shader.

shader_type canvas_item;

void fragment() {
	float x = UV.x - 0.5;
	float y = UV.y - 0.5;
	float a = atan(y, x);
	x += 0.001;
	y += 0.001;
	float b = float(sqrt(x * x + y * y) < 0.5) * float(sqrt(x * x + y * y) > 0.42);
	x -= 0.002;
	float b2 = float(sqrt(x * x + y * y) < 0.5) * float(sqrt(x * x + y * y) > 0.42);
	y -= 0.002;
	float b3 = float(sqrt(x * x + y * y) < 0.5) * float(sqrt(x * x + y * y) > 0.42);
	x += 0.002;
	float b4 = float(sqrt(x * x + y * y) < 0.5) * float(sqrt(x * x + y * y) > 0.42);

	COLOR = vec4(clamp((abs(fract(((a - TAU) / TAU) + vec3(3.0, 2.0, 1.0) / 3.0) * 6.0 - 3.0) - 1.0), 0.0, 1.0), (b + b2 + b3 + b4) / 4.00);
}
)");

	circle_shader.instantiate();
	circle_shader->set_code(R"(
// ColorPicker circle shader.

shader_type canvas_item;

uniform float v = 1.0;

void fragment() {
	float x = UV.x - 0.5;
	float y = UV.y - 0.5;
	float a = atan(y, x);
	x += 0.001;
	y += 0.001;
	float b = float(sqrt(x * x + y * y) < 0.5);
	x -= 0.002;
	float b2 = float(sqrt(x * x + y * y) < 0.5);
	y -= 0.002;
	float b3 = float(sqrt(x * x + y * y) < 0.5);
	x += 0.002;
	float b4 = float(sqrt(x * x + y * y) < 0.5);

	COLOR = vec4(mix(vec3(1.0), clamp(abs(fract(vec3((a - TAU) / TAU) + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - vec3(3.0)) - vec3(1.0), 0.0, 1.0), ((float(sqrt(x * x + y * y)) * 2.0)) / 1.0) * vec3(v), (b + b2 + b3 + b4) / 4.00);
})");

	circle_ok_color_shader.instantiate();
	circle_ok_color_shader->set_code(OK_COLOR_SHADER + R"(
// ColorPicker ok color hsv circle shader.

uniform float v = 1.0;

void fragment() {
	float x = UV.x - 0.5;
	float y = UV.y - 0.5;
	float h = atan(y, x) / (2.0 * M_PI);
	float s = sqrt(x * x + y * y) * 2.0;
	vec3 col = okhsl_to_srgb(vec3(h, s, v));
	x += 0.001;
	y += 0.001;
	float b = float(sqrt(x * x + y * y) < 0.5);
	x -= 0.002;
	float b2 = float(sqrt(x * x + y * y) < 0.5);
	y -= 0.002;
	float b3 = float(sqrt(x * x + y * y) < 0.5);
	x += 0.002;
	float b4 = float(sqrt(x * x + y * y) < 0.5);
	COLOR = vec4(col, (b + b2 + b3 + b4) / 4.00);
})");
}

void ColorPicker::finish_shaders() {
	wheel_shader.unref();
	circle_shader.unref();
	circle_ok_color_shader.unref();
}

void ColorPicker::set_focus_on_line_edit() {
	callable_mp((Control *)c_text, &Control::grab_focus).call_deferred();
}

void ColorPicker::_update_controls() {
	int mode_sliders_count = modes[current_mode]->get_slider_count();

	for (int i = current_slider_count; i < mode_sliders_count; i++) {
		sliders[i]->show();
		labels[i]->show();
		values[i]->show();
	}
	for (int i = mode_sliders_count; i < current_slider_count; i++) {
		sliders[i]->hide();
		labels[i]->hide();
		values[i]->hide();
	}
	current_slider_count = mode_sliders_count;

	for (int i = 0; i < current_slider_count; i++) {
		labels[i]->set_text(modes[current_mode]->get_slider_label(i));
	}
	alpha_label->set_text("A");

	slider_theme_modified = modes[current_mode]->apply_theme();

	if (edit_alpha) {
		alpha_value->show();
		alpha_slider->show();
		alpha_label->show();
	} else {
		alpha_value->hide();
		alpha_slider->hide();
		alpha_label->hide();
	}

	switch (_get_actual_shape()) {
		case SHAPE_HSV_RECTANGLE:
			wheel_edit->hide();
			w_edit->show();
			uv_edit->show();
			btn_shape->show();
			break;
		case SHAPE_HSV_WHEEL:
			wheel_edit->show();
			w_edit->hide();
			uv_edit->hide();
			btn_shape->show();
			wheel->set_material(wheel_mat);
			break;
		case SHAPE_VHS_CIRCLE:
			wheel_edit->show();
			w_edit->show();
			uv_edit->hide();
			btn_shape->show();
			wheel->set_material(circle_mat);
			circle_mat->set_shader(circle_shader);
			break;
		case SHAPE_OKHSL_CIRCLE:
			wheel_edit->show();
			w_edit->show();
			uv_edit->hide();
			btn_shape->show();
			wheel->set_material(circle_mat);
			circle_mat->set_shader(circle_ok_color_shader);
			break;
		case SHAPE_NONE:
			wheel_edit->hide();
			w_edit->hide();
			uv_edit->hide();
			btn_shape->hide();
			break;
		default: {
		}
	}
}

void ColorPicker::_set_pick_color(const Color &p_color, bool p_update_sliders) {
	if (text_changed) {
		add_recent_preset(color);
		text_changed = false;
	}

	color = p_color;
	if (color != last_color) {
		_copy_color_to_hsv();
		last_color = color;
	}

	if (!is_inside_tree()) {
		return;
	}

	_update_color(p_update_sliders);
}

void ColorPicker::set_pick_color(const Color &p_color) {
	_set_pick_color(p_color, true); //because setters can't have more arguments
}

void ColorPicker::set_old_color(const Color &p_color) {
	old_color = p_color;
}

void ColorPicker::set_display_old_color(bool p_enabled) {
	display_old_color = p_enabled;
}

bool ColorPicker::is_displaying_old_color() const {
	return display_old_color;
}

void ColorPicker::set_edit_alpha(bool p_show) {
	if (edit_alpha == p_show) {
		return;
	}
	edit_alpha = p_show;
	_update_controls();

	if (!is_inside_tree()) {
		return;
	}

	_update_color();
	sample->queue_redraw();
}

bool ColorPicker::is_editing_alpha() const {
	return edit_alpha;
}

void ColorPicker::_slider_drag_started() {
	currently_dragging = true;
}

void ColorPicker::_slider_value_changed() {
	if (updating) {
		return;
	}

	color = modes[current_mode]->get_color();
	modes[current_mode]->_value_changed();

	if (current_mode == MODE_HSV || current_mode == MODE_OKHSL) {
		h = sliders[0]->get_value() / 360.0;
		s = sliders[1]->get_value() / 100.0;
		v = sliders[2]->get_value() / 100.0;
		last_color = color;
	}

	_set_pick_color(color, false);
	if (!deferred_mode_enabled || !currently_dragging) {
		emit_signal(SNAME("color_changed"), color);
	}
}

void ColorPicker::_slider_drag_ended() {
	currently_dragging = false;
	if (deferred_mode_enabled) {
		emit_signal(SNAME("color_changed"), color);
	}
}

void ColorPicker::add_mode(ColorMode *p_mode) {
	modes.push_back(p_mode);
}

void ColorPicker::create_slider(GridContainer *gc, int idx) {
	Label *lbl = memnew(Label);
	lbl->set_v_size_flags(SIZE_SHRINK_CENTER);
	lbl->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	gc->add_child(lbl);

	HSlider *slider = memnew(HSlider);
	slider->set_v_size_flags(SIZE_SHRINK_CENTER);
	slider->set_focus_mode(FOCUS_NONE);
	gc->add_child(slider);

	SpinBox *val = memnew(SpinBox);
	slider->share(val);
	val->set_select_all_on_focus(true);
	gc->add_child(val);

	LineEdit *vle = val->get_line_edit();
	vle->connect(SceneStringName(text_changed), callable_mp(this, &ColorPicker::_text_changed));
	vle->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_line_edit_input));
	vle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);

	val->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_slider_or_spin_input));

	slider->set_h_size_flags(SIZE_EXPAND_FILL);

	slider->connect("drag_started", callable_mp(this, &ColorPicker::_slider_drag_started));
	slider->connect(SceneStringName(value_changed), callable_mp(this, &ColorPicker::_slider_value_changed).unbind(1));
	slider->connect("drag_ended", callable_mp(this, &ColorPicker::_slider_drag_ended).unbind(1));
	slider->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_slider_draw).bind(idx));
	slider->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_slider_or_spin_input));

	if (idx < SLIDER_COUNT) {
		sliders[idx] = slider;
		values[idx] = val;
		labels[idx] = lbl;
	} else {
		alpha_slider = slider;
		alpha_value = val;
		alpha_label = lbl;
	}
}

#ifdef TOOLS_ENABLED
void ColorPicker::set_editor_settings(Object *p_editor_settings) {
	if (editor_settings) {
		return;
	}
	editor_settings = p_editor_settings;

	if (preset_cache.is_empty()) {
		PackedColorArray saved_presets = editor_settings->call(SNAME("get_project_metadata"), "color_picker", "presets", PackedColorArray());
		for (int i = 0; i < saved_presets.size(); i++) {
			preset_cache.push_back(saved_presets[i]);
		}
	}

	for (const Color &preset : preset_cache) {
		presets.push_back(preset);
	}

	if (recent_preset_cache.is_empty()) {
		PackedColorArray saved_recent_presets = editor_settings->call(SNAME("get_project_metadata"), "color_picker", "recent_presets", PackedColorArray());
		for (int i = 0; i < saved_recent_presets.size(); i++) {
			recent_preset_cache.push_back(saved_recent_presets[i]);
		}
	}

	for (const Color &preset : recent_preset_cache) {
		recent_presets.push_back(preset);
	}

	_update_presets();
	_update_recent_presets();
}
#endif

HSlider *ColorPicker::get_slider(int p_idx) {
	if (p_idx < SLIDER_COUNT) {
		return sliders[p_idx];
	}
	return alpha_slider;
}

Vector<float> ColorPicker::get_active_slider_values() {
	Vector<float> cur_values;
	for (int i = 0; i < current_slider_count; i++) {
		cur_values.push_back(sliders[i]->get_value());
	}
	cur_values.push_back(alpha_slider->get_value());
	return cur_values;
}

void ColorPicker::_copy_color_to_hsv() {
	if (_get_actual_shape() == SHAPE_OKHSL_CIRCLE) {
		h = color.get_ok_hsl_h();
		s = color.get_ok_hsl_s();
		v = color.get_ok_hsl_l();
	} else {
		h = color.get_h();
		s = color.get_s();
		v = color.get_v();
	}
}

void ColorPicker::_copy_hsv_to_color() {
	if (_get_actual_shape() == SHAPE_OKHSL_CIRCLE) {
		color.set_ok_hsl(h, s, v, color.a);
	} else {
		color.set_hsv(h, s, v, color.a);
	}
}

void ColorPicker::_select_from_preset_container(const Color &p_color) {
	if (preset_group->get_pressed_button()) {
		preset_group->get_pressed_button()->set_pressed(false);
	}

	for (int i = 1; i < preset_container->get_child_count(); i++) {
		ColorPresetButton *current_btn = Object::cast_to<ColorPresetButton>(preset_container->get_child(i));
		if (current_btn && p_color == current_btn->get_preset_color()) {
			current_btn->set_pressed(true);
			break;
		}
	}
}

bool ColorPicker::_select_from_recent_preset_hbc(const Color &p_color) {
	for (int i = 0; i < recent_preset_hbc->get_child_count(); i++) {
		ColorPresetButton *current_btn = Object::cast_to<ColorPresetButton>(recent_preset_hbc->get_child(i));
		if (current_btn && p_color == current_btn->get_preset_color()) {
			current_btn->set_pressed(true);
			return true;
		}
	}
	return false;
}

ColorPicker::PickerShapeType ColorPicker::_get_actual_shape() const {
	return modes[current_mode]->get_shape_override() != SHAPE_MAX ? modes[current_mode]->get_shape_override() : current_shape;
}

void ColorPicker::_reset_sliders_theme() {
	Ref<StyleBoxFlat> style_box_flat(memnew(StyleBoxFlat));
	style_box_flat->set_content_margin(SIDE_TOP, 16 * theme_cache.base_scale);
	style_box_flat->set_bg_color(Color(0.2, 0.23, 0.31).lerp(Color(0, 0, 0, 1), 0.3).clamp());

	for (int i = 0; i < SLIDER_COUNT; i++) {
		sliders[i]->begin_bulk_theme_override();
		sliders[i]->add_theme_icon_override("grabber", theme_cache.bar_arrow);
		sliders[i]->add_theme_icon_override("grabber_highlight", theme_cache.bar_arrow);
		sliders[i]->add_theme_constant_override("grabber_offset", 8 * theme_cache.base_scale);
		if (!colorize_sliders) {
			sliders[i]->add_theme_style_override("slider", style_box_flat);
		}
		sliders[i]->end_bulk_theme_override();
	}

	alpha_slider->begin_bulk_theme_override();
	alpha_slider->add_theme_icon_override("grabber", theme_cache.bar_arrow);
	alpha_slider->add_theme_icon_override("grabber_highlight", theme_cache.bar_arrow);
	alpha_slider->add_theme_constant_override("grabber_offset", 8 * theme_cache.base_scale);
	if (!colorize_sliders) {
		alpha_slider->add_theme_style_override("slider", style_box_flat);
	}
	alpha_slider->end_bulk_theme_override();
}

void ColorPicker::_html_submitted(const String &p_html) {
	if (updating || text_is_constructor || !c_text->is_visible()) {
		return;
	}

	Color new_color = Color::from_string(p_html.strip_edges(), color);
	String html_no_prefix = p_html.strip_edges().trim_prefix("#");
	if (html_no_prefix.is_valid_hex_number(false)) {
		// Convert invalid HTML color codes that software like Figma supports.
		if (html_no_prefix.length() == 1) {
			// Turn `#1` into `#111111`.
			html_no_prefix = html_no_prefix.repeat(6);
		} else if (html_no_prefix.length() == 2) {
			// Turn `#12` into `#121212`.
			html_no_prefix = html_no_prefix.repeat(3);
		} else if (html_no_prefix.length() == 5) {
			// Turn `#12345` into `#11223344`.
			html_no_prefix = html_no_prefix.left(4);
		} else if (html_no_prefix.length() == 7) {
			// Turn `#1234567` into `#123456`.
			html_no_prefix = html_no_prefix.left(6);
		}
	}
	new_color = Color::from_string(html_no_prefix, new_color);

	if (!is_editing_alpha()) {
		new_color.a = color.a;
	}

	if (new_color.to_argb32() == color.to_argb32()) {
		return;
	}
	color = new_color;

	if (!is_inside_tree()) {
		return;
	}

	set_pick_color(color);
	emit_signal(SNAME("color_changed"), color);
}

void ColorPicker::_update_color(bool p_update_sliders) {
	updating = true;

	if (p_update_sliders) {
		float step = modes[current_mode]->get_slider_step();
		float spinbox_arrow_step = modes[current_mode]->get_spinbox_arrow_step();
		for (int i = 0; i < current_slider_count; i++) {
			sliders[i]->set_max(modes[current_mode]->get_slider_max(i));
			sliders[i]->set_step(step);
			values[i]->set_custom_arrow_step(spinbox_arrow_step);
			sliders[i]->set_value(modes[current_mode]->get_slider_value(i));
		}
		alpha_slider->set_max(modes[current_mode]->get_slider_max(current_slider_count));
		alpha_slider->set_step(step);
		alpha_slider->set_value(modes[current_mode]->get_slider_value(current_slider_count));
	}

	_update_text_value();

	sample->queue_redraw();
	uv_edit->queue_redraw();
	w_edit->queue_redraw();
	for (int i = 0; i < current_slider_count; i++) {
		sliders[i]->queue_redraw();
	}
	alpha_slider->queue_redraw();
	wheel->queue_redraw();
	wheel_uv->queue_redraw();
	updating = false;
}

void ColorPicker::_update_presets() {
	int preset_size = _get_preset_size();
	// Only update the preset button size if it has changed.
	if (preset_size != prev_preset_size) {
		prev_preset_size = preset_size;
		btn_add_preset->set_custom_minimum_size(Size2(preset_size, preset_size));
		for (int i = 1; i < preset_container->get_child_count(); i++) {
			ColorPresetButton *cpb = Object::cast_to<ColorPresetButton>(preset_container->get_child(i));
			cpb->set_custom_minimum_size(Size2(preset_size, preset_size));
		}
	}

#ifdef TOOLS_ENABLED
	if (editor_settings) {
		// Rebuild swatch color buttons, keeping the add-preset button in the first position.
		for (int i = 1; i < preset_container->get_child_count(); i++) {
			preset_container->get_child(i)->queue_free();
		}
		for (const Color &preset : preset_cache) {
			_add_preset_button(preset_size, preset);
		}
		_notification(NOTIFICATION_VISIBILITY_CHANGED);
	}
#endif
}

void ColorPicker::_update_recent_presets() {
#ifdef TOOLS_ENABLED
	if (editor_settings) {
		int recent_preset_count = recent_preset_hbc->get_child_count();
		for (int i = 0; i < recent_preset_count; i++) {
			memdelete(recent_preset_hbc->get_child(0));
		}

		recent_presets.clear();
		for (const Color &preset : recent_preset_cache) {
			recent_presets.push_back(preset);
		}

		int preset_size = _get_preset_size();
		for (const Color &preset : recent_presets) {
			_add_recent_preset_button(preset_size, preset);
		}

		_notification(NOTIFICATION_VISIBILITY_CHANGED);
	}
#endif
}

void ColorPicker::_text_type_toggled() {
	text_is_constructor = !text_is_constructor;
	if (text_is_constructor) {
		text_type->set_text("");
#ifdef TOOLS_ENABLED
		text_type->set_icon(get_editor_theme_icon(SNAME("Script")));
#endif

		c_text->set_editable(false);
		c_text->set_tooltip_text(RTR("Copy this constructor in a script."));
	} else {
		text_type->set_text("#");
		text_type->set_icon(nullptr);

		c_text->set_editable(true);
		c_text->set_tooltip_text(ETR("Enter a hex code (\"#ff0000\") or named color (\"red\")."));
	}
	_update_color();
}

Color ColorPicker::get_pick_color() const {
	return color;
}

Color ColorPicker::get_old_color() const {
	return old_color;
}

void ColorPicker::set_picker_shape(PickerShapeType p_shape) {
	ERR_FAIL_INDEX(p_shape, SHAPE_MAX);
	if (p_shape == current_shape) {
		return;
	}
	if (current_shape != SHAPE_NONE) {
		shape_popup->set_item_checked(current_shape, false);
	}
	if (p_shape != SHAPE_NONE) {
		shape_popup->set_item_checked(p_shape, true);
		btn_shape->set_icon(shape_popup->get_item_icon(p_shape));
	}

	current_shape = p_shape;

#ifdef TOOLS_ENABLED
	if (editor_settings) {
		editor_settings->call(SNAME("set_project_metadata"), "color_picker", "picker_shape", current_shape);
	}
#endif

	_copy_color_to_hsv();

	_update_controls();
	_update_color();
}

ColorPicker::PickerShapeType ColorPicker::get_picker_shape() const {
	return current_shape;
}

inline int ColorPicker::_get_preset_size() {
	return (int(get_minimum_size().width) - (preset_container->get_h_separation() * (PRESET_COLUMN_COUNT - 1))) / PRESET_COLUMN_COUNT;
}

void ColorPicker::_add_preset_button(int p_size, const Color &p_color) {
	ColorPresetButton *btn_preset_new = memnew(ColorPresetButton(p_color, p_size));
	btn_preset_new->set_tooltip_text(vformat(atr(ETR("Color: #%s\nLMB: Apply color\nRMB: Remove preset")), p_color.to_html(p_color.a < 1)));
	SET_DRAG_FORWARDING_GCDU(btn_preset_new, ColorPicker);
	btn_preset_new->set_button_group(preset_group);
	preset_container->add_child(btn_preset_new);
	btn_preset_new->set_pressed(true);
	btn_preset_new->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_preset_input).bind(p_color));
}

void ColorPicker::_add_recent_preset_button(int p_size, const Color &p_color) {
	ColorPresetButton *btn_preset_new = memnew(ColorPresetButton(p_color, p_size));
	btn_preset_new->set_tooltip_text(vformat(atr(ETR("Color: #%s\nLMB: Apply color")), p_color.to_html(p_color.a < 1)));
	btn_preset_new->set_button_group(recent_preset_group);
	recent_preset_hbc->add_child(btn_preset_new);
	recent_preset_hbc->move_child(btn_preset_new, 0);
	btn_preset_new->set_pressed(true);
	btn_preset_new->connect(SceneStringName(toggled), callable_mp(this, &ColorPicker::_recent_preset_pressed).bind(btn_preset_new));
}

void ColorPicker::_show_hide_preset(const bool &p_is_btn_pressed, Button *p_btn_preset, Container *p_preset_container) {
	if (p_is_btn_pressed) {
		p_preset_container->show();
	} else {
		p_preset_container->hide();
	}
	_update_drop_down_arrow(p_is_btn_pressed, p_btn_preset);
}

void ColorPicker::_update_drop_down_arrow(const bool &p_is_btn_pressed, Button *p_btn_preset) {
	if (p_is_btn_pressed) {
		p_btn_preset->set_icon(theme_cache.expanded_arrow);
	} else {
		p_btn_preset->set_icon(theme_cache.folded_arrow);
	}
}

void ColorPicker::_set_mode_popup_value(ColorModeType p_mode) {
	ERR_FAIL_INDEX(p_mode, MODE_MAX + 1);

	if (p_mode == MODE_MAX) {
		set_colorize_sliders(!colorize_sliders);
	} else {
		set_color_mode(p_mode);
	}
}

Variant ColorPicker::_get_drag_data_fw(const Point2 &p_point, Control *p_from_control) {
	ColorPresetButton *dragged_preset_button = Object::cast_to<ColorPresetButton>(p_from_control);

	if (!dragged_preset_button) {
		return Variant();
	}

	ColorPresetButton *drag_preview = memnew(ColorPresetButton(dragged_preset_button->get_preset_color(), _get_preset_size()));
	set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = "color_preset";
	drag_data["color_preset"] = dragged_preset_button->get_index();

	return drag_data;
}

bool ColorPicker::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control) const {
	Dictionary d = p_data;
	if (!d.has("type") || String(d["type"]) != "color_preset") {
		return false;
	}
	return true;
}

void ColorPicker::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control) {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == "color_preset") {
		int preset_from_id = d["color_preset"];
		int hover_now = p_from_control->get_index();

		if (preset_from_id == hover_now || hover_now == -1) {
			return;
		}
		preset_container->move_child(preset_container->get_child(preset_from_id), hover_now);
	}
}

void ColorPicker::add_preset(const Color &p_color) {
	List<Color>::Element *e = presets.find(p_color);
	if (e) {
		presets.move_to_back(e);
		preset_cache.move_to_back(preset_cache.find(p_color));

		preset_container->move_child(preset_group->get_pressed_button(), preset_container->get_child_count() - 1);
	} else {
		presets.push_back(p_color);
		preset_cache.push_back(p_color);

		_add_preset_button(_get_preset_size(), p_color);
	}

#ifdef TOOLS_ENABLED
	if (editor_settings) {
		PackedColorArray arr_to_save = get_presets();
		editor_settings->call(SNAME("set_project_metadata"), "color_picker", "presets", arr_to_save);
	}
#endif
}

void ColorPicker::add_recent_preset(const Color &p_color) {
	if (!_select_from_recent_preset_hbc(p_color)) {
		if (recent_preset_hbc->get_child_count() >= PRESET_COLUMN_COUNT) {
			recent_preset_cache.pop_front();
			recent_presets.pop_front();
			recent_preset_hbc->get_child(PRESET_COLUMN_COUNT - 1)->queue_free();
		}
		recent_presets.push_back(p_color);
		recent_preset_cache.push_back(p_color);
		_add_recent_preset_button(_get_preset_size(), p_color);
	}
	_select_from_preset_container(p_color);

#ifdef TOOLS_ENABLED
	if (editor_settings) {
		PackedColorArray arr_to_save = get_recent_presets();
		editor_settings->call(SNAME("set_project_metadata"), "color_picker", "recent_presets", arr_to_save);
	}
#endif
}

void ColorPicker::erase_preset(const Color &p_color) {
	List<Color>::Element *e = presets.find(p_color);
	if (e) {
		presets.erase(e);
		preset_cache.erase(preset_cache.find(p_color));

		// Find preset button to remove.
		for (int i = 1; i < preset_container->get_child_count(); i++) {
			ColorPresetButton *current_btn = Object::cast_to<ColorPresetButton>(preset_container->get_child(i));
			if (current_btn && p_color == current_btn->get_preset_color()) {
				current_btn->queue_free();
				break;
			}
		}

#ifdef TOOLS_ENABLED
		if (editor_settings) {
			PackedColorArray arr_to_save = get_presets();
			editor_settings->call(SNAME("set_project_metadata"), "color_picker", "presets", arr_to_save);
		}
#endif
	}
}

void ColorPicker::erase_recent_preset(const Color &p_color) {
	List<Color>::Element *e = recent_presets.find(p_color);
	if (e) {
		recent_presets.erase(e);
		recent_preset_cache.erase(recent_preset_cache.find(p_color));

		// Find recent preset button to remove.
		for (int i = 1; i < recent_preset_hbc->get_child_count(); i++) {
			ColorPresetButton *current_btn = Object::cast_to<ColorPresetButton>(recent_preset_hbc->get_child(i));
			if (current_btn && p_color == current_btn->get_preset_color()) {
				current_btn->queue_free();
				break;
			}
		}

#ifdef TOOLS_ENABLED
		if (editor_settings) {
			PackedColorArray arr_to_save = get_recent_presets();
			editor_settings->call(SNAME("set_project_metadata"), "color_picker", "recent_presets", arr_to_save);
		}
#endif
	}
}

PackedColorArray ColorPicker::get_presets() const {
	PackedColorArray arr;
	arr.resize(presets.size());
	int i = 0;
	for (List<Color>::ConstIterator itr = presets.begin(); itr != presets.end(); ++itr, ++i) {
		arr.set(i, *itr);
	}
	return arr;
}

PackedColorArray ColorPicker::get_recent_presets() const {
	PackedColorArray arr;
	arr.resize(recent_presets.size());
	int i = 0;
	for (List<Color>::ConstIterator itr = recent_presets.begin(); itr != recent_presets.end(); ++itr, ++i) {
		arr.set(i, *itr);
	}
	return arr;
}

void ColorPicker::set_color_mode(ColorModeType p_mode) {
	ERR_FAIL_INDEX(p_mode, MODE_MAX);

	if (current_mode == p_mode) {
		return;
	}

	if (slider_theme_modified) {
		_reset_sliders_theme();
	}

	mode_popup->set_item_checked(current_mode, false);
	mode_popup->set_item_checked(p_mode, true);

	if (p_mode < MODE_BUTTON_COUNT) {
		mode_btns[p_mode]->set_pressed(true);
	} else if (current_mode < MODE_BUTTON_COUNT) {
		mode_btns[current_mode]->set_pressed(false);
	}

	current_mode = p_mode;

#ifdef TOOLS_ENABLED
	if (editor_settings) {
		editor_settings->call(SNAME("set_project_metadata"), "color_picker", "color_mode", current_mode);
	}
#endif

	if (!is_inside_tree()) {
		return;
	}

	_update_controls();
	_update_color();
}

ColorPicker::ColorModeType ColorPicker::get_color_mode() const {
	return current_mode;
}

void ColorPicker::set_colorize_sliders(bool p_colorize_sliders) {
	if (colorize_sliders == p_colorize_sliders) {
		return;
	}

	colorize_sliders = p_colorize_sliders;
	mode_popup->set_item_checked(MODE_MAX + 1, colorize_sliders);

	if (colorize_sliders) {
		Ref<StyleBoxEmpty> style_box_empty(memnew(StyleBoxEmpty));

		if (!slider_theme_modified) {
			for (int i = 0; i < SLIDER_COUNT; i++) {
				sliders[i]->add_theme_style_override("slider", style_box_empty);
			}
		}
		alpha_slider->add_theme_style_override("slider", style_box_empty);
	} else {
		Ref<StyleBoxFlat> style_box_flat(memnew(StyleBoxFlat));
		style_box_flat->set_content_margin(SIDE_TOP, 16 * theme_cache.base_scale);
		style_box_flat->set_bg_color(Color(0.2, 0.23, 0.31).lerp(Color(0, 0, 0, 1), 0.3).clamp());

		if (!slider_theme_modified) {
			for (int i = 0; i < SLIDER_COUNT; i++) {
				sliders[i]->add_theme_style_override("slider", style_box_flat);
			}
		}
		alpha_slider->add_theme_style_override("slider", style_box_flat);
	}
}

bool ColorPicker::is_colorizing_sliders() const {
	return colorize_sliders;
}

void ColorPicker::set_deferred_mode(bool p_enabled) {
	deferred_mode_enabled = p_enabled;
}

bool ColorPicker::is_deferred_mode() const {
	return deferred_mode_enabled;
}

void ColorPicker::_update_text_value() {
	bool text_visible = true;
	if (text_is_constructor) {
		String t = "Color(" + String::num(color.r, 3) + ", " + String::num(color.g, 3) + ", " + String::num(color.b, 3);
		if (edit_alpha && color.a < 1) {
			t += ", " + String::num(color.a, 3) + ")";
		} else {
			t += ")";
		}
		c_text->set_text(t);
	}

	if (color.r > 1 || color.g > 1 || color.b > 1 || color.r < 0 || color.g < 0 || color.b < 0) {
		text_visible = false;
	} else if (!text_is_constructor) {
		c_text->set_text(color.to_html(edit_alpha && color.a < 1));
	}

	text_type->set_visible(text_visible);
	c_text->set_visible(text_visible);
}

void ColorPicker::_sample_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		const Rect2 rect_old = Rect2(Point2(), Size2(sample->get_size().width * 0.5, sample->get_size().height * 0.95));
		if (rect_old.has_point(mb->get_position())) {
			// Revert to the old color when left-clicking the old color sample.
			set_pick_color(old_color);
			emit_signal(SNAME("color_changed"), color);
		}
	}
}

void ColorPicker::_sample_draw() {
	// Covers the right half of the sample if the old color is being displayed,
	// or the whole sample if it's not being displayed.
	Rect2 rect_new;

	if (display_old_color) {
		rect_new = Rect2(Point2(sample->get_size().width * 0.5, 0), Size2(sample->get_size().width * 0.5, sample->get_size().height * 0.95));

		// Draw both old and new colors for easier comparison (only if spawned from a ColorPickerButton).
		const Rect2 rect_old = Rect2(Point2(), Size2(sample->get_size().width * 0.5, sample->get_size().height * 0.95));

		if (old_color.a < 1.0) {
			sample->draw_texture_rect(theme_cache.sample_bg, rect_old, true);
		}

		sample->draw_rect(rect_old, old_color);

		if (!old_color.is_equal_approx(color)) {
			// Draw a revert indicator to indicate that the old sample can be clicked to revert to this old color.
			// Adapt icon color to the background color (taking alpha checkerboard into account) so that it's always visible.
			sample->draw_texture(theme_cache.sample_revert,
					rect_old.size * 0.5 - theme_cache.sample_revert->get_size() * 0.5,
					Math::lerp(0.75f, old_color.get_luminance(), old_color.a) < 0.455 ? Color(1, 1, 1) : (Color(0.01, 0.01, 0.01)));
		}

		if (old_color.r > 1 || old_color.g > 1 || old_color.b > 1) {
			// Draw an indicator to denote that the old color is "overbright" and can't be displayed accurately in the preview.
			sample->draw_texture(theme_cache.overbright_indicator, Point2());
		}
	} else {
		rect_new = Rect2(Point2(), Size2(sample->get_size().width, sample->get_size().height * 0.95));
	}

	if (color.a < 1.0) {
		sample->draw_texture_rect(theme_cache.sample_bg, rect_new, true);
	}

	sample->draw_rect(rect_new, color);

	if (color.r > 1 || color.g > 1 || color.b > 1) {
		// Draw an indicator to denote that the new color is "overbright" and can't be displayed accurately in the preview.
		sample->draw_texture(theme_cache.overbright_indicator, Point2(uv_edit->get_size().width * 0.5, 0));
	}
}

void ColorPicker::_hsv_draw(int p_which, Control *c) {
	if (!c) {
		return;
	}

	PickerShapeType actual_shape = _get_actual_shape();
	if (p_which == 0) {
		Vector<Point2> points;
		Vector<Color> colors;
		Vector<Color> colors2;
		Color col = color;
		Vector2 center = c->get_size() / 2.0;

		switch (actual_shape) {
			case SHAPE_HSV_WHEEL: {
				points.resize(4);
				colors.resize(4);
				colors2.resize(4);
				real_t ring_radius_x = Math_SQRT12 * c->get_size().width * 0.42;
				real_t ring_radius_y = Math_SQRT12 * c->get_size().height * 0.42;

				points.set(0, center - Vector2(ring_radius_x, ring_radius_y));
				points.set(1, center + Vector2(ring_radius_x, -ring_radius_y));
				points.set(2, center + Vector2(ring_radius_x, ring_radius_y));
				points.set(3, center + Vector2(-ring_radius_x, ring_radius_y));
				colors.set(0, Color(1, 1, 1, 1));
				colors.set(1, Color(1, 1, 1, 1));
				colors.set(2, Color(0, 0, 0, 1));
				colors.set(3, Color(0, 0, 0, 1));
				c->draw_polygon(points, colors);

				col.set_hsv(h, 1, 1);
				col.a = 0;
				colors2.set(0, col);
				col.a = 1;
				colors2.set(1, col);
				col.set_hsv(h, 1, 0);
				colors2.set(2, col);
				col.a = 0;
				colors2.set(3, col);
				c->draw_polygon(points, colors2);
				break;
			}
			case SHAPE_HSV_RECTANGLE: {
				points.resize(4);
				colors.resize(4);
				colors2.resize(4);
				points.set(0, Vector2());
				points.set(1, Vector2(c->get_size().x, 0));
				points.set(2, c->get_size());
				points.set(3, Vector2(0, c->get_size().y));
				colors.set(0, Color(1, 1, 1, 1));
				colors.set(1, Color(1, 1, 1, 1));
				colors.set(2, Color(0, 0, 0, 1));
				colors.set(3, Color(0, 0, 0, 1));
				c->draw_polygon(points, colors);
				col = color;
				col.set_hsv(h, 1, 1);
				col.a = 0;
				colors2.set(0, col);
				col.a = 1;
				colors2.set(1, col);
				col.set_hsv(h, 1, 0);
				colors2.set(2, col);
				col.a = 0;
				colors2.set(3, col);
				c->draw_polygon(points, colors2);
				break;
			}
			default: {
			}
		}

		int x;
		int y;
		if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
			x = center.x + (center.x * Math::cos(h * Math_TAU) * s) - (theme_cache.picker_cursor->get_width() / 2);
			y = center.y + (center.y * Math::sin(h * Math_TAU) * s) - (theme_cache.picker_cursor->get_height() / 2);
		} else {
			real_t corner_x = (c == wheel_uv) ? center.x - Math_SQRT12 * c->get_size().width * 0.42 : 0;
			real_t corner_y = (c == wheel_uv) ? center.y - Math_SQRT12 * c->get_size().height * 0.42 : 0;

			Size2 real_size(c->get_size().x - corner_x * 2, c->get_size().y - corner_y * 2);
			x = CLAMP(real_size.x * s, 0, real_size.x) + corner_x - (theme_cache.picker_cursor->get_width() / 2);
			y = CLAMP(real_size.y - real_size.y * v, 0, real_size.y) + corner_y - (theme_cache.picker_cursor->get_height() / 2);
		}
		c->draw_texture(theme_cache.picker_cursor, Point2(x, y));

		col.set_hsv(h, 1, 1);
		if (actual_shape == SHAPE_HSV_WHEEL) {
			points.resize(4);
			double h1 = h - (0.5 / 360);
			double h2 = h + (0.5 / 360);
			points.set(0, Point2(center.x + (center.x * Math::cos(h1 * Math_TAU)), center.y + (center.y * Math::sin(h1 * Math_TAU))));
			points.set(1, Point2(center.x + (center.x * Math::cos(h1 * Math_TAU) * 0.84), center.y + (center.y * Math::sin(h1 * Math_TAU) * 0.84)));
			points.set(2, Point2(center.x + (center.x * Math::cos(h2 * Math_TAU)), center.y + (center.y * Math::sin(h2 * Math_TAU))));
			points.set(3, Point2(center.x + (center.x * Math::cos(h2 * Math_TAU) * 0.84), center.y + (center.y * Math::sin(h2 * Math_TAU) * 0.84)));
			c->draw_multiline(points, col.inverted());
		}

	} else if (p_which == 1) {
		if (actual_shape == SHAPE_HSV_RECTANGLE) {
			c->draw_set_transform(Point2(), -Math_PI / 2, Size2(c->get_size().x, -c->get_size().y));
			c->draw_texture_rect(theme_cache.color_hue, Rect2(Point2(), Size2(1, 1)));
			c->draw_set_transform(Point2(), 0, Size2(1, 1));
			int y = c->get_size().y - c->get_size().y * (1.0 - h);
			Color col;
			col.set_hsv(h, 1, 1);
			c->draw_line(Point2(0, y), Point2(c->get_size().x, y), col.inverted());
		} else if (actual_shape == SHAPE_OKHSL_CIRCLE) {
			Vector<Point2> points;
			Vector<Color> colors;
			Color col;
			col.set_ok_hsl(h, s, 1);
			Color col2;
			col2.set_ok_hsl(h, s, 0.5);
			Color col3;
			col3.set_ok_hsl(h, s, 0);
			points.resize(6);
			colors.resize(6);
			points.set(0, Vector2(c->get_size().x, 0));
			points.set(1, Vector2(c->get_size().x, c->get_size().y * 0.5));
			points.set(2, c->get_size());
			points.set(3, Vector2(0, c->get_size().y));
			points.set(4, Vector2(0, c->get_size().y * 0.5));
			points.set(5, Vector2());
			colors.set(0, col);
			colors.set(1, col2);
			colors.set(2, col3);
			colors.set(3, col3);
			colors.set(4, col2);
			colors.set(5, col);
			c->draw_polygon(points, colors);
			int y = c->get_size().y - c->get_size().y * CLAMP(v, 0, 1);
			col.set_ok_hsl(h, 1, v);
			c->draw_line(Point2(0, y), Point2(c->get_size().x, y), col.inverted());
		} else if (actual_shape == SHAPE_VHS_CIRCLE) {
			Vector<Point2> points;
			Vector<Color> colors;
			Color col;
			col.set_hsv(h, s, 1);
			points.resize(4);
			colors.resize(4);
			points.set(0, Vector2());
			points.set(1, Vector2(c->get_size().x, 0));
			points.set(2, c->get_size());
			points.set(3, Vector2(0, c->get_size().y));
			colors.set(0, col);
			colors.set(1, col);
			colors.set(2, Color(0, 0, 0));
			colors.set(3, Color(0, 0, 0));
			c->draw_polygon(points, colors);
			int y = c->get_size().y - c->get_size().y * CLAMP(v, 0, 1);
			col.set_hsv(h, 1, v);
			c->draw_line(Point2(0, y), Point2(c->get_size().x, y), col.inverted());
		}
	} else if (p_which == 2) {
		c->draw_rect(Rect2(Point2(), c->get_size()), Color(1, 1, 1));
		if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
			circle_mat->set_shader_parameter("v", v);
		}
	}
}

void ColorPicker::_slider_draw(int p_which) {
	if (colorize_sliders) {
		modes[current_mode]->slider_draw(p_which);
	}
}

void ColorPicker::_uv_input(const Ref<InputEvent> &p_event, Control *c) {
	Ref<InputEventMouseButton> bev = p_event;
	PickerShapeType actual_shape = _get_actual_shape();

	if (bev.is_valid()) {
		if (bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
			Vector2 center = c->get_size() / 2.0;
			if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
				real_t dist = center.distance_to(bev->get_position());
				if (dist <= center.x) {
					real_t rad = center.angle_to_point(bev->get_position());
					h = ((rad >= 0) ? rad : (Math_TAU + rad)) / Math_TAU;
					s = CLAMP(dist / center.x, 0, 1);
				} else {
					return;
				}
			} else {
				real_t corner_x = (c == wheel_uv) ? center.x - Math_SQRT12 * c->get_size().width * 0.42 : 0;
				real_t corner_y = (c == wheel_uv) ? center.y - Math_SQRT12 * c->get_size().height * 0.42 : 0;
				Size2 real_size(c->get_size().x - corner_x * 2, c->get_size().y - corner_y * 2);

				if (bev->get_position().x < corner_x || bev->get_position().x > c->get_size().x - corner_x ||
						bev->get_position().y < corner_y || bev->get_position().y > c->get_size().y - corner_y) {
					{
						real_t dist = center.distance_to(bev->get_position());

						if (dist >= center.x * 0.84 && dist <= center.x) {
							real_t rad = center.angle_to_point(bev->get_position());
							h = ((rad >= 0) ? rad : (Math_TAU + rad)) / Math_TAU;
							spinning = true;
						} else {
							return;
						}
					}
				}

				if (!spinning) {
					real_t x = CLAMP(bev->get_position().x - corner_x, 0, real_size.x);
					real_t y = CLAMP(bev->get_position().y - corner_y, 0, real_size.y);

					s = x / real_size.x;
					v = 1.0 - y / real_size.y;
				}
			}

			changing_color = true;

			_copy_hsv_to_color();
			last_color = color;
			set_pick_color(color);

			if (!deferred_mode_enabled) {
				emit_signal(SNAME("color_changed"), color);
			}
		} else if (!bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
			if (deferred_mode_enabled) {
				emit_signal(SNAME("color_changed"), color);
			}
			add_recent_preset(color);
			changing_color = false;
			spinning = false;
		} else {
			changing_color = false;
			spinning = false;
		}
	}

	Ref<InputEventMouseMotion> mev = p_event;

	if (mev.is_valid()) {
		if (!changing_color) {
			return;
		}

		Vector2 center = c->get_size() / 2.0;
		if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
			real_t dist = center.distance_to(mev->get_position());
			real_t rad = center.angle_to_point(mev->get_position());
			h = ((rad >= 0) ? rad : (Math_TAU + rad)) / Math_TAU;
			s = CLAMP(dist / center.x, 0, 1);
		} else {
			if (spinning) {
				real_t rad = center.angle_to_point(mev->get_position());
				h = ((rad >= 0) ? rad : (Math_TAU + rad)) / Math_TAU;
			} else {
				real_t corner_x = (c == wheel_uv) ? center.x - Math_SQRT12 * c->get_size().width * 0.42 : 0;
				real_t corner_y = (c == wheel_uv) ? center.y - Math_SQRT12 * c->get_size().height * 0.42 : 0;
				Size2 real_size(c->get_size().x - corner_x * 2, c->get_size().y - corner_y * 2);

				real_t x = CLAMP(mev->get_position().x - corner_x, 0, real_size.x);
				real_t y = CLAMP(mev->get_position().y - corner_y, 0, real_size.y);

				s = x / real_size.x;
				v = 1.0 - y / real_size.y;
			}
		}

		_copy_hsv_to_color();
		last_color = color;
		set_pick_color(color);

		if (!deferred_mode_enabled) {
			emit_signal(SNAME("color_changed"), color);
		}
	}
}

void ColorPicker::_w_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> bev = p_event;
	PickerShapeType actual_shape = _get_actual_shape();

	if (bev.is_valid()) {
		if (bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
			changing_color = true;
			float y = CLAMP((float)bev->get_position().y, 0, w_edit->get_size().height);
			if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
				v = 1.0 - (y / w_edit->get_size().height);
			} else {
				h = y / w_edit->get_size().height;
			}
		} else {
			changing_color = false;
		}

		_copy_hsv_to_color();
		last_color = color;
		set_pick_color(color);

		if (!bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
			add_recent_preset(color);
			emit_signal(SNAME("color_changed"), color);
		} else if (!deferred_mode_enabled) {
			emit_signal(SNAME("color_changed"), color);
		}
	}

	Ref<InputEventMouseMotion> mev = p_event;

	if (mev.is_valid()) {
		if (!changing_color) {
			return;
		}
		float y = CLAMP((float)mev->get_position().y, 0, w_edit->get_size().height);
		if (actual_shape == SHAPE_VHS_CIRCLE || actual_shape == SHAPE_OKHSL_CIRCLE) {
			v = 1.0 - (y / w_edit->get_size().height);
		} else {
			h = y / w_edit->get_size().height;
		}

		_copy_hsv_to_color();
		last_color = color;
		set_pick_color(color);

		if (!deferred_mode_enabled) {
			emit_signal(SNAME("color_changed"), color);
		}
	}
}

void ColorPicker::_slider_or_spin_input(const Ref<InputEvent> &p_event) {
	if (line_edit_mouse_release) {
		line_edit_mouse_release = false;
		return;
	}
	Ref<InputEventMouseButton> bev = p_event;
	if (bev.is_valid() && !bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
		add_recent_preset(color);
	}
}

void ColorPicker::_line_edit_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> bev = p_event;
	if (bev.is_valid() && !bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
		line_edit_mouse_release = true;
	}
}

void ColorPicker::_preset_input(const Ref<InputEvent> &p_event, const Color &p_color) {
	Ref<InputEventMouseButton> bev = p_event;

	if (bev.is_valid()) {
		if (bev->is_pressed() && bev->get_button_index() == MouseButton::LEFT) {
			set_pick_color(p_color);
			add_recent_preset(color);
			emit_signal(SNAME("color_changed"), p_color);
		} else if (bev->is_pressed() && bev->get_button_index() == MouseButton::RIGHT && can_add_swatches) {
			erase_preset(p_color);
			emit_signal(SNAME("preset_removed"), p_color);
		}
	}
}

void ColorPicker::_recent_preset_pressed(const bool p_pressed, ColorPresetButton *p_preset) {
	if (!p_pressed) {
		return;
	}
	set_pick_color(p_preset->get_preset_color());

	recent_presets.move_to_back(recent_presets.find(p_preset->get_preset_color()));
	List<Color>::Element *e = recent_preset_cache.find(p_preset->get_preset_color());
	if (e) {
		recent_preset_cache.move_to_back(e);
	}

	recent_preset_hbc->move_child(p_preset, 0);
	emit_signal(SNAME("color_changed"), p_preset->get_preset_color());
}

void ColorPicker::_text_changed(const String &) {
	text_changed = true;
}

void ColorPicker::_add_preset_pressed() {
	add_preset(color);
	emit_signal(SNAME("preset_added"), color);
}

void ColorPicker::_pick_button_pressed() {
	is_picking_color = true;
	set_process_internal(true);

	if (!picker_window) {
		picker_window = memnew(Popup);
		picker_window->set_size(Vector2i(1, 1));
		picker_window->connect(SceneStringName(visibility_changed), callable_mp(this, &ColorPicker::_pick_finished));
		add_child(picker_window, false, INTERNAL_MODE_FRONT);
	}
	picker_window->popup();
}

void ColorPicker::_pick_finished() {
	if (picker_window->is_visible()) {
		return;
	}

	if (Input::get_singleton()->is_action_just_pressed(SNAME("ui_cancel"))) {
		set_pick_color(old_color);
	} else {
		emit_signal(SNAME("color_changed"), color);
	}
	is_picking_color = false;
	set_process_internal(false);
	picker_window->hide();
}

void ColorPicker::_pick_button_pressed_legacy() {
	if (!is_inside_tree()) {
		return;
	}

	if (!picker_window) {
		picker_window = memnew(Popup);
		picker_window->hide();
		picker_window->set_transient(true);
		add_child(picker_window, false, INTERNAL_MODE_FRONT);

		picker_texture_rect = memnew(TextureRect);
		picker_texture_rect->set_anchors_preset(Control::PRESET_FULL_RECT);
		picker_texture_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		picker_window->add_child(picker_texture_rect);
		picker_texture_rect->set_default_cursor_shape(CURSOR_POINTING_HAND);
		picker_texture_rect->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_picker_texture_input));

		picker_preview_label = memnew(Label);
		picker_preview_label->set_anchors_preset(Control::PRESET_CENTER_TOP);
		picker_preview_label->set_text(ETR("Color Picking active"));

		picker_preview_style_box.instantiate();
		picker_preview_style_box->set_bg_color(Color(1.0, 1.0, 1.0));
		picker_preview_style_box->set_content_margin_all(4.0);
		picker_preview_label->add_theme_style_override(CoreStringName(normal), picker_preview_style_box);

		picker_window->add_child(picker_preview_label);
	}

	Rect2i screen_rect;
	if (picker_window->is_embedded()) {
		screen_rect = picker_window->get_embedder()->get_visible_rect();
		picker_window->set_position(Point2i());
		picker_texture_rect->set_texture(ImageTexture::create_from_image(picker_window->get_embedder()->get_texture()->get_image()));
	} else {
		screen_rect = picker_window->get_parent_rect();
		picker_window->set_position(screen_rect.position);

		Ref<Image> target_image = Image::create_empty(screen_rect.size.x, screen_rect.size.y, false, Image::FORMAT_RGB8);
		DisplayServer *ds = DisplayServer::get_singleton();

		// Add the Texture of each Window to the Image.
		Vector<DisplayServer::WindowID> wl = ds->get_window_list();
		// FIXME: sort windows by visibility.
		for (int index = 0; index < wl.size(); index++) {
			DisplayServer::WindowID wid = wl[index];
			if (wid == DisplayServer::INVALID_WINDOW_ID) {
				continue;
			}

			ObjectID woid = DisplayServer::get_singleton()->window_get_attached_instance_id(wid);
			if (woid == ObjectID()) {
				continue;
			}

			Window *w = Object::cast_to<Window>(ObjectDB::get_instance(woid));
			Ref<Image> img = w->get_texture()->get_image();
			if (!img.is_valid() || img->is_empty()) {
				continue;
			}
			img->convert(Image::FORMAT_RGB8);
			target_image->blit_rect(img, Rect2i(Point2i(0, 0), img->get_size()), w->get_position());
		}

		picker_texture_rect->set_texture(ImageTexture::create_from_image(target_image));
	}

	picker_window->set_size(screen_rect.size);
	picker_preview_label->set_custom_minimum_size(screen_rect.size / 10); // 10% of size in each axis.
	picker_window->popup();
}

void ColorPicker::_picker_texture_input(const Ref<InputEvent> &p_event) {
	if (!is_inside_tree()) {
		return;
	}

	Ref<InputEventMouseButton> bev = p_event;
	if (bev.is_valid() && bev->get_button_index() == MouseButton::LEFT && !bev->is_pressed()) {
		set_pick_color(picker_color);
		emit_signal(SNAME("color_changed"), color);
		picker_window->hide();
	}

	Ref<InputEventMouseMotion> mev = p_event;
	if (mev.is_valid()) {
		Ref<Image> img = picker_texture_rect->get_texture()->get_image();
		if (img.is_valid() && !img->is_empty()) {
			Vector2 ofs = mev->get_position();
			picker_color = img->get_pixel(ofs.x, ofs.y);
			picker_preview_style_box->set_bg_color(picker_color);
			picker_preview_label->add_theme_color_override(SceneStringName(font_color), picker_color.get_luminance() < 0.5 ? Color(1.0f, 1.0f, 1.0f) : Color(0.0f, 0.0f, 0.0f));
		}
	}
}

void ColorPicker::_html_focus_exit() {
	if (c_text->is_menu_visible()) {
		return;
	}

	if (is_visible_in_tree()) {
		_html_submitted(c_text->get_text());
	} else {
		_update_text_value();
	}
}

void ColorPicker::set_can_add_swatches(bool p_enabled) {
	if (can_add_swatches == p_enabled) {
		return;
	}
	can_add_swatches = p_enabled;
	if (!p_enabled) {
		btn_add_preset->set_disabled(true);
		btn_add_preset->set_focus_mode(FOCUS_NONE);
	} else {
		btn_add_preset->set_disabled(false);
		btn_add_preset->set_focus_mode(FOCUS_ALL);
	}
}

bool ColorPicker::are_swatches_enabled() const {
	return can_add_swatches;
}

void ColorPicker::set_presets_visible(bool p_visible) {
	if (presets_visible == p_visible) {
		return;
	}
	presets_visible = p_visible;
	btn_preset->set_visible(p_visible);
	btn_recent_preset->set_visible(p_visible);
}

bool ColorPicker::are_presets_visible() const {
	return presets_visible;
}

void ColorPicker::set_modes_visible(bool p_visible) {
	if (color_modes_visible == p_visible) {
		return;
	}
	color_modes_visible = p_visible;
	mode_hbc->set_visible(p_visible);
}

bool ColorPicker::are_modes_visible() const {
	return color_modes_visible;
}

void ColorPicker::set_sampler_visible(bool p_visible) {
	if (sampler_visible == p_visible) {
		return;
	}
	sampler_visible = p_visible;
	sample_hbc->set_visible(p_visible);
}

bool ColorPicker::is_sampler_visible() const {
	return sampler_visible;
}

void ColorPicker::set_sliders_visible(bool p_visible) {
	if (sliders_visible == p_visible) {
		return;
	}
	sliders_visible = p_visible;
	slider_gc->set_visible(p_visible);
}

bool ColorPicker::are_sliders_visible() const {
	return sliders_visible;
}

void ColorPicker::set_hex_visible(bool p_visible) {
	if (hex_visible == p_visible) {
		return;
	}
	hex_visible = p_visible;
	hex_hbc->set_visible(p_visible);
}

bool ColorPicker::is_hex_visible() const {
	return hex_visible;
}

void ColorPicker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pick_color", "color"), &ColorPicker::set_pick_color);
	ClassDB::bind_method(D_METHOD("get_pick_color"), &ColorPicker::get_pick_color);
	ClassDB::bind_method(D_METHOD("set_deferred_mode", "mode"), &ColorPicker::set_deferred_mode);
	ClassDB::bind_method(D_METHOD("is_deferred_mode"), &ColorPicker::is_deferred_mode);
	ClassDB::bind_method(D_METHOD("set_color_mode", "color_mode"), &ColorPicker::set_color_mode);
	ClassDB::bind_method(D_METHOD("get_color_mode"), &ColorPicker::get_color_mode);
	ClassDB::bind_method(D_METHOD("set_edit_alpha", "show"), &ColorPicker::set_edit_alpha);
	ClassDB::bind_method(D_METHOD("is_editing_alpha"), &ColorPicker::is_editing_alpha);
	ClassDB::bind_method(D_METHOD("set_can_add_swatches", "enabled"), &ColorPicker::set_can_add_swatches);
	ClassDB::bind_method(D_METHOD("are_swatches_enabled"), &ColorPicker::are_swatches_enabled);
	ClassDB::bind_method(D_METHOD("set_presets_visible", "visible"), &ColorPicker::set_presets_visible);
	ClassDB::bind_method(D_METHOD("are_presets_visible"), &ColorPicker::are_presets_visible);
	ClassDB::bind_method(D_METHOD("set_modes_visible", "visible"), &ColorPicker::set_modes_visible);
	ClassDB::bind_method(D_METHOD("are_modes_visible"), &ColorPicker::are_modes_visible);
	ClassDB::bind_method(D_METHOD("set_sampler_visible", "visible"), &ColorPicker::set_sampler_visible);
	ClassDB::bind_method(D_METHOD("is_sampler_visible"), &ColorPicker::is_sampler_visible);
	ClassDB::bind_method(D_METHOD("set_sliders_visible", "visible"), &ColorPicker::set_sliders_visible);
	ClassDB::bind_method(D_METHOD("are_sliders_visible"), &ColorPicker::are_sliders_visible);
	ClassDB::bind_method(D_METHOD("set_hex_visible", "visible"), &ColorPicker::set_hex_visible);
	ClassDB::bind_method(D_METHOD("is_hex_visible"), &ColorPicker::is_hex_visible);
	ClassDB::bind_method(D_METHOD("add_preset", "color"), &ColorPicker::add_preset);
	ClassDB::bind_method(D_METHOD("erase_preset", "color"), &ColorPicker::erase_preset);
	ClassDB::bind_method(D_METHOD("get_presets"), &ColorPicker::get_presets);
	ClassDB::bind_method(D_METHOD("add_recent_preset", "color"), &ColorPicker::add_recent_preset);
	ClassDB::bind_method(D_METHOD("erase_recent_preset", "color"), &ColorPicker::erase_recent_preset);
	ClassDB::bind_method(D_METHOD("get_recent_presets"), &ColorPicker::get_recent_presets);
	ClassDB::bind_method(D_METHOD("set_picker_shape", "shape"), &ColorPicker::set_picker_shape);
	ClassDB::bind_method(D_METHOD("get_picker_shape"), &ColorPicker::get_picker_shape);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_pick_color", "get_pick_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "edit_alpha"), "set_edit_alpha", "is_editing_alpha");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_mode", PROPERTY_HINT_ENUM, "RGB,HSV,RAW,OKHSL"), "set_color_mode", "get_color_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deferred_mode"), "set_deferred_mode", "is_deferred_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "picker_shape", PROPERTY_HINT_ENUM, "HSV Rectangle,HSV Rectangle Wheel,VHS Circle,OKHSL Circle,None"), "set_picker_shape", "get_picker_shape");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_add_swatches"), "set_can_add_swatches", "are_swatches_enabled");
	ADD_GROUP("Customization", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sampler_visible"), "set_sampler_visible", "is_sampler_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "color_modes_visible"), "set_modes_visible", "are_modes_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sliders_visible"), "set_sliders_visible", "are_sliders_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hex_visible"), "set_hex_visible", "is_hex_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "presets_visible"), "set_presets_visible", "are_presets_visible");

	ADD_SIGNAL(MethodInfo("color_changed", PropertyInfo(Variant::COLOR, "color")));
	ADD_SIGNAL(MethodInfo("preset_added", PropertyInfo(Variant::COLOR, "color")));
	ADD_SIGNAL(MethodInfo("preset_removed", PropertyInfo(Variant::COLOR, "color")));

	BIND_ENUM_CONSTANT(MODE_RGB);
	BIND_ENUM_CONSTANT(MODE_HSV);
	BIND_ENUM_CONSTANT(MODE_RAW);
	BIND_ENUM_CONSTANT(MODE_OKHSL);

	BIND_ENUM_CONSTANT(SHAPE_HSV_RECTANGLE);
	BIND_ENUM_CONSTANT(SHAPE_HSV_WHEEL);
	BIND_ENUM_CONSTANT(SHAPE_VHS_CIRCLE);
	BIND_ENUM_CONSTANT(SHAPE_OKHSL_CIRCLE);
	BIND_ENUM_CONSTANT(SHAPE_NONE);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, ColorPicker, content_margin, "margin");
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, ColorPicker, label_width);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, ColorPicker, sv_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, ColorPicker, sv_height);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, ColorPicker, h_width);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, ColorPicker, center_slider_grabbers);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, screen_picker);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, expanded_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, folded_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, add_preset);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, shape_rect);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, shape_rect_wheel);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, shape_circle);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, bar_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, sample_bg);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, sample_revert);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, overbright_indicator);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, picker_cursor);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, color_hue);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPicker, color_okhsl_hue);

	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, ColorPicker, mode_button_normal, "tab_unselected", "TabContainer");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, ColorPicker, mode_button_pressed, "tab_selected", "TabContainer");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, ColorPicker, mode_button_hover, "tab_selected", "TabContainer");
}

ColorPicker::ColorPicker() {
	internal_margin = memnew(MarginContainer);
	add_child(internal_margin, false, INTERNAL_MODE_FRONT);

	VBoxContainer *real_vbox = memnew(VBoxContainer);
	internal_margin->add_child(real_vbox);

	HBoxContainer *hb_edit = memnew(HBoxContainer);
	real_vbox->add_child(hb_edit);
	hb_edit->set_v_size_flags(SIZE_SHRINK_BEGIN);

	uv_edit = memnew(Control);
	hb_edit->add_child(uv_edit);
	uv_edit->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_uv_input).bind(uv_edit));
	uv_edit->set_mouse_filter(MOUSE_FILTER_PASS);
	uv_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	uv_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	uv_edit->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_hsv_draw).bind(0, uv_edit));

	sample_hbc = memnew(HBoxContainer);
	real_vbox->add_child(sample_hbc);

	btn_pick = memnew(Button);
	sample_hbc->add_child(btn_pick);

	sample = memnew(TextureRect);
	sample_hbc->add_child(sample);
	sample->set_h_size_flags(SIZE_EXPAND_FILL);
	sample->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_sample_input));
	sample->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_sample_draw));

	btn_shape = memnew(MenuButton);
	btn_shape->set_flat(false);
	sample_hbc->add_child(btn_shape);
	btn_shape->set_toggle_mode(true);
	btn_shape->set_tooltip_text(ETR("Select a picker shape."));

	current_shape = SHAPE_HSV_RECTANGLE;

	shape_popup = btn_shape->get_popup();
	shape_popup->add_radio_check_item("HSV Rectangle", SHAPE_HSV_RECTANGLE);
	shape_popup->add_radio_check_item("HSV Wheel", SHAPE_HSV_WHEEL);
	shape_popup->add_radio_check_item("VHS Circle", SHAPE_VHS_CIRCLE);
	shape_popup->add_radio_check_item("OKHSL Circle", SHAPE_OKHSL_CIRCLE);
	shape_popup->set_item_checked(current_shape, true);
	shape_popup->connect(SceneStringName(id_pressed), callable_mp(this, &ColorPicker::set_picker_shape));

	add_mode(new ColorModeRGB(this));
	add_mode(new ColorModeHSV(this));
	add_mode(new ColorModeRAW(this));
	add_mode(new ColorModeOKHSL(this));

	mode_hbc = memnew(HBoxContainer);
	real_vbox->add_child(mode_hbc);

	mode_group.instantiate();

	for (int i = 0; i < MODE_BUTTON_COUNT; i++) {
		mode_btns[i] = memnew(Button);
		mode_hbc->add_child(mode_btns[i]);
		mode_btns[i]->set_focus_mode(FOCUS_NONE);
		mode_btns[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		mode_btns[i]->set_toggle_mode(true);
		mode_btns[i]->set_text(modes[i]->get_name());
		mode_btns[i]->set_button_group(mode_group);
		mode_btns[i]->connect(SceneStringName(pressed), callable_mp(this, &ColorPicker::set_color_mode).bind((ColorModeType)i));
	}
	mode_btns[0]->set_pressed(true);

	btn_mode = memnew(MenuButton);
	btn_mode->set_text("...");
	btn_mode->set_flat(false);
	mode_hbc->add_child(btn_mode);
	btn_mode->set_toggle_mode(true);
	btn_mode->set_tooltip_text(ETR("Select a picker mode."));

	current_mode = MODE_RGB;

	mode_popup = btn_mode->get_popup();
	for (int i = 0; i < modes.size(); i++) {
		mode_popup->add_radio_check_item(modes[i]->get_name(), i);
	}
	mode_popup->add_separator();
	mode_popup->add_check_item(ETR("Colorized Sliders"), MODE_MAX);
	mode_popup->set_item_checked(current_mode, true);
	mode_popup->set_item_checked(MODE_MAX + 1, true);
	mode_popup->connect(SceneStringName(id_pressed), callable_mp(this, &ColorPicker::_set_mode_popup_value));
	VBoxContainer *vbl = memnew(VBoxContainer);
	real_vbox->add_child(vbl);

	VBoxContainer *vbr = memnew(VBoxContainer);

	real_vbox->add_child(vbr);
	vbr->set_h_size_flags(SIZE_EXPAND_FILL);

	slider_gc = memnew(GridContainer);

	vbr->add_child(slider_gc);
	slider_gc->set_h_size_flags(SIZE_EXPAND_FILL);
	slider_gc->set_columns(3);

	for (int i = 0; i < SLIDER_COUNT + 1; i++) {
		create_slider(slider_gc, i);
	}

	alpha_label->set_text("A");

	hex_hbc = memnew(HBoxContainer);
	hex_hbc->set_alignment(ALIGNMENT_BEGIN);
	vbr->add_child(hex_hbc);

	hex_hbc->add_child(memnew(Label(ETR("Hex"))));

	text_type = memnew(Button);
	hex_hbc->add_child(text_type);
	text_type->set_text("#");
	text_type->set_tooltip_text(RTR("Switch between hexadecimal and code values."));
	if (Engine::get_singleton()->is_editor_hint()) {
		text_type->connect(SceneStringName(pressed), callable_mp(this, &ColorPicker::_text_type_toggled));
	} else {
		text_type->set_flat(true);
		text_type->set_mouse_filter(MOUSE_FILTER_IGNORE);
	}

	c_text = memnew(LineEdit);
	hex_hbc->add_child(c_text);
	c_text->set_h_size_flags(SIZE_EXPAND_FILL);
	c_text->set_select_all_on_focus(true);
	c_text->set_tooltip_text(ETR("Enter a hex code (\"#ff0000\") or named color (\"red\")."));
	c_text->set_placeholder(ETR("Hex code or named color"));
	c_text->connect("text_submitted", callable_mp(this, &ColorPicker::_html_submitted));
	c_text->connect(SceneStringName(text_changed), callable_mp(this, &ColorPicker::_text_changed));
	c_text->connect(SceneStringName(focus_exited), callable_mp(this, &ColorPicker::_html_focus_exit));

	wheel_edit = memnew(AspectRatioContainer);
	wheel_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	wheel_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	hb_edit->add_child(wheel_edit);

	wheel_mat.instantiate();
	wheel_mat->set_shader(wheel_shader);
	circle_mat.instantiate();
	circle_mat->set_shader(circle_shader);

	wheel_margin = memnew(MarginContainer);
	wheel_margin->add_theme_constant_override("margin_bottom", 8);
	wheel_edit->add_child(wheel_margin);

	wheel = memnew(Control);
	wheel_margin->add_child(wheel);
	wheel->set_mouse_filter(MOUSE_FILTER_PASS);
	wheel->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_hsv_draw).bind(2, wheel));

	wheel_uv = memnew(Control);
	wheel_margin->add_child(wheel_uv);
	wheel_uv->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_uv_input).bind(wheel_uv));
	wheel_uv->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_hsv_draw).bind(0, wheel_uv));

	w_edit = memnew(Control);
	hb_edit->add_child(w_edit);
	w_edit->set_h_size_flags(SIZE_FILL);
	w_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	w_edit->connect(SceneStringName(gui_input), callable_mp(this, &ColorPicker::_w_input));
	w_edit->connect(SceneStringName(draw), callable_mp(this, &ColorPicker::_hsv_draw).bind(1, w_edit));

	_update_controls();
	updating = false;

	preset_container = memnew(GridContainer);
	preset_container->set_h_size_flags(SIZE_EXPAND_FILL);
	preset_container->set_columns(PRESET_COLUMN_COUNT);
	preset_container->hide();

	preset_group.instantiate();

	btn_preset = memnew(Button(ETR("Swatches")));
	btn_preset->set_flat(true);
	btn_preset->set_toggle_mode(true);
	btn_preset->set_focus_mode(FOCUS_NONE);
	btn_preset->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	btn_preset->connect(SceneStringName(toggled), callable_mp(this, &ColorPicker::_show_hide_preset).bind(btn_preset, preset_container));
	real_vbox->add_child(btn_preset);

	real_vbox->add_child(preset_container);

	recent_preset_hbc = memnew(HBoxContainer);
	recent_preset_hbc->set_v_size_flags(SIZE_SHRINK_BEGIN);
	recent_preset_hbc->hide();

	recent_preset_group.instantiate();

	btn_recent_preset = memnew(Button(ETR("Recent Colors")));
	btn_recent_preset->set_flat(true);
	btn_recent_preset->set_toggle_mode(true);
	btn_recent_preset->set_focus_mode(FOCUS_NONE);
	btn_recent_preset->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	btn_recent_preset->connect(SceneStringName(toggled), callable_mp(this, &ColorPicker::_show_hide_preset).bind(btn_recent_preset, recent_preset_hbc));
	real_vbox->add_child(btn_recent_preset);

	real_vbox->add_child(recent_preset_hbc);

	set_pick_color(Color(1, 1, 1));

	btn_add_preset = memnew(Button);
	btn_add_preset->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	btn_add_preset->set_tooltip_text(ETR("Add current color as a preset."));
	btn_add_preset->connect(SceneStringName(pressed), callable_mp(this, &ColorPicker::_add_preset_pressed));
	preset_container->add_child(btn_add_preset);
}

ColorPicker::~ColorPicker() {
	for (int i = 0; i < modes.size(); i++) {
		delete modes[i];
	}
}

/////////////////

void ColorPickerPopupPanel::_input_from_window(const Ref<InputEvent> &p_event) {
	if (p_event->is_action_pressed(SNAME("ui_accept"), false, true)) {
		_close_pressed();
	}
	PopupPanel::_input_from_window(p_event);
}

/////////////////

void ColorPickerButton::_about_to_popup() {
	set_pressed(true);
	if (picker) {
		picker->set_old_color(color);
	}
}

void ColorPickerButton::_color_changed(const Color &p_color) {
	color = p_color;
	queue_redraw();
	emit_signal(SNAME("color_changed"), color);
}

void ColorPickerButton::_modal_closed() {
	if (Input::get_singleton()->is_action_just_pressed(SNAME("ui_cancel"))) {
		set_pick_color(picker->get_old_color());
		emit_signal(SNAME("color_changed"), color);
	}
	emit_signal(SNAME("popup_closed"));
	set_pressed(false);
}

void ColorPickerButton::pressed() {
	_update_picker();

	Size2 minsize = popup->get_contents_minimum_size();
	float viewport_height = get_viewport_rect().size.y;

	popup->reset_size();
	picker->_update_presets();
	picker->_update_recent_presets();

	// Determine in which direction to show the popup. By default popup horizontally centered below the button.
	// But if the popup doesn't fit below and the button is in the bottom half of the viewport, show above.
	bool show_above = false;
	if (get_global_position().y + get_size().y + minsize.y > viewport_height && get_global_position().y * 2 + get_size().y > viewport_height) {
		show_above = true;
	}

	float h_offset = (get_size().x - minsize.x) / 2;
	float v_offset = show_above ? -minsize.y : get_size().y;
	popup->set_position(get_screen_position() + Vector2(h_offset, v_offset));
	popup->popup();
	if (DisplayServer::get_singleton()->has_hardware_keyboard()) {
		picker->set_focus_on_line_edit();
	}
}

void ColorPickerButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			const Rect2 r = Rect2(theme_cache.normal_style->get_offset(), get_size() - theme_cache.normal_style->get_minimum_size());
			draw_texture_rect(theme_cache.background_icon, r, true);
			draw_rect(r, color);

			if (color.r > 1 || color.g > 1 || color.b > 1) {
				// Draw an indicator to denote that the color is "overbright" and can't be displayed accurately in the preview
				draw_texture(theme_cache.overbright_indicator, theme_cache.normal_style->get_offset());
			}
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			if (popup) {
				popup->hide();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (popup && !is_visible_in_tree()) {
				popup->hide();
			}
		} break;
	}
}

void ColorPickerButton::set_pick_color(const Color &p_color) {
	if (color == p_color) {
		return;
	}
	color = p_color;
	if (picker) {
		picker->set_pick_color(p_color);
	}

	queue_redraw();
}

Color ColorPickerButton::get_pick_color() const {
	return color;
}

void ColorPickerButton::set_edit_alpha(bool p_show) {
	if (edit_alpha == p_show) {
		return;
	}
	edit_alpha = p_show;
	if (picker) {
		picker->set_edit_alpha(p_show);
	}
}

bool ColorPickerButton::is_editing_alpha() const {
	return edit_alpha;
}

ColorPicker *ColorPickerButton::get_picker() {
	_update_picker();
	return picker;
}

PopupPanel *ColorPickerButton::get_popup() {
	_update_picker();
	return popup;
}

void ColorPickerButton::_update_picker() {
	if (!picker) {
		popup = memnew(ColorPickerPopupPanel);
		popup->set_wrap_controls(true);
		picker = memnew(ColorPicker);
		picker->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		popup->add_child(picker);
		add_child(popup, false, INTERNAL_MODE_FRONT);
		picker->connect("color_changed", callable_mp(this, &ColorPickerButton::_color_changed));
		popup->connect("about_to_popup", callable_mp(this, &ColorPickerButton::_about_to_popup));
		popup->connect("popup_hide", callable_mp(this, &ColorPickerButton::_modal_closed));
		picker->connect(SceneStringName(minimum_size_changed), callable_mp((Window *)popup, &Window::reset_size));
		picker->set_pick_color(color);
		picker->set_edit_alpha(edit_alpha);
		picker->set_display_old_color(true);
		emit_signal(SNAME("picker_created"));
	}
}

void ColorPickerButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pick_color", "color"), &ColorPickerButton::set_pick_color);
	ClassDB::bind_method(D_METHOD("get_pick_color"), &ColorPickerButton::get_pick_color);
	ClassDB::bind_method(D_METHOD("get_picker"), &ColorPickerButton::get_picker);
	ClassDB::bind_method(D_METHOD("get_popup"), &ColorPickerButton::get_popup);
	ClassDB::bind_method(D_METHOD("set_edit_alpha", "show"), &ColorPickerButton::set_edit_alpha);
	ClassDB::bind_method(D_METHOD("is_editing_alpha"), &ColorPickerButton::is_editing_alpha);
	ClassDB::bind_method(D_METHOD("_about_to_popup"), &ColorPickerButton::_about_to_popup);

	ADD_SIGNAL(MethodInfo("color_changed", PropertyInfo(Variant::COLOR, "color")));
	ADD_SIGNAL(MethodInfo("popup_closed"));
	ADD_SIGNAL(MethodInfo("picker_created"));
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_pick_color", "get_pick_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "edit_alpha"), "set_edit_alpha", "is_editing_alpha");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ColorPickerButton, normal_style, "normal");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ColorPickerButton, background_icon, "bg");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_ICON, ColorPickerButton, overbright_indicator, "overbright_indicator", "ColorPicker");
}

ColorPickerButton::ColorPickerButton(const String &p_text) :
		Button(p_text) {
	set_toggle_mode(true);
}

/////////////////

void ColorPresetButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			const Rect2 r = Rect2(Point2(0, 0), get_size());
			Ref<StyleBox> sb_raw = theme_cache.foreground_style->duplicate();
			Ref<StyleBoxFlat> sb_flat = sb_raw;
			Ref<StyleBoxTexture> sb_texture = sb_raw;

			if (sb_flat.is_valid()) {
				sb_flat->set_border_width(SIDE_BOTTOM, 2);
				if (get_draw_mode() == DRAW_PRESSED || get_draw_mode() == DRAW_HOVER_PRESSED) {
					sb_flat->set_border_color(Color(1, 1, 1, 1));
				} else {
					sb_flat->set_border_color(Color(0, 0, 0, 1));
				}

				if (preset_color.a < 1) {
					// Draw a background pattern when the color is transparent.
					sb_flat->set_bg_color(Color(1, 1, 1));
					sb_flat->draw(get_canvas_item(), r);

					Rect2 bg_texture_rect = r.grow_side(SIDE_LEFT, -sb_flat->get_margin(SIDE_LEFT));
					bg_texture_rect = bg_texture_rect.grow_side(SIDE_RIGHT, -sb_flat->get_margin(SIDE_RIGHT));
					bg_texture_rect = bg_texture_rect.grow_side(SIDE_TOP, -sb_flat->get_margin(SIDE_TOP));
					bg_texture_rect = bg_texture_rect.grow_side(SIDE_BOTTOM, -sb_flat->get_margin(SIDE_BOTTOM));

					draw_texture_rect(theme_cache.background_icon, bg_texture_rect, true);
					sb_flat->set_bg_color(preset_color);
				}
				sb_flat->set_bg_color(preset_color);
				sb_flat->draw(get_canvas_item(), r);
			} else if (sb_texture.is_valid()) {
				if (preset_color.a < 1) {
					// Draw a background pattern when the color is transparent.
					bool use_tile_texture = (sb_texture->get_h_axis_stretch_mode() == StyleBoxTexture::AxisStretchMode::AXIS_STRETCH_MODE_TILE) || (sb_texture->get_h_axis_stretch_mode() == StyleBoxTexture::AxisStretchMode::AXIS_STRETCH_MODE_TILE_FIT);
					draw_texture_rect(theme_cache.background_icon, r, use_tile_texture);
				}
				sb_texture->set_modulate(preset_color);
				sb_texture->draw(get_canvas_item(), r);
			} else {
				WARN_PRINT("Unsupported StyleBox used for ColorPresetButton. Use StyleBoxFlat or StyleBoxTexture instead.");
			}
			if (preset_color.r > 1 || preset_color.g > 1 || preset_color.b > 1) {
				// Draw an indicator to denote that the color is "overbright" and can't be displayed accurately in the preview
				draw_texture(theme_cache.overbright_indicator, Vector2(0, 0));
			}

		} break;
	}
}

void ColorPresetButton::set_preset_color(const Color &p_color) {
	preset_color = p_color;
}

Color ColorPresetButton::get_preset_color() const {
	return preset_color;
}

void ColorPresetButton::_bind_methods() {
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ColorPresetButton, foreground_style, "preset_fg");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ColorPresetButton, background_icon, "preset_bg");
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, ColorPresetButton, overbright_indicator);
}

ColorPresetButton::ColorPresetButton(Color p_color, int p_size) {
	preset_color = p_color;
	set_toggle_mode(true);
	set_custom_minimum_size(Size2(p_size, p_size));
}

ColorPresetButton::~ColorPresetButton() {
}
