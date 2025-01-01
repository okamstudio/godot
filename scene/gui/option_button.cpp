/**************************************************************************/
/*  option_button.cpp                                                     */
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

#include "option_button.h"

#include "scene/theme/theme_db.h"

static const int NONE_SELECTED = -1;

void OptionButton::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (disable_shortcuts) {
		return;
	}

	if (p_event->is_pressed() && !p_event->is_echo() && !is_disabled() && is_visible_in_tree() && popup->activate_item_by_event(p_event, false)) {
		accept_event();
		return;
	}

	Button::shortcut_input(p_event);
}

Size2 OptionButton::get_minimum_size() const {
	Size2 minsize;
	if (fit_to_longest_item) {
		minsize = _cached_size;
	} else {
		minsize = Button::get_minimum_size();
	}

	if (has_theme_icon(SNAME("arrow"))) {
		const Size2 padding = _get_largest_stylebox_size();
		const Size2 arrow_size = theme_cache.arrow_icon->get_size();

		Size2 content_size = minsize - padding;
		content_size.width += arrow_size.width + MAX(0, theme_cache.h_separation);
		content_size.height = MAX(content_size.height, arrow_size.height);

		minsize = content_size + padding;
	}

	return minsize;
}

void OptionButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_refresh_size_cache();
			if (has_theme_icon(SNAME("arrow"))) {
				if (is_layout_rtl()) {
					_set_internal_margin(SIDE_LEFT, theme_cache.arrow_icon->get_width());
				} else {
					_set_internal_margin(SIDE_RIGHT, theme_cache.arrow_icon->get_width());
				}
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (!has_theme_icon(SNAME("arrow"))) {
				return;
			}

			RID ci = get_canvas_item();
			Color clr = Color(1, 1, 1);
			if (theme_cache.modulate_arrow) {
				switch (get_draw_mode()) {
					case DRAW_PRESSED:
						clr = theme_cache.font_pressed_color;
						break;
					case DRAW_HOVER:
						clr = theme_cache.font_hover_color;
						break;
					case DRAW_HOVER_PRESSED:
						clr = theme_cache.font_hover_pressed_color;
						break;
					case DRAW_DISABLED:
						clr = theme_cache.font_disabled_color;
						break;
					default:
						if (has_focus()) {
							clr = theme_cache.font_focus_color;
						} else {
							clr = theme_cache.font_color;
						}
				}
			}

			Size2 size = get_size();

			Point2 ofs;
			if (is_layout_rtl()) {
				ofs = Point2(theme_cache.arrow_margin, int(Math::abs((size.height - theme_cache.arrow_icon->get_height()) / 2)));
			} else {
				ofs = Point2(size.width - theme_cache.arrow_icon->get_width() - theme_cache.arrow_margin, int(Math::abs((size.height - theme_cache.arrow_icon->get_height()) / 2)));
			}
			theme_cache.arrow_icon->draw(ci, ofs, clr);
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			popup->set_layout_direction((Window::LayoutDirection)get_layout_direction());
			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			if (has_theme_icon(SNAME("arrow"))) {
				if (is_layout_rtl()) {
					_set_internal_margin(SIDE_LEFT, theme_cache.arrow_icon->get_width());
					_set_internal_margin(SIDE_RIGHT, 0.f);
				} else {
					_set_internal_margin(SIDE_LEFT, 0.f);
					_set_internal_margin(SIDE_RIGHT, theme_cache.arrow_icon->get_width());
				}
			}
			_refresh_size_cache();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				popup->hide();
			}
		} break;
	}
}

bool OptionButton::_set(const StringName &p_name, const Variant &p_value) {
	int index;
	const String sname = p_name;

	if (property_helper.is_property_valid(sname, &index)) {
		bool valid;
		popup->set(sname.trim_prefix("popup/"), p_value, &valid);

		if (index == current) {
			// Force refreshing currently displayed item.
			current = NONE_SELECTED;
			_select(index, false);
		}

		const String property = sname.get_slice("/", 2);
		if (property == "text" || property == "icon") {
			_queue_update_size_cache();
		}

		return valid;
	}
	return false;
}

void OptionButton::_focused(int p_which) {
	emit_signal(SNAME("item_focused"), popup->get_item_index(p_which));
}

void OptionButton::_selected(int p_which) {
	_select(p_which, true);
}

void OptionButton::pressed() {
	if (popup->is_visible()) {
		popup->hide();
		return;
	}

	show_popup();
}

void OptionButton::add_icon_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id) {
	bool first_selectable = !has_selectable_items();
	popup->add_icon_radio_check_item(p_icon, p_label, p_id);
	if (first_selectable) {
		select(get_item_count() - 1);
	}
	_queue_update_size_cache();
}

