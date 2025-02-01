/**************************************************************************/
/*  window.h                                                              */
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

#pragma once

#include "scene/main/viewport.h"
#include "scene/resources/theme.h"
#include "servers/display_server.h"

class Font;
class Shortcut;
class StyleBox;
class ThemeOwner;
class ThemeContext;

class Window : public Viewport {
	GDCLASS(Window, Viewport);

public:
	// Keep synced with enum hint for `mode` property.
	enum Mode {
		MODE_WINDOWED = DisplayServer::WINDOW_MODE_WINDOWED,
		MODE_MINIMIZED = DisplayServer::WINDOW_MODE_MINIMIZED,
		MODE_MAXIMIZED = DisplayServer::WINDOW_MODE_MAXIMIZED,
		MODE_FULLSCREEN = DisplayServer::WINDOW_MODE_FULLSCREEN,
		MODE_EXCLUSIVE_FULLSCREEN = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN,
	};

	enum Flags {
		FLAG_RESIZE_DISABLED = DisplayServer::WINDOW_FLAG_RESIZE_DISABLED,
		FLAG_BORDERLESS = DisplayServer::WINDOW_FLAG_BORDERLESS,
		FLAG_ALWAYS_ON_TOP = DisplayServer::WINDOW_FLAG_ALWAYS_ON_TOP,
		FLAG_TRANSPARENT = DisplayServer::WINDOW_FLAG_TRANSPARENT,
		FLAG_NO_FOCUS = DisplayServer::WINDOW_FLAG_NO_FOCUS,
		FLAG_POPUP = DisplayServer::WINDOW_FLAG_POPUP,
		FLAG_EXTEND_TO_TITLE = DisplayServer::WINDOW_FLAG_EXTEND_TO_TITLE,
		FLAG_MOUSE_PASSTHROUGH = DisplayServer::WINDOW_FLAG_MOUSE_PASSTHROUGH,
		FLAG_SHARP_CORNERS = DisplayServer::WINDOW_FLAG_SHARP_CORNERS,
		FLAG_EXCLUDE_FROM_CAPTURE = DisplayServer::WINDOW_FLAG_EXCLUDE_FROM_CAPTURE,
		FLAG_MAX = DisplayServer::WINDOW_FLAG_MAX,
	};

	enum ContentScaleMode {
		CONTENT_SCALE_MODE_DISABLED,
		CONTENT_SCALE_MODE_CANVAS_ITEMS,
		CONTENT_SCALE_MODE_VIEWPORT,
	};

	enum ContentScaleAspect {
		CONTENT_SCALE_ASPECT_IGNORE,
		CONTENT_SCALE_ASPECT_KEEP,
		CONTENT_SCALE_ASPECT_KEEP_WIDTH,
		CONTENT_SCALE_ASPECT_KEEP_HEIGHT,
		CONTENT_SCALE_ASPECT_EXPAND,
	};

	enum ContentScaleStretch {
		CONTENT_SCALE_STRETCH_FRACTIONAL,
		CONTENT_SCALE_STRETCH_INTEGER,
	};

	enum LayoutDirection {
		LAYOUT_DIRECTION_INHERITED,
		LAYOUT_DIRECTION_APPLICATION_LOCALE,
		LAYOUT_DIRECTION_LTR,
		LAYOUT_DIRECTION_RTL,
		LAYOUT_DIRECTION_SYSTEM_LOCALE,
		LAYOUT_DIRECTION_MAX,
#ifndef DISABLE_DEPRECATED
		LAYOUT_DIRECTION_LOCALE = LAYOUT_DIRECTION_APPLICATION_LOCALE,
#endif // DISABLE_DEPRECATED
	};

	enum {
		DEFAULT_WINDOW_SIZE = 100,
	};

	// Keep synced with enum hint for `initial_position` property.
	enum WindowInitialPosition {
		WINDOW_INITIAL_POSITION_ABSOLUTE,
		WINDOW_INITIAL_POSITION_CENTER_PRIMARY_SCREEN,
		WINDOW_INITIAL_POSITION_CENTER_MAIN_WINDOW_SCREEN,
		WINDOW_INITIAL_POSITION_CENTER_OTHER_SCREEN,
		WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_MOUSE_FOCUS,
		WINDOW_INITIAL_POSITION_CENTER_SCREEN_WITH_KEYBOARD_FOCUS,
	};

private:
	DisplayServer::WindowID window_id = DisplayServer::INVALID_WINDOW_ID;
	bool initialized = false;

