/**************************************************************************/
/*  editor_toaster.cpp                                                    */
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

#include "editor_toaster.h"

#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/style_box_flat.h"

EditorToaster *EditorToaster::singleton = nullptr;

void EditorToaster::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			double delta = get_process_delta_time();

			// Check if one element is hovered, if so, don't elapse time.
			bool hovered = false;
			for (const KeyValue<Control *, Toast> &element : toasts) {
				if (Rect2(Vector2(), element.key->get_size()).has_point(element.key->get_local_mouse_position())) {
					hovered = true;
					break;
				}
			}

			// Elapses the time and remove toasts if needed.
			if (!hovered) {
				for (const KeyValue<Control *, Toast> &element : toasts) {
					if (!element.value.popped || element.value.duration <= 0) {
						continue;
					}
					toasts[element.key].remaining_time -= delta;
					if (toasts[element.key].remaining_time < 0) {
						close(element.key);
					}
					element.key->queue_redraw();
				}
			} else {
				// Reset the timers when hovered.
				for (const KeyValue<Control *, Toast> &element : toasts) {
					if (!element.value.popped || element.value.duration <= 0) {
						continue;
					}
					toasts[element.key].remaining_time = element.value.duration;
					element.key->queue_redraw();
				}
			}

			// Change alpha over time.
			bool needs_update = false;
			for (const KeyValue<Control *, Toast> &element : toasts) {
				Color modulate_fade = element.key->get_modulate();

				// Change alpha over time.
				if (element.value.popped && modulate_fade.a < 1.0) {
					modulate_fade.a += delta * 3;
					element.key->set_modulate(modulate_fade);
				} else if (!element.value.popped && modulate_fade.a > 0.0) {
					modulate_fade.a -= delta * 2;
					element.key->set_modulate(modulate_fade);
				}

				// Hide element if it is not visible anymore.
				if (modulate_fade.a <= 0.0 && element.key->is_visible()) {
					element.key->hide();
					needs_update = true;
				} else if (modulate_fade.a > 0.0 && !element.key->is_visible()) {
					element.key->show();
					needs_update = true;
				}
			}

			if (needs_update) {
				_update_vbox_position();
				_update_disable_notifications_button();
				main_button->queue_redraw();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			if (vbox_container->is_visible()) {
				main_button->set_button_icon(get_editor_theme_icon(SNAME("Notification")));
			} else {
				main_button->set_button_icon(get_editor_theme_icon(SNAME("NotificationDisabled")));
			}
			disable_notifications_button->set_button_icon(get_editor_theme_icon(SNAME("NotificationDisabled")));

			// Styleboxes background.
			info_panel_style_background->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)));

			warning_panel_style_background->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)));
			warning_panel_style_background->set_border_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));

			error_panel_style_background->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)));
			error_panel_style_background->set_border_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

			// Styleboxes progress.
			info_panel_style_progress->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)).lightened(0.03));

			warning_panel_style_progress->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)).lightened(0.03));
			warning_panel_style_progress->set_border_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));

			error_panel_style_progress->set_bg_color(get_theme_color(SNAME("base_color"), EditorStringName(Editor)).lightened(0.03));
			error_panel_style_progress->set_border_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			_update_vbox_position();
			_update_disable_notifications_button();
		} break;
	}
}

void EditorToaster::_error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	// This may be called from a thread. Since we will deal with non-thread-safe elements,
	// we have to put it in the queue for safety.
	callable_mp_static(&EditorToaster::_error_handler_impl).call_deferred(String::utf8(p_file), p_line, String::utf8(p_error), String::utf8(p_errorexp), p_editor_notify, p_type);
}

