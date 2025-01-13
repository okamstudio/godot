/**************************************************************************/
/*  popup_menu.cpp                                                        */
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

#include "popup_menu.h"
#include "popup_menu.compat.inc"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/string/translation.h"
#include "scene/gui/menu_bar.h"
#include "scene/theme/theme_db.h"

HashMap<NativeMenu::SystemMenus, PopupMenu *> PopupMenu::system_menus;

bool PopupMenu::_set_item_accelerator(int p_index, const Ref<InputEventKey> &p_ie) {
	NativeMenu *nmenu = NativeMenu::get_singleton();
	if (p_ie->get_physical_keycode() == Key::NONE && p_ie->get_keycode() == Key::NONE && p_ie->get_key_label() != Key::NONE) {
		nmenu->set_item_accelerator(global_menu, p_index, p_ie->get_key_label_with_modifiers());
		return true;
	} else if (p_ie->get_keycode() != Key::NONE) {
		nmenu->set_item_accelerator(global_menu, p_index, p_ie->get_keycode_with_modifiers());
		return true;
	} else if (p_ie->get_physical_keycode() != Key::NONE) {
		nmenu->set_item_accelerator(global_menu, p_index, DisplayServer::get_singleton()->keyboard_get_keycode_from_physical(p_ie->get_physical_keycode_with_modifiers()));
		return true;
	}
	return false;
}

void PopupMenu::_set_item_checkable_type(int p_index, int p_checkable_type) {
	switch (p_checkable_type) {
		case Item::CHECKABLE_TYPE_NONE: {
			set_item_as_checkable(p_index, false);
		} break;
		case Item::CHECKABLE_TYPE_CHECK_BOX: {
			set_item_as_checkable(p_index, true);
		} break;
		case Item::CHECKABLE_TYPE_RADIO_BUTTON: {
			set_item_as_radio_checkable(p_index, true);
		} break;
	}
}

int PopupMenu::_get_item_checkable_type(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, items.size(), Item::CHECKABLE_TYPE_NONE);
	return items[p_index].checkable_type;
}

RID PopupMenu::bind_global_menu() {
#ifdef TOOLS_ENABLED
	if (is_part_of_edited_scene()) {
		return RID();
	}
#endif
	if (!NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_POPUP_MENU)) {
		return RID();
	}

	if (global_menu.is_valid()) {
		return global_menu; // Already bound;
	}

	NativeMenu *nmenu = NativeMenu::get_singleton();

	if (system_menu_id != NativeMenu::INVALID_MENU_ID && nmenu->has_system_menu(system_menu_id)) {
		if (system_menus.has(system_menu_id)) {
			WARN_PRINT(vformat("Attempting to bind PopupMenu to the system menu %s, but another menu is already bound to it. This menu: %s, current menu: %s", nmenu->get_system_menu_name(system_menu_id), get_description(), system_menus[system_menu_id]->get_description()));
			global_menu = nmenu->create_menu();
		} else {
			system_menus[system_menu_id] = this;
			system_menu = nmenu->get_system_menu(system_menu_id);
			global_menu = system_menu;
		}
	} else {
		global_menu = nmenu->create_menu();
	}

	nmenu->set_interface_direction(global_menu, control->is_layout_rtl());
	nmenu->set_popup_open_callback(global_menu, callable_mp(this, &PopupMenu::_about_to_popup));
	nmenu->set_popup_close_callback(global_menu, callable_mp(this, &PopupMenu::_about_to_close));
	for (int i = 0; i < items.size(); i++) {
		Item &item = items.write[i];
		if (item.separator) {
			nmenu->add_separator(global_menu);
		} else {
			int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), item.shortcut_is_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), i);
			if (item.submenu) {
				RID submenu_rid = item.submenu->bind_global_menu();
				nmenu->set_item_submenu(global_menu, index, submenu_rid);
				item.submenu_bound = true;
			}
			if (item.checkable_type == Item::CHECKABLE_TYPE_CHECK_BOX) {
				nmenu->set_item_checkable(global_menu, index, true);
			} else if (item.checkable_type == Item::CHECKABLE_TYPE_RADIO_BUTTON) {
				nmenu->set_item_radio_checkable(global_menu, index, true);
			}
			nmenu->set_item_checked(global_menu, index, item.checked);
			nmenu->set_item_disabled(global_menu, index, item.disabled);
			nmenu->set_item_max_states(global_menu, index, item.max_states);
			nmenu->set_item_icon(global_menu, index, item.icon);
			nmenu->set_item_state(global_menu, index, item.state);
			nmenu->set_item_indentation_level(global_menu, index, item.indent);
			nmenu->set_item_tooltip(global_menu, index, item.tooltip);
			if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
				Array events = item.shortcut->get_events();
				for (int j = 0; j < events.size(); j++) {
					Ref<InputEventKey> ie = events[j];
					if (ie.is_valid() && _set_item_accelerator(index, ie)) {
						break;
					}
				}
			} else if (item.accel != Key::NONE) {
				nmenu->set_item_accelerator(global_menu, index, item.accel);
			}
		}
	}
	return global_menu;
}

void PopupMenu::unbind_global_menu() {
	if (global_menu.is_null()) {
		return;
	}

	if (global_menu == system_menu && system_menus[system_menu_id] == this) {
		system_menus.erase(system_menu_id);
	}

	for (int i = 0; i < items.size(); i++) {
		Item &item = items.write[i];
		if (item.submenu) {
			item.submenu->unbind_global_menu();
			item.submenu_bound = false;
		}
	}
	if (system_menu != global_menu) {
		NativeMenu::get_singleton()->free_menu(global_menu);
	} else {
		NativeMenu::get_singleton()->clear(global_menu);
	}

	system_menu = RID();
	global_menu = RID();
}

bool PopupMenu::is_system_menu() const {
	return (global_menu == system_menu) && (system_menu_id != NativeMenu::INVALID_MENU_ID);
}

void PopupMenu::set_system_menu(NativeMenu::SystemMenus p_system_menu_id) {
	if (is_inside_tree() && system_menu_id != NativeMenu::INVALID_MENU_ID) {
		unbind_global_menu();
	}
	system_menu_id = p_system_menu_id;
	if (is_inside_tree() && system_menu_id != NativeMenu::INVALID_MENU_ID) {
		bind_global_menu();
	}
}

NativeMenu::SystemMenus PopupMenu::get_system_menu() const {
	return system_menu_id;
}

String PopupMenu::_get_accel_text(const Item &p_item) const {
	if (p_item.shortcut.is_valid()) {
		return p_item.shortcut->get_as_text();
	} else if (p_item.accel != Key::NONE) {
		return keycode_get_string(p_item.accel);
	}
	return String();
}

Size2 PopupMenu::_get_item_icon_size(int p_idx) const {
	const PopupMenu::Item &item = items[p_idx];
	Size2 icon_size = item.get_icon_size();

	int max_width = 0;
	if (theme_cache.icon_max_width > 0) {
		max_width = theme_cache.icon_max_width;
	}
	if (item.icon_max_width > 0 && (max_width == 0 || item.icon_max_width < max_width)) {
		max_width = item.icon_max_width;
	}

	if (max_width > 0 && icon_size.width > max_width) {
		icon_size.height = icon_size.height * max_width / icon_size.width;
		icon_size.width = max_width;
	}

	return icon_size;
}

Size2 PopupMenu::_get_contents_minimum_size() const {
	Size2 minsize = theme_cache.panel_style->get_minimum_size();
	minsize.width += scroll_container->get_v_scroll_bar()->get_size().width;

	float max_w = 0.0;
	float icon_w = 0.0;
	int check_w = MAX(theme_cache.checked->get_width(), theme_cache.radio_checked->get_width()) + theme_cache.h_separation;
	int accel_max_w = 0;
	bool has_check = false;

	for (int i = 0; i < items.size(); i++) {
		Size2 item_size;
		_shape_item(i);

		Size2 icon_size = _get_item_icon_size(i);
		item_size.height = _get_item_height(i);
		icon_w = MAX(icon_size.width, icon_w);

		item_size.width += items[i].indent * theme_cache.indent;

		if (items[i].checkable_type && !items[i].separator) {
			has_check = true;
		}

		item_size.width += items[i].text_buf->get_size().x;
		item_size.height += theme_cache.v_separation;

		if (items[i].accel != Key::NONE || (items[i].shortcut.is_valid() && items[i].shortcut->has_valid_event())) {
			int accel_w = theme_cache.h_separation * 2;
			accel_w += items[i].accel_text_buf->get_size().x;
			accel_max_w = MAX(accel_w, accel_max_w);
		}

		if (items[i].submenu) {
			item_size.width += theme_cache.submenu->get_width();
		}

		max_w = MAX(max_w, item_size.width);

		minsize.height += item_size.height;
	}

	int item_side_padding = theme_cache.item_start_padding + theme_cache.item_end_padding;
	minsize.width += max_w + icon_w + accel_max_w + item_side_padding;

	if (has_check) {
		minsize.width += check_w;
	}

	if (is_inside_tree()) {
		int height_limit = get_usable_parent_rect().size.height;
		if (minsize.height > height_limit) {
			minsize.height = height_limit;
		}
	}

	minsize.height = Math::ceil(minsize.height); // Ensures enough height at fractional content scales to prevent the v_scroll_bar from showing.
	return minsize;
}

int PopupMenu::_get_item_height(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), 0);

	Size2 icon_size = _get_item_icon_size(p_idx);
	int icon_height = icon_size.height;
	if (items[p_idx].checkable_type && !items[p_idx].separator) {
		icon_height = MAX(icon_height, MAX(theme_cache.checked->get_height(), theme_cache.radio_checked->get_height()));
	}

	int text_height = items[p_idx].text_buf->get_size().height;
	if (text_height == 0 && !items[p_idx].separator) {
		text_height = theme_cache.font->get_height(theme_cache.font_size);
	}

	int separator_height = 0;
	if (items[p_idx].separator) {
		separator_height = MAX(theme_cache.separator_style->get_minimum_size().height, MAX(theme_cache.labeled_separator_left->get_minimum_size().height, theme_cache.labeled_separator_right->get_minimum_size().height));
	}

	return MAX(separator_height, MAX(text_height, icon_height));
}

int PopupMenu::_get_items_total_height() const {
	// Get total height of all items by taking max of icon height and font height
	int items_total_height = 0;
	for (int i = 0; i < items.size(); i++) {
		items_total_height += _get_item_height(i) + theme_cache.v_separation;
	}

	return items_total_height;
}

