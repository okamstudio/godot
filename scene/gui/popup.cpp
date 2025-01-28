/**************************************************************************/
/*  popup.cpp                                                             */
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

#include "popup.h"

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#endif
#include "scene/gui/panel.h"
#include "scene/resources/style_box_flat.h"
#include "scene/theme/theme_db.h"

void Popup::_input_from_window(const Ref<InputEvent> &p_event) {
	if (get_flag(FLAG_POPUP) && p_event->is_action_pressed(SNAME("ui_cancel"), false, true)) {
		hide_reason = HIDE_REASON_CANCELED; // ESC pressed, mark as canceled unconditionally.
		_close_pressed();
	}
	Window::_input_from_window(p_event);
}

void Popup::_initialize_visible_parents() {
	if (is_embedded()) {
		visible_parents.clear();

		Window *parent_window = this;
		while (parent_window) {
			parent_window = parent_window->get_parent_visible_window();
			if (parent_window) {
				visible_parents.push_back(parent_window);
				parent_window->connect(SceneStringName(focus_entered), callable_mp(this, &Popup::_parent_focused));
				parent_window->connect(SceneStringName(tree_exited), callable_mp(this, &Popup::_deinitialize_visible_parents));
			}
		}
	}
}

void Popup::_deinitialize_visible_parents() {
	if (is_embedded()) {
		for (Window *parent_window : visible_parents) {
			parent_window->disconnect(SceneStringName(focus_entered), callable_mp(this, &Popup::_parent_focused));
			parent_window->disconnect(SceneStringName(tree_exited), callable_mp(this, &Popup::_deinitialize_visible_parents));
		}

		visible_parents.clear();
	}
}

void Popup::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_in_edited_scene_root()) {
				if (is_visible()) {
					_initialize_visible_parents();
				} else {
					_deinitialize_visible_parents();
					if (hide_reason == HIDE_REASON_NONE) {
						hide_reason = HIDE_REASON_CANCELED;
					}
					emit_signal(SNAME("popup_hide"));
					popped_up = false;
				}
			}
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			if (!is_in_edited_scene_root()) {
				if (has_focus()) {
					popped_up = true;
					hide_reason = HIDE_REASON_NONE;
				}
			}
		} break;

		case NOTIFICATION_UNPARENTED:
		case NOTIFICATION_EXIT_TREE: {
			if (!is_in_edited_scene_root()) {
				_deinitialize_visible_parents();
			}
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			if (!is_in_edited_scene_root()) {
				if (hide_reason == HIDE_REASON_NONE) {
					hide_reason = HIDE_REASON_UNFOCUSED;
				}
				_close_pressed();
			}
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			if (!is_in_edited_scene_root() && get_flag(FLAG_POPUP)) {
				if (hide_reason == HIDE_REASON_NONE) {
					hide_reason = HIDE_REASON_UNFOCUSED;
				}
				_close_pressed();
			}
		} break;
	}
}

void Popup::_parent_focused() {
	if (popped_up && get_flag(FLAG_POPUP)) {
		if (hide_reason == HIDE_REASON_NONE) {
			hide_reason = HIDE_REASON_UNFOCUSED;
		}
		_close_pressed();
	}
}

void Popup::_close_pressed() {
	popped_up = false;

	_deinitialize_visible_parents();

	callable_mp((Window *)this, &Window::hide).call_deferred();
}

void Popup::_post_popup() {
	Window::_post_popup();
	popped_up = true;
}

