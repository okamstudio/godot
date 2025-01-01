/**************************************************************************/
/*  particle_process_material_editor_plugin.cpp                           */
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

#include "particle_process_material_editor_plugin.h"

#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/resources/particle_process_material.h"

void ParticleProcessMaterialMinMaxPropertyEditor::_update_sizing() {
	edit_size = range_edit_widget->get_size();
	margin = Vector2(range_slider_left_icon->get_width(), (edit_size.y - range_slider_left_icon->get_height()) * 0.5);
	usable_area = edit_size - margin * 2;
}

void ParticleProcessMaterialMinMaxPropertyEditor::_range_edit_draw() {
	ERR_FAIL_COND(range_slider_left_icon.is_null());
	ERR_FAIL_COND(range_slider_right_icon.is_null());
	_update_sizing();

	bool widget_active = mouse_inside || drag != Drag::NONE;

	// FIXME: Need to offset by 1 due to some outline bug.
	range_edit_widget->draw_rect(Rect2(margin + Vector2(1, 1), usable_area - Vector2(1, 1)), widget_active ? background_color.lerp(normal_color, 0.3) : background_color, false, 1.0);

	Color draw_color;

	if (widget_active) {
		float icon_offset = _get_left_offset() - range_slider_left_icon->get_width() - 1;

		if (drag == Drag::LEFT || drag == Drag::SCALE) {
			draw_color = drag_color;
		} else if (hover == Hover::LEFT) {
			draw_color = hovered_color;
		} else {
			draw_color = normal_color;
		}
		range_edit_widget->draw_texture(range_slider_left_icon, Vector2(icon_offset, margin.y), draw_color);

		icon_offset = _get_right_offset();

		if (drag == Drag::RIGHT || drag == Drag::SCALE) {
			draw_color = drag_color;
		} else if (hover == Hover::RIGHT) {
			draw_color = hovered_color;
		} else {
			draw_color = normal_color;
		}
		range_edit_widget->draw_texture(range_slider_right_icon, Vector2(icon_offset, margin.y), draw_color);
	}

	if (drag == Drag::MIDDLE || drag == Drag::SCALE) {
		draw_color = drag_color;
	} else if (hover == Hover::MIDDLE) {
		draw_color = hovered_color;
	} else {
		draw_color = normal_color;
	}
	range_edit_widget->draw_rect(_get_middle_rect(), draw_color);

	Rect2 midpoint_rect(Vector2(margin.x + usable_area.x * (_get_min_ratio() + _get_max_ratio()) * 0.5 - 1, margin.y + 2),
			Vector2(2, usable_area.y - 4));

	range_edit_widget->draw_rect(midpoint_rect, midpoint_color);
}

void ParticleProcessMaterialMinMaxPropertyEditor::_range_edit_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	Ref<InputEventMouseMotion> mm = p_event;

	// Prevent unnecessary computations.
	if ((mb.is_null() || mb->get_button_index() != MouseButton::LEFT) && (mm.is_null())) {
		return;
	}

	ERR_FAIL_COND(range_slider_left_icon.is_null());
	ERR_FAIL_COND(range_slider_right_icon.is_null());
	_update_sizing();

	if (mb.is_valid()) {
		const Drag prev_drag = drag;

		if (mb->is_pressed()) {
			if (mb->is_shift_pressed()) {
				drag = Drag::SCALE;
				drag_from_value = (max_range->get_value() - min_range->get_value()) * 0.5;
				drag_midpoint = (max_range->get_value() + min_range->get_value()) * 0.5;
			} else if (hover == Hover::LEFT) {
				drag = Drag::LEFT;
				drag_from_value = min_range->get_value();
			} else if (hover == Hover::RIGHT) {
				drag = Drag::RIGHT;
				drag_from_value = max_range->get_value();
			} else {
				drag = Drag::MIDDLE;
				drag_from_value = min_range->get_value();
			}
			drag_origin = mb->get_position().x;
		} else {
			drag = Drag::NONE;
		}

		if (drag != prev_drag) {
			range_edit_widget->queue_redraw();
		}
	}

	float property_length = property_range.y - property_range.x;
	if (mm.is_valid()) {
		switch (drag) {
			case Drag::NONE: {
				const Hover prev_hover = hover;
				float left_icon_offset = _get_left_offset() - range_slider_left_icon->get_width() - 1;

				if (Rect2(Vector2(left_icon_offset, 0), range_slider_left_icon->get_size()).has_point(mm->get_position())) {
					hover = Hover::LEFT;
				} else if (Rect2(Vector2(_get_right_offset(), 0), range_slider_right_icon->get_size()).has_point(mm->get_position())) {
					hover = Hover::RIGHT;
				} else if (_get_middle_rect().has_point(mm->get_position())) {
					hover = Hover::MIDDLE;
				} else {
					hover = Hover::NONE;
				}

				if (hover != prev_hover) {
					range_edit_widget->queue_redraw();
				}
			} break;

			case Drag::LEFT:
			case Drag::RIGHT: {
				float new_value = drag_from_value + (mm->get_position().x - drag_origin) / usable_area.x * property_length;
				if (drag == Drag::LEFT) {
					new_value = MIN(new_value, max_range->get_value());
					_set_clamped_values(new_value, max_range->get_value());
				} else {
					new_value = MAX(new_value, min_range->get_value());
					_set_clamped_values(min_range->get_value(), new_value);
				}
			} break;

			case Drag::MIDDLE: {
				float delta = (mm->get_position().x - drag_origin) / usable_area.x * property_length;
				float diff = max_range->get_value() - min_range->get_value();
				delta = CLAMP(drag_from_value + delta, property_range.x, property_range.y - diff) - drag_from_value;
				_set_clamped_values(drag_from_value + delta, drag_from_value + delta + diff);
			} break;

			case Drag::SCALE: {
				float delta = (mm->get_position().x - drag_origin) / usable_area.x * property_length + drag_from_value;
				_set_clamped_values(MIN(drag_midpoint, drag_midpoint - delta), MAX(drag_midpoint, drag_midpoint + delta));
			} break;
		}
	}
}