int PopupMenu::_get_mouse_over(const Point2 &p_over) const {
	float win_scale = get_content_scale_factor();
	if (p_over.x < 0 || p_over.x >= get_size().width * win_scale || p_over.y < theme_cache.panel_style->get_margin(Side::SIDE_TOP) * win_scale) {
		return -1;
	}

	Point2 ofs = Point2(0, theme_cache.v_separation * 0.5) * win_scale;

	for (int i = 0; i < items.size(); i++) {
		ofs.y += i > 0 ? (float)theme_cache.v_separation * win_scale : (float)theme_cache.v_separation * win_scale * 0.5;
		ofs.y += _get_item_height(i) * win_scale;
		if (p_over.y - control->get_position().y * win_scale < ofs.y) {
			return i;
		}
	}

	return -1;
}

void PopupMenu::_activate_submenu(int p_over, bool p_by_keyboard) {
	Popup *submenu_popup = items[p_over].submenu;
	if (submenu_popup->is_visible()) {
		return; // Already visible.
	}

	Point2 this_pos = get_position();
	Rect2 this_rect(this_pos, get_size());

	float scroll_offset = control->get_position().y;
	float scaled_ofs_cache = items[p_over]._ofs_cache * get_content_scale_factor();
	float scaled_height_cache = items[p_over]._height_cache * get_content_scale_factor();

	submenu_popup->reset_size(); // Shrink the popup size to its contents.
	Size2 submenu_size = submenu_popup->get_size();

	Point2 submenu_pos;
	if (control->is_layout_rtl()) {
		submenu_pos = this_pos + Point2(-submenu_size.width, scaled_ofs_cache + scroll_offset - theme_cache.v_separation / 2);
	} else {
		submenu_pos = this_pos + Point2(this_rect.size.width, scaled_ofs_cache + scroll_offset - theme_cache.v_separation / 2);
	}

	// Fix pos if going outside parent rect.
	if (submenu_pos.x < get_parent_rect().position.x) {
		submenu_pos.x = this_pos.x + submenu_size.width;
	}

	if (submenu_pos.x + submenu_size.width > get_parent_rect().position.x + get_parent_rect().size.width) {
		submenu_pos.x = this_pos.x - submenu_size.width;
	}

	submenu_popup->set_position(submenu_pos);

	PopupMenu *submenu_pum = Object::cast_to<PopupMenu>(submenu_popup);
	if (!submenu_pum) {
		submenu_popup->popup();
		return;
	}

	submenu_pum->activated_by_keyboard = p_by_keyboard;

	// If not triggered by the mouse, start the popup with its first enabled item focused.
	if (p_by_keyboard) {
		for (int i = 0; i < submenu_pum->get_item_count(); i++) {
			if (!submenu_pum->is_item_disabled(i)) {
				submenu_pum->set_focused_item(i);
				break;
			}
		}
	}

	submenu_pum->popup();

	// Set autohide areas.

	Rect2 safe_area = this_rect;
	safe_area.position.y += scaled_ofs_cache + scroll_offset + theme_cache.panel_style->get_offset().height - theme_cache.v_separation / 2;
	safe_area.size.y = scaled_height_cache + theme_cache.v_separation;
	Viewport *vp = submenu_popup->get_embedder();
	if (vp) {
		vp->subwindow_set_popup_safe_rect(submenu_popup, safe_area);
	} else {
		DisplayServer::get_singleton()->window_set_popup_safe_rect(submenu_popup->get_window_id(), safe_area);
	}

	// Make the position of the parent popup relative to submenu popup.
	this_rect.position = this_rect.position - submenu_pum->get_position();

	// Autohide area above the submenu item.
	submenu_pum->clear_autohide_areas();
	submenu_pum->add_autohide_area(Rect2(this_rect.position.x, this_rect.position.y, this_rect.size.x, scaled_ofs_cache + scroll_offset + theme_cache.panel_style->get_offset().height - theme_cache.v_separation / 2));

	// If there is an area below the submenu item, add an autohide area there.
	if (scaled_ofs_cache + scaled_height_cache + scroll_offset <= control->get_size().height) {
		int from = scaled_ofs_cache + scaled_height_cache + scroll_offset + theme_cache.v_separation / 2 + theme_cache.panel_style->get_offset().height;
		submenu_pum->add_autohide_area(Rect2(this_rect.position.x, this_rect.position.y + from, this_rect.size.x, this_rect.size.y - from));
	}
}

void PopupMenu::_parent_focused() {
	if (is_embedded()) {
		Point2 mouse_pos_adjusted;
		Window *window_parent = Object::cast_to<Window>(get_parent()->get_viewport());
		while (window_parent) {
			if (!window_parent->is_embedded()) {
				mouse_pos_adjusted += window_parent->get_position();
				break;
			}

			window_parent = Object::cast_to<Window>(window_parent->get_parent()->get_viewport());
		}

		Rect2 safe_area = get_embedder()->subwindow_get_popup_safe_rect(this);
		Point2 pos = DisplayServer::get_singleton()->mouse_get_position() - mouse_pos_adjusted;
		if (safe_area == Rect2i() || !safe_area.has_point(pos)) {
			Popup::_parent_focused();
		} else {
			grab_focus();
		}
	}
}

void PopupMenu::_submenu_timeout() {
	if (mouse_over == submenu_over) {
		_activate_submenu(mouse_over);
	}

	submenu_over = -1;
}

void PopupMenu::_input_from_window(const Ref<InputEvent> &p_event) {
	if (p_event.is_valid()) {
		_input_from_window_internal(p_event);
	} else {
		WARN_PRINT_ONCE("PopupMenu has received an invalid InputEvent. Consider filtering invalid events out.");
	}
	Popup::_input_from_window(p_event);
}

void PopupMenu::_input_from_window_internal(const Ref<InputEvent> &p_event) {
	if (!items.is_empty()) {
		Input *input = Input::get_singleton();
		Ref<InputEventJoypadMotion> joypadmotion_event = p_event;
		Ref<InputEventJoypadButton> joypadbutton_event = p_event;
		bool is_joypad_event = (joypadmotion_event.is_valid() || joypadbutton_event.is_valid());

		if (p_event->is_action("ui_down", true) && p_event->is_pressed()) {
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_down", true)) {
					return;
				}
				set_process_internal(true);
			}
			int search_from = mouse_over + 1;
			if (search_from >= items.size()) {
				search_from = 0;
			}

			bool match_found = false;
			for (int i = search_from; i < items.size(); i++) {
				if (!items[i].separator && !items[i].disabled) {
					mouse_over = i;
					emit_signal(SNAME("id_focused"), items[i].id);
					scroll_to_item(i);
					control->queue_redraw();
					set_input_as_handled();
					match_found = true;
					break;
				}
			}

			if (!match_found) {
				// If the last item is not selectable, try re-searching from the start.
				for (int i = 0; i < search_from; i++) {
					if (!items[i].separator && !items[i].disabled) {
						mouse_over = i;
						emit_signal(SNAME("id_focused"), items[i].id);
						scroll_to_item(i);
						control->queue_redraw();
						set_input_as_handled();
						break;
					}
				}
			}
		} else if (p_event->is_action("ui_up", true) && p_event->is_pressed()) {
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_up", true)) {
					return;
				}
				set_process_internal(true);
			}
			int search_from = mouse_over - 1;
			if (search_from < 0) {
				search_from = items.size() - 1;
			}

			bool match_found = false;
			for (int i = search_from; i >= 0; i--) {
				if (!items[i].separator && !items[i].disabled) {
					mouse_over = i;
					emit_signal(SNAME("id_focused"), items[i].id);
					scroll_to_item(i);
					control->queue_redraw();
					set_input_as_handled();
					match_found = true;
					break;
				}
			}

			if (!match_found) {
				// If the first item is not selectable, try re-searching from the end.
				for (int i = items.size() - 1; i >= search_from; i--) {
					if (!items[i].separator && !items[i].disabled) {
						mouse_over = i;
						emit_signal(SNAME("id_focused"), items[i].id);
						scroll_to_item(i);
						control->queue_redraw();
						set_input_as_handled();
						break;
					}
				}
			}
		} else if (p_event->is_action("ui_left", true) && p_event->is_pressed()) {
			Node *n = get_parent();
			if (n) {
				if (Object::cast_to<PopupMenu>(n)) {
					hide();
					set_input_as_handled();
				} else if (Object::cast_to<MenuBar>(n)) {
					Object::cast_to<MenuBar>(n)->gui_input(p_event);
					set_input_as_handled();
					return;
				}
			}
		} else if (p_event->is_action("ui_right", true) && p_event->is_pressed()) {
			if (mouse_over >= 0 && mouse_over < items.size() && !items[mouse_over].separator && items[mouse_over].submenu && submenu_over != mouse_over) {
				_activate_submenu(mouse_over, true);
				set_input_as_handled();
			} else {
				Node *n = get_parent();
				if (n && Object::cast_to<MenuBar>(n)) {
					Object::cast_to<MenuBar>(n)->gui_input(p_event);
					set_input_as_handled();
					return;
				}
			}
		} else if (p_event->is_action("ui_accept", true) && p_event->is_pressed()) {
			if (mouse_over >= 0 && mouse_over < items.size() && !items[mouse_over].separator) {
				if (items[mouse_over].submenu && submenu_over != mouse_over) {
					_activate_submenu(mouse_over, true);
				} else {
					activate_item(mouse_over);
				}
				set_input_as_handled();
			}
		}
	}

	// Make an area which does not include v scrollbar, so that items are not activated when dragging scrollbar.
	Rect2 item_clickable_area = scroll_container->get_rect();
	if (scroll_container->get_v_scroll_bar()->is_visible_in_tree()) {
		if (is_layout_rtl()) {
			item_clickable_area.position.x += scroll_container->get_v_scroll_bar()->get_size().width;
		}
		item_clickable_area.size.width -= scroll_container->get_v_scroll_bar()->get_size().width;
	}
	item_clickable_area.size = item_clickable_area.size * get_content_scale_factor();

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		MouseButton button_idx = b->get_button_index();
		// Activate the item on release of either the left mouse button or
		// any mouse button held down when the popup was opened.
		// This allows for opening the popup and triggering an action in a single mouse click.
		if (button_idx == MouseButton::LEFT || initial_button_mask.has_flag(mouse_button_to_mask(button_idx))) {
			if (b->is_pressed()) {
				during_grabbed_click = false;
				is_scrolling = is_layout_rtl() ? b->get_position().x < item_clickable_area.position.x : b->get_position().x > item_clickable_area.size.width;

				if (!item_clickable_area.has_point(b->get_position())) {
					return;
				}
				_mouse_over_update(b->get_position());
			} else {
				if (is_scrolling) {
					is_scrolling = false;
					return;
				}
				bool was_during_grabbed_click = during_grabbed_click;
				during_grabbed_click = false;
				initial_button_mask.clear();

				if (!item_clickable_area.has_point(b->get_position())) {
					return;
				}
				// Disable clicks under a time threshold to avoid selection right when opening the popup.
				if (was_during_grabbed_click && OS::get_singleton()->get_ticks_msec() - popup_time_msec < 400) {
					return;
				}

				int over = _get_mouse_over(b->get_position());
				if (over < 0) {
					if (!was_during_grabbed_click) {
						hide();
					}
					return;
				}

				if (items[over].separator || items[over].disabled) {
					return;
				}

				if (items[over].submenu) {
					_activate_submenu(over);
					return;
				}
				activate_item(over);
			}
		}
	}

	Ref<InputEventMouseMotion> m = p_event;

	if (m.is_valid()) {
		if (m->get_velocity().is_zero_approx()) {
			return;
		}
		activated_by_keyboard = false;

		for (const Rect2 &E : autohide_areas) {
			if (!Rect2(Point2(), get_size()).has_point(m->get_position()) && E.has_point(m->get_position())) {
				// The mouse left the safe area, prepare to close.
				_close_pressed();
				return;
			}
		}

		if (!minimum_lifetime_timer->is_stopped()) {
			// The mouse left the safe area, but came back again, so cancel the auto-closing.
			minimum_lifetime_timer->stop();
		}

		if (!item_clickable_area.has_point(m->get_position())) {
			return;
		}
		_mouse_over_update(m->get_position());
	}

	Ref<InputEventKey> k = p_event;

	if (allow_search && k.is_valid() && k->get_unicode() && k->is_pressed()) {
		uint64_t now = OS::get_singleton()->get_ticks_msec();
		uint64_t diff = now - search_time_msec;
		uint64_t max_interval = uint64_t(GLOBAL_GET("gui/timers/incremental_search_max_interval_msec"));
		search_time_msec = now;

		if (diff > max_interval) {
			search_string = "";
		}

		if (String::chr(k->get_unicode()) != search_string) {
			search_string += String::chr(k->get_unicode());
		}

		for (int i = mouse_over + 1; i <= items.size(); i++) {
			if (i == items.size()) {
				if (mouse_over <= 0) {
					break;
				} else {
					i = 0;
				}
			}

			if (i == mouse_over) {
				break;
			}

			if (items[i].text.findn(search_string) == 0) {
				mouse_over = i;
				emit_signal(SNAME("id_focused"), items[i].id);
				scroll_to_item(i);
				control->queue_redraw();
				set_input_as_handled();
				break;
			}
		}
	}
}