void Popup::_validate_property(PropertyInfo &p_property) const {
	if (
			p_property.name == "transient" ||
			p_property.name == "exclusive" ||
			p_property.name == "popup_window" ||
			p_property.name == "unfocusable") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

Rect2i Popup::_popup_adjust_rect() const {
	ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
	Rect2i parent_rect = get_usable_parent_rect();

	if (parent_rect == Rect2i()) {
		return Rect2i();
	}

	Rect2i current(get_position(), get_size());

	if (current.position.x + current.size.x > parent_rect.position.x + parent_rect.size.x) {
		current.position.x = parent_rect.position.x + parent_rect.size.x - current.size.x;
	}

	if (current.position.x < parent_rect.position.x) {
		current.position.x = parent_rect.position.x;
	}

	if (current.position.y + current.size.y > parent_rect.position.y + parent_rect.size.y) {
		current.position.y = parent_rect.position.y + parent_rect.size.y - current.size.y;
	}

	if (current.position.y < parent_rect.position.y) {
		current.position.y = parent_rect.position.y;
	}

	if (current.size.y > parent_rect.size.y) {
		current.size.y = parent_rect.size.y;
	}

	if (current.size.x > parent_rect.size.x) {
		current.size.x = parent_rect.size.x;
	}

	// Early out if max size not set.
	Size2i popup_max_size = get_max_size();
	if (popup_max_size <= Size2()) {
		return current;
	}

	if (current.size.x > popup_max_size.x) {
		current.size.x = popup_max_size.x;
	}

	if (current.size.y > popup_max_size.y) {
		current.size.y = popup_max_size.y;
	}

	return current;
}

void Popup::_bind_methods() {
	ADD_SIGNAL(MethodInfo("popup_hide"));
}

Popup::Popup() {
	set_wrap_controls(true);
	set_visible(false);
	set_transient(true);
	set_flag(FLAG_BORDERLESS, true);
	set_flag(FLAG_RESIZE_DISABLED, true);
	set_flag(FLAG_POPUP, true);
}

Popup::~Popup() {
}

#ifdef TOOLS_ENABLED
PackedStringArray PopupPanel::get_configuration_warnings() const {
	PackedStringArray warnings = Popup::get_configuration_warnings();

	if (!DisplayServer::get_singleton()->is_window_transparency_available() && !GLOBAL_GET("display/window/subwindows/embed_subwindows")) {
		Ref<StyleBoxFlat> sb = theme_cache.panel_style;
		if (sb.is_valid() && (sb->get_shadow_size() > 0 || sb->get_corner_radius(CORNER_TOP_LEFT) > 0 || sb->get_corner_radius(CORNER_TOP_RIGHT) > 0 || sb->get_corner_radius(CORNER_BOTTOM_LEFT) > 0 || sb->get_corner_radius(CORNER_BOTTOM_RIGHT) > 0)) {
			warnings.push_back(RTR("The current theme style has shadows and/or rounded corners for popups, but those won't display correctly if \"display/window/per_pixel_transparency/allowed\" isn't enabled in the Project Settings, nor if it isn't supported."));
		}
	}

	return warnings;
}
#endif

void PopupPanel::_input_from_window(const Ref<InputEvent> &p_event) {
	if (p_event.is_valid()) {
		if (!get_flag(FLAG_POPUP)) {
			return;
		}

		Ref<InputEventMouseButton> b = p_event;
		// Hide it if the shadows have been clicked.
		if (b.is_valid() && b->is_pressed() && b->get_button_index() == MouseButton::LEFT && !panel->get_global_rect().has_point(b->get_position())) {
			_close_pressed();
		}
	} else {
		WARN_PRINT_ONCE("PopupPanel has received an invalid InputEvent. Consider filtering out invalid events.");
	}

	Popup::_input_from_window(p_event);
}

Size2 PopupPanel::_get_contents_minimum_size() const {
	Size2 ms;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c || c == panel) {
			continue;
		}

		if (c->is_set_as_top_level()) {
			continue;
		}

		Size2 cms = c->get_combined_minimum_size();
		ms = cms.max(ms);
	}

	// Take shadows into account.
	ms.width += panel->get_offset(SIDE_LEFT) - panel->get_offset(SIDE_RIGHT);
	ms.height += panel->get_offset(SIDE_TOP) - panel->get_offset(SIDE_BOTTOM);

	return ms + theme_cache.panel_style->get_minimum_size();
}

Rect2i PopupPanel::_popup_adjust_rect() const {
	Rect2i current = Popup::_popup_adjust_rect();
	if (current == Rect2i()) {
		return current;
	}

	pre_popup_rect = current;

	_update_shadow_offsets();
	_update_child_rects();

	if (is_layout_rtl()) {
		current.position -= Vector2(ABS(panel->get_offset(SIDE_RIGHT)), panel->get_offset(SIDE_TOP)) * get_content_scale_factor();
	} else {
		current.position -= Vector2(panel->get_offset(SIDE_LEFT), panel->get_offset(SIDE_TOP)) * get_content_scale_factor();
	}
	current.size += Vector2(panel->get_offset(SIDE_LEFT) - panel->get_offset(SIDE_RIGHT), panel->get_offset(SIDE_TOP) - panel->get_offset(SIDE_BOTTOM)) * get_content_scale_factor();

	return current;
}