	String title;
	String tr_title;
	mutable int current_screen = 0;
	mutable Point2i position;
	mutable Size2i size = Size2i(DEFAULT_WINDOW_SIZE, DEFAULT_WINDOW_SIZE);
	mutable Size2i min_size;
	mutable Size2i max_size;
	mutable Vector<Vector2> mpath;
	mutable Mode mode = MODE_WINDOWED;
	mutable bool flags[FLAG_MAX] = {};
	bool visible = true;
	bool focused = false;
	WindowInitialPosition initial_position = WINDOW_INITIAL_POSITION_ABSOLUTE;
	bool force_native = false;

	bool use_font_oversampling = false;
	bool transient = false;
	bool transient_to_focused = false;
	bool exclusive = false;
	bool wrap_controls = false;
	bool updating_child_controls = false;
	bool updating_embedded_window = false;
	bool clamp_to_embedder = false;
	bool unparent_when_invisible = false;
	bool keep_title_visible = false;

	LayoutDirection layout_dir = LAYOUT_DIRECTION_INHERITED;

	void _update_child_controls();
	void _update_embedded_window();

	Size2i content_scale_size;
	ContentScaleMode content_scale_mode = CONTENT_SCALE_MODE_DISABLED;
	ContentScaleAspect content_scale_aspect = CONTENT_SCALE_ASPECT_IGNORE;
	ContentScaleStretch content_scale_stretch = CONTENT_SCALE_STRETCH_FRACTIONAL;
	real_t content_scale_factor = 1.0;

	void _make_window();
	void _clear_window();
	void _update_from_window();

	bool _try_parent_dialog(Node *p_from_node);

	Size2i max_size_used;

	Size2i _clamp_limit_size(const Size2i &p_limit_size);
	Size2i _clamp_window_size(const Size2i &p_size);
	void _validate_limit_size();
	void _update_viewport_size();
	void _update_window_size();

	void _propagate_window_notification(Node *p_node, int p_notification);

	void _update_window_callbacks();

	Window *transient_parent = nullptr;
	Window *exclusive_child = nullptr;
	HashSet<Window *> transient_children;

	void _clear_transient();
	void _make_transient();
	void _set_transient_exclusive_child(bool p_clear_invalid = false);

	ThemeOwner *theme_owner = nullptr;
	Ref<Theme> theme;
	StringName theme_type_variation;

	bool bulk_theme_override = false;
	Theme::ThemeIconMap theme_icon_override;
	Theme::ThemeStyleMap theme_style_override;
	Theme::ThemeFontMap theme_font_override;
	Theme::ThemeFontSizeMap theme_font_size_override;
	Theme::ThemeColorMap theme_color_override;
	Theme::ThemeConstantMap theme_constant_override;

	mutable HashMap<StringName, Theme::ThemeIconMap> theme_icon_cache;
	mutable HashMap<StringName, Theme::ThemeStyleMap> theme_style_cache;
	mutable HashMap<StringName, Theme::ThemeFontMap> theme_font_cache;
	mutable HashMap<StringName, Theme::ThemeFontSizeMap> theme_font_size_cache;
	mutable HashMap<StringName, Theme::ThemeColorMap> theme_color_cache;
	mutable HashMap<StringName, Theme::ThemeConstantMap> theme_constant_cache;

	void _theme_changed();
	void _notify_theme_override_changed();
	void _invalidate_theme_cache();

	struct ThemeCache {
		Ref<StyleBox> embedded_border;
		Ref<StyleBox> embedded_unfocused_border;

		Ref<Font> title_font;
		int title_font_size = 0;
		Color title_color;
		int title_height = 0;
		Color title_outline_modulate;
		int title_outline_size = 0;

		Ref<Texture2D> close;
		Ref<Texture2D> close_pressed;
		int close_h_offset = 0;
		int close_v_offset = 0;

		int resize_margin = 0;
	} theme_cache;

	void _settings_changed();

	Viewport *embedder = nullptr;

	Transform2D window_transform;

	friend class Viewport; //friend back, can call the methods below

	void _window_input(const Ref<InputEvent> &p_ev);
	void _window_input_text(const String &p_text);
	void _window_drop_files(const Vector<String> &p_files);
	void _rect_changed_callback(const Rect2i &p_callback);
	void _event_callback(DisplayServer::WindowEvent p_event);
	virtual bool _can_consume_input_events() const override;

	bool mouse_in_window = false;
	void _update_mouse_over(Vector2 p_pos) override;
	void _mouse_leave_viewport() override;

	Ref<Shortcut> debugger_stop_shortcut;

