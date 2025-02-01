/**************************************************************************/
/*  editor_theme_manager.h                                                */
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

#include "editor/themes/editor_theme.h"
#include "scene/resources/style_box_flat.h"

class EditorThemeManager {
	static int benchmark_run;
	static inline bool outdated_cache = false;
	static inline bool outdated_cache_dirty = true;

	static String get_benchmark_key();

	enum ColorMode {
		AUTO_COLOR,
		DARK_COLOR,
		LIGHT_COLOR,
	};

	struct ThemeConfiguration {
		// Basic properties.

		String preset;
		String spacing_preset;

		Color base_color;
		Color accent_color;
		float contrast = 1.0;
		float icon_saturation = 1.0;

		// Extra properties.

		int base_spacing = 4;
		int extra_spacing = 0;
		Size2 dialogs_buttons_min_size = Size2(105, 34);
		int border_width = 0;
		int corner_radius = 3;

		bool draw_extra_borders = false;
		float relationship_line_opacity = 1.0;
		int thumb_size = 16;
		int class_icon_size = 16;
		bool increase_scrollbar_touch_area = false;
		float gizmo_handle_scale = 1.0;
		int color_picker_button_height = 28;
		float subresource_hue_tint = 0.0;

		float default_contrast = 1.0;

		// Generated properties.

		bool dark_theme = false;

		int base_margin = 4;
		int increased_margin = 4;
		int separation_margin = 4;
		int popup_margin = 12;
		int window_border_margin = 8;
		int top_bar_separation = 8;
		int forced_even_separation = 0;

		Color mono_color;
		Color dark_color_1;
		Color dark_color_2;
		Color dark_color_3;
		Color contrast_color_1;
		Color contrast_color_2;
		Color highlight_color;
		Color highlight_disabled_color;
		Color success_color;
		Color warning_color;
		Color error_color;
		Color extra_border_color_1;
		Color extra_border_color_2;

		Color font_color;
		Color font_focus_color;
		Color font_hover_color;
		Color font_pressed_color;
		Color font_hover_pressed_color;
		Color font_disabled_color;
		Color font_readonly_color;
		Color font_placeholder_color;
		Color font_outline_color;

		Color icon_normal_color;
		Color icon_focus_color;
		Color icon_hover_color;
		Color icon_pressed_color;
		Color icon_disabled_color;

		Color shadow_color;
		Color selection_color;
		Color disabled_border_color;
		Color disabled_bg_color;
		Color separator_color;

		Ref<StyleBoxFlat> base_style;
		Ref<StyleBoxEmpty> base_empty_style;

		Ref<StyleBoxFlat> button_style;
		Ref<StyleBoxFlat> button_style_disabled;
		Ref<StyleBoxFlat> button_style_focus;
		Ref<StyleBoxFlat> button_style_pressed;
		Ref<StyleBoxFlat> button_style_hover;

		Ref<StyleBoxFlat> popup_style;
		Ref<StyleBoxFlat> popup_border_style;
		Ref<StyleBoxFlat> window_style;
		Ref<StyleBoxFlat> dialog_style;
		Ref<StyleBoxFlat> panel_container_style;
		Ref<StyleBoxFlat> content_panel_style;
		Ref<StyleBoxFlat> tree_panel_style;

		Vector2 widget_margin;

		uint32_t hash();
		uint32_t hash_fonts();
		uint32_t hash_icons();
	};

	static Ref<EditorTheme> _create_base_theme(const Ref<EditorTheme> &p_old_theme = nullptr);
	static ThemeConfiguration _create_theme_config(const Ref<EditorTheme> &p_theme);

	static void _create_shared_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);
	static void _populate_standard_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);
	static void _populate_editor_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);

	static void _generate_text_editor_defaults(ThemeConfiguration &p_config);
	static void _populate_text_editor_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);
	static void _populate_visual_shader_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);

	static void _reset_dirty_flag();

public:
	static Ref<EditorTheme> generate_theme(const Ref<EditorTheme> &p_old_theme = nullptr);
	static bool is_generated_theme_outdated();

	static bool is_dark_theme();

	static void initialize();
	static void finalize();
};