void PopupMenu::_mouse_over_update(const Point2 &p_over) {
	int over = _get_mouse_over(p_over);
	int id = (over < 0 || items[over].separator || items[over].disabled) ? -1 : (items[over].id >= 0 ? items[over].id : over);

	if (id < 0) {
		mouse_over = -1;
		control->queue_redraw();
		return;
	}

	if (!is_scrolling && items[over].submenu && submenu_over != over) {
		submenu_over = over;
		submenu_timer->start();
	}

	if (over != mouse_over) {
		mouse_over = over;
		control->queue_redraw();
	}
}

void PopupMenu::_draw_items() {
	control->set_custom_minimum_size(Size2(0, _get_items_total_height()));
	RID ci = control->get_canvas_item();

	// Space between the item content and the sides of popup menu.
	bool rtl = control->is_layout_rtl();
	// In Item::checkable_type enum order (less the non-checkable member), with disabled repeated at the end.
	Ref<Texture2D> check[] = { theme_cache.checked, theme_cache.radio_checked, theme_cache.checked_disabled, theme_cache.radio_checked_disabled };
	Ref<Texture2D> uncheck[] = { theme_cache.unchecked, theme_cache.radio_unchecked, theme_cache.unchecked_disabled, theme_cache.radio_unchecked_disabled };
	Ref<Texture2D> submenu;
	if (rtl) {
		submenu = theme_cache.submenu_mirrored;
	} else {
		submenu = theme_cache.submenu;
	}

	float display_width = control->get_size().width;

	// Find the widest icon and whether any items have a checkbox, and store the offsets for each.
	float icon_ofs = 0.0;
	bool has_check = false;
	for (int i = 0; i < items.size(); i++) {
		if (items[i].separator) {
			continue;
		}

		Size2 icon_size = _get_item_icon_size(i);
		icon_ofs = MAX(icon_size.width, icon_ofs);

		if (items[i].checkable_type) {
			has_check = true;
		}
	}
	if (icon_ofs > 0.0) {
		icon_ofs += theme_cache.h_separation;
	}

	float check_ofs = 0.0;
	if (has_check) {
		for (int i = 0; i < 4; i++) {
			check_ofs = MAX(check_ofs, check[i]->get_width());
			check_ofs = MAX(check_ofs, uncheck[i]->get_width());
		}
		check_ofs += theme_cache.h_separation;
	}

	Point2 ofs;

	// Loop through all items and draw each.
	for (int i = 0; i < items.size(); i++) {
		// For the first item only add half a separation. For all other items, add a whole separation to the offset.
		ofs.y += i > 0 ? theme_cache.v_separation : (float)theme_cache.v_separation / 2;

		_shape_item(i);

		Point2 item_ofs = ofs;
		Size2 icon_size = _get_item_icon_size(i);
		float h = _get_item_height(i);

		if (i == mouse_over) {
			theme_cache.hover_style->draw(ci, Rect2(item_ofs + Point2(0, -theme_cache.v_separation / 2), Size2(display_width, h + theme_cache.v_separation)));
		}

		String text = items[i].xl_text;

		// Separator
		item_ofs.x += items[i].indent * theme_cache.indent;
		if (items[i].separator) {
			if (!text.is_empty() || items[i].icon.is_valid()) {
				int content_size = items[i].text_buf->get_size().width + theme_cache.h_separation * 2;
				if (items[i].icon.is_valid()) {
					content_size += icon_size.width + theme_cache.h_separation;
				}

				int content_center = display_width / 2;
				int content_left = content_center - content_size / 2;
				int content_right = content_center + content_size / 2;
				if (content_left > item_ofs.x) {
					int sep_h = theme_cache.labeled_separator_left->get_minimum_size().height;
					int sep_ofs = Math::floor((h - sep_h) / 2.0);
					theme_cache.labeled_separator_left->draw(ci, Rect2(item_ofs + Point2(0, sep_ofs), Size2(MAX(0, content_left - item_ofs.x), sep_h)));
				}
				if (content_right < display_width) {
					int sep_h = theme_cache.labeled_separator_right->get_minimum_size().height;
					int sep_ofs = Math::floor((h - sep_h) / 2.0);
					theme_cache.labeled_separator_right->draw(ci, Rect2(Point2(content_right, item_ofs.y + sep_ofs), Size2(MAX(0, display_width - content_right), sep_h)));
				}
			} else {
				int sep_h = theme_cache.separator_style->get_minimum_size().height;
				int sep_ofs = Math::floor((h - sep_h) / 2.0);
				theme_cache.separator_style->draw(ci, Rect2(item_ofs + Point2(0, sep_ofs), Size2(display_width, sep_h)));
			}
		}

		Color icon_color = items[i].icon_modulate;

		// For non-separator items, add some padding for the content.
		if (!items[i].separator) {
			item_ofs.x += theme_cache.item_start_padding;
		}

		// Checkboxes
		if (items[i].checkable_type && !items[i].separator) {
			int disabled = int(items[i].disabled) * 2;
			Texture2D *icon = (items[i].checked ? check[items[i].checkable_type - 1 + disabled] : uncheck[items[i].checkable_type - 1 + disabled]).ptr();
			if (rtl) {
				icon->draw(ci, Size2(control->get_size().width - item_ofs.x - icon->get_width(), item_ofs.y) + Point2(0, Math::floor((h - icon->get_height()) / 2.0)), icon_color);
			} else {
				icon->draw(ci, item_ofs + Point2(0, Math::floor((h - icon->get_height()) / 2.0)), icon_color);
			}
		}

		int separator_ofs = (display_width - items[i].text_buf->get_size().width) / 2;

		// Icon
		if (items[i].icon.is_valid()) {
			const Point2 icon_offset = Point2(0, Math::floor((h - icon_size.height) / 2.0));
			Point2 icon_pos;

			if (items[i].separator) {
				separator_ofs -= (icon_size.width + theme_cache.h_separation) / 2;

				if (rtl) {
					icon_pos = Size2(control->get_size().width - item_ofs.x - separator_ofs - icon_size.width, item_ofs.y);
				} else {
					icon_pos = item_ofs + Size2(separator_ofs, 0);
					separator_ofs += icon_size.width + theme_cache.h_separation;
				}
			} else {
				if (rtl) {
					icon_pos = Size2(control->get_size().width - item_ofs.x - check_ofs - icon_size.width, item_ofs.y);
				} else {
					icon_pos = item_ofs + Size2(check_ofs, 0);
				}
			}

			items[i].icon->draw_rect(ci, Rect2(icon_pos + icon_offset, icon_size), false, icon_color);
		}

		// Submenu arrow on right hand side.
		if (items[i].submenu) {
			if (rtl) {
				submenu->draw(ci, Point2(theme_cache.panel_style->get_margin(SIDE_LEFT) + theme_cache.item_end_padding, item_ofs.y + Math::floor(h - submenu->get_height()) / 2), icon_color);
			} else {
				submenu->draw(ci, Point2(display_width - theme_cache.panel_style->get_margin(SIDE_RIGHT) - submenu->get_width() - theme_cache.item_end_padding, item_ofs.y + Math::floor(h - submenu->get_height()) / 2), icon_color);
			}
		}

		// Text
		if (items[i].separator) {
			if (!text.is_empty()) {
				Vector2 text_pos = Point2(separator_ofs, item_ofs.y + Math::floor((h - items[i].text_buf->get_size().y) / 2.0));

				if (theme_cache.font_separator_outline_size > 0 && theme_cache.font_separator_outline_color.a > 0) {
					items[i].text_buf->draw_outline(ci, text_pos, theme_cache.font_separator_outline_size, theme_cache.font_separator_outline_color);
				}
				items[i].text_buf->draw(ci, text_pos, theme_cache.font_separator_color);
			}
		} else {
			item_ofs.x += icon_ofs + check_ofs;

			if (rtl) {
				Vector2 text_pos = Size2(control->get_size().width - items[i].text_buf->get_size().width - item_ofs.x, item_ofs.y) + Point2(0, Math::floor((h - items[i].text_buf->get_size().y) / 2.0));
				if (theme_cache.font_outline_size > 0 && theme_cache.font_outline_color.a > 0) {
					items[i].text_buf->draw_outline(ci, text_pos, theme_cache.font_outline_size, theme_cache.font_outline_color);
				}
				items[i].text_buf->draw(ci, text_pos, items[i].disabled ? theme_cache.font_disabled_color : (i == mouse_over ? theme_cache.font_hover_color : theme_cache.font_color));
			} else {
				Vector2 text_pos = item_ofs + Point2(0, Math::floor((h - items[i].text_buf->get_size().y) / 2.0));
				if (theme_cache.font_outline_size > 0 && theme_cache.font_outline_color.a > 0) {
					items[i].text_buf->draw_outline(ci, text_pos, theme_cache.font_outline_size, theme_cache.font_outline_color);
				}
				items[i].text_buf->draw(ci, text_pos, items[i].disabled ? theme_cache.font_disabled_color : (i == mouse_over ? theme_cache.font_hover_color : theme_cache.font_color));
			}
		}

		// Accelerator / Shortcut
		if (items[i].accel != Key::NONE || (items[i].shortcut.is_valid() && items[i].shortcut->has_valid_event())) {
			if (rtl) {
				item_ofs.x = theme_cache.panel_style->get_margin(SIDE_LEFT) + theme_cache.item_end_padding;
			} else {
				item_ofs.x = display_width - theme_cache.panel_style->get_margin(SIDE_RIGHT) - items[i].accel_text_buf->get_size().x - theme_cache.item_end_padding;
			}
			Vector2 text_pos = item_ofs + Point2(0, Math::floor((h - items[i].text_buf->get_size().y) / 2.0));
			if (theme_cache.font_outline_size > 0 && theme_cache.font_outline_color.a > 0) {
				items[i].accel_text_buf->draw_outline(ci, text_pos, theme_cache.font_outline_size, theme_cache.font_outline_color);
			}
			items[i].accel_text_buf->draw(ci, text_pos, i == mouse_over ? theme_cache.font_hover_color : theme_cache.font_accelerator_color);
		}

		// Cache the item vertical offset from the first item and the height.
		items.write[i]._ofs_cache = ofs.y;
		items.write[i]._height_cache = h;

		ofs.y += h;
	}
}