void OptionButton::add_item(const String &p_label, int p_id) {
	bool first_selectable = !has_selectable_items();
	popup->add_radio_check_item(p_label, p_id);
	if (first_selectable) {
		select(get_item_count() - 1);
	}
	_queue_update_size_cache();
}

void OptionButton::set_item_text(int p_idx, const String &p_text) {
	popup->set_item_text(p_idx, p_text);

	if (current == p_idx) {
		set_text(p_text);
	}
	_queue_update_size_cache();
}

void OptionButton::set_item_icon(int p_idx, const Ref<Texture2D> &p_icon) {
	popup->set_item_icon(p_idx, p_icon);

	if (current == p_idx) {
		set_button_icon(p_icon);
	}
	_queue_update_size_cache();
}

void OptionButton::set_item_id(int p_idx, int p_id) {
	popup->set_item_id(p_idx, p_id);
}

void OptionButton::set_item_metadata(int p_idx, const Variant &p_metadata) {
	popup->set_item_metadata(p_idx, p_metadata);
}

void OptionButton::set_item_tooltip(int p_idx, const String &p_tooltip) {
	popup->set_item_tooltip(p_idx, p_tooltip);
}

void OptionButton::set_item_disabled(int p_idx, bool p_disabled) {
	popup->set_item_disabled(p_idx, p_disabled);
}

String OptionButton::get_item_text(int p_idx) const {
	return popup->get_item_text(p_idx);
}

Ref<Texture2D> OptionButton::get_item_icon(int p_idx) const {
	return popup->get_item_icon(p_idx);
}

int OptionButton::get_item_id(int p_idx) const {
	if (p_idx == NONE_SELECTED) {
		return NONE_SELECTED;
	}

	return popup->get_item_id(p_idx);
}

int OptionButton::get_item_index(int p_id) const {
	return popup->get_item_index(p_id);
}

Variant OptionButton::get_item_metadata(int p_idx) const {
	return popup->get_item_metadata(p_idx);
}

String OptionButton::get_item_tooltip(int p_idx) const {
	return popup->get_item_tooltip(p_idx);
}

bool OptionButton::is_item_disabled(int p_idx) const {
	return popup->is_item_disabled(p_idx);
}

bool OptionButton::is_item_separator(int p_idx) const {
	return popup->is_item_separator(p_idx);
}
void OptionButton::set_item_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);

	int count_old = get_item_count();
	if (p_count == count_old) {
		return;
	}

	popup->set_item_count(p_count);

	if (p_count > count_old) {
		for (int i = count_old; i < p_count; i++) {
			popup->set_item_as_radio_checkable(i, true);
		}
	}

	if (!initialized) {
		if (queued_current != current) {
			current = queued_current;
		}
		initialized = true;
	}

	_refresh_size_cache();
	notify_property_list_changed();
}

bool OptionButton::has_selectable_items() const {
	for (int i = 0; i < get_item_count(); i++) {
		if (!is_item_disabled(i) && !is_item_separator(i)) {
			return true;
		}
	}
	return false;
}
int OptionButton::get_selectable_item(bool p_from_last) const {
	if (!p_from_last) {
		for (int i = 0; i < get_item_count(); i++) {
			if (!is_item_disabled(i) && !is_item_separator(i)) {
				return i;
			}
		}
	} else {
		for (int i = get_item_count() - 1; i >= 0; i--) {
			if (!is_item_disabled(i) && !is_item_separator(i)) {
				return i;
			}
		}
	}
	return -1;
}

int OptionButton::get_item_count() const {
	return popup->get_item_count();
}

void OptionButton::set_fit_to_longest_item(bool p_fit) {
	if (p_fit == fit_to_longest_item) {
		return;
	}
	fit_to_longest_item = p_fit;

	_refresh_size_cache();
}

bool OptionButton::is_fit_to_longest_item() const {
	return fit_to_longest_item;
}

void OptionButton::set_allow_reselect(bool p_allow) {
	allow_reselect = p_allow;
}

bool OptionButton::get_allow_reselect() const {
	return allow_reselect;
}

void OptionButton::add_separator(const String &p_text) {
	popup->add_separator(p_text);
}

void OptionButton::clear() {
	popup->clear();
	set_text("");
	current = NONE_SELECTED;
	_refresh_size_cache();
}

