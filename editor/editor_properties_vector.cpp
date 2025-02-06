/**************************************************************************/
/*  editor_properties_vector.cpp                                          */
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

#include "editor_properties_vector.h"

#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/texture_button.h"

const String EditorPropertyVectorN::COMPONENT_LABELS[4] = { "x", "y", "z", "w" };

void EditorPropertyVectorN::_set_read_only(bool p_read_only) {
	for (EditorSpinSlider *spin : spin_sliders) {
		spin->set_read_only(p_read_only);
	}
}

void EditorPropertyVectorN::_value_changed(double val, const String &p_name) {
	if (linked->is_pressed()) {
		int changed_component = -1;
		for (int i = 0; i < component_count; i++) {
			if (p_name == COMPONENT_LABELS[i]) {
				changed_component = i;
				break;
			}
		}
		DEV_ASSERT(changed_component >= 0);

		for (int i = 0; i < component_count - 1; i++) {
			int slider_idx = (changed_component + 1 + i) % component_count;
			int ratio_idx = changed_component * (component_count - 1) + i;

			if (ratio[ratio_idx] == 0) {
				continue;
			}

			spin_sliders[slider_idx]->set_value_no_signal(spin_sliders[changed_component]->get_value() * ratio[ratio_idx]);
		}
	}

	Variant v;
	Callable::CallError cerror;
	Variant::construct(vector_type, v, nullptr, 0, cerror);

	for (int i = 0; i < component_count; i++) {
		if (radians_as_degrees) {
			v.set(i, Math::deg_to_rad(spin_sliders[i]->get_value()));
		} else {
			v.set(i, spin_sliders[i]->get_value());
		}
	}
	emit_changed(get_edited_property(), v, linked->is_pressed() ? "" : p_name);
}

void EditorPropertyVectorN::update_property() {
	Variant val = get_edited_property_value();
	for (int i = 0; i < component_count; i++) {
		if (radians_as_degrees) {
			spin_sliders[i]->set_value_no_signal(Math::rad_to_deg((real_t)val.get(i)));
		} else {
			spin_sliders[i]->set_value_no_signal(val.get(i));
		}
	}

	if (!is_grabbed) {
		_update_ratio();
	}
}

void EditorPropertyVectorN::_update_ratio() {
	linked->set_modulate(Color(1, 1, 1, linked->is_pressed() ? 1.0 : 0.5));

	double *ratio_write = ratio.ptrw();
	for (int i = 0; i < ratio.size(); i++) {
		int base_slider_idx = i / (component_count - 1);
		int secondary_slider_idx = ((base_slider_idx + 1) + i % (component_count - 1)) % component_count;

		if (spin_sliders[base_slider_idx]->get_value() != 0) {
			ratio_write[i] = spin_sliders[secondary_slider_idx]->get_value() / spin_sliders[base_slider_idx]->get_value();
		}
	}
}

void EditorPropertyVectorN::_store_link(bool p_linked) {
	if (!get_edited_object()) {
		return;
	}
	const String key = vformat("%s:%s", get_edited_object()->get_class(), get_edited_property());
	EditorSettings::get_singleton()->set_project_metadata("linked_properties", key, p_linked);
}

void EditorPropertyVectorN::_grab_changed(bool p_grab) {
	if (p_grab) {
		_update_ratio();
	}
	is_grabbed = p_grab;
}

void EditorPropertyVectorN::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (linked->is_visible()) {
				if (get_edited_object()) {
					const String key = vformat("%s:%s", get_edited_object()->get_class(), get_edited_property());
					linked->set_pressed_no_signal(EditorSettings::get_singleton()->get_project_metadata("linked_properties", key, true));
					_update_ratio();
				}
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));

			linked->set_texture_normal(get_editor_theme_icon(SNAME("Unlinked")));
			linked->set_texture_pressed(get_editor_theme_icon(SNAME("Instance")));
			linked->set_custom_minimum_size(Size2(icon_size + 8 * EDSCALE, 0));

			const Color *colors = _get_property_colors();
			for (int i = 0; i < component_count; i++) {
				spin_sliders[i]->add_theme_color_override("label_color", colors[i]);
			}
		} break;
	}
}