	static int root_layout_direction;

protected:
	virtual Rect2i _popup_adjust_rect() const { return Rect2i(); }
	virtual void _post_popup() {}

	virtual void _update_theme_item_cache();
	virtual void _input_from_window(const Ref<InputEvent> &p_event) {}

	void _notification(int p_what);
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;

	virtual void add_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;

	GDVIRTUAL0RC(Vector2, _get_contents_minimum_size)

public:
	enum {
		NOTIFICATION_VISIBILITY_CHANGED = 30,
		NOTIFICATION_POST_POPUP = 31,
		NOTIFICATION_THEME_CHANGED = 32
	};

	static void set_root_layout_direction(int p_root_dir);
	static Window *get_from_id(DisplayServer::WindowID p_window_id);

	void set_title(const String &p_title);
	String get_title() const;
	String get_translated_title() const;

	void set_initial_position(WindowInitialPosition p_initial_position);
	WindowInitialPosition get_initial_position() const;

	void set_force_native(bool p_force_native);
	bool get_force_native() const;

	void set_current_screen(int p_screen);
	int get_current_screen() const;

	void set_position(const Point2i &p_position);
	Point2i get_position() const;
	void move_to_center();

	void set_size(const Size2i &p_size);
	Size2i get_size() const;
	void reset_size();

	Point2i get_position_with_decorations() const;
	Size2i get_size_with_decorations() const;

	void set_max_size(const Size2i &p_max_size);
	Size2i get_max_size() const;

	void set_min_size(const Size2i &p_min_size);
	Size2i get_min_size() const;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	void set_flag(Flags p_flag, bool p_enabled);
	bool get_flag(Flags p_flag) const;

	bool is_maximize_allowed() const;

	void request_attention();
#ifndef DISABLE_DEPRECATED
	void move_to_foreground();
#endif // DISABLE_DEPRECATED

	virtual void set_visible(bool p_visible);
	bool is_visible() const;

	void update_mouse_cursor_state() override;

	void show();
	void hide();

	void set_transient(bool p_transient);
	bool is_transient() const;

	void set_transient_to_focused(bool p_transient_to_focused);
	bool is_transient_to_focused() const;

	void set_exclusive(bool p_exclusive);
	bool is_exclusive() const;

	void set_clamp_to_embedder(bool p_enable);
	bool is_clamped_to_embedder() const;

	void set_unparent_when_invisible(bool p_unparent);

	bool is_in_edited_scene_root() const;

	bool can_draw() const;

	void set_ime_active(bool p_active);
	void set_ime_position(const Point2i &p_pos);

	bool is_embedded() const;
	Viewport *get_embedder() const;

	void set_content_scale_size(const Size2i &p_size);
	Size2i get_content_scale_size() const;

	void set_content_scale_mode(ContentScaleMode p_mode);
	ContentScaleMode get_content_scale_mode() const;

	void set_content_scale_aspect(ContentScaleAspect p_aspect);
	ContentScaleAspect get_content_scale_aspect() const;

	void set_content_scale_stretch(ContentScaleStretch p_stretch);
	ContentScaleStretch get_content_scale_stretch() const;

	void set_keep_title_visible(bool p_title_visible);
	bool get_keep_title_visible() const;

	void set_content_scale_factor(real_t p_factor);
	real_t get_content_scale_factor() const;

	void set_use_font_oversampling(bool p_oversampling);
	bool is_using_font_oversampling() const;

	void set_mouse_passthrough_polygon(const Vector<Vector2> &p_region);
	Vector<Vector2> get_mouse_passthrough_polygon() const;

	void set_wrap_controls(bool p_enable);
	bool is_wrapping_controls() const;
	void child_controls_changed();

	Window *get_exclusive_child() const { return exclusive_child; }
	Window *get_parent_visible_window() const;
	Viewport *get_parent_viewport() const;

	virtual void popup(const Rect2i &p_screen_rect = Rect2i());
	void popup_on_parent(const Rect2i &p_parent_rect);
	void popup_centered(const Size2i &p_minsize = Size2i());
	void popup_centered_ratio(float p_ratio = 0.8);
	void popup_centered_clamped(const Size2i &p_size = Size2i(), float p_fallback_ratio = 0.75);

	void popup_exclusive(Node *p_from_node, const Rect2i &p_screen_rect = Rect2i());
	void popup_exclusive_on_parent(Node *p_from_node, const Rect2i &p_parent_rect);
	void popup_exclusive_centered(Node *p_from_node, const Size2i &p_minsize = Size2i());
	void popup_exclusive_centered_ratio(Node *p_from_node, float p_ratio = 0.8);
	void popup_exclusive_centered_clamped(Node *p_from_node, const Size2i &p_size = Size2i(), float p_fallback_ratio = 0.75);

