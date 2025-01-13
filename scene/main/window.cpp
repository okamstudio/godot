/**************************************************************************/
/*  window.cpp                                                            */
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

#include "window.h"

#include "core/config/project_settings.h"
#include "core/input/shortcut.h"
#include "core/string/translation_server.h"
#include "scene/gui/control.h"
#include "scene/theme/theme_db.h"
#include "scene/theme/theme_owner.h"

// Editor integration.

int Window::root_layout_direction = 0;

void Window::set_root_layout_direction(int p_root_dir) {
	root_layout_direction = p_root_dir;
}

// Dynamic properties.

bool Window::_set(const StringName &p_name, const Variant &p_value) {
	ERR_MAIN_THREAD_GUARD_V(false);
	String name = p_name;

	if (!name.begins_with("theme_override")) {
		return false;
	}

	if (p_value.get_type() == Variant::NIL || (p_value.get_type() == Variant::OBJECT && (Object *)p_value == nullptr)) {
		if (name.begins_with("theme_override_icons/")) {
			String dname = name.get_slicec('/', 1);
			if (theme_icon_override.has(dname)) {
				theme_icon_override[dname]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
			}
			theme_icon_override.erase(dname);
			_notify_theme_override_changed();
		} else if (name.begins_with("theme_override_styles/")) {
			String dname = name.get_slicec('/', 1);
			if (theme_style_override.has(dname)) {
				theme_style_override[dname]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
			}
			theme_style_override.erase(dname);
			_notify_theme_override_changed();
		} else if (name.begins_with("theme_override_fonts/")) {
			String dname = name.get_slicec('/', 1);
			if (theme_font_override.has(dname)) {
				theme_font_override[dname]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
			}
			theme_font_override.erase(dname);
			_notify_theme_override_changed();
		} else if (name.begins_with("theme_override_font_sizes/")) {
			String dname = name.get_slicec('/', 1);
			theme_font_size_override.erase(dname);
			_notify_theme_override_changed();
		} else if (name.begins_with("theme_override_colors/")) {
			String dname = name.get_slicec('/', 1);
			theme_color_override.erase(dname);
			_notify_theme_override_changed();
		} else if (name.begins_with("theme_override_constants/")) {
			String dname = name.get_slicec('/', 1);
			theme_constant_override.erase(dname);
			_notify_theme_override_changed();
		} else {
			return false;
		}
	} else {
		if (name.begins_with("theme_override_icons/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_icon_override(dname, p_value);
		} else if (name.begins_with("theme_override_styles/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_style_override(dname, p_value);
		} else if (name.begins_with("theme_override_fonts/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_font_override(dname, p_value);
		} else if (name.begins_with("theme_override_font_sizes/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_font_size_override(dname, p_value);
		} else if (name.begins_with("theme_override_colors/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_color_override(dname, p_value);
		} else if (name.begins_with("theme_override_constants/")) {
			String dname = name.get_slicec('/', 1);
			add_theme_constant_override(dname, p_value);
		} else {
			return false;
		}
	}
	return true;
}

bool Window::_get(const StringName &p_name, Variant &r_ret) const {
	ERR_READ_THREAD_GUARD_V(false);
	String sname = p_name;

	if (!sname.begins_with("theme_override")) {
		return false;
	}

	if (sname.begins_with("theme_override_icons/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_icon_override.has(name) ? Variant(theme_icon_override[name]) : Variant();
	} else if (sname.begins_with("theme_override_styles/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_style_override.has(name) ? Variant(theme_style_override[name]) : Variant();
	} else if (sname.begins_with("theme_override_fonts/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_font_override.has(name) ? Variant(theme_font_override[name]) : Variant();
	} else if (sname.begins_with("theme_override_font_sizes/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_font_size_override.has(name) ? Variant(theme_font_size_override[name]) : Variant();
	} else if (sname.begins_with("theme_override_colors/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_color_override.has(name) ? Variant(theme_color_override[name]) : Variant();
	} else if (sname.begins_with("theme_override_constants/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = theme_constant_override.has(name) ? Variant(theme_constant_override[name]) : Variant();
	} else {
		return false;
	}

	return true;
}

void Window::_get_property_list(List<PropertyInfo> *p_list) const {
	ERR_READ_THREAD_GUARD;

	Ref<Theme> default_theme = ThemeDB::get_singleton()->get_default_theme();

	p_list->push_back(PropertyInfo(Variant::NIL, GNAME("Theme Overrides", "theme_override_"), PROPERTY_HINT_NONE, "theme_override_", PROPERTY_USAGE_GROUP));

	{
		List<StringName> names;
		default_theme->get_color_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_color_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::COLOR, PNAME("theme_override_colors") + String("/") + E, PROPERTY_HINT_NONE, "", usage));
		}
	}
	{
		List<StringName> names;
		default_theme->get_constant_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_constant_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::INT, PNAME("theme_override_constants") + String("/") + E, PROPERTY_HINT_RANGE, "-16384,16384", usage));
		}
	}
	{
		List<StringName> names;
		default_theme->get_font_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_font_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("theme_override_fonts") + String("/") + E, PROPERTY_HINT_RESOURCE_TYPE, "Font", usage));
		}
	}
	{
		List<StringName> names;
		default_theme->get_font_size_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_font_size_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::INT, PNAME("theme_override_font_sizes") + String("/") + E, PROPERTY_HINT_RANGE, "1,256,1,or_greater,suffix:px", usage));
		}
	}
	{
		List<StringName> names;
		default_theme->get_icon_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_icon_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("theme_override_icons") + String("/") + E, PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", usage));
		}
	}
	{
		List<StringName> names;
		default_theme->get_stylebox_list(get_class_name(), &names);
		for (const StringName &E : names) {
			uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (theme_style_override.has(E)) {
				usage |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;
			}

			p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("theme_override_styles") + String("/") + E, PROPERTY_HINT_RESOURCE_TYPE, "StyleBox", usage));
		}
	}
}

void Window::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "position" && initial_position != WINDOW_INITIAL_POSITION_ABSOLUTE) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "current_screen" && initial_position != WINDOW_INITIAL_POSITION_CENTER_OTHER_SCREEN) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "theme_type_variation") {
		List<StringName> names;

		// Only the default theme and the project theme are used for the list of options.
		// This is an imposed limitation to simplify the logic needed to leverage those options.
		ThemeDB::get_singleton()->get_default_theme()->get_type_variation_list(get_class_name(), &names);
		if (ThemeDB::get_singleton()->get_project_theme().is_valid()) {
			ThemeDB::get_singleton()->get_project_theme()->get_type_variation_list(get_class_name(), &names);
		}
		names.sort_custom<StringName::AlphCompare>();

		Vector<StringName> unique_names;
		String hint_string;
		for (const StringName &E : names) {
			// Skip duplicate values.
			if (unique_names.has(E)) {
				continue;
			}

			hint_string += String(E) + ",";
			unique_names.append(E);
		}

		p_property.hint_string = hint_string;
	}
}

//

Window *Window::get_from_id(DisplayServer::WindowID p_window_id) {
	if (p_window_id == DisplayServer::INVALID_WINDOW_ID) {
		return nullptr;
	}
	return Object::cast_to<Window>(ObjectDB::get_instance(DisplayServer::get_singleton()->window_get_attached_instance_id(p_window_id)));
}

void Window::set_title(const String &p_title) {
	ERR_MAIN_THREAD_GUARD;

	title = p_title;
	tr_title = atr(p_title);

#ifdef DEBUG_ENABLED
	if (window_id == DisplayServer::MAIN_WINDOW_ID && !Engine::get_singleton()->is_project_manager_hint()) {
		// Append a suffix to the window title to denote that the project is running
		// from a debug build (including the editor, excluding the project manager).
		// Since this results in lower performance, this should be clearly presented
		// to the user.
		tr_title = vformat("%s (DEBUG)", tr_title);
	}
#endif

	if (embedder) {
		embedder->_sub_window_update(this);
	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_title(tr_title, window_id);
		if (keep_title_visible) {
			Size2i title_size = DisplayServer::get_singleton()->window_get_title_size(tr_title, window_id);
			Size2i size_limit = get_clamped_minimum_size();
			if (title_size.x > size_limit.x || title_size.y > size_limit.y) {
				_update_window_size();
			}
		}
	}
	emit_signal("title_changed");
}

String Window::get_title() const {
	ERR_READ_THREAD_GUARD_V(String());
	return title;
}

String Window::get_translated_title() const {
	ERR_READ_THREAD_GUARD_V(String());
	return tr_title;
}

void Window::_settings_changed() {
	if (visible && initial_position != WINDOW_INITIAL_POSITION_ABSOLUTE && is_in_edited_scene_root()) {
		Size2 screen_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
		position = (screen_size - size) / 2;
		if (embedder) {
			embedder->_sub_window_update(this);
		}
	}
}

void Window::set_initial_position(Window::WindowInitialPosition p_initial_position) {
	ERR_MAIN_THREAD_GUARD;

	initial_position = p_initial_position;
	_settings_changed();
	notify_property_list_changed();
}

Window::WindowInitialPosition Window::get_initial_position() const {
	ERR_READ_THREAD_GUARD_V(WINDOW_INITIAL_POSITION_ABSOLUTE);
	return initial_position;
}

void Window::set_current_screen(int p_screen) {
	ERR_MAIN_THREAD_GUARD;

	current_screen = p_screen;
	if (window_id == DisplayServer::INVALID_WINDOW_ID) {
		return;
	}
	DisplayServer::get_singleton()->window_set_current_screen(p_screen, window_id);
}

int Window::get_current_screen() const {
	ERR_READ_THREAD_GUARD_V(0);

	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		current_screen = DisplayServer::get_singleton()->window_get_current_screen(window_id);
	}
	return current_screen;
}

void Window::set_position(const Point2i &p_position) {
	ERR_MAIN_THREAD_GUARD;

	position = p_position;

	if (embedder) {
		embedder->_sub_window_update(this);

	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_position(p_position, window_id);
	}
}

Point2i Window::get_position() const {
	ERR_READ_THREAD_GUARD_V(Point2i());

	return position;
}

void Window::move_to_center() {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());

	Rect2 parent_rect;

	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		int parent_screen = DisplayServer::get_singleton()->window_get_current_screen(get_window_id());
		parent_rect.position = DisplayServer::get_singleton()->screen_get_position(parent_screen);
		parent_rect.size = DisplayServer::get_singleton()->screen_get_size(parent_screen);
	}

	if (parent_rect != Rect2()) {
		set_position(parent_rect.position + (parent_rect.size - get_size()) / 2);
	}
}

void Window::set_size(const Size2i &p_size) {
	ERR_MAIN_THREAD_GUARD;
#if defined(ANDROID_ENABLED)
	if (!get_parent() && is_inside_tree()) {
		// Can't set root window size on Android.
		return;
	}
#endif

	size = p_size;
	_update_window_size();
	_settings_changed();
}

Size2i Window::get_size() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	return size;
}

void Window::reset_size() {
	ERR_MAIN_THREAD_GUARD;
	set_size(Size2i());
}

Point2i Window::get_position_with_decorations() const {
	ERR_READ_THREAD_GUARD_V(Point2i());
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		return DisplayServer::get_singleton()->window_get_position_with_decorations(window_id);
	}
	if (visible && is_embedded() && !get_flag(Window::FLAG_BORDERLESS)) {
		Size2 border_offset;
		if (theme_cache.embedded_border.is_valid()) {
			border_offset = theme_cache.embedded_border->get_offset();
		}
		if (theme_cache.embedded_unfocused_border.is_valid()) {
			border_offset = border_offset.max(theme_cache.embedded_unfocused_border->get_offset());
		}
		return position - border_offset;
	}
	return position;
}

Size2i Window::get_size_with_decorations() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		return DisplayServer::get_singleton()->window_get_size_with_decorations(window_id);
	}
	if (visible && is_embedded() && !get_flag(Window::FLAG_BORDERLESS)) {
		Size2 border_size;
		if (theme_cache.embedded_border.is_valid()) {
			border_size = theme_cache.embedded_border->get_minimum_size();
		}
		if (theme_cache.embedded_unfocused_border.is_valid()) {
			border_size = border_size.max(theme_cache.embedded_unfocused_border->get_minimum_size());
		}
		return size + border_size;
	}
	return size;
}

Size2i Window::_clamp_limit_size(const Size2i &p_limit_size) {
	// Force window limits to respect size limitations of rendering server.
	Size2i max_window_size = RS::get_singleton()->get_maximum_viewport_size();
	if (max_window_size != Size2i()) {
		return p_limit_size.clamp(Vector2i(), max_window_size);
	} else {
		return p_limit_size.maxi(0);
	}
}

void Window::_validate_limit_size() {
	// When max_size is invalid, max_size_used falls back to respect size limitations of rendering server.
	bool max_size_valid = (max_size.x > 0 || max_size.y > 0) && max_size.x >= min_size.x && max_size.y >= min_size.y;
	max_size_used = max_size_valid ? max_size : RS::get_singleton()->get_maximum_viewport_size();
}