void EditorPropertyVectorN::setup(double p_min, double p_max, double p_step, bool p_hide_slider, bool p_link, const String &p_suffix, bool p_radians_as_degrees, bool p_is_int) {
	radians_as_degrees = p_radians_as_degrees;

	for (EditorSpinSlider *spin : spin_sliders) {
		spin->set_min(p_min);
		spin->set_max(p_max);
		spin->set_step(p_step);
		spin->set_hide_slider(p_hide_slider);
		spin->set_allow_greater(true);
		spin->set_allow_lesser(true);
		spin->set_suffix(p_suffix);
		spin->set_editing_integer(p_is_int);
	}

	if (!p_link) {
		linked->hide();
	}
}

EditorPropertyVectorN::EditorPropertyVectorN(Variant::Type p_type, bool p_force_wide, bool p_horizontal) {
	vector_type = p_type;
	switch (vector_type) {
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
			component_count = 2;
			break;

		case Variant::VECTOR3:
		case Variant::VECTOR3I:
			component_count = 3;
			break;

		case Variant::VECTOR4:
		case Variant::VECTOR4I:
			component_count = 4;
			break;

		default: // Needed to silence a warning.
			ERR_PRINT("Not a Vector type.");
			break;
	}
	bool horizontal = p_force_wide || p_horizontal;

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_h_size_flags(SIZE_EXPAND_FILL);

	BoxContainer *bc;

	if (p_force_wide) {
		bc = memnew(HBoxContainer);
		hb->add_child(bc);
	} else if (horizontal) {
		bc = memnew(HBoxContainer);
		hb->add_child(bc);
		set_bottom_editor(hb);
	} else {
		bc = memnew(VBoxContainer);
		hb->add_child(bc);
	}
	bc->set_h_size_flags(SIZE_EXPAND_FILL);

	spin_sliders.resize(component_count);
	EditorSpinSlider **spin = spin_sliders.ptrw();

	for (int i = 0; i < component_count; i++) {
		spin[i] = memnew(EditorSpinSlider);
		bc->add_child(spin[i]);
		spin[i]->set_flat(true);
		spin[i]->set_label(String(COMPONENT_LABELS[i]));
		if (horizontal) {
			spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyVectorN::_value_changed).bind(String(COMPONENT_LABELS[i])));
		spin[i]->connect(SNAME("grabbed"), callable_mp(this, &EditorPropertyVectorN::_grab_changed).bind(true));
		spin[i]->connect(SNAME("ungrabbed"), callable_mp(this, &EditorPropertyVectorN::_grab_changed).bind(false));
		add_focusable(spin[i]);
	}

	ratio.resize(component_count * (component_count - 1));
	ratio.fill(1.0);

	linked = memnew(TextureButton);
	linked->set_toggle_mode(true);
	linked->set_stretch_mode(TextureButton::STRETCH_KEEP_CENTERED);
	linked->set_tooltip_text(TTR("Lock/Unlock Component Ratio"));
	linked->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyVectorN::_update_ratio));
	linked->connect(SceneStringName(toggled), callable_mp(this, &EditorPropertyVectorN::_store_link));
	hb->add_child(linked);

	add_child(hb);
	if (!horizontal) {
		set_label_reference(spin_sliders[0]); // Show text and buttons around this.
	}
}

EditorPropertyVector2::EditorPropertyVector2(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR2, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector2_editing")) {}

EditorPropertyVector2i::EditorPropertyVector2i(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR2I, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector2_editing")) {}

EditorPropertyVector3::EditorPropertyVector3(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR3, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector_types_editing")) {}

EditorPropertyVector3i::EditorPropertyVector3i(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR3I, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector_types_editing")) {}

EditorPropertyVector4::EditorPropertyVector4(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR4, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector_types_editing")) {}

EditorPropertyVector4i::EditorPropertyVector4i(bool p_force_wide) :
		EditorPropertyVectorN(Variant::VECTOR4I, p_force_wide, EDITOR_GET("interface/inspector/horizontal_vector_types_editing")) {}
