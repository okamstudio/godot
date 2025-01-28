/**************************************************************************/
/*  color_channel_selector.cpp                                            */
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

#include "color_channel_selector.h"

#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/style_box_flat.h"

ColorChannelSelector::ColorChannelSelector() {
	toggle_button = memnew(Button);
	toggle_button->set_flat(true);
	toggle_button->set_toggle_mode(true);
	toggle_button->connect(SceneStringName(toggled), callable_mp(this, &ColorChannelSelector::on_toggled));
	toggle_button->add_theme_style_override("focus", memnew(StyleBoxEmpty));
	add_child(toggle_button);

	panel = memnew(PanelContainer);
	panel->hide();

	HBoxContainer *container = memnew(HBoxContainer);
	container->add_theme_constant_override("separation", 0);

	create_button(0, "R", container);
	create_button(1, "G", container);
	create_button(2, "B", container);
	create_button(3, "A", container);

	// Use a bit of transparency to be less distracting.
	set_modulate(Color(1, 1, 1, 0.7));

	panel->add_child(container);

	add_child(panel);
}

void ColorChannelSelector::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		// PanelContainer's background is invisible in the editor. We need a background.
		// And we need this in turn because buttons don't look good without background (for example, hover is transparent).
		Ref<StyleBox> bg_style = get_theme_stylebox(SceneStringName(panel), "TabContainer");
		ERR_FAIL_COND(bg_style.is_null());
		bg_style = bg_style->duplicate();
		// The default content margin makes the widget become a bit too large. It should be like mini-toolbar.
		const float editor_scale = EditorScale::get_scale();
		bg_style->set_content_margin(SIDE_LEFT, 1.0f * editor_scale);
		bg_style->set_content_margin(SIDE_RIGHT, 1.0f * editor_scale);
		bg_style->set_content_margin(SIDE_TOP, 1.0f * editor_scale);
		bg_style->set_content_margin(SIDE_BOTTOM, 1.0f * editor_scale);
		panel->add_theme_style_override(SceneStringName(panel), bg_style);

		Ref<Texture2D> icon = get_editor_theme_icon(SNAME("TexturePreviewChannels"));
		toggle_button->set_button_icon(icon);
	}
}

void ColorChannelSelector::set_available_channels_mask(uint32_t p_mask) {
	for (unsigned int i = 0; i < CHANNEL_COUNT; ++i) {
		const bool available = (p_mask & (1u << i)) != 0;
		Button *button = channel_buttons[i];
		button->set_visible(available);
	}
}

void ColorChannelSelector::on_channel_button_toggled(bool p_unused_pressed) {
	emit_signal("selected_channels_changed");
}

uint32_t ColorChannelSelector::get_selected_channels_mask() const {
	uint32_t mask = 0;
	for (unsigned int i = 0; i < CHANNEL_COUNT; ++i) {
		Button *button = channel_buttons[i];
		if (button->is_visible() && channel_buttons[i]->is_pressed()) {
			mask |= (1 << i);
		}
	}
	return mask;
}

// Helper
Vector4 ColorChannelSelector::get_selected_channel_factors() const {
	Vector4 channel_factors;
	const uint32_t mask = get_selected_channels_mask();
	for (unsigned int i = 0; i < 4; ++i) {
		if ((mask & (1 << i)) != 0) {
			channel_factors[i] = 1;
		}
	}
	return channel_factors;
}

void ColorChannelSelector::create_button(unsigned int p_channel_index, const String &p_text, Control *p_parent) {
	ERR_FAIL_COND(p_channel_index >= CHANNEL_COUNT);
	ERR_FAIL_COND(channel_buttons[p_channel_index] != nullptr);
	Button *button = memnew(Button);
	button->set_text(p_text);
	button->set_toggle_mode(true);
	button->set_pressed(true);

	// Don't show focus, it stands out too much and remains visible which can be confusing.
	button->add_theme_style_override("focus", memnew(StyleBoxEmpty));

	// Make it look similar to toolbar buttons.
	button->set_theme_type_variation(SceneStringName(FlatButton));

	button->connect(SceneStringName(toggled), callable_mp(this, &ColorChannelSelector::on_channel_button_toggled));
	p_parent->add_child(button);
	channel_buttons[p_channel_index] = button;
}

void ColorChannelSelector::on_toggled(bool p_pressed) {
	panel->set_visible(p_pressed);
}

void ColorChannelSelector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("selected_channels_changed"));
}