void Window::set_max_size(const Size2i &p_max_size) {
	ERR_MAIN_THREAD_GUARD;
#if defined(ANDROID_ENABLED)
	if (!get_parent() && is_inside_tree()) {
		// Can't set root window size on Android.
		return;
	}
#endif
	Size2i max_size_clamped = _clamp_limit_size(p_max_size);
	if (max_size == max_size_clamped) {
		return;
	}
	max_size = max_size_clamped;

	_validate_limit_size();
	_update_window_size();
}

Size2i Window::get_max_size() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	return max_size;
}

void Window::set_min_size(const Size2i &p_min_size) {
	ERR_MAIN_THREAD_GUARD;
#if defined(ANDROID_ENABLED)
	if (!get_parent() && is_inside_tree()) {
		// Can't set root window size on Android.
		return;
	}
#endif
	Size2i min_size_clamped = _clamp_limit_size(p_min_size);
	if (min_size == min_size_clamped) {
		return;
	}
	min_size = min_size_clamped;

	_validate_limit_size();
	_update_window_size();
}

Size2i Window::get_min_size() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	return min_size;
}

void Window::set_mode(Mode p_mode) {
	ERR_MAIN_THREAD_GUARD;
	mode = p_mode;

	if (embedder) {
		embedder->_sub_window_update(this);

	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_mode(DisplayServer::WindowMode(p_mode), window_id);
	}
}

Window::Mode Window::get_mode() const {
	ERR_READ_THREAD_GUARD_V(MODE_WINDOWED);
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		mode = (Mode)DisplayServer::get_singleton()->window_get_mode(window_id);
	}
	return mode;
}

void Window::set_flag(Flags p_flag, bool p_enabled) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags[p_flag] = p_enabled;

	if (p_flag == FLAG_TRANSPARENT) {
		set_transparent_background(p_enabled);
	}

	if (embedder) {
		embedder->_sub_window_update(this);
	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		if (!is_in_edited_scene_root()) {
			DisplayServer::get_singleton()->window_set_flag(DisplayServer::WindowFlags(p_flag), p_enabled, window_id);
		}
	}
}

bool Window::get_flag(Flags p_flag) const {
	ERR_READ_THREAD_GUARD_V(false);
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		if (!is_in_edited_scene_root()) {
			flags[p_flag] = DisplayServer::get_singleton()->window_get_flag(DisplayServer::WindowFlags(p_flag), window_id);
		}
	}
	return flags[p_flag];
}

bool Window::is_maximize_allowed() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		return DisplayServer::get_singleton()->window_is_maximize_allowed(window_id);
	}
	return true;
}

void Window::request_attention() {
	ERR_MAIN_THREAD_GUARD;
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_request_attention(window_id);
	}
}

#ifndef DISABLE_DEPRECATED
void Window::move_to_foreground() {
	WARN_DEPRECATED_MSG(R"*(The "move_to_foreground()" method is deprecated, use "grab_focus()" instead.)*");
	grab_focus();
}
#endif // DISABLE_DEPRECATED

bool Window::can_draw() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!is_inside_tree()) {
		return false;
	}
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		return DisplayServer::get_singleton()->window_can_draw(window_id);
	}

	return visible;
}

void Window::set_ime_active(bool p_active) {
	ERR_MAIN_THREAD_GUARD;
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_ime_active(p_active, window_id);
	}
}

void Window::set_ime_position(const Point2i &p_pos) {
	ERR_MAIN_THREAD_GUARD;
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_ime_position(p_pos, window_id);
	}
}

bool Window::is_embedded() const {
	ERR_READ_THREAD_GUARD_V(false);
	return get_embedder() != nullptr;
}

bool Window::is_in_edited_scene_root() const {
	ERR_READ_THREAD_GUARD_V(false);
#ifdef TOOLS_ENABLED
	return is_part_of_edited_scene();
#else
	return false;
#endif
}

void Window::_make_window() {
	ERR_FAIL_COND(window_id != DisplayServer::INVALID_WINDOW_ID);

	if (transient && transient_to_focused) {
		_make_transient();
	}

	uint32_t f = 0;
	for (int i = 0; i < FLAG_MAX; i++) {
		if (flags[i]) {
			f |= (1 << i);
		}
	}

	DisplayServer::VSyncMode vsync_mode = DisplayServer::get_singleton()->window_get_vsync_mode(DisplayServer::MAIN_WINDOW_ID);
	Rect2i window_rect;
	if (initial_position == WINDOW_INITIAL_POSITION_ABSOLUTE) {
		window_rect = Rect2i(position, size);
	} else if (initial_position == WINDOW_INITIAL_POSITION_CENTER_PRIMARY_SCREEN) {
		window_rect = Rect2i(DisplayServer::get_singleton()->screen_get_position(DisplayServer::SCREEN_PRIMARY) + (DisplayServer::get_singleton()->screen_get_size(DisplayServer::SCREEN_PRIMARY) - size) / 2, size);
	} else if (initial_position == WINDOW_INITIAL_POSITION_CENTER_MAIN_WINDOW_SCREEN) {
		window_rect = Rect2i(DisplayServer::get_singleton()->screen_get_position(DisplayServer::SCREEN_OF_MAIN_WINDOW) + (DisplayServer::get_singleton()->screen_get_size(DisplayServer::SCREEN_OF_MAIN_WINDOW) - size) / 2, size);
	} else if (initial_position == WINDOW_INITIAL_POSITION_CENTER_OTHER_SCREEN) {
		window_rect = Rect2i(DisplayServer::get_singleton()->screen_get_position(current_screen) + (DisplayServer::get_singleton()->screen_get_size(current_screen) - size) / 2, size);
	} else if (initial_position == WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_MOUSE_FOCUS) {
		window_rect = Rect2i(DisplayServer::get_singleton()->screen_get_position(DisplayServer::SCREEN_WITH_MOUSE_FOCUS) + (DisplayServer::get_singleton()->screen_get_size(DisplayServer::SCREEN_WITH_MOUSE_FOCUS) - size) / 2, size);
	} else if (initial_position == WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_KEYBOARD_FOCUS) {
		window_rect = Rect2i(DisplayServer::get_singleton()->screen_get_position(DisplayServer::SCREEN_WITH_KEYBOARD_FOCUS) + (DisplayServer::get_singleton()->screen_get_size(DisplayServer::SCREEN_WITH_KEYBOARD_FOCUS) - size) / 2, size);
	}

	window_id = DisplayServer::get_singleton()->create_sub_window(DisplayServer::WindowMode(mode), vsync_mode, f, window_rect, is_in_edited_scene_root() ? false : exclusive, transient_parent ? transient_parent->window_id : DisplayServer::INVALID_WINDOW_ID);
	ERR_FAIL_COND(window_id == DisplayServer::INVALID_WINDOW_ID);
	DisplayServer::get_singleton()->window_set_max_size(Size2i(), window_id);
	DisplayServer::get_singleton()->window_set_min_size(Size2i(), window_id);
	DisplayServer::get_singleton()->window_set_mouse_passthrough(mpath, window_id);
	DisplayServer::get_singleton()->window_set_title(tr_title, window_id);
	DisplayServer::get_singleton()->window_attach_instance_id(get_instance_id(), window_id);

	_update_window_size();

	if (transient_parent) {
		for (const Window *E : transient_children) {
			if (E->window_id != DisplayServer::INVALID_WINDOW_ID) {
				DisplayServer::get_singleton()->window_set_transient(E->window_id, transient_parent->window_id);
			}
		}
	}

	_update_window_callbacks();

	RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_WHEN_VISIBLE);
	DisplayServer::get_singleton()->show_window(window_id);
}

void Window::_update_from_window() {
	ERR_FAIL_COND(window_id == DisplayServer::INVALID_WINDOW_ID);
	mode = (Mode)DisplayServer::get_singleton()->window_get_mode(window_id);
	for (int i = 0; i < FLAG_MAX; i++) {
		flags[i] = DisplayServer::get_singleton()->window_get_flag(DisplayServer::WindowFlags(i), window_id);
	}
}

void Window::_clear_window() {
	ERR_FAIL_COND(window_id == DisplayServer::INVALID_WINDOW_ID);

	bool had_focus = has_focus();

	if (transient_parent && transient_parent->window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_transient(window_id, DisplayServer::INVALID_WINDOW_ID);
	}

	for (const Window *E : transient_children) {
		if (E->window_id != DisplayServer::INVALID_WINDOW_ID) {
			DisplayServer::get_singleton()->window_set_transient(E->window_id, DisplayServer::INVALID_WINDOW_ID);
		}
	}

	_update_from_window();

	DisplayServer::get_singleton()->delete_sub_window(window_id);
	window_id = DisplayServer::INVALID_WINDOW_ID;

	// If closing window was focused and has a parent, return focus.
	if (had_focus && transient_parent) {
		transient_parent->grab_focus();
	}

	_update_viewport_size();
	RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_DISABLED);

	if (transient && transient_to_focused) {
		_clear_transient();
	}
}

void Window::_rect_changed_callback(const Rect2i &p_callback) {
	//we must always accept this as the truth
	if (size == p_callback.size && position == p_callback.position) {
		return;
	}

	if (position != p_callback.position) {
		position = p_callback.position;
		_propagate_window_notification(this, NOTIFICATION_WM_POSITION_CHANGED);
	}

	if (size != p_callback.size) {
		size = p_callback.size;
		_update_viewport_size();
	}
}

void Window::_propagate_window_notification(Node *p_node, int p_notification) {
	p_node->notification(p_notification);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		Window *window = Object::cast_to<Window>(child);
		if (window) {
			continue;
		}
		_propagate_window_notification(child, p_notification);
	}
}

void Window::_event_callback(DisplayServer::WindowEvent p_event) {
	switch (p_event) {
		case DisplayServer::WINDOW_EVENT_MOUSE_ENTER: {
			if (!is_inside_tree()) {
				return;
			}
			Window *root = get_tree()->get_root();
			if (mouse_in_window && root->gui.windowmanager_window_over == this) {
				return;
			}
			if (root->gui.windowmanager_window_over) {
#ifdef DEV_ENABLED
				WARN_PRINT_ONCE("Entering a window while a window is hovered should never happen in DisplayServer.");
#endif // DEV_ENABLED
				root->gui.windowmanager_window_over->_event_callback(DisplayServer::WINDOW_EVENT_MOUSE_EXIT);
			}
			_propagate_window_notification(this, NOTIFICATION_WM_MOUSE_ENTER);
			root->gui.windowmanager_window_over = this;
			mouse_in_window = true;
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CURSOR_SHAPE)) {
				DisplayServer::get_singleton()->cursor_set_shape(DisplayServer::CURSOR_ARROW); //restore cursor shape
			}
		} break;
		case DisplayServer::WINDOW_EVENT_MOUSE_EXIT: {
			if (!is_inside_tree()) {
				return;
			}
			// Ensure keeping the order of input events and window events when input events are buffered or accumulated.
			Input::get_singleton()->flush_buffered_events();

			Window *root = get_tree()->get_root();
			if (!root->gui.windowmanager_window_over) {
#ifdef DEV_ENABLED
				WARN_PRINT_ONCE("Exiting a window while no window is hovered should never happen in DisplayServer.");
#endif // DEV_ENABLED
				return;
			}
			mouse_in_window = false;
			root->gui.windowmanager_window_over->_mouse_leave_viewport();
			root->gui.windowmanager_window_over = nullptr;
			_propagate_window_notification(this, NOTIFICATION_WM_MOUSE_EXIT);
		} break;
		case DisplayServer::WINDOW_EVENT_FOCUS_IN: {
			focused = true;
			_propagate_window_notification(this, NOTIFICATION_WM_WINDOW_FOCUS_IN);
			emit_signal(SceneStringName(focus_entered));

		} break;
		case DisplayServer::WINDOW_EVENT_FOCUS_OUT: {
			focused = false;
			_propagate_window_notification(this, NOTIFICATION_WM_WINDOW_FOCUS_OUT);
			emit_signal(SceneStringName(focus_exited));
		} break;
		case DisplayServer::WINDOW_EVENT_CLOSE_REQUEST: {
			if (exclusive_child != nullptr) {
				break; //has an exclusive child, can't get events until child is closed
			}
			_propagate_window_notification(this, NOTIFICATION_WM_CLOSE_REQUEST);
			emit_signal(SNAME("close_requested"));
		} break;
		case DisplayServer::WINDOW_EVENT_GO_BACK_REQUEST: {
			_propagate_window_notification(this, NOTIFICATION_WM_GO_BACK_REQUEST);
			emit_signal(SNAME("go_back_requested"));
		} break;
		case DisplayServer::WINDOW_EVENT_DPI_CHANGE: {
			_update_viewport_size();
			_propagate_window_notification(this, NOTIFICATION_WM_DPI_CHANGE);
			emit_signal(SNAME("dpi_changed"));
		} break;
		case DisplayServer::WINDOW_EVENT_TITLEBAR_CHANGE: {
			emit_signal(SNAME("titlebar_changed"));
		} break;
	}
}

void Window::update_mouse_cursor_state() {
	ERR_MAIN_THREAD_GUARD;
	// Update states based on mouse cursor position.
	// This includes updated mouse_enter or mouse_exit signals or the current mouse cursor shape.
	// These details are set in Viewport::_gui_input_event. To instantly
	// see the changes in the viewport, we need to trigger a mouse motion event.
	// This function should be called whenever scene tree changes affect the mouse cursor.
	Ref<InputEventMouseMotion> mm;
	Vector2 pos = get_mouse_position();
	Transform2D xform = get_global_canvas_transform().affine_inverse();
	mm.instantiate();
	mm->set_position(pos);
	mm->set_global_position(xform.xform(pos));
	mm->set_device(InputEvent::DEVICE_ID_INTERNAL);
	push_input(mm, true);
}