void PopupMenu::_minimum_lifetime_timeout() {
	close_allowed = true;
	// If the mouse still isn't in this popup after timer expires, close.
	if (!activated_by_keyboard && !get_visible_rect().has_point(get_mouse_position())) {
		_close_pressed();
	}
}

void PopupMenu::_close_pressed() {
	// Only apply minimum lifetime to submenus.
	PopupMenu *parent_pum = Object::cast_to<PopupMenu>(get_parent());
	if (!parent_pum) {
		Popup::_close_pressed();
		return;
	}

	// If the timer has expired, close. If timer is still running, do nothing.
	if (close_allowed) {
		close_allowed = false;
		Popup::_close_pressed();
	} else if (minimum_lifetime_timer->is_stopped()) {
		minimum_lifetime_timer->start();
	}
}

void PopupMenu::_shape_item(int p_idx) const {
	if (items.write[p_idx].dirty) {
		items.write[p_idx].text_buf->clear();

		Ref<Font> font = items[p_idx].separator ? theme_cache.font_separator : theme_cache.font;
		int font_size = items[p_idx].separator ? theme_cache.font_separator_size : theme_cache.font_size;

		if (items[p_idx].text_direction == Control::TEXT_DIRECTION_INHERITED) {
			items.write[p_idx].text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
		} else {
			items.write[p_idx].text_buf->set_direction((TextServer::Direction)items[p_idx].text_direction);
		}
		items.write[p_idx].text_buf->add_string(items.write[p_idx].xl_text, font, font_size, items[p_idx].language);

		items.write[p_idx].accel_text_buf->clear();
		items.write[p_idx].accel_text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
		items.write[p_idx].accel_text_buf->add_string(_get_accel_text(items.write[p_idx]), font, font_size);
		items.write[p_idx].dirty = false;
	}
}

void PopupMenu::_menu_changed() {
	emit_signal(SNAME("menu_changed"));
}

void PopupMenu::add_child_notify(Node *p_child) {
	Window::add_child_notify(p_child);

	if (global_menu.is_valid()) {
		PopupMenu *pm = Object::cast_to<PopupMenu>(p_child);
		for (int i = 0; i < items.size(); i++) {
			if (items[i].submenu == p_child) {
				RID submenu_rid = pm->bind_global_menu();
				NativeMenu::get_singleton()->set_item_submenu(global_menu, i, submenu_rid);
				items.write[i].submenu_bound = true;
			}
		}
	}
	_menu_changed();
}

void PopupMenu::remove_child_notify(Node *p_child) {
	Window::remove_child_notify(p_child);

	PopupMenu *pm = Object::cast_to<PopupMenu>(p_child);
	if (!pm) {
		return;
	}
	if (Object::cast_to<PopupMenu>(p_child) && global_menu.is_valid()) {
		for (int i = 0; i < items.size(); i++) {
			if (items[i].submenu == p_child) {
				NativeMenu::get_singleton()->set_item_submenu(global_menu, i, RID());
				items.write[i].submenu_bound = false;
			}
		}
		pm->unbind_global_menu();
	}
	_menu_changed();
}

void PopupMenu::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			PopupMenu *pm = Object::cast_to<PopupMenu>(get_parent());
			if (pm) {
				// Inherit submenu's popup delay time from parent menu.
				float pm_delay = pm->get_submenu_popup_delay();
				set_submenu_popup_delay(pm_delay);
			}
			if (system_menu_id != NativeMenu::INVALID_MENU_ID) {
				bind_global_menu();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (system_menu_id != NativeMenu::INVALID_MENU_ID) {
				unbind_global_menu();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			scroll_container->add_theme_style_override(SceneStringName(panel), theme_cache.panel_style);

			[[fallthrough]];
		}
		case Control::NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			NativeMenu *nmenu = NativeMenu::get_singleton();
			bool is_global = global_menu.is_valid();
			if (is_global) {
				nmenu->set_interface_direction(global_menu, control->is_layout_rtl());
			}
			for (int i = 0; i < items.size(); i++) {
				Item &item = items.write[i];
				item.xl_text = atr(item.text);
				item.dirty = true;
				if (is_global) {
					nmenu->set_item_text(global_menu, i, item.xl_text);
				}
				_shape_item(i);
			}

			child_controls_changed();
			_menu_changed();
			control->queue_redraw();
		} break;

		case NOTIFICATION_WM_MOUSE_ENTER: {
			grab_focus();
		} break;

		case NOTIFICATION_WM_MOUSE_EXIT: {
			if (mouse_over >= 0 && (!items[mouse_over].submenu || submenu_over != -1)) {
				mouse_over = -1;
				control->queue_redraw();
			}
		} break;

		case NOTIFICATION_POST_POPUP: {
			popup_time_msec = OS::get_singleton()->get_ticks_msec();
			initial_button_mask = Input::get_singleton()->get_mouse_button_mask();
			during_grabbed_click = (bool)initial_button_mask;
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			Input *input = Input::get_singleton();

			if (input->is_action_just_released("ui_up") || input->is_action_just_released("ui_down")) {
				gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;
				set_process_internal(false);
				return;
			}
			gamepad_event_delay_ms -= get_process_delta_time();
			if (gamepad_event_delay_ms <= 0) {
				if (input->is_action_pressed("ui_down")) {
					gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
					int search_from = mouse_over + 1;
					if (search_from >= items.size()) {
						search_from = 0;
					}

					bool match_found = false;
					for (int i = search_from; i < items.size(); i++) {
						if (!items[i].separator && !items[i].disabled) {
							mouse_over = i;
							emit_signal(SNAME("id_focused"), items[i].id);
							scroll_to_item(i);
							control->queue_redraw();
							match_found = true;
							break;
						}
					}

					if (!match_found) {
						// If the last item is not selectable, try re-searching from the start.
						for (int i = 0; i < search_from; i++) {
							if (!items[i].separator && !items[i].disabled) {
								mouse_over = i;
								emit_signal(SNAME("id_focused"), items[i].id);
								scroll_to_item(i);
								control->queue_redraw();
								break;
							}
						}
					}
				}

				if (input->is_action_pressed("ui_up")) {
					gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
					int search_from = mouse_over - 1;
					if (search_from < 0) {
						search_from = items.size() - 1;
					}

					bool match_found = false;
					for (int i = search_from; i >= 0; i--) {
						if (!items[i].separator && !items[i].disabled) {
							mouse_over = i;
							emit_signal(SNAME("id_focused"), items[i].id);
							scroll_to_item(i);
							control->queue_redraw();
							match_found = true;
							break;
						}
					}

					if (!match_found) {
						// If the first item is not selectable, try re-searching from the end.
						for (int i = items.size() - 1; i >= search_from; i--) {
							if (!items[i].separator && !items[i].disabled) {
								mouse_over = i;
								emit_signal(SNAME("id_focused"), items[i].id);
								scroll_to_item(i);
								control->queue_redraw();
								break;
							}
						}
					}
				}
			}

			// Only used when using operating system windows.
			if (!activated_by_keyboard && !is_embedded() && autohide_areas.size()) {
				Point2 mouse_pos = DisplayServer::get_singleton()->mouse_get_position();
				mouse_pos -= get_position();

				for (const Rect2 &E : autohide_areas) {
					if (!Rect2(Point2(), get_size()).has_point(mouse_pos) && E.has_point(mouse_pos)) {
						_close_pressed();
						return;
					}
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				if (mouse_over >= 0) {
					mouse_over = -1;
					control->queue_redraw();
				}

				for (int i = 0; i < items.size(); i++) {
					if (!items[i].submenu) {
						continue;
					}
					items[i].submenu->hide();
				}

				set_process_internal(false);
			} else {
				if (!is_embedded()) {
					set_process_internal(true);
				}
			}
		} break;
	}
}

/* Methods to add items with or without icon, checkbox, shortcut.
 * Be sure to keep them in sync when adding new properties in the Item struct.
 */

#define ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel) \
	item.text = p_label;                              \
	item.xl_text = atr(p_label);                      \
	item.id = p_id == -1 ? items.size() : p_id;       \
	item.accel = p_accel;