void PopupPanel::_update_shadow_offsets() const {
	if (!DisplayServer::get_singleton()->is_window_transparency_available() && !is_embedded()) {
		panel->set_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
		return;
	}

	const Ref<StyleBoxFlat> sb = theme_cache.panel_style;
	if (sb.is_null()) {
		panel->set_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
		return;
	}

	const int shadow_size = sb->get_shadow_size();
	if (shadow_size == 0) {
		panel->set_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
		return;
	}

	// Offset the background panel so it leaves space inside the window for the shadows to be drawn.
	const Point2 shadow_offset = sb->get_shadow_offset();
	if (is_layout_rtl()) {
		panel->set_offset(SIDE_LEFT, shadow_size + shadow_offset.x);
		panel->set_offset(SIDE_RIGHT, -shadow_size + shadow_offset.x);
	} else {
		panel->set_offset(SIDE_LEFT, shadow_size - shadow_offset.x);
		panel->set_offset(SIDE_RIGHT, -shadow_size - shadow_offset.x);
	}
	panel->set_offset(SIDE_TOP, shadow_size - shadow_offset.y);
	panel->set_offset(SIDE_BOTTOM, -shadow_size - shadow_offset.y);
}

void PopupPanel::_update_child_rects() const {
	Vector2 cpos(theme_cache.panel_style->get_offset());
	cpos += Vector2(is_layout_rtl() ? -panel->get_offset(SIDE_RIGHT) : panel->get_offset(SIDE_LEFT), panel->get_offset(SIDE_TOP));

	Vector2 csize = Vector2(get_size()) / get_content_scale_factor() - theme_cache.panel_style->get_minimum_size();
	// Trim shadows.
	csize.width -= panel->get_offset(SIDE_LEFT) - panel->get_offset(SIDE_RIGHT);
	csize.height -= panel->get_offset(SIDE_TOP) - panel->get_offset(SIDE_BOTTOM);

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c || c == panel) {
			continue;
		}

		if (c->is_set_as_top_level()) {
			continue;
		}

		c->set_position(cpos);
		c->set_size(csize);
	}
}

void PopupPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			panel->add_theme_style_override(SceneStringName(panel), theme_cache.panel_style);

			if (is_visible()) {
				_update_shadow_offsets();
			}

			_update_child_rects();

#ifdef TOOLS_ENABLED
			update_configuration_warnings();
#endif
		} break;

		case Control::NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			if (is_visible()) {
				_update_shadow_offsets();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				// Remove the extra space used by the shadows, so they can be ignored when the popup is hidden.
				panel->set_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
				_update_child_rects();

				if (pre_popup_rect != Rect2i()) {
					set_position(pre_popup_rect.position);
					set_size(pre_popup_rect.size);

					pre_popup_rect = Rect2i();
				}
			} else if (pre_popup_rect == Rect2i()) {
				// The popup was made visible directly (without `popup_*()`), so just update the offsets without touching the rect.
				_update_shadow_offsets();
				_update_child_rects();
			}
		} break;

		case NOTIFICATION_WM_SIZE_CHANGED: {
			_update_child_rects();

			if (is_visible()) {
				const Vector2i offsets = Vector2i(panel->get_offset(SIDE_LEFT) - panel->get_offset(SIDE_RIGHT), panel->get_offset(SIDE_TOP) - panel->get_offset(SIDE_BOTTOM));
				// Check if the size actually changed.
				if (pre_popup_rect.size + offsets != get_size()) {
					// Play safe, and stick with the new size.
					pre_popup_rect = Rect2i();
				}
			}
		} break;
	}
}

void PopupPanel::_bind_methods() {
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, PopupPanel, panel_style, "panel");
}

PopupPanel::PopupPanel() {
	set_flag(FLAG_TRANSPARENT, true);

	panel = memnew(Panel);
	panel->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(panel, false, INTERNAL_MODE_FRONT);

#ifdef TOOLS_ENABLED
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp((Node *)this, &Node::update_configuration_warnings));
#endif
}