void Window::show() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(true);
}

void Window::hide() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(false);
}

void Window::set_visible(bool p_visible) {
	ERR_MAIN_THREAD_GUARD;
	if (visible == p_visible) {
		return;
	}

	if (!is_inside_tree()) {
		visible = p_visible;
		return;
	}

	ERR_FAIL_NULL_MSG(get_parent(), "Can't change visibility of main window.");

	visible = p_visible;

	// Stop any queued resizing, as the window will be resized right now.
	updating_child_controls = false;

	Viewport *embedder_vp = get_embedder();

	if (!embedder_vp) {
		if (!p_visible && window_id != DisplayServer::INVALID_WINDOW_ID) {
			_clear_window();
		}
		if (p_visible && window_id == DisplayServer::INVALID_WINDOW_ID) {
			_make_window();
		}
	} else {
		if (visible) {
			embedder = embedder_vp;
			if (initial_position != WINDOW_INITIAL_POSITION_ABSOLUTE) {
				if (is_in_edited_scene_root()) {
					Size2 screen_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
					position = (screen_size - size) / 2;
				} else {
					position = (embedder->get_visible_rect().size - size) / 2;
				}
			}
			embedder->_sub_window_register(this);
			RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_WHEN_PARENT_VISIBLE);
		} else {
			embedder->_sub_window_remove(this);
			embedder = nullptr;
			RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_DISABLED);
		}
		_update_window_size();
	}

	if (!visible) {
		focused = false;
	}
	notification(NOTIFICATION_VISIBILITY_CHANGED);
	emit_signal(SceneStringName(visibility_changed));

	RS::get_singleton()->viewport_set_active(get_viewport_rid(), visible);

	//update transient exclusive
	if (transient_parent) {
		_set_transient_exclusive_child(true);
	}
}

void Window::_clear_transient() {
	if (transient_parent) {
		if (transient_parent->window_id != DisplayServer::INVALID_WINDOW_ID && window_id != DisplayServer::INVALID_WINDOW_ID) {
			DisplayServer::get_singleton()->window_set_transient(window_id, DisplayServer::INVALID_WINDOW_ID);
		}
		transient_parent->transient_children.erase(this);
		if (transient_parent->exclusive_child == this) {
			transient_parent->exclusive_child = nullptr;
		}
		transient_parent = nullptr;
	}
}

void Window::_make_transient() {
	if (!get_parent()) {
		//main window, can't be transient
		return;
	}
	//find transient parent

	Window *window = nullptr;

	if (!is_embedded() && transient_to_focused) {
		DisplayServer::WindowID focused_window_id = DisplayServer::get_singleton()->get_focused_window();
		if (focused_window_id != DisplayServer::INVALID_WINDOW_ID) {
			window = Window::get_from_id(focused_window_id);
		}
	}

	if (!window) {
		Viewport *vp = get_parent()->get_viewport();
		while (vp) {
			window = Object::cast_to<Window>(vp);
			if (window) {
				break;
			}
			if (!vp->get_parent()) {
				break;
			}

			vp = vp->get_parent()->get_viewport();
		}
	}

	if (window) {
		transient_parent = window;
		window->transient_children.insert(this);
		_set_transient_exclusive_child();
	}

	//see if we can make transient
	if (transient_parent->window_id != DisplayServer::INVALID_WINDOW_ID && window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_set_transient(window_id, transient_parent->window_id);
	}
}

void Window::_set_transient_exclusive_child(bool p_clear_invalid) {
	if (exclusive && visible && is_inside_tree()) {
		if (!is_in_edited_scene_root()) {
			// Transient parent has another exclusive child.
			if (transient_parent->exclusive_child && transient_parent->exclusive_child != this) {
				ERR_PRINT(vformat("Attempting to make child window exclusive, but the parent window already has another exclusive child. This window: %s, parent window: %s, current exclusive child window: %s", get_description(), transient_parent->get_description(), transient_parent->exclusive_child->get_description()));
			}
			transient_parent->exclusive_child = this;
		}
	} else if (p_clear_invalid) {
		if (transient_parent->exclusive_child == this) {
			transient_parent->exclusive_child = nullptr;
		}
	}
}

void Window::set_transient(bool p_transient) {
	ERR_MAIN_THREAD_GUARD;
	if (transient == p_transient) {
		return;
	}

	transient = p_transient;

	if (!is_inside_tree()) {
		return;
	}

	if (transient) {
		if (!transient_to_focused) {
			_make_transient();
		}
	} else {
		_clear_transient();
	}
}

bool Window::is_transient() const {
	return transient;
}

void Window::set_transient_to_focused(bool p_transient_to_focused) {
	ERR_MAIN_THREAD_GUARD;
	if (transient_to_focused == p_transient_to_focused) {
		return;
	}

	transient_to_focused = p_transient_to_focused;
}

bool Window::is_transient_to_focused() const {
	ERR_READ_THREAD_GUARD_V(false);
	return transient_to_focused;
}

void Window::set_exclusive(bool p_exclusive) {
	ERR_MAIN_THREAD_GUARD;
	if (exclusive == p_exclusive) {
		return;
	}

	exclusive = p_exclusive;

	if (!embedder && window_id != DisplayServer::INVALID_WINDOW_ID) {
		if (is_in_edited_scene_root()) {
			DisplayServer::get_singleton()->window_set_exclusive(window_id, false);
		} else {
			DisplayServer::get_singleton()->window_set_exclusive(window_id, exclusive);
		}
	}

	if (transient_parent) {
		_set_transient_exclusive_child(true);
	}
}

bool Window::is_exclusive() const {
	ERR_READ_THREAD_GUARD_V(false);
	return exclusive;
}

bool Window::is_visible() const {
	ERR_READ_THREAD_GUARD_V(false);
	return visible;
}

Size2i Window::_clamp_window_size(const Size2i &p_size) {
	Size2i window_size_clamped = p_size;
	Size2 minsize = get_clamped_minimum_size();
	window_size_clamped = window_size_clamped.max(minsize);

	if (max_size_used != Size2i()) {
		window_size_clamped = window_size_clamped.min(max_size_used);
	}

	return window_size_clamped;
}

void Window::_update_window_size() {
	Size2i size_limit = get_clamped_minimum_size();
	if (!embedder && window_id != DisplayServer::INVALID_WINDOW_ID && keep_title_visible) {
		Size2i title_size = DisplayServer::get_singleton()->window_get_title_size(tr_title, window_id);
		size_limit = size_limit.max(title_size);
	}

	size = size.max(size_limit);

	bool reset_min_first = false;

	if (max_size_used != Size2i()) {
		// Force window size to respect size limitations of max_size_used.
		size = size.min(max_size_used);

		if (size_limit.x > max_size_used.x) {
			size_limit.x = max_size_used.x;
			reset_min_first = true;
		}
		if (size_limit.y > max_size_used.y) {
			size_limit.y = max_size_used.y;
			reset_min_first = true;
		}
	}

	if (embedder) {
		size = size.maxi(1);

		embedder->_sub_window_update(this);
	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		// When main window embedded in the editor, we can't resize the main window.
		if (window_id != DisplayServer::MAIN_WINDOW_ID || !Engine::get_singleton()->is_embedded_in_editor()) {
			if (reset_min_first && wrap_controls) {
				// Avoid an error if setting max_size to a value between min_size and the previous size_limit.
				DisplayServer::get_singleton()->window_set_min_size(Size2i(), window_id);
			}

			DisplayServer::get_singleton()->window_set_max_size(max_size_used, window_id);
			DisplayServer::get_singleton()->window_set_min_size(size_limit, window_id);
			DisplayServer::get_singleton()->window_set_size(size, window_id);
		}
	}

	//update the viewport
	_update_viewport_size();
}

void Window::_update_viewport_size() {
	//update the viewport part

	Size2i final_size;
	Size2i final_size_override;
	Rect2i attach_to_screen_rect(Point2i(), size);
	double font_oversampling = 1.0;
	window_transform = Transform2D();

	if (content_scale_stretch == Window::CONTENT_SCALE_STRETCH_INTEGER) {
		// We always want to make sure that the content scale factor is a whole
		// number, else there will be pixel wobble no matter what.
		content_scale_factor = Math::floor(content_scale_factor);

		// A content scale factor of zero is pretty useless.
		if (content_scale_factor < 1) {
			content_scale_factor = 1;
		}
	}

	if (content_scale_mode == CONTENT_SCALE_MODE_DISABLED || content_scale_size.x == 0 || content_scale_size.y == 0) {
		font_oversampling = content_scale_factor;
		final_size = size;
		final_size_override = Size2(size) / content_scale_factor;
	} else {
		//actual screen video mode
		Size2 video_mode = size;
		Size2 desired_res = content_scale_size;

		Size2 viewport_size;
		Size2 screen_size;

		float viewport_aspect = desired_res.aspect();
		float video_mode_aspect = video_mode.aspect();

		if (content_scale_aspect == CONTENT_SCALE_ASPECT_IGNORE || Math::is_equal_approx(viewport_aspect, video_mode_aspect)) {
			//same aspect or ignore aspect
			viewport_size = desired_res;
			screen_size = video_mode;
		} else if (viewport_aspect < video_mode_aspect) {
			// screen ratio is smaller vertically

			if (content_scale_aspect == CONTENT_SCALE_ASPECT_KEEP_HEIGHT || content_scale_aspect == CONTENT_SCALE_ASPECT_EXPAND) {
				//will stretch horizontally
				viewport_size.x = desired_res.y * video_mode_aspect;
				viewport_size.y = desired_res.y;
				screen_size = video_mode;

			} else {
				//will need black bars
				viewport_size = desired_res;
				screen_size.x = video_mode.y * viewport_aspect;
				screen_size.y = video_mode.y;
			}
		} else {
			//screen ratio is smaller horizontally
			if (content_scale_aspect == CONTENT_SCALE_ASPECT_KEEP_WIDTH || content_scale_aspect == CONTENT_SCALE_ASPECT_EXPAND) {
				//will stretch horizontally
				viewport_size.x = desired_res.x;
				viewport_size.y = desired_res.x / video_mode_aspect;
				screen_size = video_mode;

			} else {
				//will need black bars
				viewport_size = desired_res;
				screen_size.x = video_mode.x;
				screen_size.y = video_mode.x / viewport_aspect;
			}
		}

		screen_size = screen_size.floor();
		viewport_size = viewport_size.floor();

		if (content_scale_stretch == Window::CONTENT_SCALE_STRETCH_INTEGER) {
			Size2i screen_scale = (screen_size / viewport_size).floor();
			int scale_factor = MIN(screen_scale.x, screen_scale.y);

			if (scale_factor < 1) {
				scale_factor = 1;
			}

			screen_size = viewport_size * scale_factor;
		}

		Size2 margin;
		Size2 offset;

		if (screen_size.x < video_mode.x) {
			margin.x = Math::round((video_mode.x - screen_size.x) / 2.0);
			offset.x = Math::round(margin.x * viewport_size.y / screen_size.y);
		}

		if (screen_size.y < video_mode.y) {
			margin.y = Math::round((video_mode.y - screen_size.y) / 2.0);
			offset.y = Math::round(margin.y * viewport_size.x / screen_size.x);
		}

		switch (content_scale_mode) {
			case CONTENT_SCALE_MODE_DISABLED: {
				// Already handled above
				//_update_font_oversampling(1.0);
			} break;
			case CONTENT_SCALE_MODE_CANVAS_ITEMS: {
				final_size = screen_size;
				final_size_override = viewport_size / content_scale_factor;
				attach_to_screen_rect = Rect2(margin, screen_size);
				font_oversampling = (screen_size.x / viewport_size.x) * content_scale_factor;

				window_transform.translate_local(margin);
			} break;
			case CONTENT_SCALE_MODE_VIEWPORT: {
				final_size = (viewport_size / content_scale_factor).floor();
				attach_to_screen_rect = Rect2(margin, screen_size);

				window_transform.translate_local(margin);
				if (final_size.x != 0 && final_size.y != 0) {
					Transform2D scale_transform;
					scale_transform.scale(Vector2(attach_to_screen_rect.size) / Vector2(final_size));
					window_transform *= scale_transform;
				}
			} break;
		}
	}

	bool allocate = is_inside_tree() && visible && (window_id != DisplayServer::INVALID_WINDOW_ID || embedder != nullptr);
	bool ci_updated = _set_size(final_size, final_size_override, allocate);

	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		RenderingServer::get_singleton()->viewport_attach_to_screen(get_viewport_rid(), attach_to_screen_rect, window_id);
	} else if (!is_embedded()) {
		RenderingServer::get_singleton()->viewport_attach_to_screen(get_viewport_rid(), Rect2i(), DisplayServer::INVALID_WINDOW_ID);
	}

	if (window_id == DisplayServer::MAIN_WINDOW_ID) {
		if (!use_font_oversampling) {
			font_oversampling = 1.0;
		}
		if (!Math::is_equal_approx(TS->font_get_global_oversampling(), font_oversampling)) {
			TS->font_set_global_oversampling(font_oversampling);
			if (!ci_updated) {
				update_canvas_items();
				emit_signal(SNAME("size_changed"));
			}
		}
	}

	notification(NOTIFICATION_WM_SIZE_CHANGED);

	if (embedder) {
		float scale = MIN(embedder->stretch_transform.get_scale().width, embedder->stretch_transform.get_scale().height);
		Size2 s = Size2(final_size.width * scale, final_size.height * scale).ceil();
		RS::get_singleton()->viewport_set_global_canvas_transform(get_viewport_rid(), global_canvas_transform * scale * content_scale_factor);
		RS::get_singleton()->viewport_set_size(get_viewport_rid(), s.width, s.height);
		embedder->_sub_window_update(this);
	}
}