void EditorToaster::_error_handler_impl(const String &p_file, int p_line, const String &p_error, const String &p_errorexp, bool p_editor_notify, int p_type) {
	if (!EditorToaster::get_singleton() || !EditorToaster::get_singleton()->is_inside_tree()) {
		return;
	}

#ifdef DEV_ENABLED
	bool in_dev = true;
#else
	bool in_dev = false;
#endif

	int show_all_setting = EDITOR_GET("interface/editor/show_internal_errors_in_toast_notifications");

	if (p_editor_notify || (show_all_setting == 0 && in_dev) || show_all_setting == 1) {
		String err_str = !p_errorexp.is_empty() ? p_errorexp : p_error;
		String tooltip_str = p_file + ":" + itos(p_line);

		if (!p_editor_notify) {
			if (p_type == ERR_HANDLER_WARNING) {
				err_str = "INTERNAL WARNING: " + err_str;
			} else {
				err_str = "INTERNAL ERROR: " + err_str;
			}
		}

		Severity severity = ((ErrorHandlerType)p_type == ERR_HANDLER_WARNING) ? SEVERITY_WARNING : SEVERITY_ERROR;
		EditorToaster::get_singleton()->popup_str(err_str, severity, tooltip_str);
	}
}

void EditorToaster::_update_vbox_position() {
	// This is kind of a workaround because it's hard to keep the VBox anchroed to the bottom.
	vbox_container->set_size(Vector2());
	vbox_container->set_position(get_global_position() - vbox_container->get_size() + Vector2(get_size().x, -5 * EDSCALE));
}

void EditorToaster::_update_disable_notifications_button() {
	bool any_visible = false;
	for (KeyValue<Control *, Toast> element : toasts) {
		if (element.key->is_visible()) {
			any_visible = true;
			break;
		}
	}

	if (!any_visible || !vbox_container->is_visible()) {
		disable_notifications_panel->hide();
	} else {
		disable_notifications_panel->show();
		disable_notifications_panel->set_position(get_global_position() + Vector2(5 * EDSCALE, -disable_notifications_panel->get_minimum_size().y) + Vector2(get_size().x, -5 * EDSCALE));
	}
}

void EditorToaster::_auto_hide_or_free_toasts() {
	// Hide or free old temporary items.
	int visible_temporary = 0;
	int temporary = 0;
	LocalVector<Control *> to_delete;
	for (int i = vbox_container->get_child_count() - 1; i >= 0; i--) {
		Control *control = Object::cast_to<Control>(vbox_container->get_child(i));
		if (toasts[control].duration <= 0) {
			continue; // Ignore non-temporary toasts.
		}

		temporary++;
		if (control->is_visible()) {
			visible_temporary++;
		}

		// Hide
		if (visible_temporary > max_temporary_count) {
			close(control);
		}

		// Free
		if (temporary > max_temporary_count * 2) {
			to_delete.push_back(control);
		}
	}

	// Delete the control right away (removed as child) as it might cause issues otherwise when iterative over the vbox_container children.
	for (Control *c : to_delete) {
		vbox_container->remove_child(c);
		c->queue_free();
		toasts.erase(c);
	}

	if (toasts.is_empty()) {
		main_button->set_tooltip_text(TTR("No notifications."));
		main_button->set_modulate(Color(0.5, 0.5, 0.5));
		main_button->set_disabled(true);
		set_process_internal(false);
	} else {
		main_button->set_tooltip_text(TTR("Show notifications."));
		main_button->set_modulate(Color(1, 1, 1));
		main_button->set_disabled(false);
	}
}

void EditorToaster::_draw_button() {
	bool has_one = false;
	Severity highest_severity = SEVERITY_INFO;
	for (const KeyValue<Control *, Toast> &element : toasts) {
		if (!element.key->is_visible()) {
			continue;
		}
		has_one = true;
		if (element.value.severity > highest_severity) {
			highest_severity = element.value.severity;
		}
	}

	if (!has_one) {
		return;
	}

	Color color;
	real_t button_radius = main_button->get_size().x / 8;
	switch (highest_severity) {
		case SEVERITY_INFO:
			color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			break;
		case SEVERITY_WARNING:
			color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
			break;
		case SEVERITY_ERROR:
			color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
			break;
		default:
			break;
	}
	main_button->draw_circle(Vector2(button_radius * 2, button_radius * 2), button_radius, color);
}