void ParticleProcessMaterialMinMaxPropertyEditor::_set_mouse_inside(bool p_inside) {
	mouse_inside = p_inside;
	if (!p_inside) {
		hover = Hover::NONE;
	}
	range_edit_widget->queue_redraw();
}

float ParticleProcessMaterialMinMaxPropertyEditor::_get_min_ratio() const {
	return (min_range->get_value() - property_range.x) / (property_range.y - property_range.x);
}

float ParticleProcessMaterialMinMaxPropertyEditor::_get_max_ratio() const {
	return (max_range->get_value() - property_range.x) / (property_range.y - property_range.x);
}

float ParticleProcessMaterialMinMaxPropertyEditor::_get_left_offset() const {
	return margin.x + usable_area.x * _get_min_ratio();
}

float ParticleProcessMaterialMinMaxPropertyEditor::_get_right_offset() const {
	return margin.x + usable_area.x * _get_max_ratio();
}

Rect2 ParticleProcessMaterialMinMaxPropertyEditor::_get_middle_rect() const {
	if (Math::is_equal_approx(min_range->get_value(), max_range->get_value())) {
		return Rect2();
	}

	return Rect2(
			Vector2(_get_left_offset() - 1, margin.y),
			Vector2(usable_area.x * (_get_max_ratio() - _get_min_ratio()) + 1, usable_area.y));
}

void ParticleProcessMaterialMinMaxPropertyEditor::_set_clamped_values(float p_min, float p_max) {
	// This is required for editing widget in case the properties have or_less or or_greater hint.
	min_range->set_value(MAX(p_min, property_range.x));
	max_range->set_value(MIN(p_max, property_range.y));
	_update_slider_values();
	_sync_property();
}

void ParticleProcessMaterialMinMaxPropertyEditor::_sync_property() {
	const Vector2 value = Vector2(min_range->get_value(), max_range->get_value());
	emit_changed(get_edited_property(), value, "", true);
	range_edit_widget->queue_redraw();
}

void ParticleProcessMaterialMinMaxPropertyEditor::_update_mode() {
	max_edit->set_read_only(false);

	switch (slider_mode) {
		case Mode::RANGE: {
			min_edit->set_label("min");
			max_edit->set_label("max");
			max_edit->set_block_signals(true);
			max_edit->set_min(max_range->get_min());
			max_edit->set_max(max_range->get_max());
			max_edit->set_block_signals(false);

			min_edit->set_allow_lesser(min_range->is_lesser_allowed());
			min_edit->set_allow_greater(min_range->is_greater_allowed());
			max_edit->set_allow_lesser(max_range->is_lesser_allowed());
			max_edit->set_allow_greater(max_range->is_greater_allowed());
		} break;

		case Mode::MIDPOINT: {
			min_edit->set_label("val");
			max_edit->set_label(U"±");
			max_edit->set_block_signals(true);
			max_edit->set_min(0);
			max_edit->set_block_signals(false);

			min_edit->set_allow_lesser(min_range->is_lesser_allowed());
			min_edit->set_allow_greater(max_range->is_greater_allowed());
			max_edit->set_allow_lesser(false);
			max_edit->set_allow_greater(min_range->is_lesser_allowed() && max_range->is_greater_allowed());
		} break;
	}
	_update_slider_values();
}