void Window::_update_window_callbacks() {
	DisplayServer::get_singleton()->window_set_rect_changed_callback(callable_mp(this, &Window::_rect_changed_callback), window_id);
	DisplayServer::get_singleton()->window_set_window_event_callback(callable_mp(this, &Window::_event_callback), window_id);
	DisplayServer::get_singleton()->window_set_input_event_callback(callable_mp(this, &Window::_window_input), window_id);
	DisplayServer::get_singleton()->window_set_input_text_callback(callable_mp(this, &Window::_window_input_text), window_id);
	DisplayServer::get_singleton()->window_set_drop_files_callback(callable_mp(this, &Window::_window_drop_files), window_id);
}

void Window::set_force_native(bool p_force_native) {
	if (force_native == p_force_native) {
		return;
	}
	if (is_visible() && !is_in_edited_scene_root()) {
		ERR_FAIL_MSG("Can't change \"force_native\" while a window is displayed. Consider hiding window before changing this value.");
	}
	force_native = p_force_native;
}

bool Window::get_force_native() const {
	return force_native;
}

Viewport *Window::get_embedder() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	if (force_native && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_SUBWINDOWS) && !is_in_edited_scene_root()) {
		return nullptr;
	}

	Viewport *vp = get_parent_viewport();

	while (vp) {
		if (vp->is_embedding_subwindows()) {
			return vp;
		}

		if (vp->get_parent()) {
			vp = vp->get_parent()->get_viewport();
		} else {
			vp = nullptr;
		}
	}
	return nullptr;
}

void Window::_notification(int p_what) {
	ERR_MAIN_THREAD_GUARD;
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			initialized = true;

			_invalidate_theme_cache();
			_update_theme_item_cache();
		} break;

		case NOTIFICATION_PARENTED: {
			theme_owner->assign_theme_on_parented(this);
		} break;

		case NOTIFICATION_UNPARENTED: {
			theme_owner->clear_theme_on_unparented(this);
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (is_in_edited_scene_root()) {
				if (!ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &Window::_settings_changed))) {
					ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &Window::_settings_changed));
				}
			}

			bool embedded = false;
			{
				embedder = get_embedder();
				if (embedder) {
					embedded = true;
					if (!visible) {
						embedder = nullptr; // Not yet since not visible.
					}
				}
			}

			if (embedded) {
				// Create as embedded.
				if (embedder) {
					if (initial_position != WINDOW_INITIAL_POSITION_ABSOLUTE) {
						if (is_in_edited_scene_root()) {
							Size2 screen_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
							position = (screen_size - size) / 2;
						} else {
							position = (embedder->get_visible_rect().size - size) / 2;
						}
					}
					embedder->_sub_window_register(this);
					RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_WHEN_PARENT_VISIBLE);
					_update_window_size();
				}

			} else {
				if (!get_parent()) {
					// It's the root window!
					visible = true; // Always visible.
					window_id = DisplayServer::MAIN_WINDOW_ID;
					DisplayServer::get_singleton()->window_attach_instance_id(get_instance_id(), window_id);
					_update_from_window();
					// Since this window already exists (created on start), we must update pos and size from it.
					{
						position = DisplayServer::get_singleton()->window_get_position(window_id);
						size = DisplayServer::get_singleton()->window_get_size(window_id);
						focused = DisplayServer::get_singleton()->window_is_focused(window_id);
					}
					_update_window_size(); // Inform DisplayServer of minimum and maximum size.
					_update_viewport_size(); // Then feed back to the viewport.
					_update_window_callbacks();
					// Simulate mouse-enter event when mouse is over the window, since OS event might arrive before setting callbacks.
					if (!mouse_in_window && Rect2(position, size).has_point(DisplayServer::get_singleton()->mouse_get_position())) {
						_event_callback(DisplayServer::WINDOW_EVENT_MOUSE_ENTER);
					}
					RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_WHEN_VISIBLE);
					if (DisplayServer::get_singleton()->window_get_flag(DisplayServer::WindowFlags(FLAG_TRANSPARENT), window_id)) {
						set_transparent_background(true);
					}
				} else {
					// Create.
					if (visible) {
						_make_window();
					}
				}
			}

			if (transient && !transient_to_focused) {
				_make_transient();
			}
			if (visible) {
				notification(NOTIFICATION_VISIBILITY_CHANGED);
				emit_signal(SceneStringName(visibility_changed));
				RS::get_singleton()->viewport_set_active(get_viewport_rid(), true);
			}

			// Emits NOTIFICATION_THEME_CHANGED internally.
			set_theme_context(ThemeDB::get_singleton()->get_nearest_theme_context(this));
		} break;

		case NOTIFICATION_READY: {
			if (wrap_controls) {
				// Finish any resizing immediately so it doesn't interfere on stuff overriding _ready().
				_update_child_controls();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			emit_signal(SceneStringName(theme_changed));
			_invalidate_theme_cache();
			_update_theme_item_cache();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			_invalidate_theme_cache();
			_update_theme_item_cache();

			tr_title = atr(title);
#ifdef DEBUG_ENABLED
			if (window_id == DisplayServer::MAIN_WINDOW_ID && !Engine::get_singleton()->is_project_manager_hint()) {
				// Append a suffix to the window title to denote that the project is running
				// from a debug build (including the editor, excluding the project manager).
				// Since this results in lower performance, this should be clearly presented
				// to the user.
				tr_title = vformat("%s (DEBUG)", tr_title);
			}
#endif

			if (!embedder && window_id != DisplayServer::INVALID_WINDOW_ID) {
				DisplayServer::get_singleton()->window_set_title(tr_title, window_id);
				if (keep_title_visible) {
					Size2i title_size = DisplayServer::get_singleton()->window_get_title_size(tr_title, window_id);
					Size2i size_limit = get_clamped_minimum_size();
					if (title_size.x > size_limit.x || title_size.y > size_limit.y) {
						_update_window_size();
					}
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (unparent_when_invisible && !is_visible()) {
				Node *p = get_parent();
				if (p) {
					p->remove_child(this);
				}
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &Window::_settings_changed))) {
				ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &Window::_settings_changed));
			}

			set_theme_context(nullptr, false);

			if (transient) {
				_clear_transient();
			}

			if (!is_embedded() && window_id != DisplayServer::INVALID_WINDOW_ID) {
				if (window_id == DisplayServer::MAIN_WINDOW_ID) {
					RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_DISABLED);
					_update_window_callbacks();
				} else {
					_clear_window();
				}
			} else {
				if (embedder) {
					embedder->_sub_window_remove(this);
					embedder = nullptr;
					RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_DISABLED);
				}
				_update_viewport_size(); //called by clear and make, which does not happen here
			}

			RS::get_singleton()->viewport_set_active(get_viewport_rid(), false);
		} break;

		case NOTIFICATION_VP_MOUSE_ENTER: {
			emit_signal(SceneStringName(mouse_entered));
		} break;

		case NOTIFICATION_VP_MOUSE_EXIT: {
			emit_signal(SceneStringName(mouse_exited));
		} break;
	}
}

void Window::set_content_scale_size(const Size2i &p_size) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(p_size.x < 0);
	ERR_FAIL_COND(p_size.y < 0);
	content_scale_size = p_size;
	_update_viewport_size();
}

Size2i Window::get_content_scale_size() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	return content_scale_size;
}

void Window::set_content_scale_mode(ContentScaleMode p_mode) {
	ERR_MAIN_THREAD_GUARD;
	content_scale_mode = p_mode;
	_update_viewport_size();
}

Window::ContentScaleMode Window::get_content_scale_mode() const {
	ERR_READ_THREAD_GUARD_V(CONTENT_SCALE_MODE_DISABLED);
	return content_scale_mode;
}

void Window::set_content_scale_aspect(ContentScaleAspect p_aspect) {
	ERR_MAIN_THREAD_GUARD;
	content_scale_aspect = p_aspect;
	_update_viewport_size();
}

Window::ContentScaleAspect Window::get_content_scale_aspect() const {
	ERR_READ_THREAD_GUARD_V(CONTENT_SCALE_ASPECT_IGNORE);
	return content_scale_aspect;
}

void Window::set_content_scale_stretch(ContentScaleStretch p_stretch) {
	content_scale_stretch = p_stretch;
	_update_viewport_size();
}

Window::ContentScaleStretch Window::get_content_scale_stretch() const {
	return content_scale_stretch;
}

void Window::set_keep_title_visible(bool p_title_visible) {
	if (keep_title_visible == p_title_visible) {
		return;
	}
	keep_title_visible = p_title_visible;
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		_update_window_size();
	}
}

bool Window::get_keep_title_visible() const {
	return keep_title_visible;
}

void Window::set_content_scale_factor(real_t p_factor) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(p_factor <= 0);
	content_scale_factor = p_factor;
	_update_viewport_size();
}

real_t Window::get_content_scale_factor() const {
	ERR_READ_THREAD_GUARD_V(0);
	return content_scale_factor;
}

void Window::set_use_font_oversampling(bool p_oversampling) {
	ERR_MAIN_THREAD_GUARD;
	if (is_inside_tree() && window_id != DisplayServer::MAIN_WINDOW_ID) {
		ERR_FAIL_MSG("Only the root window can set and use font oversampling.");
	}
	use_font_oversampling = p_oversampling;
	_update_viewport_size();
}

bool Window::is_using_font_oversampling() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_font_oversampling;
}

DisplayServer::WindowID Window::get_window_id() const {
	ERR_READ_THREAD_GUARD_V(DisplayServer::INVALID_WINDOW_ID);
	if (embedder) {
		return parent->get_window_id();
	}
	return window_id;
}

void Window::set_mouse_passthrough_polygon(const Vector<Vector2> &p_region) {
	ERR_MAIN_THREAD_GUARD;
	mpath = p_region;
	if (window_id == DisplayServer::INVALID_WINDOW_ID) {
		return;
	}
	DisplayServer::get_singleton()->window_set_mouse_passthrough(mpath, window_id);
}

Vector<Vector2> Window::get_mouse_passthrough_polygon() const {
	return mpath;
}

void Window::set_wrap_controls(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	wrap_controls = p_enable;

	if (!is_inside_tree()) {
		return;
	}

	if (updating_child_controls) {
		_update_child_controls();
	} else {
		_update_window_size();
	}
}

bool Window::is_wrapping_controls() const {
	ERR_READ_THREAD_GUARD_V(false);
	return wrap_controls;
}

Size2 Window::_get_contents_minimum_size() const {
	Size2 max;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (c) {
			Point2i pos = c->get_position();
			Size2i min = c->get_combined_minimum_size();

			max = max.max(pos + min);
		}
	}

	return max * content_scale_factor;
}

void Window::child_controls_changed() {
	ERR_MAIN_THREAD_GUARD;
	if (!is_inside_tree() || !visible || updating_child_controls) {
		return;
	}

	updating_child_controls = true;
	callable_mp(this, &Window::_update_child_controls).call_deferred();
}

void Window::_update_child_controls() {
	if (!updating_child_controls) {
		return;
	}

	_update_window_size();

	updating_child_controls = false;
}

bool Window::_can_consume_input_events() const {
	return exclusive_child == nullptr;
}

void Window::_window_input(const Ref<InputEvent> &p_ev) {
	ERR_MAIN_THREAD_GUARD;

	if (exclusive_child != nullptr) {
		if (!is_embedding_subwindows()) { // Not embedding, no need for event.
			return;
		}
	}

	// If the event needs to be handled in a Window-derived class, then it should overwrite
	// `_input_from_window` instead of subscribing to the `window_input` signal, because the signal
	// filters out internal events.
	_input_from_window(p_ev);

	if (p_ev->get_device() != InputEvent::DEVICE_ID_INTERNAL && is_inside_tree()) {
		emit_signal(SceneStringName(window_input), p_ev);
	}

	if (is_inside_tree()) {
		push_input(p_ev);
	}
}

void Window::_window_input_text(const String &p_text) {
	push_text_input(p_text);
}

void Window::_window_drop_files(const Vector<String> &p_files) {
	emit_signal(SNAME("files_dropped"), p_files);
}

Viewport *Window::get_parent_viewport() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	if (get_parent()) {
		return get_parent()->get_viewport();
	} else {
		return nullptr;
	}
}

Window *Window::get_parent_visible_window() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	Viewport *vp = get_parent_viewport();
	Window *window = nullptr;
	while (vp) {
		window = Object::cast_to<Window>(vp);
		if (window && window->visible) {
			break;
		}
		if (!vp->get_parent()) {
			break;
		}

		vp = vp->get_parent()->get_viewport();
	}
	return window;
}