void OptionButton::_select(int p_which, bool p_emit) {
	if (p_which == current && !allow_reselect) {
		return;
	}

	if (p_which == NONE_SELECTED) {
		for (int i = 0; i < popup->get_item_count(); i++) {
			popup->set_item_checked(i, false);
		}

		current = NONE_SELECTED;
		set_text("");
		set_button_icon(nullptr);
	} else {
		ERR_FAIL_INDEX(p_which, popup->get_item_count());

		for (int i = 0; i < popup->get_item_count(); i++) {
			popup->set_item_checked(i, i == p_which);
		}

		current = p_which;
		set_text(popup->get_item_text(current));
		set_button_icon(popup->get_item_icon(current));
	}

	if (is_inside_tree() && p_emit) {
		emit_signal(SceneStringName(item_selected), current);
	}
}

void OptionButton::_select_int(int p_which) {
	if (p_which < NONE_SELECTED) {
		return;
	}
	if (p_which >= popup->get_item_count()) {
		if (!initialized) {
			queued_current = p_which;
		}
		return;
	}
	_select(p_which, false);
}

void OptionButton::_refresh_size_cache() {
	cache_refresh_pending = false;

	if (fit_to_longest_item) {
		_cached_size = theme_cache.normal->get_minimum_size();
		for (int i = 0; i < get_item_count(); i++) {
			_cached_size = _cached_size.max(get_minimum_size_for_text_and_icon(popup->get_item_xl_text(i), get_item_icon(i)));
		}
	}
	update_minimum_size();
}

void OptionButton::_queue_update_size_cache() {
	if (cache_refresh_pending) {
		return;
	}
	cache_refresh_pending = true;

	callable_mp(this, &OptionButton::_refresh_size_cache).call_deferred();
}

void OptionButton::select(int p_idx) {
	_select(p_idx, false);
}

int OptionButton::get_selected() const {
	return current;
}

int OptionButton::get_selected_id() const {
	return get_item_id(current);
}

Variant OptionButton::get_selected_metadata() const {
	int idx = get_selected();
	if (idx < 0) {
		return Variant();
	}
	return get_item_metadata(current);
}

void OptionButton::remove_item(int p_idx) {
	popup->remove_item(p_idx);
	if (current == p_idx) {
		_select(NONE_SELECTED);
	}
	_queue_update_size_cache();
}

PopupMenu *OptionButton::get_popup() const {
	return popup;
}

void OptionButton::show_popup() {
	if (!get_viewport()) {
		return;
	}

	Rect2 rect = get_screen_rect();
	rect.position.y += rect.size.height;
	rect.size.height = 0;
	popup->set_position(rect.position);
	popup->set_size(rect.size);

	// If not triggered by the mouse, start the popup with the checked item (or the first enabled one) focused.
	if (current != NONE_SELECTED && !popup->is_item_disabled(current)) {
		if (!_was_pressed_by_mouse()) {
			popup->set_focused_item(current);
		} else {
			popup->scroll_to_item(current);
		}
	} else {
		for (int i = 0; i < popup->get_item_count(); i++) {
			if (!popup->is_item_disabled(i)) {
				if (!_was_pressed_by_mouse()) {
					popup->set_focused_item(i);
				} else {
					popup->scroll_to_item(i);
				}

				break;
			}
		}
	}

	popup->popup();
}