void ParticleProcessMaterialMinMaxPropertyEditor::_toggle_mode(bool p_edit_mode) {
	slider_mode = p_edit_mode ? Mode::MIDPOINT : Mode::RANGE;
	EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "particle_spin_mode", int(slider_mode));
	_update_mode();
}

void ParticleProcessMaterialMinMaxPropertyEditor::_update_slider_values() {
	switch (slider_mode) {
		case Mode::RANGE: {
			min_edit->set_value_no_signal(min_range->get_value());
			max_edit->set_value_no_signal(max_range->get_value());
		} break;

		case Mode::MIDPOINT: {
			min_edit->set_value_no_signal((min_range->get_value() + max_range->get_value()) * 0.5);
			max_edit->set_value_no_signal((max_range->get_value() - min_range->get_value()) * 0.5);

			max_edit->set_block_signals(true);
			max_edit->set_max(_get_max_spread());
			max_edit->set_read_only(max_edit->get_max() == 0);
			max_edit->set_block_signals(false);
		} break;
	}
}

void ParticleProcessMaterialMinMaxPropertyEditor::_sync_sliders(float, const EditorSpinSlider *p_changed_slider) {
	switch (slider_mode) {
		case Mode::RANGE: {
			if (p_changed_slider == max_edit) {
				min_edit->set_value_no_signal(MIN(min_edit->get_value(), max_edit->get_value()));
			}
			min_range->set_value(min_edit->get_value());
			if (p_changed_slider == min_edit) {
				max_edit->set_value_no_signal(MAX(min_edit->get_value(), max_edit->get_value()));
			}
			max_range->set_value(max_edit->get_value());
			_sync_property();
		} break;

		case Mode::MIDPOINT: {
			if (p_changed_slider == min_edit) {
				max_edit->set_block_signals(true); // If max changes, value may change.
				max_edit->set_max(_get_max_spread());
				max_edit->set_read_only(max_edit->get_max() == 0);
				max_edit->set_block_signals(false);
			}
			min_range->set_value(min_edit->get_value() - max_edit->get_value());
			max_range->set_value(min_edit->get_value() + max_edit->get_value());
			_sync_property();
		} break;
	}

	property_range.x = MIN(min_range->get_value(), min_range->get_min());
	property_range.y = MAX(max_range->get_value(), max_range->get_max());
}

float ParticleProcessMaterialMinMaxPropertyEditor::_get_max_spread() const {
	float max_spread = max_range->get_max() - min_range->get_min();

	if (max_edit->is_greater_allowed()) {
		return max_spread;
	}

	if (!min_edit->is_lesser_allowed()) {
		max_spread = MIN(max_spread, min_edit->get_value() - min_edit->get_min());
	}

	if (!min_edit->is_greater_allowed()) {
		max_spread = MIN(max_spread, min_edit->get_max() - min_edit->get_value());
	}

	return max_spread;
}

void ParticleProcessMaterialMinMaxPropertyEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			toggle_mode_button->set_button_icon(get_editor_theme_icon(SNAME("Anchor")));
			range_slider_left_icon = get_editor_theme_icon(SNAME("RangeSliderLeft"));
			range_slider_right_icon = get_editor_theme_icon(SNAME("RangeSliderRight"));

			min_edit->add_theme_color_override(SNAME("label_color"), get_theme_color(SNAME("property_color_x"), EditorStringName(Editor)));
			max_edit->add_theme_color_override(SNAME("label_color"), get_theme_color(SNAME("property_color_y"), EditorStringName(Editor)));

			const bool dark_theme = EditorThemeManager::is_dark_theme();
			const Color accent_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			background_color = dark_theme ? Color(0.3, 0.3, 0.3) : Color(0.7, 0.7, 0.7);
			normal_color = dark_theme ? Color(0.5, 0.5, 0.5) : Color(0.8, 0.8, 0.8);
			hovered_color = dark_theme ? Color(0.8, 0.8, 0.8) : Color(0.6, 0.6, 0.6);
			drag_color = hovered_color.lerp(accent_color, 0.8);
			midpoint_color = dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);

			range_edit_widget->set_custom_minimum_size(Vector2(0, range_slider_left_icon->get_height() + 8));
		} break;
	}
}