void PopupMenu::add_item(const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.icon = p_icon;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_check_item(const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.checkable_type = Item::CHECKABLE_TYPE_CHECK_BOX;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_check_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.icon = p_icon;
	item.checkable_type = Item::CHECKABLE_TYPE_CHECK_BOX;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
		nmenu->set_item_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_radio_check_item(const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.checkable_type = Item::CHECKABLE_TYPE_RADIO_BUTTON;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_radio_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_radio_check_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.icon = p_icon;
	item.checkable_type = Item::CHECKABLE_TYPE_RADIO_BUTTON;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
		nmenu->set_item_radio_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_multistate_item(const String &p_label, int p_max_states, int p_default_state, int p_id, Key p_accel) {
	Item item;
	ITEM_SETUP_WITH_ACCEL(p_label, p_id, p_accel);
	item.max_states = p_max_states;
	item.state = p_default_state;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		if (item.accel != Key::NONE) {
			nmenu->set_item_accelerator(global_menu, index, item.accel);
		}
		nmenu->set_item_max_states(global_menu, index, item.max_states);
		nmenu->set_item_state(global_menu, index, item.state);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	_menu_changed();
	notify_property_list_changed();
}

#define ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, p_allow_echo)             \
	ERR_FAIL_COND_MSG(p_shortcut.is_null(), "Cannot add item with invalid Shortcut."); \
	_ref_shortcut(p_shortcut);                                                         \
	item.text = p_shortcut->get_name();                                                \
	item.xl_text = atr(item.text);                                                     \
	item.id = p_id == -1 ? items.size() : p_id;                                        \
	item.shortcut = p_shortcut;                                                        \
	item.shortcut_is_global = p_global;                                                \
	item.allow_echo = p_allow_echo;

void PopupMenu::add_shortcut(const Ref<Shortcut> &p_shortcut, int p_id, bool p_global, bool p_allow_echo) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, p_allow_echo);
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id, bool p_global, bool p_allow_echo) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, p_allow_echo);
	item.icon = p_icon;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_check_shortcut(const Ref<Shortcut> &p_shortcut, int p_id, bool p_global) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, false); // Echo for check shortcuts doesn't make sense.
	item.checkable_type = Item::CHECKABLE_TYPE_CHECK_BOX;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
		nmenu->set_item_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_check_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id, bool p_global) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, false);
	item.icon = p_icon;
	item.checkable_type = Item::CHECKABLE_TYPE_CHECK_BOX;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
		nmenu->set_item_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_radio_check_shortcut(const Ref<Shortcut> &p_shortcut, int p_id, bool p_global) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, false);
	item.checkable_type = Item::CHECKABLE_TYPE_RADIO_BUTTON;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
		nmenu->set_item_radio_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_icon_radio_check_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id, bool p_global) {
	Item item;
	ITEM_SETUP_WITH_SHORTCUT(p_shortcut, p_id, p_global, false);
	item.icon = p_icon;
	item.checkable_type = Item::CHECKABLE_TYPE_RADIO_BUTTON;
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), p_global ? callable_mp(this, &PopupMenu::activate_item) : Callable(), items.size() - 1);
		if (!item.shortcut_is_disabled && item.shortcut.is_valid() && item.shortcut->has_valid_event()) {
			Array events = item.shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(index, ie)) {
					break;
				}
			}
		}
		nmenu->set_item_icon(global_menu, index, item.icon);
		nmenu->set_item_radio_checkable(global_menu, index, true);
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::add_submenu_item(const String &p_label, const String &p_submenu, int p_id) {
	PopupMenu *pm = Object::cast_to<PopupMenu>(get_node_or_null(p_submenu));
	ERR_FAIL_NULL_MSG(pm, vformat("Child PopupMenu \"%s\" does not exist.", p_submenu));

	add_submenu_node_item(p_label, pm, p_id);
}

void PopupMenu::add_submenu_node_item(const String &p_label, PopupMenu *p_submenu, int p_id) {
	ERR_FAIL_NULL(p_submenu);

	if (p_submenu->get_parent() != this) {
		ERR_FAIL_COND_MSG(p_submenu->get_parent() != nullptr, vformat("The submenu \"%s\" already has a different parent.", p_submenu->get_name()));
		add_child(p_submenu);
	}

	Item item;
	item.text = p_label;
	item.xl_text = atr(p_label);
	item.id = p_id == -1 ? items.size() : p_id;
	item.submenu = p_submenu;
	item.submenu_name = p_submenu->get_name();
	items.push_back(item);

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		int index = nmenu->add_item(global_menu, item.xl_text, callable_mp(this, &PopupMenu::activate_item), Callable(), items.size() - 1);
		RID submenu_rid = p_submenu->bind_global_menu();
		nmenu->set_item_submenu(global_menu, index, submenu_rid);
		items.write[index].submenu_bound = true;
	}

	_shape_item(items.size() - 1);
	control->queue_redraw();

	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

#undef ITEM_SETUP_WITH_ACCEL
#undef ITEM_SETUP_WITH_SHORTCUT

/* Methods to modify existing items. */

void PopupMenu::set_item_text(int p_idx, const String &p_text) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());
	if (items[p_idx].text == p_text) {
		return;
	}
	items.write[p_idx].text = p_text;
	items.write[p_idx].xl_text = atr(p_text);
	items.write[p_idx].dirty = true;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_text(global_menu, p_idx, items[p_idx].xl_text);
	}
	_shape_item(p_idx);

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_text_direction(int p_idx, Control::TextDirection p_text_direction) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (items[p_idx].text_direction != p_text_direction) {
		items.write[p_idx].text_direction = p_text_direction;
		items.write[p_idx].dirty = true;
		control->queue_redraw();
	}
}

void PopupMenu::set_item_language(int p_idx, const String &p_language) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());
	if (items[p_idx].language != p_language) {
		items.write[p_idx].language = p_language;
		items.write[p_idx].dirty = true;
		control->queue_redraw();
	}
}

void PopupMenu::set_item_icon(int p_idx, const Ref<Texture2D> &p_icon) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].icon == p_icon) {
		return;
	}

	items.write[p_idx].icon = p_icon;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_icon(global_menu, p_idx, items[p_idx].icon);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_icon_max_width(int p_idx, int p_width) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].icon_max_width == p_width) {
		return;
	}

	items.write[p_idx].icon_max_width = p_width;

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_icon_modulate(int p_idx, const Color &p_modulate) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].icon_modulate == p_modulate) {
		return;
	}

	items.write[p_idx].icon_modulate = p_modulate;
	control->queue_redraw();
}

void PopupMenu::set_item_checked(int p_idx, bool p_checked) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].checked == p_checked) {
		return;
	}

	items.write[p_idx].checked = p_checked;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_checked(global_menu, p_idx, p_checked);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_id(int p_idx, int p_id) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].id == p_id) {
		return;
	}

	items.write[p_idx].id = p_id;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_tag(global_menu, p_idx, p_id);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_accelerator(int p_idx, Key p_accel) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].accel == p_accel) {
		return;
	}

	items.write[p_idx].accel = p_accel;
	items.write[p_idx].dirty = true;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_accelerator(global_menu, p_idx, p_accel);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_metadata(int p_idx, const Variant &p_meta) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].metadata == p_meta) {
		return;
	}

	items.write[p_idx].metadata = p_meta;
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_disabled(int p_idx, bool p_disabled) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].disabled == p_disabled) {
		return;
	}

	items.write[p_idx].disabled = p_disabled;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_disabled(global_menu, p_idx, p_disabled);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_submenu(int p_idx, const String &p_submenu) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].submenu_name == p_submenu) {
		return;
	}

	PopupMenu *pm = Object::cast_to<PopupMenu>(get_node_or_null(p_submenu));
	ERR_FAIL_NULL_MSG(pm, vformat("Child PopupMenu \"%s\" does not exist.", p_submenu));

	set_item_submenu_node(p_idx, pm);
}

void PopupMenu::set_item_submenu_node(int p_idx, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (p_submenu->get_parent() != this) {
		ERR_FAIL_COND_MSG(p_submenu->get_parent() != nullptr, vformat("The submenu \"%s\" already has a different parent.", p_submenu->get_name()));
		add_child(p_submenu);
	}

	if (global_menu.is_valid()) {
		if (items[p_idx].submenu_bound) {
			PopupMenu *pm = items[p_idx].submenu;
			if (pm) {
				NativeMenu::get_singleton()->set_item_submenu(global_menu, p_idx, RID());
				pm->unbind_global_menu();
			}
			items.write[p_idx].submenu_bound = false;
		}
	}

	items.write[p_idx].submenu = p_submenu;

	if (global_menu.is_valid()) {
		PopupMenu *pm = items[p_idx].submenu;
		if (pm) {
			RID submenu_rid = pm->bind_global_menu();
			NativeMenu::get_singleton()->set_item_submenu(global_menu, p_idx, submenu_rid);
			items.write[p_idx].submenu_bound = true;
		}
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::toggle_item_checked(int p_idx) {
	ERR_FAIL_INDEX(p_idx, items.size());
	items.write[p_idx].checked = !items[p_idx].checked;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_checked(global_menu, p_idx, items[p_idx].checked);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

String PopupMenu::get_item_text(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), "");
	return items[p_idx].text;
}

String PopupMenu::get_item_xl_text(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), "");
	return items[p_idx].xl_text;
}

Control::TextDirection PopupMenu::get_item_text_direction(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Control::TEXT_DIRECTION_INHERITED);
	return items[p_idx].text_direction;
}

String PopupMenu::get_item_language(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), "");
	return items[p_idx].language;
}

int PopupMenu::get_item_idx_from_text(const String &text) const {
	for (int idx = 0; idx < items.size(); idx++) {
		if (items[idx].text == text) {
			return idx;
		}
	}

	return -1;
}

Ref<Texture2D> PopupMenu::get_item_icon(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Ref<Texture2D>());
	return items[p_idx].icon;
}

int PopupMenu::get_item_icon_max_width(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), 0);
	return items[p_idx].icon_max_width;
}

Color PopupMenu::get_item_icon_modulate(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Color());
	return items[p_idx].icon_modulate;
}

Key PopupMenu::get_item_accelerator(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Key::NONE);
	return items[p_idx].accel;
}

Variant PopupMenu::get_item_metadata(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Variant());
	return items[p_idx].metadata;
}

bool PopupMenu::is_item_disabled(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].disabled;
}

bool PopupMenu::is_item_checked(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].checked;
}

int PopupMenu::get_item_id(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), 0);
	return items[p_idx].id;
}

int PopupMenu::get_item_index(int p_id) const {
	for (int i = 0; i < items.size(); i++) {
		if (items[i].id == p_id) {
			return i;
		}
	}

	return -1;
}

String PopupMenu::get_item_submenu(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), "");
	return items[p_idx].submenu_name;
}

PopupMenu *PopupMenu::get_item_submenu_node(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), nullptr);
	return items[p_idx].submenu;
}

String PopupMenu::get_item_tooltip(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), "");
	return items[p_idx].tooltip;
}

Ref<Shortcut> PopupMenu::get_item_shortcut(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), Ref<Shortcut>());
	return items[p_idx].shortcut;
}

int PopupMenu::get_item_indent(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), 0);
	return items[p_idx].indent;
}

int PopupMenu::get_item_max_states(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), -1);
	return items[p_idx].max_states;
}

int PopupMenu::get_item_state(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), -1);
	return items[p_idx].state;
}

void PopupMenu::set_item_as_separator(int p_idx, bool p_separator) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].separator == p_separator) {
		return;
	}

	items.write[p_idx].separator = p_separator;
	control->queue_redraw();
}

bool PopupMenu::is_item_separator(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].separator;
}