void Window::popup_on_parent(const Rect2i &p_parent_rect) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND_MSG(window_id == DisplayServer::MAIN_WINDOW_ID, "Can't popup the main window.");

	if (!is_embedded()) {
		Window *window = get_parent_visible_window();

		if (!window) {
			popup(p_parent_rect);
		} else {
			popup(Rect2i(window->get_position() + p_parent_rect.position, p_parent_rect.size));
		}
	} else {
		popup(p_parent_rect);
	}
}

void Window::popup_centered_clamped(const Size2i &p_size, float p_fallback_ratio) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND_MSG(window_id == DisplayServer::MAIN_WINDOW_ID, "Can't popup the main window.");

	// Consider the current size when calling with the default value.
	Size2i expected_size = p_size == Size2i() ? size : p_size;

	Rect2 parent_rect;

	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		DisplayServer::WindowID parent_id = get_parent_visible_window()->get_window_id();
		int parent_screen = DisplayServer::get_singleton()->window_get_current_screen(parent_id);
		parent_rect.position = DisplayServer::get_singleton()->screen_get_position(parent_screen);
		parent_rect.size = DisplayServer::get_singleton()->screen_get_size(parent_screen);
	}

	Vector2i size_ratio = parent_rect.size * p_fallback_ratio;

	Rect2i popup_rect;
	popup_rect.size = size_ratio.min(expected_size);
	popup_rect.size = _clamp_window_size(popup_rect.size);

	if (parent_rect != Rect2()) {
		popup_rect.position = parent_rect.position + (parent_rect.size - popup_rect.size) / 2;
	}

	popup(popup_rect);
}

void Window::popup_centered(const Size2i &p_minsize) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND_MSG(window_id == DisplayServer::MAIN_WINDOW_ID, "Can't popup the main window.");

	// Consider the current size when calling with the default value.
	Size2i expected_size = p_minsize == Size2i() ? size : p_minsize;

	Rect2 parent_rect;

	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		DisplayServer::WindowID parent_id = get_parent_visible_window()->get_window_id();
		int parent_screen = DisplayServer::get_singleton()->window_get_current_screen(parent_id);
		parent_rect.position = DisplayServer::get_singleton()->screen_get_position(parent_screen);
		parent_rect.size = DisplayServer::get_singleton()->screen_get_size(parent_screen);
	}

	Rect2i popup_rect;
	popup_rect.size = _clamp_window_size(expected_size);

	if (parent_rect != Rect2()) {
		popup_rect.position = parent_rect.position + (parent_rect.size - popup_rect.size) / 2;
	}

	popup(popup_rect);
}

void Window::popup_centered_ratio(float p_ratio) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND_MSG(window_id == DisplayServer::MAIN_WINDOW_ID, "Can't popup the main window.");
	ERR_FAIL_COND_MSG(p_ratio <= 0.0 || p_ratio > 1.0, "Ratio must be between 0.0 and 1.0!");

	Rect2 parent_rect;

	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		DisplayServer::WindowID parent_id = get_parent_visible_window()->get_window_id();
		int parent_screen = DisplayServer::get_singleton()->window_get_current_screen(parent_id);
		parent_rect.position = DisplayServer::get_singleton()->screen_get_position(parent_screen);
		parent_rect.size = DisplayServer::get_singleton()->screen_get_size(parent_screen);
	}

	Rect2i popup_rect;
	if (parent_rect != Rect2()) {
		popup_rect.size = parent_rect.size * p_ratio;
		popup_rect.size = _clamp_window_size(popup_rect.size);
		popup_rect.position = parent_rect.position + (parent_rect.size - popup_rect.size) / 2;
	}

	popup(popup_rect);
}

void Window::popup(const Rect2i &p_screen_rect) {
	ERR_MAIN_THREAD_GUARD;
	emit_signal(SNAME("about_to_popup"));

	if (!get_embedder() && get_flag(FLAG_POPUP)) {
		// Send a focus-out notification when opening a Window Manager Popup.
		SceneTree *scene_tree = get_tree();
		if (scene_tree) {
			scene_tree->notify_group_flags(SceneTree::GROUP_CALL_DEFERRED, "_viewports", NOTIFICATION_WM_WINDOW_FOCUS_OUT);
		}
	}

	// Update window size to calculate the actual window size based on contents minimum size and minimum size.
	_update_window_size();

	if (p_screen_rect != Rect2i()) {
		set_position(p_screen_rect.position);
		int screen_id = DisplayServer::get_singleton()->get_screen_from_rect(p_screen_rect);
		Size2i screen_size = DisplayServer::get_singleton()->screen_get_usable_rect(screen_id).size;
		Size2i new_size = p_screen_rect.size.min(screen_size);
		set_size(new_size);
	}

	Rect2i adjust = _popup_adjust_rect();
	if (adjust != Rect2i()) {
		set_position(adjust.position);
		set_size(adjust.size);
	}

	int scr = DisplayServer::get_singleton()->get_screen_count();
	for (int i = 0; i < scr; i++) {
		Rect2i r = DisplayServer::get_singleton()->screen_get_usable_rect(i);
		if (r.has_point(position)) {
			current_screen = i;
			break;
		}
	}

	set_transient(true);
	set_visible(true);

	Rect2i parent_rect;
	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		int screen_id = DisplayServer::get_singleton()->window_get_current_screen(get_window_id());
		parent_rect = DisplayServer::get_singleton()->screen_get_usable_rect(screen_id);
	}
	if (parent_rect != Rect2i() && !parent_rect.intersects(Rect2i(position, size))) {
		ERR_PRINT(vformat("Window %d spawned at invalid position: %s.", get_window_id(), position));
		set_position((parent_rect.size - size) / 2);
	}
	if (parent_rect != Rect2i() && is_clamped_to_embedder() && is_embedded()) {
		Rect2i new_rect = fit_rect_in_parent(Rect2i(position, size), parent_rect);
		set_position(new_rect.position);
		set_size(new_rect.size);
	}

	_post_popup();
	notification(NOTIFICATION_POST_POPUP);
}

bool Window::_try_parent_dialog(Node *p_from_node) {
	ERR_FAIL_NULL_V(p_from_node, false);
	ERR_FAIL_COND_V_MSG(is_inside_tree(), false, "Attempting to parent and popup a dialog that already has a parent.");

	Window *w = p_from_node->get_last_exclusive_window();
	if (w && w != this) {
		w->add_child(this);
		return true;
	}
	return false;
}

void Window::popup_exclusive(Node *p_from_node, const Rect2i &p_screen_rect) {
	if (_try_parent_dialog(p_from_node)) {
		popup(p_screen_rect);
	}
}

void Window::popup_exclusive_on_parent(Node *p_from_node, const Rect2i &p_parent_rect) {
	if (_try_parent_dialog(p_from_node)) {
		popup_on_parent(p_parent_rect);
	}
}

void Window::popup_exclusive_centered(Node *p_from_node, const Size2i &p_minsize) {
	if (_try_parent_dialog(p_from_node)) {
		popup_centered(p_minsize);
	}
}

void Window::popup_exclusive_centered_ratio(Node *p_from_node, float p_ratio) {
	if (_try_parent_dialog(p_from_node)) {
		popup_centered_ratio(p_ratio);
	}
}

void Window::popup_exclusive_centered_clamped(Node *p_from_node, const Size2i &p_size, float p_fallback_ratio) {
	if (_try_parent_dialog(p_from_node)) {
		popup_centered_clamped(p_size, p_fallback_ratio);
	}
}

Rect2i Window::fit_rect_in_parent(Rect2i p_rect, const Rect2i &p_parent_rect) const {
	ERR_READ_THREAD_GUARD_V(Rect2i());
	Size2i limit = p_parent_rect.size;
	if (p_rect.position.x + p_rect.size.x > limit.x) {
		p_rect.position.x = limit.x - p_rect.size.x;
	}
	if (p_rect.position.y + p_rect.size.y > limit.y) {
		p_rect.position.y = limit.y - p_rect.size.y;
	}

	if (p_rect.position.x < 0) {
		p_rect.position.x = 0;
	}

	int title_height = get_flag(Window::FLAG_BORDERLESS) ? 0 : theme_cache.title_height;

	if (p_rect.position.y < title_height) {
		p_rect.position.y = title_height;
	}

	return p_rect;
}

Size2 Window::get_contents_minimum_size() const {
	ERR_READ_THREAD_GUARD_V(Size2());
	Vector2 ms;
	if (GDVIRTUAL_CALL(_get_contents_minimum_size, ms)) {
		return ms;
	}
	return _get_contents_minimum_size();
}

Size2 Window::get_clamped_minimum_size() const {
	ERR_READ_THREAD_GUARD_V(Size2());
	if (!wrap_controls) {
		return min_size;
	}

	return min_size.max(get_contents_minimum_size());
}

void Window::grab_focus() {
	ERR_MAIN_THREAD_GUARD;
	if (embedder) {
		embedder->_sub_window_grab_focus(this);
	} else if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_move_to_foreground(window_id);
	}
}

bool Window::has_focus() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		return DisplayServer::get_singleton()->window_is_focused(window_id);
	}
	return focused;
}

void Window::start_drag() {
	ERR_MAIN_THREAD_GUARD;
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_start_drag(window_id);
	} else if (embedder) {
		embedder->_window_start_drag(this);
	}
}

void Window::start_resize(DisplayServer::WindowResizeEdge p_edge) {
	ERR_MAIN_THREAD_GUARD;
	if (get_flag(FLAG_RESIZE_DISABLED)) {
		return;
	}
	if (window_id != DisplayServer::INVALID_WINDOW_ID) {
		DisplayServer::get_singleton()->window_start_resize(p_edge, window_id);
	} else if (embedder) {
		switch (p_edge) {
			case DisplayServer::WINDOW_EDGE_TOP_LEFT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_TOP_LEFT, this);
			} break;
			case DisplayServer::WINDOW_EDGE_TOP: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_TOP, this);
			} break;
			case DisplayServer::WINDOW_EDGE_TOP_RIGHT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_TOP_RIGHT, this);
			} break;
			case DisplayServer::WINDOW_EDGE_LEFT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_LEFT, this);
			} break;
			case DisplayServer::WINDOW_EDGE_RIGHT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_RIGHT, this);
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_LEFT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_BOTTOM_LEFT, this);
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_BOTTOM, this);
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT: {
				embedder->_window_start_resize(Viewport::SUB_WINDOW_RESIZE_BOTTOM_RIGHT, this);
			} break;
			default:
				break;
		}
	}
}

Rect2i Window::get_usable_parent_rect() const {
	ERR_READ_THREAD_GUARD_V(Rect2i());
	ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
	Rect2i parent_rect;
	if (is_embedded()) {
		parent_rect = get_embedder()->get_visible_rect();
	} else {
		const Window *w = is_visible() ? this : get_parent_visible_window();
		//find a parent that can contain us
		ERR_FAIL_NULL_V(w, Rect2());

		parent_rect = DisplayServer::get_singleton()->screen_get_usable_rect(DisplayServer::get_singleton()->window_get_current_screen(w->get_window_id()));
	}
	return parent_rect;
}

void Window::add_child_notify(Node *p_child) {
	if (is_inside_tree() && wrap_controls) {
		child_controls_changed();
	}
}

void Window::remove_child_notify(Node *p_child) {
	if (is_inside_tree() && wrap_controls) {
		child_controls_changed();
	}
}

// Theming.

void Window::set_theme_owner_node(Node *p_node) {
	ERR_MAIN_THREAD_GUARD;
	theme_owner->set_owner_node(p_node);
}

Node *Window::get_theme_owner_node() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return theme_owner->get_owner_node();
}

bool Window::has_theme_owner_node() const {
	ERR_READ_THREAD_GUARD_V(false);
	return theme_owner->has_owner_node();
}

void Window::set_theme_context(ThemeContext *p_context, bool p_propagate) {
	ERR_MAIN_THREAD_GUARD;
	theme_owner->set_owner_context(p_context, p_propagate);
}

void Window::set_theme(const Ref<Theme> &p_theme) {
	ERR_MAIN_THREAD_GUARD;
	if (theme == p_theme) {
		return;
	}

	if (theme.is_valid()) {
		theme->disconnect_changed(callable_mp(this, &Window::_theme_changed));
	}

	theme = p_theme;
	if (theme.is_valid()) {
		theme_owner->propagate_theme_changed(this, this, is_inside_tree(), true);
		theme->connect_changed(callable_mp(this, &Window::_theme_changed), CONNECT_DEFERRED);
		return;
	}

	Control *parent_c = Object::cast_to<Control>(get_parent());
	if (parent_c && parent_c->has_theme_owner_node()) {
		theme_owner->propagate_theme_changed(this, parent_c->get_theme_owner_node(), is_inside_tree(), true);
		return;
	}

	Window *parent_w = cast_to<Window>(get_parent());
	if (parent_w && parent_w->has_theme_owner_node()) {
		theme_owner->propagate_theme_changed(this, parent_w->get_theme_owner_node(), is_inside_tree(), true);
		return;
	}

	theme_owner->propagate_theme_changed(this, nullptr, is_inside_tree(), true);
}