void EditorToaster::_draw_progress(Control *panel) {
	if (toasts.has(panel) && toasts[panel].remaining_time > 0 && toasts[panel].duration > 0) {
		Size2 size = panel->get_size();
		size.x *= MIN(1, Math::remap(toasts[panel].remaining_time, 0, toasts[panel].duration, 0, 2));

		Ref<StyleBoxFlat> stylebox;
		switch (toasts[panel].severity) {
			case SEVERITY_INFO:
				stylebox = info_panel_style_progress;
				break;
			case SEVERITY_WARNING:
				stylebox = warning_panel_style_progress;
				break;
			case SEVERITY_ERROR:
				stylebox = error_panel_style_progress;
				break;
			default:
				break;
		}
		panel->draw_style_box(stylebox, Rect2(Vector2(), size));
	}
}

void EditorToaster::_set_notifications_enabled(bool p_enabled) {
	vbox_container->set_visible(p_enabled);
	if (p_enabled) {
		main_button->set_button_icon(get_editor_theme_icon(SNAME("Notification")));
	} else {
		main_button->set_button_icon(get_editor_theme_icon(SNAME("NotificationDisabled")));
	}
	_update_disable_notifications_button();
}

void EditorToaster::_repop_old() {
	// Repop olds, up to max_temporary_count
	bool needs_update = false;
	int visible_count = 0;
	for (int i = vbox_container->get_child_count() - 1; i >= 0; i--) {
		Control *control = Object::cast_to<Control>(vbox_container->get_child(i));
		if (!control->is_visible()) {
			control->show();
			toasts[control].remaining_time = toasts[control].duration;
			toasts[control].popped = true;
			needs_update = true;
		}
		visible_count++;
		if (visible_count >= max_temporary_count) {
			break;
		}
	}
	if (needs_update) {
		_update_vbox_position();
		_update_disable_notifications_button();
		main_button->queue_redraw();
	}
}

Control *EditorToaster::popup(Control *p_control, Severity p_severity, double p_time, const String &p_tooltip) {
	// Create the panel according to the severity.
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_tooltip_text(p_tooltip);
	switch (p_severity) {
		case SEVERITY_INFO:
			panel->add_theme_style_override(SceneStringName(panel), info_panel_style_background);
			break;
		case SEVERITY_WARNING:
			panel->add_theme_style_override(SceneStringName(panel), warning_panel_style_background);
			break;
		case SEVERITY_ERROR:
			panel->add_theme_style_override(SceneStringName(panel), error_panel_style_background);
			break;
		default:
			break;
	}
	panel->set_modulate(Color(1, 1, 1, 0));
	panel->connect(SceneStringName(draw), callable_mp(this, &EditorToaster::_draw_progress).bind(panel));
	panel->connect(SceneStringName(theme_changed), callable_mp(this, &EditorToaster::_toast_theme_changed).bind(panel));

	Toast &toast = toasts[panel];

	// Horizontal container.
	HBoxContainer *hbox_container = memnew(HBoxContainer);
	hbox_container->set_h_size_flags(SIZE_EXPAND_FILL);
	panel->add_child(hbox_container);

	// Content control.
	p_control->set_h_size_flags(SIZE_EXPAND_FILL);
	hbox_container->add_child(p_control);

	// Add buttons.
	if (p_time > 0.0) {
		Button *copy_button = memnew(Button);
		copy_button->set_flat(true);
		copy_button->connect(SceneStringName(pressed), callable_mp(this, &EditorToaster::copy).bind(panel));
		hbox_container->add_child(copy_button);

		Button *close_button = memnew(Button);
		close_button->set_flat(true);
		close_button->connect(SceneStringName(pressed), callable_mp(this, &EditorToaster::instant_close).bind(panel));
		hbox_container->add_child(close_button);

		toast.copy_button = copy_button;
		toast.close_button = close_button;
	}

	toast.severity = p_severity;
	if (p_time > 0.0) {
		toast.duration = p_time;
		toast.remaining_time = p_time;
	} else {
		toast.duration = -1.0;
	}
	toast.popped = true;
	vbox_container->add_child(panel);
	_auto_hide_or_free_toasts();
	_update_vbox_position();
	_update_disable_notifications_button();
	main_button->queue_redraw();

	return panel;
}