void ParticleProcessMaterialMinMaxPropertyEditor::setup(float p_min, float p_max, float p_step, bool p_allow_less, bool p_allow_greater, bool p_degrees) {
	property_range = Vector2(p_min, p_max);

	// Initially all Ranges share properties.
	for (Range *range : Vector<Range *>{ min_range, min_edit, max_range, max_edit }) {
		range->set_min(p_min);
		range->set_max(p_max);
		range->set_step(p_step);
		range->set_allow_lesser(p_allow_less);
		range->set_allow_greater(p_allow_greater);
	}

	if (p_degrees) {
		min_edit->set_suffix(U" \u00B0");
		max_edit->set_suffix(U" \u00B0");
	}
	_update_mode();
}

void ParticleProcessMaterialMinMaxPropertyEditor::update_property() {
	const Vector2 value = get_edited_property_value();
	min_range->set_value(value.x);
	max_range->set_value(value.y);
	_update_slider_values();
	range_edit_widget->queue_redraw();
}

ParticleProcessMaterialMinMaxPropertyEditor::ParticleProcessMaterialMinMaxPropertyEditor() {
	VBoxContainer *content_vb = memnew(VBoxContainer);
	content_vb->add_theme_constant_override(SNAME("separation"), 0);
	add_child(content_vb);

	// Helper Range objects to keep absolute min and max values.
	min_range = memnew(Range);
	min_range->hide();
	add_child(min_range);

	max_range = memnew(Range);
	max_range->hide();
	add_child(max_range);

	// Range edit widget.
	HBoxContainer *hb = memnew(HBoxContainer);
	content_vb->add_child(hb);

	range_edit_widget = memnew(Control);
	range_edit_widget->set_h_size_flags(SIZE_EXPAND_FILL);
	range_edit_widget->set_tooltip_text(TTR("Hold Shift to scale around midpoint instead of moving."));
	hb->add_child(range_edit_widget);
	range_edit_widget->connect(SceneStringName(draw), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_range_edit_draw));
	range_edit_widget->connect(SceneStringName(gui_input), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_range_edit_gui_input));
	range_edit_widget->connect(SceneStringName(mouse_entered), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_set_mouse_inside).bind(true));
	range_edit_widget->connect(SceneStringName(mouse_exited), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_set_mouse_inside).bind(false));

	// Range controls for actual editing. Their min/max may depend on editing mode.
	hb = memnew(HBoxContainer);
	content_vb->add_child(hb);

	min_edit = memnew(EditorSpinSlider);
	min_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(min_edit);
	min_edit->connect(SceneStringName(value_changed), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_sync_sliders).bind(min_edit));

	max_edit = memnew(EditorSpinSlider);
	max_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(max_edit);
	max_edit->connect(SceneStringName(value_changed), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_sync_sliders).bind(max_edit));

	toggle_mode_button = memnew(Button);
	toggle_mode_button->set_toggle_mode(true);
	toggle_mode_button->set_tooltip_text(TTR("Toggle between minimum/maximum and base value/spread modes."));
	hb->add_child(toggle_mode_button);
	toggle_mode_button->connect(SceneStringName(toggled), callable_mp(this, &ParticleProcessMaterialMinMaxPropertyEditor::_toggle_mode));

	set_bottom_editor(content_vb);
}

bool EditorInspectorParticleProcessMaterialPlugin::can_handle(Object *p_object) {
	return Object::cast_to<ParticleProcessMaterial>(p_object);
}

bool EditorInspectorParticleProcessMaterialPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (!ParticleProcessMaterial::has_min_max_property(p_path)) {
		return false;
	}
	ERR_FAIL_COND_V(p_hint != PROPERTY_HINT_RANGE, false);

	Ref<ParticleProcessMaterial> mat = Ref<ParticleProcessMaterial>(p_object);
	ERR_FAIL_COND_V(mat.is_null(), false);

	PackedStringArray range_hint = p_hint_text.split(",");
	float min = range_hint[0].to_float();
	float max = range_hint[1].to_float();
	float step = range_hint[2].to_float();
	bool allow_less = range_hint.find("or_less", 3) > -1;
	bool allow_greater = range_hint.find("or_greater", 3) > -1;
	bool degrees = range_hint.find("degrees", 3) > -1;

	ParticleProcessMaterialMinMaxPropertyEditor *ed = memnew(ParticleProcessMaterialMinMaxPropertyEditor);
	ed->setup(min, max, step, allow_less, allow_greater, degrees);
	add_property_editor(p_path, ed);

	return true;
}