Ref<Theme> Window::get_theme() const {
	ERR_READ_THREAD_GUARD_V(Ref<Theme>());
	return theme;
}

void Window::_theme_changed() {
	if (is_inside_tree()) {
		theme_owner->propagate_theme_changed(this, this, true, false);
	}
}

void Window::_notify_theme_override_changed() {
	if (!bulk_theme_override && is_inside_tree()) {
		notification(NOTIFICATION_THEME_CHANGED);
	}
}

void Window::_invalidate_theme_cache() {
	theme_icon_cache.clear();
	theme_style_cache.clear();
	theme_font_cache.clear();
	theme_font_size_cache.clear();
	theme_color_cache.clear();
	theme_constant_cache.clear();
}

void Window::_update_theme_item_cache() {
	// Request an update on the next frame to reflect theme changes.
	// Updating without a delay can cause a lot of lag.
	if (!wrap_controls) {
		updating_embedded_window = true;
		callable_mp(this, &Window::_update_embedded_window).call_deferred();
	} else {
		child_controls_changed();
	}

	ThemeDB::get_singleton()->update_class_instance_items(this);
}

void Window::_update_embedded_window() {
	if (!updating_embedded_window) {
		return;
	}

	if (embedder) {
		embedder->_sub_window_update(this);
	};

	updating_embedded_window = false;
}

void Window::set_theme_type_variation(const StringName &p_theme_type) {
	ERR_MAIN_THREAD_GUARD;
	theme_type_variation = p_theme_type;
	if (is_inside_tree()) {
		notification(NOTIFICATION_THEME_CHANGED);
	}
}

StringName Window::get_theme_type_variation() const {
	ERR_READ_THREAD_GUARD_V(StringName());
	return theme_type_variation;
}

/// Theme property lookup.

Ref<Texture2D> Window::get_theme_icon(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(Ref<Texture2D>());
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const Ref<Texture2D> *tex = theme_icon_override.getptr(p_name);
		if (tex) {
			return *tex;
		}
	}

	if (theme_icon_cache.has(p_theme_type) && theme_icon_cache[p_theme_type].has(p_name)) {
		return theme_icon_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	Ref<Texture2D> icon = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_ICON, p_name, theme_types);
	theme_icon_cache[p_theme_type][p_name] = icon;
	return icon;
}

Ref<StyleBox> Window::get_theme_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(Ref<StyleBox>());
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const Ref<StyleBox> *style = theme_style_override.getptr(p_name);
		if (style) {
			return *style;
		}
	}

	if (theme_style_cache.has(p_theme_type) && theme_style_cache[p_theme_type].has(p_name)) {
		return theme_style_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	Ref<StyleBox> style = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_STYLEBOX, p_name, theme_types);
	theme_style_cache[p_theme_type][p_name] = style;
	return style;
}

Ref<Font> Window::get_theme_font(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(Ref<Font>());
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const Ref<Font> *font = theme_font_override.getptr(p_name);
		if (font) {
			return *font;
		}
	}

	if (theme_font_cache.has(p_theme_type) && theme_font_cache[p_theme_type].has(p_name)) {
		return theme_font_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	Ref<Font> font = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_FONT, p_name, theme_types);
	theme_font_cache[p_theme_type][p_name] = font;
	return font;
}

int Window::get_theme_font_size(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(0);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const int *font_size = theme_font_size_override.getptr(p_name);
		if (font_size && (*font_size) > 0) {
			return *font_size;
		}
	}

	if (theme_font_size_cache.has(p_theme_type) && theme_font_size_cache[p_theme_type].has(p_name)) {
		return theme_font_size_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	int font_size = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_FONT_SIZE, p_name, theme_types);
	theme_font_size_cache[p_theme_type][p_name] = font_size;
	return font_size;
}

Color Window::get_theme_color(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(Color());
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const Color *color = theme_color_override.getptr(p_name);
		if (color) {
			return *color;
		}
	}

	if (theme_color_cache.has(p_theme_type) && theme_color_cache[p_theme_type].has(p_name)) {
		return theme_color_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	Color color = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_COLOR, p_name, theme_types);
	theme_color_cache[p_theme_type][p_name] = color;
	return color;
}

int Window::get_theme_constant(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(0);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		const int *constant = theme_constant_override.getptr(p_name);
		if (constant) {
			return *constant;
		}
	}

	if (theme_constant_cache.has(p_theme_type) && theme_constant_cache[p_theme_type].has(p_name)) {
		return theme_constant_cache[p_theme_type][p_name];
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	int constant = theme_owner->get_theme_item_in_types(Theme::DATA_TYPE_CONSTANT, p_name, theme_types);
	theme_constant_cache[p_theme_type][p_name] = constant;
	return constant;
}

Variant Window::get_theme_item(Theme::DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR:
			return get_theme_color(p_name, p_theme_type);
		case Theme::DATA_TYPE_CONSTANT:
			return get_theme_constant(p_name, p_theme_type);
		case Theme::DATA_TYPE_FONT:
			return get_theme_font(p_name, p_theme_type);
		case Theme::DATA_TYPE_FONT_SIZE:
			return get_theme_font_size(p_name, p_theme_type);
		case Theme::DATA_TYPE_ICON:
			return get_theme_icon(p_name, p_theme_type);
		case Theme::DATA_TYPE_STYLEBOX:
			return get_theme_stylebox(p_name, p_theme_type);
		case Theme::DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return Variant();
}

#ifdef TOOLS_ENABLED
Ref<Texture2D> Window::get_editor_theme_icon(const StringName &p_name) const {
	return get_theme_icon(p_name, SNAME("EditorIcons"));
}
#endif

bool Window::has_theme_icon(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_icon_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_ICON, p_name, theme_types);
}

bool Window::has_theme_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_stylebox_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_STYLEBOX, p_name, theme_types);
}

bool Window::has_theme_font(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_font_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_FONT, p_name, theme_types);
}

bool Window::has_theme_font_size(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_font_size_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_FONT_SIZE, p_name, theme_types);
}

bool Window::has_theme_color(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_color_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_COLOR, p_name, theme_types);
}

bool Window::has_theme_constant(const StringName &p_name, const StringName &p_theme_type) const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!initialized) {
		WARN_PRINT_ONCE(vformat("Attempting to access theme items too early in %s; prefer NOTIFICATION_POSTINITIALIZE and NOTIFICATION_THEME_CHANGED", get_description()));
	}

	if (p_theme_type == StringName() || p_theme_type == get_class_name() || p_theme_type == theme_type_variation) {
		if (has_theme_constant_override(p_name)) {
			return true;
		}
	}

	Vector<StringName> theme_types;
	theme_owner->get_theme_type_dependencies(this, p_theme_type, theme_types);
	return theme_owner->has_theme_item_in_types(Theme::DATA_TYPE_CONSTANT, p_name, theme_types);
}

/// Local property overrides.

void Window::add_theme_icon_override(const StringName &p_name, const Ref<Texture2D> &p_icon) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(p_icon.is_null());

	if (theme_icon_override.has(p_name)) {
		theme_icon_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_icon_override[p_name] = p_icon;
	theme_icon_override[p_name]->connect_changed(callable_mp(this, &Window::_notify_theme_override_changed), CONNECT_REFERENCE_COUNTED);
	_notify_theme_override_changed();
}

void Window::add_theme_style_override(const StringName &p_name, const Ref<StyleBox> &p_style) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(p_style.is_null());

	if (theme_style_override.has(p_name)) {
		theme_style_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_style_override[p_name] = p_style;
	theme_style_override[p_name]->connect_changed(callable_mp(this, &Window::_notify_theme_override_changed), CONNECT_REFERENCE_COUNTED);
	_notify_theme_override_changed();
}

void Window::add_theme_font_override(const StringName &p_name, const Ref<Font> &p_font) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(p_font.is_null());

	if (theme_font_override.has(p_name)) {
		theme_font_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_font_override[p_name] = p_font;
	theme_font_override[p_name]->connect_changed(callable_mp(this, &Window::_notify_theme_override_changed), CONNECT_REFERENCE_COUNTED);
	_notify_theme_override_changed();
}

void Window::add_theme_font_size_override(const StringName &p_name, int p_font_size) {
	ERR_MAIN_THREAD_GUARD;
	theme_font_size_override[p_name] = p_font_size;
	_notify_theme_override_changed();
}

void Window::add_theme_color_override(const StringName &p_name, const Color &p_color) {
	ERR_MAIN_THREAD_GUARD;
	theme_color_override[p_name] = p_color;
	_notify_theme_override_changed();
}

void Window::add_theme_constant_override(const StringName &p_name, int p_constant) {
	ERR_MAIN_THREAD_GUARD;
	theme_constant_override[p_name] = p_constant;
	_notify_theme_override_changed();
}

void Window::remove_theme_icon_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	if (theme_icon_override.has(p_name)) {
		theme_icon_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_icon_override.erase(p_name);
	_notify_theme_override_changed();
}

void Window::remove_theme_style_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	if (theme_style_override.has(p_name)) {
		theme_style_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_style_override.erase(p_name);
	_notify_theme_override_changed();
}

void Window::remove_theme_font_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	if (theme_font_override.has(p_name)) {
		theme_font_override[p_name]->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	theme_font_override.erase(p_name);
	_notify_theme_override_changed();
}

void Window::remove_theme_font_size_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	theme_font_size_override.erase(p_name);
	_notify_theme_override_changed();
}

void Window::remove_theme_color_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	theme_color_override.erase(p_name);
	_notify_theme_override_changed();
}

void Window::remove_theme_constant_override(const StringName &p_name) {
	ERR_MAIN_THREAD_GUARD;
	theme_constant_override.erase(p_name);
	_notify_theme_override_changed();
}

bool Window::has_theme_icon_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const Ref<Texture2D> *tex = theme_icon_override.getptr(p_name);
	return tex != nullptr;
}

bool Window::has_theme_stylebox_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const Ref<StyleBox> *style = theme_style_override.getptr(p_name);
	return style != nullptr;
}

bool Window::has_theme_font_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const Ref<Font> *font = theme_font_override.getptr(p_name);
	return font != nullptr;
}

bool Window::has_theme_font_size_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const int *font_size = theme_font_size_override.getptr(p_name);
	return font_size != nullptr;
}

bool Window::has_theme_color_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const Color *color = theme_color_override.getptr(p_name);
	return color != nullptr;
}

bool Window::has_theme_constant_override(const StringName &p_name) const {
	ERR_READ_THREAD_GUARD_V(false);
	const int *constant = theme_constant_override.getptr(p_name);
	return constant != nullptr;
}

/// Default theme properties.

float Window::get_theme_default_base_scale() const {
	ERR_READ_THREAD_GUARD_V(0);
	return theme_owner->get_theme_default_base_scale();
}

Ref<Font> Window::get_theme_default_font() const {
	ERR_READ_THREAD_GUARD_V(Ref<Font>());
	return theme_owner->get_theme_default_font();
}

int Window::get_theme_default_font_size() const {
	ERR_READ_THREAD_GUARD_V(0);
	return theme_owner->get_theme_default_font_size();
}

/// Bulk actions.

void Window::begin_bulk_theme_override() {
	ERR_MAIN_THREAD_GUARD;
	bulk_theme_override = true;
}

void Window::end_bulk_theme_override() {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!bulk_theme_override);

	bulk_theme_override = false;
	_notify_theme_override_changed();
}

//

Rect2i Window::get_parent_rect() const {
	ERR_READ_THREAD_GUARD_V(Rect2i());
	ERR_FAIL_COND_V(!is_inside_tree(), Rect2i());
	if (is_embedded()) {
		//viewport
		Node *n = get_parent();
		ERR_FAIL_NULL_V(n, Rect2i());
		Viewport *p = n->get_viewport();
		ERR_FAIL_NULL_V(p, Rect2i());

		return p->get_visible_rect();
	} else {
		int x = get_position().x;
		int closest_dist = 0x7FFFFFFF;
		Rect2i closest_rect;
		for (int i = 0; i < DisplayServer::get_singleton()->get_screen_count(); i++) {
			Rect2i s(DisplayServer::get_singleton()->screen_get_position(i), DisplayServer::get_singleton()->screen_get_size(i));
			int d;
			if (x >= s.position.x && x < s.size.x) {
				//contained
				closest_rect = s;
				break;
			} else if (x < s.position.x) {
				d = s.position.x - x;
			} else {
				d = x - (s.position.x + s.size.x);
			}

			if (d < closest_dist) {
				closest_dist = d;
				closest_rect = s;
			}
		}
		return closest_rect;
	}
}

void Window::set_clamp_to_embedder(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	clamp_to_embedder = p_enable;
}

bool Window::is_clamped_to_embedder() const {
	ERR_READ_THREAD_GUARD_V(false);
	return clamp_to_embedder;
}

void Window::set_unparent_when_invisible(bool p_unparent) {
	unparent_when_invisible = p_unparent;
}

void Window::set_layout_direction(Window::LayoutDirection p_direction) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_direction, LAYOUT_DIRECTION_MAX);

	layout_dir = p_direction;
	propagate_notification(Control::NOTIFICATION_LAYOUT_DIRECTION_CHANGED);
}

Window::LayoutDirection Window::get_layout_direction() const {
	ERR_READ_THREAD_GUARD_V(LAYOUT_DIRECTION_INHERITED);
	return layout_dir;
}