void EditorToaster::popup_str(const String &p_message, Severity p_severity, const String &p_tooltip) {
	if (is_processing_error) {
		return;
	}

	// Since "_popup_str" adds nodes to the tree, and since the "add_child" method is not
	// thread-safe, it's better to defer the call to the next cycle to be thread-safe.
	is_processing_error = true;
	callable_mp(this, &EditorToaster::_popup_str).call_deferred(p_message, p_severity, p_tooltip);
	is_processing_error = false;
}

void EditorToaster::_popup_str(const String &p_message, Severity p_severity, const String &p_tooltip) {
	is_processing_error = true;
	// Check if we already have a popup with the given message.
	Control *control = nullptr;
	for (KeyValue<Control *, Toast> element : toasts) {
		if (element.value.message == p_message && element.value.severity == p_severity && element.value.tooltip == p_tooltip) {
			control = element.key;
			break;
		}
	}

	// Create a new message if needed.
	if (control == nullptr) {
		HBoxContainer *hb = memnew(HBoxContainer);
		hb->add_theme_constant_override("separation", 0);

		Label *label = memnew(Label);
		hb->add_child(label);

		Label *count_label = memnew(Label);
		hb->add_child(count_label);

		control = popup(hb, p_severity, default_message_duration, p_tooltip);

		Toast &toast = toasts[control];
		toast.message = p_message;
		toast.tooltip = p_tooltip;
		toast.count = 1;
		toast.message_label = label;
		toast.message_count_label = count_label;
	} else {
		Toast &toast = toasts[control];
		if (toast.popped) {
			toast.count += 1;
		} else {
			toast.count = 1;
		}
		toast.remaining_time = toast.duration;
		toast.popped = true;
		control->show();
		vbox_container->move_child(control, vbox_container->get_child_count());
		_auto_hide_or_free_toasts();
		_update_vbox_position();
		_update_disable_notifications_button();
		main_button->queue_redraw();
	}

	// Retrieve the label back, then update the text.
	Label *message_label = toasts[control].message_label;
	ERR_FAIL_NULL(message_label);
	message_label->set_text(p_message);
	message_label->set_text_overrun_behavior(TextServer::OVERRUN_NO_TRIMMING);
	message_label->set_custom_minimum_size(Size2());

	Size2i size = message_label->get_combined_minimum_size();
	int limit_width = get_viewport_rect().size.x / 2; // Limit label size to half the viewport size.
	if (size.x > limit_width) {
		message_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		message_label->set_custom_minimum_size(Size2(limit_width, 0));
	}

	// Retrieve the count label back, then update the text.
	Label *message_count_label = toasts[control].message_count_label;
	if (toasts[control].count == 1) {
		message_count_label->hide();
	} else {
		message_count_label->set_text(vformat("(%d)", toasts[control].count));
		message_count_label->show();
	}

	vbox_container->reset_size();

	is_processing_error = false;
	set_process_internal(true);
}

void EditorToaster::_toast_theme_changed(Control *p_control) {
	ERR_FAIL_COND(!toasts.has(p_control));

	Toast &toast = toasts[p_control];
	if (toast.close_button) {
		toast.close_button->set_button_icon(get_editor_theme_icon(SNAME("Close")));
	}
	if (toast.copy_button) {
		toast.copy_button->set_button_icon(get_editor_theme_icon(SNAME("ActionCopy")));
	}
}

void EditorToaster::close(Control *p_control) {
	ERR_FAIL_COND(!toasts.has(p_control));
	toasts[p_control].remaining_time = -1.0;
	toasts[p_control].popped = false;
}

void EditorToaster::instant_close(Control *p_control) {
	close(p_control);
	p_control->set_modulate(Color(1, 1, 1, 0));
}

void EditorToaster::copy(Control *p_control) {
	ERR_FAIL_COND(!toasts.has(p_control));
	DisplayServer::get_singleton()->clipboard_set(toasts[p_control].message);
}