void PopupMenu::set_item_as_checkable(int p_idx, bool p_checkable) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	int type = (int)(p_checkable ? Item::CHECKABLE_TYPE_CHECK_BOX : Item::CHECKABLE_TYPE_NONE);
	if (type == items[p_idx].checkable_type) {
		return;
	}

	items.write[p_idx].checkable_type = p_checkable ? Item::CHECKABLE_TYPE_CHECK_BOX : Item::CHECKABLE_TYPE_NONE;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_checkable(global_menu, p_idx, p_checkable);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_as_radio_checkable(int p_idx, bool p_radio_checkable) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	int type = (int)(p_radio_checkable ? Item::CHECKABLE_TYPE_RADIO_BUTTON : Item::CHECKABLE_TYPE_NONE);
	if (type == items[p_idx].checkable_type) {
		return;
	}

	items.write[p_idx].checkable_type = p_radio_checkable ? Item::CHECKABLE_TYPE_RADIO_BUTTON : Item::CHECKABLE_TYPE_NONE;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_radio_checkable(global_menu, p_idx, p_radio_checkable);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_tooltip(int p_idx, const String &p_tooltip) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].tooltip == p_tooltip) {
		return;
	}

	items.write[p_idx].tooltip = p_tooltip;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_tooltip(global_menu, p_idx, p_tooltip);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_shortcut(int p_idx, const Ref<Shortcut> &p_shortcut, bool p_global) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].shortcut == p_shortcut && items[p_idx].shortcut_is_global == p_global && items[p_idx].shortcut.is_valid() == p_shortcut.is_valid()) {
		return;
	}

	if (items[p_idx].shortcut.is_valid()) {
		_unref_shortcut(items[p_idx].shortcut);
	}
	items.write[p_idx].shortcut = p_shortcut;
	items.write[p_idx].shortcut_is_global = p_global;
	items.write[p_idx].dirty = true;

	if (items[p_idx].shortcut.is_valid()) {
		_ref_shortcut(items[p_idx].shortcut);
	}

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		nmenu->set_item_accelerator(global_menu, p_idx, Key::NONE);
		if (!items[p_idx].shortcut_is_disabled && items[p_idx].shortcut.is_valid() && items[p_idx].shortcut->has_valid_event()) {
			Array events = items[p_idx].shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(p_idx, ie)) {
					break;
				}
			}
			if (p_global) {
				nmenu->set_item_key_callback(global_menu, p_idx, callable_mp(this, &PopupMenu::activate_item));
			} else {
				nmenu->set_item_key_callback(global_menu, p_idx, Callable());
			}
		}
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_indent(int p_idx, int p_indent) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items.write[p_idx].indent == p_indent) {
		return;
	}
	items.write[p_idx].indent = p_indent;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_indentation_level(global_menu, p_idx, p_indent);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::set_item_max_states(int p_idx, int p_max_states) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].max_states == p_max_states) {
		return;
	}

	items.write[p_idx].max_states = p_max_states;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_max_states(global_menu, p_idx, p_max_states);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_multistate(int p_idx, int p_state) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].state == p_state) {
		return;
	}

	items.write[p_idx].state = p_state;

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_state(global_menu, p_idx, p_state);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::set_item_shortcut_disabled(int p_idx, bool p_disabled) {
	if (p_idx < 0) {
		p_idx += get_item_count();
	}
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].shortcut_is_disabled == p_disabled) {
		return;
	}

	items.write[p_idx].shortcut_is_disabled = p_disabled;

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		nmenu->set_item_accelerator(global_menu, p_idx, Key::NONE);
		if (!items[p_idx].shortcut_is_disabled && items[p_idx].shortcut.is_valid() && items[p_idx].shortcut->has_valid_event()) {
			Array events = items[p_idx].shortcut->get_events();
			for (int j = 0; j < events.size(); j++) {
				Ref<InputEventKey> ie = events[j];
				if (ie.is_valid() && _set_item_accelerator(p_idx, ie)) {
					break;
				}
			}
		}
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::toggle_item_multistate(int p_idx) {
	ERR_FAIL_INDEX(p_idx, items.size());
	if (0 >= items[p_idx].max_states) {
		return;
	}

	++items.write[p_idx].state;
	if (items.write[p_idx].max_states <= items[p_idx].state) {
		items.write[p_idx].state = 0;
	}

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->set_item_state(global_menu, p_idx, items[p_idx].state);
	}

	control->queue_redraw();
	_menu_changed();
}

bool PopupMenu::is_item_checkable(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].checkable_type;
}

bool PopupMenu::is_item_radio_checkable(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].checkable_type == Item::CHECKABLE_TYPE_RADIO_BUTTON;
}

bool PopupMenu::is_item_shortcut_global(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].shortcut_is_global;
}

bool PopupMenu::is_item_shortcut_disabled(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, items.size(), false);
	return items[p_idx].shortcut_is_disabled;
}

void PopupMenu::set_focused_item(int p_idx) {
	if (p_idx != -1) {
		ERR_FAIL_INDEX(p_idx, items.size());
	}

	if (mouse_over == p_idx) {
		return;
	}

	mouse_over = p_idx;
	if (mouse_over != -1) {
		scroll_to_item(mouse_over);
	}

	control->queue_redraw();
}

int PopupMenu::get_focused_item() const {
	return mouse_over;
}

void PopupMenu::set_item_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);
	int prev_size = items.size();

	if (prev_size == p_count) {
		return;
	}

	NativeMenu *nmenu = NativeMenu::get_singleton();
	bool is_global = global_menu.is_valid();

	if (is_global && prev_size > p_count) {
		for (int i = prev_size - 1; i >= p_count; i--) {
			nmenu->remove_item(global_menu, i);
		}
	}

	items.resize(p_count);

	if (prev_size < p_count) {
		for (int i = prev_size; i < p_count; i++) {
			items.write[i].id = i;
			if (is_global) {
				nmenu->add_item(global_menu, String(), callable_mp(this, &PopupMenu::activate_item), Callable(), i);
			}
		}
	}

	control->queue_redraw();
	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

int PopupMenu::get_item_count() const {
	return items.size();
}

void PopupMenu::scroll_to_item(int p_idx) {
	ERR_FAIL_INDEX(p_idx, items.size());

	// Calculate the position of the item relative to the visible area.
	int item_y = items[p_idx]._ofs_cache;
	int visible_height = scroll_container->get_size().height;
	int relative_y = item_y - scroll_container->get_v_scroll();

	// If item is not fully visible, adjust scroll.
	if (relative_y < 0) {
		scroll_container->set_v_scroll(item_y);
	} else if (relative_y + items[p_idx]._height_cache > visible_height) {
		scroll_container->set_v_scroll(item_y + items[p_idx]._height_cache - visible_height);
	}
}

void PopupMenu::set_prefer_native_menu(bool p_enabled) {
	if (prefer_native != p_enabled) {
		prefer_native = p_enabled;
		if (prefer_native) {
			bind_global_menu();
		} else {
			unbind_global_menu();
		}
	}
}

bool PopupMenu::is_prefer_native_menu() const {
	return prefer_native;
}

bool PopupMenu::is_native_menu() const {
#ifdef TOOLS_ENABLED
	if (is_part_of_edited_scene()) {
		return false;
	}
#endif

	return global_menu.is_valid();
}

bool PopupMenu::activate_item_by_event(const Ref<InputEvent> &p_event, bool p_for_global_only) {
	ERR_FAIL_COND_V(p_event.is_null(), false);
	Key code = Key::NONE;
	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		code = k->get_keycode();
		if (code == Key::NONE) {
			code = (Key)k->get_unicode();
		}
		if (k->is_ctrl_pressed()) {
			code |= KeyModifierMask::CTRL;
		}
		if (k->is_alt_pressed()) {
			code |= KeyModifierMask::ALT;
		}
		if (k->is_meta_pressed()) {
			code |= KeyModifierMask::META;
		}
		if (k->is_shift_pressed()) {
			code |= KeyModifierMask::SHIFT;
		}
	}

	for (int i = 0; i < items.size(); i++) {
		if (is_item_disabled(i) || items[i].shortcut_is_disabled || (!items[i].allow_echo && p_event->is_echo())) {
			continue;
		}

		if (items[i].shortcut.is_valid() && items[i].shortcut->matches_event(p_event) && (items[i].shortcut_is_global || !p_for_global_only)) {
			activate_item(i);
			return true;
		}

		if (code != Key::NONE && items[i].accel == code) {
			activate_item(i);
			return true;
		}

		if (items[i].submenu) {
			if (items[i].submenu->activate_item_by_event(p_event, p_for_global_only)) {
				return true;
			}
		}
	}
	return false;
}

void PopupMenu::_about_to_popup() {
	ERR_MAIN_THREAD_GUARD;
	emit_signal(SNAME("about_to_popup"));
}

void PopupMenu::_about_to_close() {
	ERR_MAIN_THREAD_GUARD;
	emit_signal(SNAME("popup_hide"));
}

void PopupMenu::activate_item(int p_idx) {
	ERR_FAIL_INDEX(p_idx, items.size());
	ERR_FAIL_COND(items[p_idx].separator);
	int id = items[p_idx].id >= 0 ? items[p_idx].id : p_idx;

	//hide all parent PopupMenus
	Node *next = get_parent();
	PopupMenu *pop = Object::cast_to<PopupMenu>(next);
	while (pop) {
		// We close all parents that are chained together,
		// with hide_on_item_selection enabled

		if (items[p_idx].checkable_type) {
			if (!hide_on_checkable_item_selection || !pop->is_hide_on_checkable_item_selection()) {
				break;
			}
		} else if (0 < items[p_idx].max_states) {
			if (!hide_on_multistate_item_selection || !pop->is_hide_on_multistate_item_selection()) {
				break;
			}
		} else if (!hide_on_item_selection || !pop->is_hide_on_item_selection()) {
			break;
		}

		pop->hide();
		next = next->get_parent();
		pop = Object::cast_to<PopupMenu>(next);
	}

	// Hides popup by default; unless otherwise specified
	// by using set_hide_on_item_selection and set_hide_on_checkable_item_selection

	bool need_hide = true;

	if (items[p_idx].checkable_type) {
		if (!hide_on_checkable_item_selection) {
			need_hide = false;
		}
	} else if (0 < items[p_idx].max_states) {
		if (!hide_on_multistate_item_selection) {
			need_hide = false;
		}
	} else if (!hide_on_item_selection) {
		need_hide = false;
	}

	if (need_hide) {
		hide();
	}

	emit_signal(SceneStringName(id_pressed), id);
	emit_signal(SNAME("index_pressed"), p_idx);
}

void PopupMenu::remove_item(int p_idx) {
	ERR_FAIL_INDEX(p_idx, items.size());

	if (items[p_idx].shortcut.is_valid()) {
		_unref_shortcut(items[p_idx].shortcut);
	}

	items.remove_at(p_idx);

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->remove_item(global_menu, p_idx);
	}

	control->queue_redraw();
	child_controls_changed();
	_menu_changed();
}

void PopupMenu::add_separator(const String &p_text, int p_id) {
	Item sep;
	sep.separator = true;
	sep.id = p_id;
	if (!p_text.is_empty()) {
		sep.text = p_text;
		sep.xl_text = atr(p_text);
	}
	items.push_back(sep);

	if (global_menu.is_valid()) {
		NativeMenu::get_singleton()->add_separator(global_menu);
	}

	control->queue_redraw();
	_menu_changed();
}