bool Window::is_layout_rtl() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (layout_dir == LAYOUT_DIRECTION_INHERITED) {
#ifdef TOOLS_ENABLED
		if (is_part_of_edited_scene() && GLOBAL_GET(SNAME("internationalization/rendering/force_right_to_left_layout_direction"))) {
			return true;
		}
		if (is_inside_tree()) {
			Node *edited_scene_root = get_tree()->get_edited_scene_root();
			if (edited_scene_root == this) {
				int proj_root_layout_direction = GLOBAL_GET(SNAME("internationalization/rendering/root_node_layout_direction"));
				if (proj_root_layout_direction == 1) {
					return false;
				} else if (proj_root_layout_direction == 2) {
					return true;
				} else if (proj_root_layout_direction == 3) {
					String locale = OS::get_singleton()->get_locale();
					return TS->is_locale_right_to_left(locale);
				} else {
					String locale = TranslationServer::get_singleton()->get_tool_locale();
					return TS->is_locale_right_to_left(locale);
				}
			}
		}
#else
		if (GLOBAL_GET(SNAME("internationalization/rendering/force_right_to_left_layout_direction"))) {
			return true;
		}
#endif
		Node *parent_node = get_parent();
		while (parent_node) {
			Control *parent_control = Object::cast_to<Control>(parent_node);
			if (parent_control) {
				return parent_control->is_layout_rtl();
			}

			Window *parent_window = Object::cast_to<Window>(parent_node);
			if (parent_window) {
				return parent_window->is_layout_rtl();
			}
			parent_node = parent_node->get_parent();
		}

		if (root_layout_direction == 1) {
			return false;
		} else if (root_layout_direction == 2) {
			return true;
		} else if (root_layout_direction == 3) {
			String locale = OS::get_singleton()->get_locale();
			return TS->is_locale_right_to_left(locale);
		} else {
			String locale = TranslationServer::get_singleton()->get_tool_locale();
			return TS->is_locale_right_to_left(locale);
		}
	} else if (layout_dir == LAYOUT_DIRECTION_APPLICATION_LOCALE) {
		if (GLOBAL_GET(SNAME("internationalization/rendering/force_right_to_left_layout_direction"))) {
			return true;
		} else {
			String locale = TranslationServer::get_singleton()->get_tool_locale();
			return TS->is_locale_right_to_left(locale);
		}
	} else if (layout_dir == LAYOUT_DIRECTION_SYSTEM_LOCALE) {
		if (GLOBAL_GET(SNAME("internationalization/rendering/force_right_to_left_layout_direction"))) {
			return true;
		} else {
			String locale = OS::get_singleton()->get_locale();
			return TS->is_locale_right_to_left(locale);
		}
	} else {
		return (layout_dir == LAYOUT_DIRECTION_RTL);
	}
}

#ifndef DISABLE_DEPRECATED
void Window::set_auto_translate(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	set_auto_translate_mode(p_enable ? AUTO_TRANSLATE_MODE_ALWAYS : AUTO_TRANSLATE_MODE_DISABLED);
}

bool Window::is_auto_translating() const {
	ERR_READ_THREAD_GUARD_V(false);
	return can_auto_translate();
}
#endif

Transform2D Window::get_final_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return window_transform * stretch_transform * global_canvas_transform;
}

Transform2D Window::get_screen_transform_internal(bool p_absolute_position) const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	Transform2D embedder_transform;
	if (get_embedder()) {
		embedder_transform.translate_local(get_position());
		embedder_transform = get_embedder()->get_screen_transform_internal(p_absolute_position) * embedder_transform;
	} else if (p_absolute_position) {
		embedder_transform.translate_local(get_position());
	}
	return embedder_transform * get_final_transform();
}

Transform2D Window::get_popup_base_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	if (is_embedding_subwindows()) {
		return Transform2D();
	}
	Transform2D popup_base_transform;
	popup_base_transform.set_origin(get_position());
	popup_base_transform *= get_final_transform();
	if (get_embedder()) {
		return get_embedder()->get_popup_base_transform() * popup_base_transform;
	}
	return popup_base_transform;
}

Viewport *Window::get_section_root_viewport() const {
	if (get_embedder()) {
		return get_embedder()->get_section_root_viewport();
	}
	if (is_inside_tree()) {
		// Native window.
		return SceneTree::get_singleton()->get_root();
	}
	Window *vp = const_cast<Window *>(this);
	return vp;
}

bool Window::is_attached_in_viewport() const {
	return get_embedder();
}

void Window::_update_mouse_over(Vector2 p_pos) {
	if (!mouse_in_window) {
		if (is_embedded()) {
			mouse_in_window = true;
			_propagate_window_notification(this, NOTIFICATION_WM_MOUSE_ENTER);
		}
	}

	bool new_in = get_visible_rect().has_point(p_pos);
	if (new_in == gui.mouse_in_viewport) {
		if (new_in) {
			Viewport::_update_mouse_over(p_pos);
		}
		return;
	}

	if (new_in) {
		notification(NOTIFICATION_VP_MOUSE_ENTER);
		Viewport::_update_mouse_over(p_pos);
	} else {
		Viewport::_mouse_leave_viewport();
	}
}

void Window::_mouse_leave_viewport() {
	Viewport::_mouse_leave_viewport();
	if (is_embedded()) {
		mouse_in_window = false;
		_propagate_window_notification(this, NOTIFICATION_WM_MOUSE_EXIT);
	}
}

void Window::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &Window::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &Window::get_title);

	ClassDB::bind_method(D_METHOD("get_window_id"), &Window::get_window_id);

	ClassDB::bind_method(D_METHOD("set_initial_position", "initial_position"), &Window::set_initial_position);
	ClassDB::bind_method(D_METHOD("get_initial_position"), &Window::get_initial_position);

	ClassDB::bind_method(D_METHOD("set_current_screen", "index"), &Window::set_current_screen);
	ClassDB::bind_method(D_METHOD("get_current_screen"), &Window::get_current_screen);

	ClassDB::bind_method(D_METHOD("set_position", "position"), &Window::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &Window::get_position);
	ClassDB::bind_method(D_METHOD("move_to_center"), &Window::move_to_center);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &Window::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &Window::get_size);
	ClassDB::bind_method(D_METHOD("reset_size"), &Window::reset_size);

	ClassDB::bind_method(D_METHOD("get_position_with_decorations"), &Window::get_position_with_decorations);
	ClassDB::bind_method(D_METHOD("get_size_with_decorations"), &Window::get_size_with_decorations);

	ClassDB::bind_method(D_METHOD("set_max_size", "max_size"), &Window::set_max_size);
	ClassDB::bind_method(D_METHOD("get_max_size"), &Window::get_max_size);

	ClassDB::bind_method(D_METHOD("set_min_size", "min_size"), &Window::set_min_size);
	ClassDB::bind_method(D_METHOD("get_min_size"), &Window::get_min_size);

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &Window::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &Window::get_mode);

	ClassDB::bind_method(D_METHOD("set_flag", "flag", "enabled"), &Window::set_flag);
	ClassDB::bind_method(D_METHOD("get_flag", "flag"), &Window::get_flag);

	ClassDB::bind_method(D_METHOD("is_maximize_allowed"), &Window::is_maximize_allowed);

	ClassDB::bind_method(D_METHOD("request_attention"), &Window::request_attention);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("move_to_foreground"), &Window::move_to_foreground);
#endif // DISABLE_DEPRECATED

	ClassDB::bind_method(D_METHOD("set_visible", "visible"), &Window::set_visible);
	ClassDB::bind_method(D_METHOD("is_visible"), &Window::is_visible);

	ClassDB::bind_method(D_METHOD("hide"), &Window::hide);
	ClassDB::bind_method(D_METHOD("show"), &Window::show);

	ClassDB::bind_method(D_METHOD("set_transient", "transient"), &Window::set_transient);
	ClassDB::bind_method(D_METHOD("is_transient"), &Window::is_transient);

	ClassDB::bind_method(D_METHOD("set_transient_to_focused", "enable"), &Window::set_transient_to_focused);
	ClassDB::bind_method(D_METHOD("is_transient_to_focused"), &Window::is_transient_to_focused);

	ClassDB::bind_method(D_METHOD("set_exclusive", "exclusive"), &Window::set_exclusive);
	ClassDB::bind_method(D_METHOD("is_exclusive"), &Window::is_exclusive);

	ClassDB::bind_method(D_METHOD("set_unparent_when_invisible", "unparent"), &Window::set_unparent_when_invisible);

	ClassDB::bind_method(D_METHOD("can_draw"), &Window::can_draw);
	ClassDB::bind_method(D_METHOD("has_focus"), &Window::has_focus);
	ClassDB::bind_method(D_METHOD("grab_focus"), &Window::grab_focus);

	ClassDB::bind_method(D_METHOD("start_drag"), &Window::start_drag);
	ClassDB::bind_method(D_METHOD("start_resize", "edge"), &Window::start_resize);

	ClassDB::bind_method(D_METHOD("set_ime_active", "active"), &Window::set_ime_active);
	ClassDB::bind_method(D_METHOD("set_ime_position", "position"), &Window::set_ime_position);

	ClassDB::bind_method(D_METHOD("is_embedded"), &Window::is_embedded);

	ClassDB::bind_method(D_METHOD("get_contents_minimum_size"), &Window::get_contents_minimum_size);

	ClassDB::bind_method(D_METHOD("set_force_native", "force_native"), &Window::set_force_native);
	ClassDB::bind_method(D_METHOD("get_force_native"), &Window::get_force_native);

	ClassDB::bind_method(D_METHOD("set_content_scale_size", "size"), &Window::set_content_scale_size);
	ClassDB::bind_method(D_METHOD("get_content_scale_size"), &Window::get_content_scale_size);

	ClassDB::bind_method(D_METHOD("set_content_scale_mode", "mode"), &Window::set_content_scale_mode);
	ClassDB::bind_method(D_METHOD("get_content_scale_mode"), &Window::get_content_scale_mode);

	ClassDB::bind_method(D_METHOD("set_content_scale_aspect", "aspect"), &Window::set_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("get_content_scale_aspect"), &Window::get_content_scale_aspect);

	ClassDB::bind_method(D_METHOD("set_content_scale_stretch", "stretch"), &Window::set_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("get_content_scale_stretch"), &Window::get_content_scale_stretch);

	ClassDB::bind_method(D_METHOD("set_keep_title_visible", "title_visible"), &Window::set_keep_title_visible);
	ClassDB::bind_method(D_METHOD("get_keep_title_visible"), &Window::get_keep_title_visible);

	ClassDB::bind_method(D_METHOD("set_content_scale_factor", "factor"), &Window::set_content_scale_factor);
	ClassDB::bind_method(D_METHOD("get_content_scale_factor"), &Window::get_content_scale_factor);

	ClassDB::bind_method(D_METHOD("set_use_font_oversampling", "enable"), &Window::set_use_font_oversampling);
	ClassDB::bind_method(D_METHOD("is_using_font_oversampling"), &Window::is_using_font_oversampling);

	ClassDB::bind_method(D_METHOD("set_mouse_passthrough_polygon", "polygon"), &Window::set_mouse_passthrough_polygon);
	ClassDB::bind_method(D_METHOD("get_mouse_passthrough_polygon"), &Window::get_mouse_passthrough_polygon);

	ClassDB::bind_method(D_METHOD("set_wrap_controls", "enable"), &Window::set_wrap_controls);
	ClassDB::bind_method(D_METHOD("is_wrapping_controls"), &Window::is_wrapping_controls);
	ClassDB::bind_method(D_METHOD("child_controls_changed"), &Window::child_controls_changed);

	ClassDB::bind_method(D_METHOD("set_theme", "theme"), &Window::set_theme);
	ClassDB::bind_method(D_METHOD("get_theme"), &Window::get_theme);

	ClassDB::bind_method(D_METHOD("set_theme_type_variation", "theme_type"), &Window::set_theme_type_variation);
	ClassDB::bind_method(D_METHOD("get_theme_type_variation"), &Window::get_theme_type_variation);

	ClassDB::bind_method(D_METHOD("begin_bulk_theme_override"), &Window::begin_bulk_theme_override);
	ClassDB::bind_method(D_METHOD("end_bulk_theme_override"), &Window::end_bulk_theme_override);

	ClassDB::bind_method(D_METHOD("add_theme_icon_override", "name", "texture"), &Window::add_theme_icon_override);
	ClassDB::bind_method(D_METHOD("add_theme_stylebox_override", "name", "stylebox"), &Window::add_theme_style_override);
	ClassDB::bind_method(D_METHOD("add_theme_font_override", "name", "font"), &Window::add_theme_font_override);
	ClassDB::bind_method(D_METHOD("add_theme_font_size_override", "name", "font_size"), &Window::add_theme_font_size_override);
	ClassDB::bind_method(D_METHOD("add_theme_color_override", "name", "color"), &Window::add_theme_color_override);
	ClassDB::bind_method(D_METHOD("add_theme_constant_override", "name", "constant"), &Window::add_theme_constant_override);

	ClassDB::bind_method(D_METHOD("remove_theme_icon_override", "name"), &Window::remove_theme_icon_override);
	ClassDB::bind_method(D_METHOD("remove_theme_stylebox_override", "name"), &Window::remove_theme_style_override);
	ClassDB::bind_method(D_METHOD("remove_theme_font_override", "name"), &Window::remove_theme_font_override);
	ClassDB::bind_method(D_METHOD("remove_theme_font_size_override", "name"), &Window::remove_theme_font_size_override);
	ClassDB::bind_method(D_METHOD("remove_theme_color_override", "name"), &Window::remove_theme_color_override);
	ClassDB::bind_method(D_METHOD("remove_theme_constant_override", "name"), &Window::remove_theme_constant_override);

	ClassDB::bind_method(D_METHOD("get_theme_icon", "name", "theme_type"), &Window::get_theme_icon, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_theme_stylebox", "name", "theme_type"), &Window::get_theme_stylebox, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_theme_font", "name", "theme_type"), &Window::get_theme_font, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_theme_font_size", "name", "theme_type"), &Window::get_theme_font_size, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_theme_color", "name", "theme_type"), &Window::get_theme_color, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_theme_constant", "name", "theme_type"), &Window::get_theme_constant, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("has_theme_icon_override", "name"), &Window::has_theme_icon_override);
	ClassDB::bind_method(D_METHOD("has_theme_stylebox_override", "name"), &Window::has_theme_stylebox_override);
	ClassDB::bind_method(D_METHOD("has_theme_font_override", "name"), &Window::has_theme_font_override);
	ClassDB::bind_method(D_METHOD("has_theme_font_size_override", "name"), &Window::has_theme_font_size_override);
	ClassDB::bind_method(D_METHOD("has_theme_color_override", "name"), &Window::has_theme_color_override);
	ClassDB::bind_method(D_METHOD("has_theme_constant_override", "name"), &Window::has_theme_constant_override);

	ClassDB::bind_method(D_METHOD("has_theme_icon", "name", "theme_type"), &Window::has_theme_icon, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_theme_stylebox", "name", "theme_type"), &Window::has_theme_stylebox, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_theme_font", "name", "theme_type"), &Window::has_theme_font, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_theme_font_size", "name", "theme_type"), &Window::has_theme_font_size, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_theme_color", "name", "theme_type"), &Window::has_theme_color, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_theme_constant", "name", "theme_type"), &Window::has_theme_constant, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("get_theme_default_base_scale"), &Window::get_theme_default_base_scale);
	ClassDB::bind_method(D_METHOD("get_theme_default_font"), &Window::get_theme_default_font);
	ClassDB::bind_method(D_METHOD("get_theme_default_font_size"), &Window::get_theme_default_font_size);

	ClassDB::bind_method(D_METHOD("set_layout_direction", "direction"), &Window::set_layout_direction);
	ClassDB::bind_method(D_METHOD("get_layout_direction"), &Window::get_layout_direction);
	ClassDB::bind_method(D_METHOD("is_layout_rtl"), &Window::is_layout_rtl);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_auto_translate", "enable"), &Window::set_auto_translate);
	ClassDB::bind_method(D_METHOD("is_auto_translating"), &Window::is_auto_translating);