	Rect2i fit_rect_in_parent(Rect2i p_rect, const Rect2i &p_parent_rect) const;
	Size2 get_contents_minimum_size() const;
	Size2 get_clamped_minimum_size() const;

	void grab_focus();
	bool has_focus() const;

	void start_drag();
	void start_resize(DisplayServer::WindowResizeEdge p_edge);

	Rect2i get_usable_parent_rect() const;

	// Internationalization.

	void set_layout_direction(LayoutDirection p_direction);
	LayoutDirection get_layout_direction() const;
	bool is_layout_rtl() const;

#ifndef DISABLE_DEPRECATED
	void set_auto_translate(bool p_enable);
	bool is_auto_translating() const;
#endif

	// Theming.

	void set_theme_owner_node(Node *p_node);
	Node *get_theme_owner_node() const;
	bool has_theme_owner_node() const;

	void set_theme_context(ThemeContext *p_context, bool p_propagate = true);

	void set_theme(const Ref<Theme> &p_theme);
	Ref<Theme> get_theme() const;

	void set_theme_type_variation(const StringName &p_theme_type);
	StringName get_theme_type_variation() const;

	void begin_bulk_theme_override();
	void end_bulk_theme_override();

	void add_theme_icon_override(const StringName &p_name, const Ref<Texture2D> &p_icon);
	void add_theme_style_override(const StringName &p_name, const Ref<StyleBox> &p_style);
	void add_theme_font_override(const StringName &p_name, const Ref<Font> &p_font);
	void add_theme_font_size_override(const StringName &p_name, int p_font_size);
	void add_theme_color_override(const StringName &p_name, const Color &p_color);
	void add_theme_constant_override(const StringName &p_name, int p_constant);

	void remove_theme_icon_override(const StringName &p_name);
	void remove_theme_style_override(const StringName &p_name);
	void remove_theme_font_override(const StringName &p_name);
	void remove_theme_font_size_override(const StringName &p_name);
	void remove_theme_color_override(const StringName &p_name);
	void remove_theme_constant_override(const StringName &p_name);

	Ref<Texture2D> get_theme_icon(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	Ref<StyleBox> get_theme_stylebox(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	Ref<Font> get_theme_font(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	int get_theme_font_size(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	Color get_theme_color(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	int get_theme_constant(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	Variant get_theme_item(Theme::DataType p_data_type, const StringName &p_name, const StringName &p_theme_type = StringName()) const;
#ifdef TOOLS_ENABLED
	Ref<Texture2D> get_editor_theme_icon(const StringName &p_name) const;
#endif

	bool has_theme_icon_override(const StringName &p_name) const;
	bool has_theme_stylebox_override(const StringName &p_name) const;
	bool has_theme_font_override(const StringName &p_name) const;
	bool has_theme_font_size_override(const StringName &p_name) const;
	bool has_theme_color_override(const StringName &p_name) const;
	bool has_theme_constant_override(const StringName &p_name) const;

	bool has_theme_icon(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	bool has_theme_stylebox(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	bool has_theme_font(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	bool has_theme_font_size(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	bool has_theme_color(const StringName &p_name, const StringName &p_theme_type = StringName()) const;
	bool has_theme_constant(const StringName &p_name, const StringName &p_theme_type = StringName()) const;

	float get_theme_default_base_scale() const;
	Ref<Font> get_theme_default_font() const;
	int get_theme_default_font_size() const;

	//

	virtual Transform2D get_final_transform() const override;
	virtual Transform2D get_screen_transform_internal(bool p_absolute_position = false) const override;
	virtual Transform2D get_popup_base_transform() const override;
	virtual Viewport *get_section_root_viewport() const override;
	virtual bool is_attached_in_viewport() const override;

	Rect2i get_parent_rect() const;
	virtual DisplayServer::WindowID get_window_id() const override;

	virtual Size2 _get_contents_minimum_size() const;

	Window();
	~Window();
};

VARIANT_ENUM_CAST(Window::Mode);
VARIANT_ENUM_CAST(Window::Flags);
VARIANT_ENUM_CAST(Window::ContentScaleMode);
VARIANT_ENUM_CAST(Window::ContentScaleAspect);
VARIANT_ENUM_CAST(Window::ContentScaleStretch);
VARIANT_ENUM_CAST(Window::LayoutDirection);
VARIANT_ENUM_CAST(Window::WindowInitialPosition);