void EditorToaster::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_toast", "message", "severity", "tooltip"), &EditorToaster::_popup_str, DEFVAL(EditorToaster::SEVERITY_INFO), DEFVAL(String()));

	BIND_ENUM_CONSTANT(SEVERITY_INFO);
	BIND_ENUM_CONSTANT(SEVERITY_WARNING);
	BIND_ENUM_CONSTANT(SEVERITY_ERROR);
}

EditorToaster *EditorToaster::get_singleton() {
	return singleton;
}

EditorToaster::EditorToaster() {
	set_notify_transform(true);

	// VBox.
	vbox_container = memnew(VBoxContainer);
	vbox_container->set_as_top_level(true);
	vbox_container->connect(SceneStringName(resized), callable_mp(this, &EditorToaster::_update_vbox_position));
	add_child(vbox_container);

	// Theming (background).
	info_panel_style_background.instantiate();
	info_panel_style_background->set_corner_radius_all(stylebox_radius * EDSCALE);

	warning_panel_style_background.instantiate();
	warning_panel_style_background->set_border_width(SIDE_LEFT, stylebox_radius * EDSCALE);
	warning_panel_style_background->set_corner_radius_all(stylebox_radius * EDSCALE);

	error_panel_style_background.instantiate();
	error_panel_style_background->set_border_width(SIDE_LEFT, stylebox_radius * EDSCALE);
	error_panel_style_background->set_corner_radius_all(stylebox_radius * EDSCALE);

	Ref<StyleBoxFlat> boxes[] = { info_panel_style_background, warning_panel_style_background, error_panel_style_background };
	for (int i = 0; i < 3; i++) {
		boxes[i]->set_content_margin_individual(int(stylebox_radius * 2.5), 3, int(stylebox_radius * 2.5), 3);
	}

	// Theming (progress).
	info_panel_style_progress.instantiate();
	info_panel_style_progress->set_corner_radius_all(stylebox_radius * EDSCALE);

	warning_panel_style_progress.instantiate();
	warning_panel_style_progress->set_border_width(SIDE_LEFT, stylebox_radius * EDSCALE);
	warning_panel_style_progress->set_corner_radius_all(stylebox_radius * EDSCALE);

	error_panel_style_progress.instantiate();
	error_panel_style_progress->set_border_width(SIDE_LEFT, stylebox_radius * EDSCALE);
	error_panel_style_progress->set_corner_radius_all(stylebox_radius * EDSCALE);

	// Main button.
	main_button = memnew(Button);
	main_button->set_tooltip_text(TTR("No notifications."));
	main_button->set_modulate(Color(0.5, 0.5, 0.5));
	main_button->set_disabled(true);
	main_button->set_theme_type_variation("FlatMenuButton");
	main_button->connect(SceneStringName(pressed), callable_mp(this, &EditorToaster::_set_notifications_enabled).bind(true));
	main_button->connect(SceneStringName(pressed), callable_mp(this, &EditorToaster::_repop_old));
	main_button->connect(SceneStringName(draw), callable_mp(this, &EditorToaster::_draw_button));
	add_child(main_button);

	// Disable notification button.
	disable_notifications_panel = memnew(PanelContainer);
	disable_notifications_panel->set_as_top_level(true);
	disable_notifications_panel->add_theme_style_override(SceneStringName(panel), info_panel_style_background);
	add_child(disable_notifications_panel);

	disable_notifications_button = memnew(Button);
	disable_notifications_button->set_tooltip_text(TTR("Silence the notifications."));
	disable_notifications_button->set_flat(true);
	disable_notifications_button->connect(SceneStringName(pressed), callable_mp(this, &EditorToaster::_set_notifications_enabled).bind(false));
	disable_notifications_panel->add_child(disable_notifications_button);

	// Other
	singleton = this;

	eh.errfunc = _error_handler;
	add_error_handler(&eh);
}

EditorToaster::~EditorToaster() {
	singleton = nullptr;
	remove_error_handler(&eh);
}