#endif

	ClassDB::bind_method(D_METHOD("popup", "rect"), &Window::popup, DEFVAL(Rect2i()));
	ClassDB::bind_method(D_METHOD("popup_on_parent", "parent_rect"), &Window::popup_on_parent);
	ClassDB::bind_method(D_METHOD("popup_centered", "minsize"), &Window::popup_centered, DEFVAL(Size2i()));
	ClassDB::bind_method(D_METHOD("popup_centered_ratio", "ratio"), &Window::popup_centered_ratio, DEFVAL(0.8));
	ClassDB::bind_method(D_METHOD("popup_centered_clamped", "minsize", "fallback_ratio"), &Window::popup_centered_clamped, DEFVAL(Size2i()), DEFVAL(0.75));

	ClassDB::bind_method(D_METHOD("popup_exclusive", "from_node", "rect"), &Window::popup_exclusive, DEFVAL(Rect2i()));
	ClassDB::bind_method(D_METHOD("popup_exclusive_on_parent", "from_node", "parent_rect"), &Window::popup_exclusive_on_parent);
	ClassDB::bind_method(D_METHOD("popup_exclusive_centered", "from_node", "minsize"), &Window::popup_exclusive_centered, DEFVAL(Size2i()));
	ClassDB::bind_method(D_METHOD("popup_exclusive_centered_ratio", "from_node", "ratio"), &Window::popup_exclusive_centered_ratio, DEFVAL(0.8));
	ClassDB::bind_method(D_METHOD("popup_exclusive_centered_clamped", "from_node", "minsize", "fallback_ratio"), &Window::popup_exclusive_centered_clamped, DEFVAL(Size2i()), DEFVAL(0.75));

	// Keep the enum values in sync with the `Mode` enum.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Windowed,Minimized,Maximized,Fullscreen,Exclusive Fullscreen"), "set_mode", "get_mode");

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");

	// Keep the enum values in sync with the `WindowInitialPosition` enum.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "initial_position", PROPERTY_HINT_ENUM, "Absolute,Center of Primary Screen,Center of Main Window Screen,Center of Other Screen,Center of Screen With Mouse Pointer,Center of Screen With Keyboard Focus"), "set_initial_position", "get_initial_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "position", PROPERTY_HINT_NONE, "suffix:px"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size", PROPERTY_HINT_NONE, "suffix:px"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_screen", PROPERTY_HINT_RANGE, "0,64,1,or_greater"), "set_current_screen", "get_current_screen");

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "mouse_passthrough_polygon"), "set_mouse_passthrough_polygon", "get_mouse_passthrough_polygon");

	ADD_GROUP("Flags", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "is_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "wrap_controls"), "set_wrap_controls", "is_wrapping_controls");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transient"), "set_transient", "is_transient");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transient_to_focused"), "set_transient_to_focused", "is_transient_to_focused");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "exclusive"), "set_exclusive", "is_exclusive");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "unresizable"), "set_flag", "get_flag", FLAG_RESIZE_DISABLED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "borderless"), "set_flag", "get_flag", FLAG_BORDERLESS);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "always_on_top"), "set_flag", "get_flag", FLAG_ALWAYS_ON_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "transparent"), "set_flag", "get_flag", FLAG_TRANSPARENT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "unfocusable"), "set_flag", "get_flag", FLAG_NO_FOCUS);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "popup_window"), "set_flag", "get_flag", FLAG_POPUP);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "extend_to_title"), "set_flag", "get_flag", FLAG_EXTEND_TO_TITLE);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "mouse_passthrough"), "set_flag", "get_flag", FLAG_MOUSE_PASSTHROUGH);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "sharp_corners"), "set_flag", "get_flag", FLAG_SHARP_CORNERS);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "exclude_from_capture"), "set_flag", "get_flag", FLAG_EXCLUDE_FROM_CAPTURE);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "force_native"), "set_force_native", "get_force_native");

	ADD_GROUP("Limits", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "min_size", PROPERTY_HINT_NONE, "suffix:px"), "set_min_size", "get_min_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "max_size", PROPERTY_HINT_NONE, "suffix:px"), "set_max_size", "get_max_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_title_visible"), "set_keep_title_visible", "get_keep_title_visible");

	ADD_GROUP("Content Scale", "content_scale_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "content_scale_size"), "set_content_scale_size", "get_content_scale_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_mode", PROPERTY_HINT_ENUM, "Disabled,Canvas Items,Viewport"), "set_content_scale_mode", "get_content_scale_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_aspect", PROPERTY_HINT_ENUM, "Ignore,Keep,Keep Width,Keep Height,Expand"), "set_content_scale_aspect", "get_content_scale_aspect");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_stretch", PROPERTY_HINT_ENUM, "Fractional,Integer"), "set_content_scale_stretch", "get_content_scale_stretch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "content_scale_factor", PROPERTY_HINT_RANGE, "0.5,8.0,0.01"), "set_content_scale_factor", "get_content_scale_factor");

#ifndef DISABLE_DEPRECATED
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_translate", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_auto_translate", "is_auto_translating");
#endif

	ADD_GROUP("Theme", "theme_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "theme", PROPERTY_HINT_RESOURCE_TYPE, "Theme"), "set_theme", "get_theme");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "theme_type_variation", PROPERTY_HINT_ENUM_SUGGESTION), "set_theme_type_variation", "get_theme_type_variation");

	ADD_SIGNAL(MethodInfo("window_input", PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEvent")));
	ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files")));
	ADD_SIGNAL(MethodInfo("mouse_entered"));
	ADD_SIGNAL(MethodInfo("mouse_exited"));
	ADD_SIGNAL(MethodInfo("focus_entered"));
	ADD_SIGNAL(MethodInfo("focus_exited"));
	ADD_SIGNAL(MethodInfo("close_requested"));
	ADD_SIGNAL(MethodInfo("go_back_requested"));
	ADD_SIGNAL(MethodInfo("visibility_changed"));
	ADD_SIGNAL(MethodInfo("about_to_popup"));
	ADD_SIGNAL(MethodInfo("theme_changed"));
	ADD_SIGNAL(MethodInfo("dpi_changed"));
	ADD_SIGNAL(MethodInfo("titlebar_changed"));
	ADD_SIGNAL(MethodInfo("title_changed"));

	BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
	BIND_CONSTANT(NOTIFICATION_THEME_CHANGED);

	BIND_ENUM_CONSTANT(MODE_WINDOWED);
	BIND_ENUM_CONSTANT(MODE_MINIMIZED);
	BIND_ENUM_CONSTANT(MODE_MAXIMIZED);
	BIND_ENUM_CONSTANT(MODE_FULLSCREEN);
	BIND_ENUM_CONSTANT(MODE_EXCLUSIVE_FULLSCREEN);

	BIND_ENUM_CONSTANT(FLAG_RESIZE_DISABLED);
	BIND_ENUM_CONSTANT(FLAG_BORDERLESS);
	BIND_ENUM_CONSTANT(FLAG_ALWAYS_ON_TOP);
	BIND_ENUM_CONSTANT(FLAG_TRANSPARENT);
	BIND_ENUM_CONSTANT(FLAG_NO_FOCUS);
	BIND_ENUM_CONSTANT(FLAG_POPUP);
	BIND_ENUM_CONSTANT(FLAG_EXTEND_TO_TITLE);
	BIND_ENUM_CONSTANT(FLAG_MOUSE_PASSTHROUGH);
	BIND_ENUM_CONSTANT(FLAG_SHARP_CORNERS);
	BIND_ENUM_CONSTANT(FLAG_EXCLUDE_FROM_CAPTURE);
	BIND_ENUM_CONSTANT(FLAG_MAX);

	BIND_ENUM_CONSTANT(CONTENT_SCALE_MODE_DISABLED);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_MODE_CANVAS_ITEMS);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_MODE_VIEWPORT);

	BIND_ENUM_CONSTANT(CONTENT_SCALE_ASPECT_IGNORE);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_ASPECT_KEEP);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_ASPECT_KEEP_WIDTH);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_ASPECT_KEEP_HEIGHT);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_ASPECT_EXPAND);

	BIND_ENUM_CONSTANT(CONTENT_SCALE_STRETCH_FRACTIONAL);
	BIND_ENUM_CONSTANT(CONTENT_SCALE_STRETCH_INTEGER);

	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_INHERITED);
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_APPLICATION_LOCALE);
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_LTR);
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_RTL);
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_SYSTEM_LOCALE);
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_MAX);
#ifndef DISABLE_DEPRECATED
	BIND_ENUM_CONSTANT(LAYOUT_DIRECTION_LOCALE);
#endif // DISABLE_DEPRECATED

	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_ABSOLUTE);
	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_CENTER_PRIMARY_SCREEN);
	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_CENTER_MAIN_WINDOW_SCREEN);
	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_CENTER_OTHER_SCREEN);
	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_MOUSE_FOCUS);
	BIND_ENUM_CONSTANT(WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_KEYBOARD_FOCUS);

	GDVIRTUAL_BIND(_get_contents_minimum_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Window, embedded_border);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Window, embedded_unfocused_border);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, Window, title_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, Window, title_font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Window, title_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Window, title_height);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Window, title_outline_modulate);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Window, title_outline_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Window, close);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Window, close_pressed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Window, close_h_offset);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Window, close_v_offset);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Window, resize_margin);
}

Window::Window() {
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server) {
		max_size = rendering_server->get_maximum_viewport_size();
		max_size_used = max_size; // Update max_size_used.
	}

	theme_owner = memnew(ThemeOwner(this));
	RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::VIEWPORT_UPDATE_DISABLED);
}

Window::~Window() {
	memdelete(theme_owner);

	// Resources need to be disconnected.
	for (KeyValue<StringName, Ref<Texture2D>> &E : theme_icon_override) {
		E.value->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}
	for (KeyValue<StringName, Ref<StyleBox>> &E : theme_style_override) {
		E.value->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}
	for (KeyValue<StringName, Ref<Font>> &E : theme_font_override) {
		E.value->disconnect_changed(callable_mp(this, &Window::_notify_theme_override_changed));
	}

	// Then override maps can be simply cleared.
	theme_icon_override.clear();
	theme_style_override.clear();
	theme_font_override.clear();
	theme_font_size_override.clear();
	theme_color_override.clear();
	theme_constant_override.clear();
}