void OptionButton::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "text" || p_property.name == "icon") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void OptionButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_item", "label", "id"), &OptionButton::add_item, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("add_icon_item", "texture", "label", "id"), &OptionButton::add_icon_item, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_item_text", "idx", "text"), &OptionButton::set_item_text);
	ClassDB::bind_method(D_METHOD("set_item_icon", "idx", "texture"), &OptionButton::set_item_icon);
	ClassDB::bind_method(D_METHOD("set_item_disabled", "idx", "disabled"), &OptionButton::set_item_disabled);
	ClassDB::bind_method(D_METHOD("set_item_id", "idx", "id"), &OptionButton::set_item_id);
	ClassDB::bind_method(D_METHOD("set_item_metadata", "idx", "metadata"), &OptionButton::set_item_metadata);
	ClassDB::bind_method(D_METHOD("set_item_tooltip", "idx", "tooltip"), &OptionButton::set_item_tooltip);
	ClassDB::bind_method(D_METHOD("get_item_text", "idx"), &OptionButton::get_item_text);
	ClassDB::bind_method(D_METHOD("get_item_icon", "idx"), &OptionButton::get_item_icon);
	ClassDB::bind_method(D_METHOD("get_item_id", "idx"), &OptionButton::get_item_id);
	ClassDB::bind_method(D_METHOD("get_item_index", "id"), &OptionButton::get_item_index);
	ClassDB::bind_method(D_METHOD("get_item_metadata", "idx"), &OptionButton::get_item_metadata);
	ClassDB::bind_method(D_METHOD("get_item_tooltip", "idx"), &OptionButton::get_item_tooltip);
	ClassDB::bind_method(D_METHOD("is_item_disabled", "idx"), &OptionButton::is_item_disabled);
	ClassDB::bind_method(D_METHOD("is_item_separator", "idx"), &OptionButton::is_item_separator);
	ClassDB::bind_method(D_METHOD("add_separator", "text"), &OptionButton::add_separator, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("clear"), &OptionButton::clear);
	ClassDB::bind_method(D_METHOD("select", "idx"), &OptionButton::select);
	ClassDB::bind_method(D_METHOD("get_selected"), &OptionButton::get_selected);
	ClassDB::bind_method(D_METHOD("get_selected_id"), &OptionButton::get_selected_id);
	ClassDB::bind_method(D_METHOD("get_selected_metadata"), &OptionButton::get_selected_metadata);
	ClassDB::bind_method(D_METHOD("remove_item", "idx"), &OptionButton::remove_item);
	ClassDB::bind_method(D_METHOD("_select_int", "idx"), &OptionButton::_select_int);

	ClassDB::bind_method(D_METHOD("get_popup"), &OptionButton::get_popup);
	ClassDB::bind_method(D_METHOD("show_popup"), &OptionButton::show_popup);

	ClassDB::bind_method(D_METHOD("set_item_count", "count"), &OptionButton::set_item_count);
	ClassDB::bind_method(D_METHOD("get_item_count"), &OptionButton::get_item_count);
	ClassDB::bind_method(D_METHOD("has_selectable_items"), &OptionButton::has_selectable_items);
	ClassDB::bind_method(D_METHOD("get_selectable_item", "from_last"), &OptionButton::get_selectable_item, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_fit_to_longest_item", "fit"), &OptionButton::set_fit_to_longest_item);
	ClassDB::bind_method(D_METHOD("is_fit_to_longest_item"), &OptionButton::is_fit_to_longest_item);
	ClassDB::bind_method(D_METHOD("set_allow_reselect", "allow"), &OptionButton::set_allow_reselect);
	ClassDB::bind_method(D_METHOD("get_allow_reselect"), &OptionButton::get_allow_reselect);
	ClassDB::bind_method(D_METHOD("set_disable_shortcuts", "disabled"), &OptionButton::set_disable_shortcuts);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "selected"), "_select_int", "get_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fit_to_longest_item"), "set_fit_to_longest_item", "is_fit_to_longest_item");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_reselect"), "set_allow_reselect", "get_allow_reselect");
	ADD_ARRAY_COUNT("Items", "item_count", "set_item_count", "get_item_count", "popup/item_");

	ADD_SIGNAL(MethodInfo("item_selected", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("item_focused", PropertyInfo(Variant::INT, "index")));

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, OptionButton, normal);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_focus_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_hover_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, OptionButton, font_disabled_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, OptionButton, h_separation);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, OptionButton, arrow_icon, "arrow");
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, OptionButton, arrow_margin);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, OptionButton, modulate_arrow);

	PopupMenu::Item defaults(true);

	base_property_helper.set_prefix("popup/item_");
	base_property_helper.set_array_length_getter(&OptionButton::get_item_count);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "text"), defaults.text, &OptionButton::_dummy_setter, &OptionButton::get_item_text);
	base_property_helper.register_property(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), defaults.icon, &OptionButton::_dummy_setter, &OptionButton::get_item_icon);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_RANGE, "0,10,1,or_greater", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL), defaults.id, &OptionButton::_dummy_setter, &OptionButton::get_item_id);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "disabled"), defaults.disabled, &OptionButton::_dummy_setter, &OptionButton::is_item_disabled);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "separator"), defaults.separator, &OptionButton::_dummy_setter, &OptionButton::is_item_separator);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

void OptionButton::set_disable_shortcuts(bool p_disabled) {
	disable_shortcuts = p_disabled;
}

OptionButton::OptionButton(const String &p_text) :
		Button(p_text) {
	set_toggle_mode(true);
	set_process_shortcut_input(true);
	set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	set_action_mode(ACTION_MODE_BUTTON_PRESS);

	popup = memnew(PopupMenu);
	popup->hide();
	add_child(popup, false, INTERNAL_MODE_FRONT);
	popup->connect("index_pressed", callable_mp(this, &OptionButton::_selected));
	popup->connect("id_focused", callable_mp(this, &OptionButton::_focused));
	popup->connect("popup_hide", callable_mp((BaseButton *)this, &BaseButton::set_pressed).bind(false));

	property_helper.setup_for_instance(base_property_helper, this);
}

OptionButton::~OptionButton() {
}