void PopupMenu::clear(bool p_free_submenus) {
	for (const Item &I : items) {
		if (I.shortcut.is_valid()) {
			_unref_shortcut(I.shortcut);
		}

		if (p_free_submenus && I.submenu) {
			remove_child(I.submenu);
			I.submenu->queue_free();
		}
	}

	if (global_menu.is_valid()) {
		NativeMenu *nmenu = NativeMenu::get_singleton();
		for (int i = items.size() - 1; i >= 0; i--) {
			Item &item = items.write[i];
			if (item.submenu) {
				item.submenu->unbind_global_menu();
				item.submenu_bound = false;
			}
			nmenu->remove_item(global_menu, i);
		}
	}
	items.clear();

	mouse_over = -1;
	control->queue_redraw();
	child_controls_changed();
	notify_property_list_changed();
	_menu_changed();
}

void PopupMenu::_ref_shortcut(Ref<Shortcut> p_sc) {
	if (!shortcut_refcount.has(p_sc)) {
		shortcut_refcount[p_sc] = 1;
		p_sc->connect_changed(callable_mp(this, &PopupMenu::_shortcut_changed));
	} else {
		shortcut_refcount[p_sc] += 1;
	}
}

void PopupMenu::_unref_shortcut(Ref<Shortcut> p_sc) {
	ERR_FAIL_COND(!shortcut_refcount.has(p_sc));
	shortcut_refcount[p_sc]--;
	if (shortcut_refcount[p_sc] == 0) {
		p_sc->disconnect_changed(callable_mp(this, &PopupMenu::_shortcut_changed));
		shortcut_refcount.erase(p_sc);
	}
}

void PopupMenu::_shortcut_changed() {
	for (int i = 0; i < items.size(); i++) {
		items.write[i].dirty = true;
	}
	control->queue_redraw();
}

// Hide on item selection determines whether or not the popup will close after item selection
void PopupMenu::set_hide_on_item_selection(bool p_enabled) {
	hide_on_item_selection = p_enabled;
}

bool PopupMenu::is_hide_on_item_selection() const {
	return hide_on_item_selection;
}

void PopupMenu::set_hide_on_checkable_item_selection(bool p_enabled) {
	hide_on_checkable_item_selection = p_enabled;
}

bool PopupMenu::is_hide_on_checkable_item_selection() const {
	return hide_on_checkable_item_selection;
}

void PopupMenu::set_hide_on_multistate_item_selection(bool p_enabled) {
	hide_on_multistate_item_selection = p_enabled;
}

bool PopupMenu::is_hide_on_multistate_item_selection() const {
	return hide_on_multistate_item_selection;
}

void PopupMenu::set_submenu_popup_delay(float p_time) {
	if (p_time <= 0) {
		p_time = 0.01;
	}

	submenu_timer->set_wait_time(p_time);
}

float PopupMenu::get_submenu_popup_delay() const {
	return submenu_timer->get_wait_time();
}

void PopupMenu::set_allow_search(bool p_allow) {
	allow_search = p_allow;
}

bool PopupMenu::get_allow_search() const {
	return allow_search;
}

String PopupMenu::get_tooltip(const Point2 &p_pos) const {
	int over = _get_mouse_over(p_pos);
	if (over < 0 || over >= items.size()) {
		return "";
	}
	return items[over].tooltip;
}

void PopupMenu::add_autohide_area(const Rect2 &p_area) {
	autohide_areas.push_back(p_area);
}

void PopupMenu::clear_autohide_areas() {
	autohide_areas.clear();
}

bool PopupMenu::_set(const StringName &p_name, const Variant &p_value) {
	if (property_helper.property_set_value(p_name, p_value)) {
		return true;
	}

#ifndef DISABLE_DEPRECATED
	// Compatibility.
	if (p_name == "items") {
		Array arr = p_value;
		ERR_FAIL_COND_V(arr.size() % 10, false);
		clear();

		for (int i = 0; i < arr.size(); i += 10) {
			String text = arr[i + 0];
			Ref<Texture2D> icon = arr[i + 1];
			// For compatibility, use false/true for no/checkbox and integers for other values
			bool checkable = arr[i + 2];
			bool radio_checkable = (int)arr[i + 2] == Item::CHECKABLE_TYPE_RADIO_BUTTON;
			bool checked = arr[i + 3];
			bool disabled = arr[i + 4];

			int id = arr[i + 5];
			int accel = arr[i + 6];
			Variant meta = arr[i + 7];
			String subm = arr[i + 8];
			bool sep = arr[i + 9];

			int idx = get_item_count();
			add_item(text, id);
			set_item_icon(idx, icon);
			if (checkable) {
				if (radio_checkable) {
					set_item_as_radio_checkable(idx, true);
				} else {
					set_item_as_checkable(idx, true);
				}
			}
			set_item_checked(idx, checked);
			set_item_disabled(idx, disabled);
			set_item_id(idx, id);
			set_item_metadata(idx, meta);
			set_item_as_separator(idx, sep);
			set_item_accelerator(idx, (Key)accel);
			set_item_submenu(idx, subm);
		}
	}
#endif
	return false;
}

void PopupMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("activate_item_by_event", "event", "for_global_only"), &PopupMenu::activate_item_by_event, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_prefer_native_menu", "enabled"), &PopupMenu::set_prefer_native_menu);
	ClassDB::bind_method(D_METHOD("is_prefer_native_menu"), &PopupMenu::is_prefer_native_menu);
	ClassDB::bind_method(D_METHOD("is_native_menu"), &PopupMenu::is_native_menu);

	ClassDB::bind_method(D_METHOD("add_item", "label", "id", "accel"), &PopupMenu::add_item, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_icon_item", "texture", "label", "id", "accel"), &PopupMenu::add_icon_item, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_check_item", "label", "id", "accel"), &PopupMenu::add_check_item, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_icon_check_item", "texture", "label", "id", "accel"), &PopupMenu::add_icon_check_item, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_radio_check_item", "label", "id", "accel"), &PopupMenu::add_radio_check_item, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_icon_radio_check_item", "texture", "label", "id", "accel"), &PopupMenu::add_icon_radio_check_item, DEFVAL(-1), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("add_multistate_item", "label", "max_states", "default_state", "id", "accel"), &PopupMenu::add_multistate_item, DEFVAL(0), DEFVAL(-1), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("add_shortcut", "shortcut", "id", "global", "allow_echo"), &PopupMenu::add_shortcut, DEFVAL(-1), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_icon_shortcut", "texture", "shortcut", "id", "global", "allow_echo"), &PopupMenu::add_icon_shortcut, DEFVAL(-1), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_check_shortcut", "shortcut", "id", "global"), &PopupMenu::add_check_shortcut, DEFVAL(-1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_icon_check_shortcut", "texture", "shortcut", "id", "global"), &PopupMenu::add_icon_check_shortcut, DEFVAL(-1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_radio_check_shortcut", "shortcut", "id", "global"), &PopupMenu::add_radio_check_shortcut, DEFVAL(-1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_icon_radio_check_shortcut", "texture", "shortcut", "id", "global"), &PopupMenu::add_icon_radio_check_shortcut, DEFVAL(-1), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("add_submenu_item", "label", "submenu", "id"), &PopupMenu::add_submenu_item, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("add_submenu_node_item", "label", "submenu", "id"), &PopupMenu::add_submenu_node_item, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("set_item_text", "index", "text"), &PopupMenu::set_item_text);
	ClassDB::bind_method(D_METHOD("set_item_text_direction", "index", "direction"), &PopupMenu::set_item_text_direction);
	ClassDB::bind_method(D_METHOD("set_item_language", "index", "language"), &PopupMenu::set_item_language);
	ClassDB::bind_method(D_METHOD("set_item_icon", "index", "icon"), &PopupMenu::set_item_icon);
	ClassDB::bind_method(D_METHOD("set_item_icon_max_width", "index", "width"), &PopupMenu::set_item_icon_max_width);
	ClassDB::bind_method(D_METHOD("set_item_icon_modulate", "index", "modulate"), &PopupMenu::set_item_icon_modulate);
	ClassDB::bind_method(D_METHOD("set_item_checked", "index", "checked"), &PopupMenu::set_item_checked);
	ClassDB::bind_method(D_METHOD("set_item_id", "index", "id"), &PopupMenu::set_item_id);
	ClassDB::bind_method(D_METHOD("set_item_accelerator", "index", "accel"), &PopupMenu::set_item_accelerator);
	ClassDB::bind_method(D_METHOD("set_item_metadata", "index", "metadata"), &PopupMenu::set_item_metadata);
	ClassDB::bind_method(D_METHOD("set_item_disabled", "index", "disabled"), &PopupMenu::set_item_disabled);
	ClassDB::bind_method(D_METHOD("set_item_submenu", "index", "submenu"), &PopupMenu::set_item_submenu);
	ClassDB::bind_method(D_METHOD("set_item_submenu_node", "index", "submenu"), &PopupMenu::set_item_submenu_node);
	ClassDB::bind_method(D_METHOD("set_item_as_separator", "index", "enable"), &PopupMenu::set_item_as_separator);
	ClassDB::bind_method(D_METHOD("set_item_as_checkable", "index", "enable"), &PopupMenu::set_item_as_checkable);
	ClassDB::bind_method(D_METHOD("set_item_as_radio_checkable", "index", "enable"), &PopupMenu::set_item_as_radio_checkable);
	ClassDB::bind_method(D_METHOD("set_item_tooltip", "index", "tooltip"), &PopupMenu::set_item_tooltip);
	ClassDB::bind_method(D_METHOD("set_item_shortcut", "index", "shortcut", "global"), &PopupMenu::set_item_shortcut, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_item_indent", "index", "indent"), &PopupMenu::set_item_indent);
	ClassDB::bind_method(D_METHOD("set_item_multistate", "index", "state"), &PopupMenu::set_item_multistate);
	ClassDB::bind_method(D_METHOD("set_item_multistate_max", "index", "max_states"), &PopupMenu::set_item_max_states);
	ClassDB::bind_method(D_METHOD("set_item_shortcut_disabled", "index", "disabled"), &PopupMenu::set_item_shortcut_disabled);

	ClassDB::bind_method(D_METHOD("toggle_item_checked", "index"), &PopupMenu::toggle_item_checked);
	ClassDB::bind_method(D_METHOD("toggle_item_multistate", "index"), &PopupMenu::toggle_item_multistate);

	ClassDB::bind_method(D_METHOD("get_item_text", "index"), &PopupMenu::get_item_text);
	ClassDB::bind_method(D_METHOD("get_item_text_direction", "index"), &PopupMenu::get_item_text_direction);
	ClassDB::bind_method(D_METHOD("get_item_language", "index"), &PopupMenu::get_item_language);
	ClassDB::bind_method(D_METHOD("get_item_icon", "index"), &PopupMenu::get_item_icon);
	ClassDB::bind_method(D_METHOD("get_item_icon_max_width", "index"), &PopupMenu::get_item_icon_max_width);
	ClassDB::bind_method(D_METHOD("get_item_icon_modulate", "index"), &PopupMenu::get_item_icon_modulate);
	ClassDB::bind_method(D_METHOD("is_item_checked", "index"), &PopupMenu::is_item_checked);
	ClassDB::bind_method(D_METHOD("get_item_id", "index"), &PopupMenu::get_item_id);
	ClassDB::bind_method(D_METHOD("get_item_index", "id"), &PopupMenu::get_item_index);
	ClassDB::bind_method(D_METHOD("get_item_accelerator", "index"), &PopupMenu::get_item_accelerator);
	ClassDB::bind_method(D_METHOD("get_item_metadata", "index"), &PopupMenu::get_item_metadata);
	ClassDB::bind_method(D_METHOD("is_item_disabled", "index"), &PopupMenu::is_item_disabled);
	ClassDB::bind_method(D_METHOD("get_item_submenu", "index"), &PopupMenu::get_item_submenu);
	ClassDB::bind_method(D_METHOD("get_item_submenu_node", "index"), &PopupMenu::get_item_submenu_node);
	ClassDB::bind_method(D_METHOD("is_item_separator", "index"), &PopupMenu::is_item_separator);
	ClassDB::bind_method(D_METHOD("is_item_checkable", "index"), &PopupMenu::is_item_checkable);
	ClassDB::bind_method(D_METHOD("is_item_radio_checkable", "index"), &PopupMenu::is_item_radio_checkable);
	ClassDB::bind_method(D_METHOD("is_item_shortcut_disabled", "index"), &PopupMenu::is_item_shortcut_disabled);
	ClassDB::bind_method(D_METHOD("get_item_tooltip", "index"), &PopupMenu::get_item_tooltip);
	ClassDB::bind_method(D_METHOD("get_item_shortcut", "index"), &PopupMenu::get_item_shortcut);
	ClassDB::bind_method(D_METHOD("get_item_indent", "index"), &PopupMenu::get_item_indent);

	ClassDB::bind_method(D_METHOD("get_item_multistate_max", "index"), &PopupMenu::get_item_max_states);
	ClassDB::bind_method(D_METHOD("get_item_multistate", "index"), &PopupMenu::get_item_state);

	ClassDB::bind_method(D_METHOD("set_focused_item", "index"), &PopupMenu::set_focused_item);
	ClassDB::bind_method(D_METHOD("get_focused_item"), &PopupMenu::get_focused_item);
	ClassDB::bind_method(D_METHOD("set_item_count", "count"), &PopupMenu::set_item_count);
	ClassDB::bind_method(D_METHOD("get_item_count"), &PopupMenu::get_item_count);

	ClassDB::bind_method(D_METHOD("scroll_to_item", "index"), &PopupMenu::scroll_to_item);

	ClassDB::bind_method(D_METHOD("remove_item", "index"), &PopupMenu::remove_item);

	ClassDB::bind_method(D_METHOD("add_separator", "label", "id"), &PopupMenu::add_separator, DEFVAL(String()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("clear", "free_submenus"), &PopupMenu::clear, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_hide_on_item_selection", "enable"), &PopupMenu::set_hide_on_item_selection);
	ClassDB::bind_method(D_METHOD("is_hide_on_item_selection"), &PopupMenu::is_hide_on_item_selection);

	ClassDB::bind_method(D_METHOD("set_hide_on_checkable_item_selection", "enable"), &PopupMenu::set_hide_on_checkable_item_selection);
	ClassDB::bind_method(D_METHOD("is_hide_on_checkable_item_selection"), &PopupMenu::is_hide_on_checkable_item_selection);

	ClassDB::bind_method(D_METHOD("set_hide_on_state_item_selection", "enable"), &PopupMenu::set_hide_on_multistate_item_selection);
	ClassDB::bind_method(D_METHOD("is_hide_on_state_item_selection"), &PopupMenu::is_hide_on_multistate_item_selection);

	ClassDB::bind_method(D_METHOD("set_submenu_popup_delay", "seconds"), &PopupMenu::set_submenu_popup_delay);
	ClassDB::bind_method(D_METHOD("get_submenu_popup_delay"), &PopupMenu::get_submenu_popup_delay);

	ClassDB::bind_method(D_METHOD("set_allow_search", "allow"), &PopupMenu::set_allow_search);
	ClassDB::bind_method(D_METHOD("get_allow_search"), &PopupMenu::get_allow_search);

	ClassDB::bind_method(D_METHOD("is_system_menu"), &PopupMenu::is_system_menu);
	ClassDB::bind_method(D_METHOD("set_system_menu", "system_menu_id"), &PopupMenu::set_system_menu);
	ClassDB::bind_method(D_METHOD("get_system_menu"), &PopupMenu::get_system_menu);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_on_item_selection"), "set_hide_on_item_selection", "is_hide_on_item_selection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_on_checkable_item_selection"), "set_hide_on_checkable_item_selection", "is_hide_on_checkable_item_selection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_on_state_item_selection"), "set_hide_on_state_item_selection", "is_hide_on_state_item_selection");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "submenu_popup_delay", PROPERTY_HINT_NONE, "suffix:s"), "set_submenu_popup_delay", "get_submenu_popup_delay");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_search"), "set_allow_search", "get_allow_search");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "system_menu_id", PROPERTY_HINT_ENUM, "None:0,Application Menu:2,Window Menu:3,Help Menu:4,Dock:5"), "set_system_menu", "get_system_menu");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "prefer_native_menu"), "set_prefer_native_menu", "is_prefer_native_menu");

	ADD_ARRAY_COUNT("Items", "item_count", "set_item_count", "get_item_count", "item_");

	ADD_SIGNAL(MethodInfo("id_pressed", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("id_focused", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("index_pressed", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("menu_changed"));

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, PopupMenu, panel_style, "panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, PopupMenu, hover_style, "hover");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, PopupMenu, separator_style, "separator");
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, PopupMenu, labeled_separator_left);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, PopupMenu, labeled_separator_right);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, v_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, indent);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, item_start_padding);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, item_end_padding);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, PopupMenu, icon_max_width);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, checked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, checked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, unchecked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, unchecked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, radio_checked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, radio_checked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, radio_unchecked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, radio_unchecked_disabled);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, submenu);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, PopupMenu, submenu_mirrored);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, PopupMenu, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, PopupMenu, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, PopupMenu, font_separator);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, PopupMenu, font_separator_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_accelerator_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, PopupMenu, font_outline_size, "outline_size");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_outline_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_separator_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, PopupMenu, font_separator_outline_size, "separator_outline_size");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, PopupMenu, font_separator_outline_color);

	Item defaults(true);

	base_property_helper.set_prefix("item_");
	base_property_helper.set_array_length_getter(&PopupMenu::get_item_count);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "text"), defaults.text, &PopupMenu::set_item_text, &PopupMenu::get_item_text);
	base_property_helper.register_property(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), defaults.icon, &PopupMenu::set_item_icon, &PopupMenu::get_item_icon);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "checkable", PROPERTY_HINT_ENUM, "No,As checkbox,As radio button"), defaults.checkable_type, &PopupMenu::_set_item_checkable_type, &PopupMenu::_get_item_checkable_type);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "checked"), defaults.checked, &PopupMenu::set_item_checked, &PopupMenu::is_item_checked);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_RANGE, "0,10,1,or_greater", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL), defaults.id, &PopupMenu::set_item_id, &PopupMenu::get_item_id);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "disabled"), defaults.disabled, &PopupMenu::set_item_disabled, &PopupMenu::is_item_disabled);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "separator"), defaults.separator, &PopupMenu::set_item_as_separator, &PopupMenu::is_item_separator);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

void PopupMenu::popup(const Rect2i &p_bounds) {
	bool native = global_menu.is_valid();
#ifdef TOOLS_ENABLED
	if (is_part_of_edited_scene()) {
		native = false;
	}
#endif

	if (native) {
		NativeMenu::get_singleton()->popup(global_menu, (p_bounds != Rect2i()) ? p_bounds.position : get_position());
	} else {
		set_flag(FLAG_NO_FOCUS, !is_embedded());

		moved = Vector2();
		popup_time_msec = OS::get_singleton()->get_ticks_msec();
		if (!is_embedded()) {
			float win_scale = get_parent_visible_window()->get_content_scale_factor();
			set_content_scale_factor(win_scale);
			Size2 minsize = get_contents_minimum_size() * win_scale;
			minsize.height = Math::ceil(minsize.height); // Ensures enough height at fractional content scales to prevent the v_scroll_bar from showing.
			set_min_size(minsize); // `height` is truncated here by the cast to Size2i for Window.min_size.
			set_size(Vector2(0, 0)); // Shrinkwraps to min size.
		}
		Popup::popup(p_bounds);
	}
}

void PopupMenu::set_visible(bool p_visible) {
	bool native = global_menu.is_valid();
#ifdef TOOLS_ENABLED
	if (is_part_of_edited_scene()) {
		native = false;
	}
#endif

	if (native) {
		if (p_visible) {
			NativeMenu::get_singleton()->popup(global_menu, get_position());
		}
	} else {
		set_flag(FLAG_NO_FOCUS, !is_embedded());

		Popup::set_visible(p_visible);
	}
}

PopupMenu::PopupMenu() {
	// Scroll Container
	scroll_container = memnew(ScrollContainer);
	scroll_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	scroll_container->set_clip_contents(true);
	add_child(scroll_container, false, INTERNAL_MODE_FRONT);

	// The control which will display the items
	control = memnew(Control);
	control->set_clip_contents(false);
	control->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	control->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll_container->add_child(control, false, INTERNAL_MODE_FRONT);
	control->connect(SceneStringName(draw), callable_mp(this, &PopupMenu::_draw_items));

	submenu_timer = memnew(Timer);
	submenu_timer->set_wait_time(0.3);
	submenu_timer->set_one_shot(true);
	submenu_timer->connect("timeout", callable_mp(this, &PopupMenu::_submenu_timeout));
	add_child(submenu_timer, false, INTERNAL_MODE_FRONT);

	minimum_lifetime_timer = memnew(Timer);
	minimum_lifetime_timer->set_wait_time(0.3);
	minimum_lifetime_timer->set_one_shot(true);
	minimum_lifetime_timer->connect("timeout", callable_mp(this, &PopupMenu::_minimum_lifetime_timeout));
	add_child(minimum_lifetime_timer, false, INTERNAL_MODE_FRONT);

	property_helper.setup_for_instance(base_property_helper, this);
}

PopupMenu::~PopupMenu() {
	unbind_global_menu();
}
