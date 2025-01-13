/**************************************************************************/
/*  editor_theme_manager.cpp                                              */
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

#include "editor_theme_manager.h"

#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_color_map.h"
#include "editor/themes/editor_fonts.h"
#include "editor/themes/editor_icons.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme.h"
#include "scene/gui/graph_edit.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_line.h"
#include "scene/resources/style_box_texture.h"
#include "scene/resources/texture.h"

// Theme configuration.

uint32_t EditorThemeManager::ThemeConfiguration::hash() {
	uint32_t hash = hash_murmur3_one_float(EDSCALE);

	// Basic properties.

	hash = hash_murmur3_one_32(preset.hash(), hash);
	hash = hash_murmur3_one_32(spacing_preset.hash(), hash);

	hash = hash_murmur3_one_32(base_color.to_rgba32(), hash);
	hash = hash_murmur3_one_32(accent_color.to_rgba32(), hash);
	hash = hash_murmur3_one_float(contrast, hash);
	hash = hash_murmur3_one_float(icon_saturation, hash);

	// Extra properties.

	hash = hash_murmur3_one_32(base_spacing, hash);
	hash = hash_murmur3_one_32(extra_spacing, hash);
	hash = hash_murmur3_one_32(border_width, hash);
	hash = hash_murmur3_one_32(corner_radius, hash);

	hash = hash_murmur3_one_32((int)draw_extra_borders, hash);
	hash = hash_murmur3_one_float(relationship_line_opacity, hash);
	hash = hash_murmur3_one_32(thumb_size, hash);
	hash = hash_murmur3_one_32(class_icon_size, hash);
	hash = hash_murmur3_one_32((int)increase_scrollbar_touch_area, hash);
	hash = hash_murmur3_one_float(gizmo_handle_scale, hash);
	hash = hash_murmur3_one_32(color_picker_button_height, hash);
	hash = hash_murmur3_one_float(subresource_hue_tint, hash);

	hash = hash_murmur3_one_float(default_contrast, hash);

	// Generated properties.

	hash = hash_murmur3_one_32((int)dark_theme, hash);

	return hash;
}

uint32_t EditorThemeManager::ThemeConfiguration::hash_fonts() {
	uint32_t hash = hash_murmur3_one_float(EDSCALE);

	// TODO: Implement the hash based on what editor_register_fonts() uses.

	return hash;
}

uint32_t EditorThemeManager::ThemeConfiguration::hash_icons() {
	uint32_t hash = hash_murmur3_one_float(EDSCALE);

	hash = hash_murmur3_one_32(accent_color.to_rgba32(), hash);
	hash = hash_murmur3_one_float(icon_saturation, hash);

	hash = hash_murmur3_one_32(thumb_size, hash);
	hash = hash_murmur3_one_float(gizmo_handle_scale, hash);

	hash = hash_murmur3_one_32((int)dark_theme, hash);

	return hash;
}

// Benchmarks.

int EditorThemeManager::benchmark_run = 0;

String EditorThemeManager::get_benchmark_key() {
	if (benchmark_run == 0) {
		return "EditorTheme (Startup)";
	}

	return vformat("EditorTheme (Run %d)", benchmark_run);
}

// Generation helper methods.

Ref<StyleBoxTexture> make_stylebox(Ref<Texture2D> p_texture, float p_left, float p_top, float p_right, float p_bottom, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1, bool p_draw_center = true) {
	Ref<StyleBoxTexture> style(memnew(StyleBoxTexture));
	style->set_texture(p_texture);
	style->set_texture_margin_individual(p_left * EDSCALE, p_top * EDSCALE, p_right * EDSCALE, p_bottom * EDSCALE);
	style->set_content_margin_individual((p_left + p_margin_left) * EDSCALE, (p_top + p_margin_top) * EDSCALE, (p_right + p_margin_right) * EDSCALE, (p_bottom + p_margin_bottom) * EDSCALE);
	style->set_draw_center(p_draw_center);
	return style;
}

Ref<StyleBoxEmpty> make_empty_stylebox(float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1) {
	Ref<StyleBoxEmpty> style(memnew(StyleBoxEmpty));
	style->set_content_margin_individual(p_margin_left * EDSCALE, p_margin_top * EDSCALE, p_margin_right * EDSCALE, p_margin_bottom * EDSCALE);
	return style;
}

Ref<StyleBoxFlat> make_flat_stylebox(Color p_color, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1, int p_corner_width = 0) {
	Ref<StyleBoxFlat> style(memnew(StyleBoxFlat));
	style->set_bg_color(p_color);
	// Adjust level of detail based on the corners' effective sizes.
	style->set_corner_detail(Math::ceil(0.8 * p_corner_width * EDSCALE));
	style->set_corner_radius_all(p_corner_width * EDSCALE);
	style->set_content_margin_individual(p_margin_left * EDSCALE, p_margin_top * EDSCALE, p_margin_right * EDSCALE, p_margin_bottom * EDSCALE);
	// Work around issue about antialiased edges being blurrier (GH-35279).
	style->set_anti_aliased(false);
	return style;
}

Ref<StyleBoxLine> make_line_stylebox(Color p_color, int p_thickness = 1, float p_grow_begin = 1, float p_grow_end = 1, bool p_vertical = false) {
	Ref<StyleBoxLine> style(memnew(StyleBoxLine));
	style->set_color(p_color);
	style->set_grow_begin(p_grow_begin);
	style->set_grow_end(p_grow_end);
	style->set_thickness(p_thickness);
	style->set_vertical(p_vertical);
	return style;
}

// Theme generation and population routines.

Ref<EditorTheme> EditorThemeManager::_create_base_theme(const Ref<EditorTheme> &p_old_theme) {
	OS::get_singleton()->benchmark_begin_measure(get_benchmark_key(), "Create Base Theme");

	Ref<EditorTheme> theme = memnew(EditorTheme);
	ThemeConfiguration config = _create_theme_config(theme);
	theme->set_generated_hash(config.hash());
	theme->set_generated_fonts_hash(config.hash_fonts());
	theme->set_generated_icons_hash(config.hash_icons());

	print_verbose(vformat("EditorTheme: Generating new theme for the config '%d'.", theme->get_generated_hash()));

	_create_shared_styles(theme, config);

	// Register icons.
	{
		OS::get_singleton()->benchmark_begin_measure(get_benchmark_key(), "Register Icons");

		// External functions, see editor_icons.cpp.
		editor_configure_icons(config.dark_theme);

		// If settings are comparable to the old theme, then just copy existing icons over.
		// Otherwise, regenerate them.
		bool keep_old_icons = (p_old_theme.is_valid() && theme->get_generated_icons_hash() == p_old_theme->get_generated_icons_hash());
		if (keep_old_icons) {
			print_verbose("EditorTheme: Can keep old icons, copying.");
			editor_copy_icons(theme, p_old_theme);
		} else {
			print_verbose("EditorTheme: Generating new icons.");
			editor_register_icons(theme, config.dark_theme, config.icon_saturation, config.thumb_size, config.gizmo_handle_scale);
		}

		OS::get_singleton()->benchmark_end_measure(get_benchmark_key(), "Register Icons");
	}

	// Register fonts.
	{
		OS::get_singleton()->benchmark_begin_measure(get_benchmark_key(), "Register Fonts");

		// TODO: Check if existing font definitions from the old theme are usable and copy them.

		// External function, see editor_fonts.cpp.
		print_verbose("EditorTheme: Generating new fonts.");
		editor_register_fonts(theme);

		OS::get_singleton()->benchmark_end_measure(get_benchmark_key(), "Register Fonts");
	}

	// TODO: Check if existing style definitions from the old theme are usable and copy them.

	print_verbose("EditorTheme: Generating new styles.");
	_populate_standard_styles(theme, config);
	_populate_editor_styles(theme, config);
	_populate_text_editor_styles(theme, config);
	_populate_visual_shader_styles(theme, config);

	OS::get_singleton()->benchmark_end_measure(get_benchmark_key(), "Create Base Theme");
	return theme;
}

EditorThemeManager::ThemeConfiguration EditorThemeManager::_create_theme_config(const Ref<EditorTheme> &p_theme) {
	ThemeConfiguration config;

	// Basic properties.

	config.preset = EDITOR_GET("interface/theme/preset");
	config.spacing_preset = EDITOR_GET("interface/theme/spacing_preset");

	config.base_color = EDITOR_GET("interface/theme/base_color");
	config.accent_color = EDITOR_GET("interface/theme/accent_color");
	config.contrast = EDITOR_GET("interface/theme/contrast");
	config.icon_saturation = EDITOR_GET("interface/theme/icon_saturation");

	// Extra properties.

	config.base_spacing = EDITOR_GET("interface/theme/base_spacing");
	config.extra_spacing = EDITOR_GET("interface/theme/additional_spacing");
	// Ensure borders are visible when using an editor scale below 100%.
	config.border_width = CLAMP((int)EDITOR_GET("interface/theme/border_size"), 0, 2) * MAX(1, EDSCALE);
	config.corner_radius = CLAMP((int)EDITOR_GET("interface/theme/corner_radius"), 0, 6);

	config.draw_extra_borders = EDITOR_GET("interface/theme/draw_extra_borders");
	config.relationship_line_opacity = EDITOR_GET("interface/theme/relationship_line_opacity");
	config.thumb_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
	config.class_icon_size = 16 * EDSCALE;
	config.increase_scrollbar_touch_area = EDITOR_GET("interface/touchscreen/increase_scrollbar_touch_area");
	config.gizmo_handle_scale = EDITOR_GET("interface/touchscreen/scale_gizmo_handles");
	config.color_picker_button_height = 28 * EDSCALE;
	config.subresource_hue_tint = EDITOR_GET("docks/property_editor/subresource_hue_tint");

	config.default_contrast = 0.3; // Make sure to keep this in sync with the editor settings definition.

	// Handle main theme preset.
	{
		const bool follow_system_theme = EDITOR_GET("interface/theme/follow_system_theme");
		const bool use_system_accent_color = EDITOR_GET("interface/theme/use_system_accent_color");
		DisplayServer *display_server = DisplayServer::get_singleton();
		Color system_base_color = display_server->get_base_color();
		Color system_accent_color = display_server->get_accent_color();

		if (follow_system_theme) {
			String dark_theme = "Default";
			String light_theme = "Light";

			config.preset = light_theme; // Assume light theme if we can't detect system theme attributes.

			if (system_base_color == Color(0, 0, 0, 0)) {
				if (display_server->is_dark_mode_supported() && display_server->is_dark_mode()) {
					config.preset = dark_theme;
				}
			} else {
				if (system_base_color.get_luminance() < 0.5) {
					config.preset = dark_theme;
				}
			}
		}

		if (config.preset != "Custom") {
			Color preset_accent_color;
			Color preset_base_color;
			float preset_contrast = 0;
			bool preset_draw_extra_borders = false;

			// Please use alphabetical order if you're adding a new theme here.
			if (config.preset == "Breeze Dark") {
				preset_accent_color = Color(0.26, 0.76, 1.00);
				preset_base_color = Color(0.24, 0.26, 0.28);
				preset_contrast = config.default_contrast;
			} else if (config.preset == "Godot 2") {
				preset_accent_color = Color(0.53, 0.67, 0.89);
				preset_base_color = Color(0.24, 0.23, 0.27);
				preset_contrast = config.default_contrast;
			} else if (config.preset == "Gray") {
				preset_accent_color = Color(0.44, 0.73, 0.98);
				preset_base_color = Color(0.24, 0.24, 0.24);
				preset_contrast = config.default_contrast;
			} else if (config.preset == "Light") {
				preset_accent_color = Color(0.18, 0.50, 1.00);
				preset_base_color = Color(0.9, 0.9, 0.9);
				// A negative contrast rate looks better for light themes, since it better follows the natural order of UI "elevation".
				preset_contrast = -0.06;
			} else if (config.preset == "Solarized (Dark)") {
				preset_accent_color = Color(0.15, 0.55, 0.82);
				preset_base_color = Color(0.04, 0.23, 0.27);
				preset_contrast = config.default_contrast;
			} else if (config.preset == "Solarized (Light)") {
				preset_accent_color = Color(0.15, 0.55, 0.82);
				preset_base_color = Color(0.89, 0.86, 0.79);
				// A negative contrast rate looks better for light themes, since it better follows the natural order of UI "elevation".
				preset_contrast = -0.06;
			} else if (config.preset == "Black (OLED)") {
				preset_accent_color = Color(0.45, 0.75, 1.0);
				preset_base_color = Color(0, 0, 0);
				// The contrast rate value is irrelevant on a fully black theme.
				preset_contrast = 0.0;
				preset_draw_extra_borders = true;
			} else { // Default
				preset_accent_color = Color(0.44, 0.73, 0.98);
				preset_base_color = Color(0.21, 0.24, 0.29);
				preset_contrast = config.default_contrast;
			}

			config.accent_color = preset_accent_color;
			config.base_color = preset_base_color;
			config.contrast = preset_contrast;
			config.draw_extra_borders = preset_draw_extra_borders;

			EditorSettings::get_singleton()->set_initial_value("interface/theme/accent_color", config.accent_color);
			EditorSettings::get_singleton()->set_initial_value("interface/theme/base_color", config.base_color);
			EditorSettings::get_singleton()->set_initial_value("interface/theme/contrast", config.contrast);
			EditorSettings::get_singleton()->set_initial_value("interface/theme/draw_extra_borders", config.draw_extra_borders);
		}

		if (follow_system_theme && system_base_color != Color(0, 0, 0, 0)) {
			config.base_color = system_base_color;
			config.preset = "Custom";
		}

		if (use_system_accent_color && system_accent_color != Color(0, 0, 0, 0)) {
			config.accent_color = system_accent_color;
			config.preset = "Custom";
		}

		// Enforce values in case they were adjusted or overridden.
		EditorSettings::get_singleton()->set_manually("interface/theme/preset", config.preset);
		EditorSettings::get_singleton()->set_manually("interface/theme/accent_color", config.accent_color);
		EditorSettings::get_singleton()->set_manually("interface/theme/base_color", config.base_color);
		EditorSettings::get_singleton()->set_manually("interface/theme/contrast", config.contrast);
		EditorSettings::get_singleton()->set_manually("interface/theme/draw_extra_borders", config.draw_extra_borders);
	}

	// Handle theme spacing preset.
	{
		if (config.spacing_preset != "Custom") {
			int preset_base_spacing = 0;
			int preset_extra_spacing = 0;
			Size2 preset_dialogs_buttons_min_size;

			if (config.spacing_preset == "Compact") {
				preset_base_spacing = 0;
				preset_extra_spacing = 4;
				preset_dialogs_buttons_min_size = Size2(90, 26);
			} else if (config.spacing_preset == "Spacious") {
				preset_base_spacing = 6;
				preset_extra_spacing = 2;
				preset_dialogs_buttons_min_size = Size2(112, 36);
			} else { // Default
				preset_base_spacing = 4;
				preset_extra_spacing = 0;
				preset_dialogs_buttons_min_size = Size2(105, 34);
			}

			config.base_spacing = preset_base_spacing;
			config.extra_spacing = preset_extra_spacing;
			config.dialogs_buttons_min_size = preset_dialogs_buttons_min_size;

			EditorSettings::get_singleton()->set_initial_value("interface/theme/base_spacing", config.base_spacing);
			EditorSettings::get_singleton()->set_initial_value("interface/theme/additional_spacing", config.extra_spacing);
		}

		// Enforce values in case they were adjusted or overridden.
		EditorSettings::get_singleton()->set_manually("interface/theme/spacing_preset", config.spacing_preset);
		EditorSettings::get_singleton()->set_manually("interface/theme/base_spacing", config.base_spacing);
		EditorSettings::get_singleton()->set_manually("interface/theme/additional_spacing", config.extra_spacing);
	}

	// Generated properties.

	config.dark_theme = is_dark_theme();

	config.base_margin = config.base_spacing;
	config.increased_margin = config.base_spacing + config.extra_spacing;
	config.separation_margin = (config.base_spacing + config.extra_spacing / 2) * EDSCALE;
	config.popup_margin = config.base_margin * 2.4 * EDSCALE;
	// Make sure content doesn't stick to window decorations; this can be fixed in future with layout changes.
	config.window_border_margin = MAX(1, config.base_margin * 2);
	config.top_bar_separation = config.base_margin * 2 * EDSCALE;

	// Force the v_separation to be even so that the spacing on top and bottom is even.
	// If the vsep is odd and cannot be split into 2 even groups (of pixels), then it will be lopsided.
	// We add 2 to the vsep to give it some extra spacing which looks a bit more modern (see Windows, for example).
	const int separation_base = config.increased_margin + 6;
	config.forced_even_separation = separation_base + (separation_base % 2);

	return config;
}

void EditorThemeManager::_create_shared_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config) {
	// Colors.
	{
		// Base colors.

		p_theme->set_color("base_color", EditorStringName(Editor), p_config.base_color);
		p_theme->set_color("accent_color", EditorStringName(Editor), p_config.accent_color);

		// White (dark theme) or black (light theme), will be used to generate the rest of the colors
		p_config.mono_color = p_config.dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);

		// Ensure base colors are in the 0..1 luminance range to avoid 8-bit integer overflow or text rendering issues.
		// Some places in the editor use 8-bit integer colors.
		p_config.dark_color_1 = p_config.base_color.lerp(Color(0, 0, 0, 1), p_config.contrast).clamp();
		p_config.dark_color_2 = p_config.base_color.lerp(Color(0, 0, 0, 1), p_config.contrast * 1.5).clamp();
		p_config.dark_color_3 = p_config.base_color.lerp(Color(0, 0, 0, 1), p_config.contrast * 2).clamp();

		p_config.contrast_color_1 = p_config.base_color.lerp(p_config.mono_color, MAX(p_config.contrast, p_config.default_contrast));
		p_config.contrast_color_2 = p_config.base_color.lerp(p_config.mono_color, MAX(p_config.contrast * 1.5, p_config.default_contrast * 1.5));

		p_config.highlight_color = Color(p_config.accent_color.r, p_config.accent_color.g, p_config.accent_color.b, 0.275);
		p_config.highlight_disabled_color = p_config.highlight_color.lerp(p_config.dark_theme ? Color(0, 0, 0) : Color(1, 1, 1), 0.5);

		p_config.success_color = Color(0.45, 0.95, 0.5);
		p_config.warning_color = Color(1, 0.87, 0.4);
		p_config.error_color = Color(1, 0.47, 0.42);
		if (!p_config.dark_theme) {
			// Darken some colors to be readable on a light background.
			p_config.success_color = p_config.success_color.lerp(p_config.mono_color, 0.35);
			p_config.warning_color = Color(0.82, 0.56, 0.1);
			p_config.error_color = Color(0.8, 0.22, 0.22);
		}

		p_theme->set_color("mono_color", EditorStringName(Editor), p_config.mono_color);
		p_theme->set_color("dark_color_1", EditorStringName(Editor), p_config.dark_color_1);
		p_theme->set_color("dark_color_2", EditorStringName(Editor), p_config.dark_color_2);
		p_theme->set_color("dark_color_3", EditorStringName(Editor), p_config.dark_color_3);
		p_theme->set_color("contrast_color_1", EditorStringName(Editor), p_config.contrast_color_1);
		p_theme->set_color("contrast_color_2", EditorStringName(Editor), p_config.contrast_color_2);
		p_theme->set_color("highlight_color", EditorStringName(Editor), p_config.highlight_color);
		p_theme->set_color("highlight_disabled_color", EditorStringName(Editor), p_config.highlight_disabled_color);
		p_theme->set_color("success_color", EditorStringName(Editor), p_config.success_color);
		p_theme->set_color("warning_color", EditorStringName(Editor), p_config.warning_color);
		p_theme->set_color("error_color", EditorStringName(Editor), p_config.error_color);
#ifndef DISABLE_DEPRECATED // Used before 4.3.
		p_theme->set_color("disabled_highlight_color", EditorStringName(Editor), p_config.highlight_disabled_color);
#endif

		// Only used when the Draw Extra Borders editor setting is enabled.
		p_config.extra_border_color_1 = Color(0.5, 0.5, 0.5);
		p_config.extra_border_color_2 = p_config.dark_theme ? Color(0.3, 0.3, 0.3) : Color(0.7, 0.7, 0.7);

		p_theme->set_color("extra_border_color_1", EditorStringName(Editor), p_config.extra_border_color_1);
		p_theme->set_color("extra_border_color_2", EditorStringName(Editor), p_config.extra_border_color_2);

		// Font colors.

		p_config.font_color = p_config.mono_color.lerp(p_config.base_color, 0.25);
		p_config.font_focus_color = p_config.mono_color.lerp(p_config.base_color, 0.125);
		p_config.font_hover_color = p_config.mono_color.lerp(p_config.base_color, 0.125);
		p_config.font_pressed_color = p_config.accent_color;
		p_config.font_hover_pressed_color = p_config.font_hover_color.lerp(p_config.accent_color, 0.74);
		p_config.font_disabled_color = Color(p_config.mono_color.r, p_config.mono_color.g, p_config.mono_color.b, 0.35);
		p_config.font_readonly_color = Color(p_config.mono_color.r, p_config.mono_color.g, p_config.mono_color.b, 0.65);
		p_config.font_placeholder_color = Color(p_config.mono_color.r, p_config.mono_color.g, p_config.mono_color.b, 0.6);
		p_config.font_outline_color = Color(0, 0, 0, 0);

		p_theme->set_color(SceneStringName(font_color), EditorStringName(Editor), p_config.font_color);
		p_theme->set_color("font_focus_color", EditorStringName(Editor), p_config.font_focus_color);
		p_theme->set_color("font_hover_color", EditorStringName(Editor), p_config.font_hover_color);
		p_theme->set_color("font_pressed_color", EditorStringName(Editor), p_config.font_pressed_color);
		p_theme->set_color("font_hover_pressed_color", EditorStringName(Editor), p_config.font_hover_pressed_color);
		p_theme->set_color("font_disabled_color", EditorStringName(Editor), p_config.font_disabled_color);
		p_theme->set_color("font_readonly_color", EditorStringName(Editor), p_config.font_readonly_color);
		p_theme->set_color("font_placeholder_color", EditorStringName(Editor), p_config.font_placeholder_color);
		p_theme->set_color("font_outline_color", EditorStringName(Editor), p_config.font_outline_color);
#ifndef DISABLE_DEPRECATED // Used before 4.3.
		p_theme->set_color("readonly_font_color", EditorStringName(Editor), p_config.font_readonly_color);
		p_theme->set_color("disabled_font_color", EditorStringName(Editor), p_config.font_disabled_color);
		p_theme->set_color("readonly_color", EditorStringName(Editor), p_config.font_readonly_color);
		p_theme->set_color("highlighted_font_color", EditorStringName(Editor), p_config.font_hover_color); // Closest equivalent.
#endif

		// Icon colors.

		p_config.icon_normal_color = Color(1, 1, 1);
		p_config.icon_focus_color = p_config.icon_normal_color * (p_config.dark_theme ? 1.15 : 1.45);
		p_config.icon_focus_color.a = 1.0;
		p_config.icon_hover_color = p_config.icon_focus_color;
		// Make the pressed icon color overbright because icons are not completely white on a dark theme.
		// On a light theme, icons are dark, so we need to modulate them with an even brighter color.
		p_config.icon_pressed_color = p_config.accent_color * (p_config.dark_theme ? 1.15 : 3.5);
		p_config.icon_pressed_color.a = 1.0;
		p_config.icon_disabled_color = Color(p_config.icon_normal_color, 0.4);

		p_theme->set_color("icon_normal_color", EditorStringName(Editor), p_config.icon_normal_color);
		p_theme->set_color("icon_focus_color", EditorStringName(Editor), p_config.icon_focus_color);
		p_theme->set_color("icon_hover_color", EditorStringName(Editor), p_config.icon_hover_color);
		p_theme->set_color("icon_pressed_color", EditorStringName(Editor), p_config.icon_pressed_color);
		p_theme->set_color("icon_disabled_color", EditorStringName(Editor), p_config.icon_disabled_color);

		// Additional GUI colors.

		p_config.shadow_color = Color(0, 0, 0, p_config.dark_theme ? 0.3 : 0.1);
		p_config.selection_color = p_config.accent_color * Color(1, 1, 1, 0.4);
		p_config.disabled_border_color = p_config.mono_color.inverted().lerp(p_config.base_color, 0.7);
		p_config.disabled_bg_color = p_config.mono_color.inverted().lerp(p_config.base_color, 0.9);
		p_config.separator_color = Color(p_config.mono_color.r, p_config.mono_color.g, p_config.mono_color.b, 0.1);

		p_theme->set_color("selection_color", EditorStringName(Editor), p_config.selection_color);
		p_theme->set_color("disabled_border_color", EditorStringName(Editor), p_config.disabled_border_color);
		p_theme->set_color("disabled_bg_color", EditorStringName(Editor), p_config.disabled_bg_color);
		p_theme->set_color("separator_color", EditorStringName(Editor), p_config.separator_color);

		// Additional editor colors.

		p_theme->set_color("box_selection_fill_color", EditorStringName(Editor), p_config.accent_color * Color(1, 1, 1, 0.3));
		p_theme->set_color("box_selection_stroke_color", EditorStringName(Editor), p_config.accent_color * Color(1, 1, 1, 0.8));

		p_theme->set_color("axis_x_color", EditorStringName(Editor), Color(0.96, 0.20, 0.32));
		p_theme->set_color("axis_y_color", EditorStringName(Editor), Color(0.53, 0.84, 0.01));
		p_theme->set_color("axis_z_color", EditorStringName(Editor), Color(0.16, 0.55, 0.96));
		p_theme->set_color("axis_w_color", EditorStringName(Editor), Color(0.55, 0.55, 0.55));

		const float prop_color_saturation = p_config.accent_color.get_s() * 0.75;
		const float prop_color_value = p_config.accent_color.get_v();

		p_theme->set_color("property_color_x", EditorStringName(Editor), Color().from_hsv(0.0 / 3.0 + 0.05, prop_color_saturation, prop_color_value));
		p_theme->set_color("property_color_y", EditorStringName(Editor), Color().from_hsv(1.0 / 3.0 + 0.05, prop_color_saturation, prop_color_value));
		p_theme->set_color("property_color_z", EditorStringName(Editor), Color().from_hsv(2.0 / 3.0 + 0.05, prop_color_saturation, prop_color_value));
		p_theme->set_color("property_color_w", EditorStringName(Editor), Color().from_hsv(1.5 / 3.0 + 0.05, prop_color_saturation, prop_color_value));

		// Special colors for rendering methods.

		p_theme->set_color("forward_plus_color", EditorStringName(Editor), Color::hex(0x5d8c3fff));
		p_theme->set_color("mobile_color", EditorStringName(Editor), Color::hex(0xa5557dff));
		p_theme->set_color("gl_compatibility_color", EditorStringName(Editor), Color::hex(0x5586a4ff));

		if (p_config.dark_theme) {
			p_theme->set_color("highend_color", EditorStringName(Editor), Color(1.0, 0.0, 0.0));
		} else {
			p_theme->set_color("highend_color", EditorStringName(Editor), Color::hex(0xad1128ff));
		}
	}

	// Constants.
	{
		// Can't save single float in theme, so using Color.
		p_theme->set_color("icon_saturation", EditorStringName(Editor), Color(p_config.icon_saturation, p_config.icon_saturation, p_config.icon_saturation));

		// Controls may rely on the scale for their internal drawing logic.
		p_theme->set_default_base_scale(EDSCALE);
		p_theme->set_constant("scale", EditorStringName(Editor), EDSCALE);

		p_theme->set_constant("thumb_size", EditorStringName(Editor), p_config.thumb_size);
		p_theme->set_constant("class_icon_size", EditorStringName(Editor), p_config.class_icon_size);
		p_theme->set_constant("color_picker_button_height", EditorStringName(Editor), p_config.color_picker_button_height);
		p_theme->set_constant("gizmo_handle_scale", EditorStringName(Editor), p_config.gizmo_handle_scale);

		p_theme->set_constant("base_margin", EditorStringName(Editor), p_config.base_margin);
		p_theme->set_constant("increased_margin", EditorStringName(Editor), p_config.increased_margin);
		p_theme->set_constant("window_border_margin", EditorStringName(Editor), p_config.window_border_margin);
		p_theme->set_constant("top_bar_separation", EditorStringName(Editor), p_config.top_bar_separation);

		p_theme->set_constant("dark_theme", EditorStringName(Editor), p_config.dark_theme);
	}

	// Styleboxes.
	{
		// This is the basic stylebox, used as a base for most other styleboxes (through `duplicate()`).
		p_config.base_style = make_flat_stylebox(p_config.base_color, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.corner_radius);
		p_config.base_style->set_border_width_all(p_config.border_width);
		p_config.base_style->set_border_color(p_config.base_color);

		p_config.base_empty_style = make_empty_stylebox(p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin);

		// Button styles.
		{
			p_config.widget_margin = Vector2(p_config.increased_margin + 2, p_config.increased_margin + 1) * EDSCALE;

			p_config.button_style = p_config.base_style->duplicate();
			p_config.button_style->set_content_margin_individual(p_config.widget_margin.x, p_config.widget_margin.y, p_config.widget_margin.x, p_config.widget_margin.y);
			p_config.button_style->set_bg_color(p_config.dark_color_1);
			if (p_config.draw_extra_borders) {
				p_config.button_style->set_border_width_all(Math::round(EDSCALE));
				p_config.button_style->set_border_color(p_config.extra_border_color_1);
			} else {
				p_config.button_style->set_border_color(p_config.dark_color_2);
			}

			p_config.button_style_disabled = p_config.button_style->duplicate();
			p_config.button_style_disabled->set_bg_color(p_config.disabled_bg_color);
			if (p_config.draw_extra_borders) {
				p_config.button_style_disabled->set_border_color(p_config.extra_border_color_2);
			} else {
				p_config.button_style_disabled->set_border_color(p_config.disabled_border_color);
			}

			p_config.button_style_focus = p_config.button_style->duplicate();
			p_config.button_style_focus->set_draw_center(false);
			p_config.button_style_focus->set_border_width_all(Math::round(2 * MAX(1, EDSCALE)));
			p_config.button_style_focus->set_border_color(p_config.accent_color);

			p_config.button_style_pressed = p_config.button_style->duplicate();
			p_config.button_style_pressed->set_bg_color(p_config.dark_color_1.darkened(0.125));

			p_config.button_style_hover = p_config.button_style->duplicate();
			p_config.button_style_hover->set_bg_color(p_config.mono_color * Color(1, 1, 1, 0.11));
			if (p_config.draw_extra_borders) {
				p_config.button_style_hover->set_border_color(p_config.extra_border_color_1);
			} else {
				p_config.button_style_hover->set_border_color(p_config.mono_color * Color(1, 1, 1, 0.05));
			}
		}

		// Windows and popups.
		{
			p_config.popup_style = p_config.base_style->duplicate();
			p_config.popup_style->set_content_margin_all(p_config.popup_margin);
			p_config.popup_style->set_border_color(p_config.contrast_color_1);
			p_config.popup_style->set_shadow_color(p_config.shadow_color);
			p_config.popup_style->set_shadow_size(4 * EDSCALE);
			// Popups are separate windows by default in the editor. Windows currently don't support per-pixel transparency
			// in 4.0, and even if it was, it may not always work in practice (e.g. running with compositing disabled).
			p_config.popup_style->set_corner_radius_all(0);

			p_config.popup_border_style = p_config.popup_style->duplicate();
			p_config.popup_border_style->set_content_margin_all(MAX(Math::round(EDSCALE), p_config.border_width) + 2 + (p_config.base_margin * 1.5) * EDSCALE);
			// Always display a border for popups like PopupMenus so they can be distinguished from their background.
			p_config.popup_border_style->set_border_width_all(MAX(Math::round(EDSCALE), p_config.border_width));
			if (p_config.draw_extra_borders) {
				p_config.popup_border_style->set_border_color(p_config.extra_border_color_2);
			} else {
				p_config.popup_border_style->set_border_color(p_config.dark_color_2);
			}

			p_config.window_style = p_config.popup_style->duplicate();
			p_config.window_style->set_border_color(p_config.base_color);
			p_config.window_style->set_border_width(SIDE_TOP, 24 * EDSCALE);
			p_config.window_style->set_expand_margin(SIDE_TOP, 24 * EDSCALE);

			// Prevent corner artifacts between window title and body.
			p_config.dialog_style = p_config.base_style->duplicate();
			p_config.dialog_style->set_corner_radius(CORNER_TOP_LEFT, 0);
			p_config.dialog_style->set_corner_radius(CORNER_TOP_RIGHT, 0);
			p_config.dialog_style->set_content_margin_all(p_config.popup_margin);
			// Prevent visible line between window title and body.
			p_config.dialog_style->set_expand_margin(SIDE_BOTTOM, 2 * EDSCALE);
		}

		// Panels.
		{
			p_config.panel_container_style = p_config.button_style->duplicate();
			p_config.panel_container_style->set_draw_center(false);
			p_config.panel_container_style->set_border_width_all(0);

			// Content panel for tabs and similar containers.

			// Compensate for the border.
			const int content_panel_margin = p_config.base_margin * EDSCALE + p_config.border_width;

			p_config.content_panel_style = p_config.base_style->duplicate();
			p_config.content_panel_style->set_border_color(p_config.dark_color_3);
			p_config.content_panel_style->set_border_width_all(p_config.border_width);
			p_config.content_panel_style->set_border_width(Side::SIDE_TOP, 0);
			p_config.content_panel_style->set_corner_radius(CORNER_TOP_LEFT, 0);
			p_config.content_panel_style->set_corner_radius(CORNER_TOP_RIGHT, 0);
			p_config.content_panel_style->set_content_margin_individual(content_panel_margin, 2 * EDSCALE + content_panel_margin, content_panel_margin, content_panel_margin);

			// Trees and similarly inset panels.

			p_config.tree_panel_style = p_config.base_style->duplicate();
			// Make Trees easier to distinguish from other controls by using a darker background color.
			p_config.tree_panel_style->set_bg_color(p_config.dark_color_1.lerp(p_config.dark_color_2, 0.5));
			if (p_config.draw_extra_borders) {
				p_config.tree_panel_style->set_border_width_all(Math::round(EDSCALE));
				p_config.tree_panel_style->set_border_color(p_config.extra_border_color_2);
			} else {
				p_config.tree_panel_style->set_border_color(p_config.dark_color_3);
			}
		}
	}
}

void EditorThemeManager::_populate_standard_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config) {
	// Panels.
	{
		// Panel.
		p_theme->set_stylebox(SceneStringName(panel), "Panel", make_flat_stylebox(p_config.dark_color_1, 6, 4, 6, 4, p_config.corner_radius));

		// PanelContainer.
		p_theme->set_stylebox(SceneStringName(panel), "PanelContainer", p_config.panel_container_style);

		// TooltipPanel & TooltipLabel.
		{
			// TooltipPanel is also used for custom tooltips, while TooltipLabel
			// is only relevant for default tooltips.

			p_theme->set_color(SceneStringName(font_color), "TooltipLabel", p_config.font_hover_color);
			p_theme->set_color("font_shadow_color", "TooltipLabel", Color(0, 0, 0, 0));

			Ref<StyleBoxFlat> style_tooltip = p_config.popup_style->duplicate();
			style_tooltip->set_shadow_size(0);
			style_tooltip->set_content_margin_all(p_config.base_margin * EDSCALE * 0.5);
			style_tooltip->set_bg_color(p_config.dark_color_3 * Color(0.8, 0.8, 0.8, 0.9));
			style_tooltip->set_border_width_all(0);
			p_theme->set_stylebox(SceneStringName(panel), "TooltipPanel", style_tooltip);
		}

		// PopupPanel
		p_theme->set_stylebox(SceneStringName(panel), "PopupPanel", p_config.popup_border_style);
	}

	// Buttons.
	{
		// Button.

		p_theme->set_stylebox(CoreStringName(normal), "Button", p_config.button_style);
		p_theme->set_stylebox(SceneStringName(hover), "Button", p_config.button_style_hover);
		p_theme->set_stylebox(SceneStringName(pressed), "Button", p_config.button_style_pressed);
		p_theme->set_stylebox("focus", "Button", p_config.button_style_focus);
		p_theme->set_stylebox("disabled", "Button", p_config.button_style_disabled);

		p_theme->set_color(SceneStringName(font_color), "Button", p_config.font_color);
		p_theme->set_color("font_hover_color", "Button", p_config.font_hover_color);
		p_theme->set_color("font_hover_pressed_color", "Button", p_config.font_hover_pressed_color);
		p_theme->set_color("font_focus_color", "Button", p_config.font_focus_color);
		p_theme->set_color("font_pressed_color", "Button", p_config.font_pressed_color);
		p_theme->set_color("font_disabled_color", "Button", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "Button", p_config.font_outline_color);

		p_theme->set_color("icon_normal_color", "Button", p_config.icon_normal_color);
		p_theme->set_color("icon_hover_color", "Button", p_config.icon_hover_color);
		p_theme->set_color("icon_focus_color", "Button", p_config.icon_focus_color);
		p_theme->set_color("icon_hover_pressed_color", "Button", p_config.icon_pressed_color);
		p_theme->set_color("icon_pressed_color", "Button", p_config.icon_pressed_color);
		p_theme->set_color("icon_disabled_color", "Button", p_config.icon_disabled_color);

		p_theme->set_constant("h_separation", "Button", 4 * EDSCALE);
		p_theme->set_constant("outline_size", "Button", 0);

		p_theme->set_constant("align_to_largest_stylebox", "Button", 1); // Enabled.

		// MenuButton.

		p_theme->set_stylebox(CoreStringName(normal), "MenuButton", p_config.panel_container_style);
		p_theme->set_stylebox(SceneStringName(hover), "MenuButton", p_config.button_style_hover);
		p_theme->set_stylebox(SceneStringName(pressed), "MenuButton", p_config.panel_container_style);
		p_theme->set_stylebox("focus", "MenuButton", p_config.panel_container_style);
		p_theme->set_stylebox("disabled", "MenuButton", p_config.panel_container_style);

		p_theme->set_color(SceneStringName(font_color), "MenuButton", p_config.font_color);
		p_theme->set_color("font_hover_color", "MenuButton", p_config.font_hover_color);
		p_theme->set_color("font_hover_pressed_color", "MenuButton", p_config.font_hover_pressed_color);
		p_theme->set_color("font_focus_color", "MenuButton", p_config.font_focus_color);
		p_theme->set_color("font_outline_color", "MenuButton", p_config.font_outline_color);

		p_theme->set_constant("outline_size", "MenuButton", 0);

		// MenuBar.

		p_theme->set_stylebox(CoreStringName(normal), "MenuBar", p_config.button_style);
		p_theme->set_stylebox(SceneStringName(hover), "MenuBar", p_config.button_style_hover);
		p_theme->set_stylebox(SceneStringName(pressed), "MenuBar", p_config.button_style_pressed);
		p_theme->set_stylebox("disabled", "MenuBar", p_config.button_style_disabled);

		p_theme->set_color(SceneStringName(font_color), "MenuBar", p_config.font_color);
		p_theme->set_color("font_hover_color", "MenuBar", p_config.font_hover_color);
		p_theme->set_color("font_hover_pressed_color", "MenuBar", p_config.font_hover_pressed_color);
		p_theme->set_color("font_focus_color", "MenuBar", p_config.font_focus_color);
		p_theme->set_color("font_pressed_color", "MenuBar", p_config.font_pressed_color);
		p_theme->set_color("font_disabled_color", "MenuBar", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "MenuBar", p_config.font_outline_color);

		p_theme->set_color("icon_normal_color", "MenuBar", p_config.icon_normal_color);
		p_theme->set_color("icon_hover_color", "MenuBar", p_config.icon_hover_color);
		p_theme->set_color("icon_focus_color", "MenuBar", p_config.icon_focus_color);
		p_theme->set_color("icon_pressed_color", "MenuBar", p_config.icon_pressed_color);
		p_theme->set_color("icon_disabled_color", "MenuBar", p_config.icon_disabled_color);

		p_theme->set_constant("h_separation", "MenuBar", 4 * EDSCALE);
		p_theme->set_constant("outline_size", "MenuBar", 0);

		// OptionButton.
		{
			Ref<StyleBoxFlat> option_button_focus_style = p_config.button_style_focus->duplicate();
			Ref<StyleBoxFlat> option_button_normal_style = p_config.button_style->duplicate();
			Ref<StyleBoxFlat> option_button_hover_style = p_config.button_style_hover->duplicate();
			Ref<StyleBoxFlat> option_button_pressed_style = p_config.button_style_pressed->duplicate();
			Ref<StyleBoxFlat> option_button_disabled_style = p_config.button_style_disabled->duplicate();

			option_button_focus_style->set_content_margin(SIDE_RIGHT, 4 * EDSCALE);
			option_button_normal_style->set_content_margin(SIDE_RIGHT, 4 * EDSCALE);
			option_button_hover_style->set_content_margin(SIDE_RIGHT, 4 * EDSCALE);
			option_button_pressed_style->set_content_margin(SIDE_RIGHT, 4 * EDSCALE);
			option_button_disabled_style->set_content_margin(SIDE_RIGHT, 4 * EDSCALE);

			p_theme->set_stylebox("focus", "OptionButton", option_button_focus_style);
			p_theme->set_stylebox(CoreStringName(normal), "OptionButton", p_config.button_style);
			p_theme->set_stylebox(SceneStringName(hover), "OptionButton", p_config.button_style_hover);
			p_theme->set_stylebox(SceneStringName(pressed), "OptionButton", p_config.button_style_pressed);
			p_theme->set_stylebox("disabled", "OptionButton", p_config.button_style_disabled);

			p_theme->set_stylebox("normal_mirrored", "OptionButton", option_button_normal_style);
			p_theme->set_stylebox("hover_mirrored", "OptionButton", option_button_hover_style);
			p_theme->set_stylebox("pressed_mirrored", "OptionButton", option_button_pressed_style);
			p_theme->set_stylebox("disabled_mirrored", "OptionButton", option_button_disabled_style);

			p_theme->set_color(SceneStringName(font_color), "OptionButton", p_config.font_color);
			p_theme->set_color("font_hover_color", "OptionButton", p_config.font_hover_color);
			p_theme->set_color("font_hover_pressed_color", "OptionButton", p_config.font_hover_pressed_color);
			p_theme->set_color("font_focus_color", "OptionButton", p_config.font_focus_color);
			p_theme->set_color("font_pressed_color", "OptionButton", p_config.font_pressed_color);
			p_theme->set_color("font_disabled_color", "OptionButton", p_config.font_disabled_color);
			p_theme->set_color("font_outline_color", "OptionButton", p_config.font_outline_color);

			p_theme->set_color("icon_normal_color", "OptionButton", p_config.icon_normal_color);
			p_theme->set_color("icon_hover_color", "OptionButton", p_config.icon_hover_color);
			p_theme->set_color("icon_focus_color", "OptionButton", p_config.icon_focus_color);
			p_theme->set_color("icon_pressed_color", "OptionButton", p_config.icon_pressed_color);
			p_theme->set_color("icon_disabled_color", "OptionButton", p_config.icon_disabled_color);

			p_theme->set_icon("arrow", "OptionButton", p_theme->get_icon(SNAME("GuiOptionArrow"), EditorStringName(EditorIcons)));
			p_theme->set_constant("arrow_margin", "OptionButton", p_config.widget_margin.x - 2 * EDSCALE);
			p_theme->set_constant("modulate_arrow", "OptionButton", true);
			p_theme->set_constant("h_separation", "OptionButton", 4 * EDSCALE);
			p_theme->set_constant("outline_size", "OptionButton", 0);
		}

		// CheckButton.

		p_theme->set_stylebox(CoreStringName(normal), "CheckButton", p_config.panel_container_style);
		p_theme->set_stylebox(SceneStringName(pressed), "CheckButton", p_config.panel_container_style);
		p_theme->set_stylebox("disabled", "CheckButton", p_config.panel_container_style);
		p_theme->set_stylebox(SceneStringName(hover), "CheckButton", p_config.panel_container_style);
		p_theme->set_stylebox("hover_pressed", "CheckButton", p_config.panel_container_style);

		p_theme->set_icon("checked", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOn"), EditorStringName(EditorIcons)));
		p_theme->set_icon("checked_disabled", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOnDisabled"), EditorStringName(EditorIcons)));
		p_theme->set_icon("unchecked", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOff"), EditorStringName(EditorIcons)));
		p_theme->set_icon("unchecked_disabled", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOffDisabled"), EditorStringName(EditorIcons)));

		p_theme->set_icon("checked_mirrored", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOnMirrored"), EditorStringName(EditorIcons)));
		p_theme->set_icon("checked_disabled_mirrored", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOnDisabledMirrored"), EditorStringName(EditorIcons)));
		p_theme->set_icon("unchecked_mirrored", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOffMirrored"), EditorStringName(EditorIcons)));
		p_theme->set_icon("unchecked_disabled_mirrored", "CheckButton", p_theme->get_icon(SNAME("GuiToggleOffDisabledMirrored"), EditorStringName(EditorIcons)));

		p_theme->set_color(SceneStringName(font_color), "CheckButton", p_config.font_color);
		p_theme->set_color("font_hover_color", "CheckButton", p_config.font_hover_color);
		p_theme->set_color("font_hover_pressed_color", "CheckButton", p_config.font_hover_pressed_color);
		p_theme->set_color("font_focus_color", "CheckButton", p_config.font_focus_color);
		p_theme->set_color("font_pressed_color", "CheckButton", p_config.font_pressed_color);
		p_theme->set_color("font_disabled_color", "CheckButton", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "CheckButton", p_config.font_outline_color);

		p_theme->set_color("icon_normal_color", "CheckButton", p_config.icon_normal_color);
		p_theme->set_color("icon_hover_color", "CheckButton", p_config.icon_hover_color);
		p_theme->set_color("icon_focus_color", "CheckButton", p_config.icon_focus_color);
		p_theme->set_color("icon_pressed_color", "CheckButton", p_config.icon_pressed_color);
		p_theme->set_color("icon_disabled_color", "CheckButton", p_config.icon_disabled_color);

		p_theme->set_constant("h_separation", "CheckButton", 8 * EDSCALE);
		p_theme->set_constant("check_v_offset", "CheckButton", 0);
		p_theme->set_constant("outline_size", "CheckButton", 0);

		// CheckBox.
		{
			Ref<StyleBoxFlat> checkbox_style = p_config.panel_container_style->duplicate();

			p_theme->set_stylebox(CoreStringName(normal), "CheckBox", checkbox_style);
			p_theme->set_stylebox(SceneStringName(pressed), "CheckBox", checkbox_style);
			p_theme->set_stylebox("disabled", "CheckBox", checkbox_style);
			p_theme->set_stylebox(SceneStringName(hover), "CheckBox", checkbox_style);
			p_theme->set_stylebox("hover_pressed", "CheckBox", checkbox_style);
			p_theme->set_icon("checked", "CheckBox", p_theme->get_icon(SNAME("GuiChecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked", "CheckBox", p_theme->get_icon(SNAME("GuiUnchecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_checked", "CheckBox", p_theme->get_icon(SNAME("GuiRadioChecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_unchecked", "CheckBox", p_theme->get_icon(SNAME("GuiRadioUnchecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("checked_disabled", "CheckBox", p_theme->get_icon(SNAME("GuiCheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked_disabled", "CheckBox", p_theme->get_icon(SNAME("GuiUncheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_checked_disabled", "CheckBox", p_theme->get_icon(SNAME("GuiRadioCheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_unchecked_disabled", "CheckBox", p_theme->get_icon(SNAME("GuiRadioUncheckedDisabled"), EditorStringName(EditorIcons)));

			p_theme->set_color(SceneStringName(font_color), "CheckBox", p_config.font_color);
			p_theme->set_color("font_hover_color", "CheckBox", p_config.font_hover_color);
			p_theme->set_color("font_hover_pressed_color", "CheckBox", p_config.font_hover_pressed_color);
			p_theme->set_color("font_focus_color", "CheckBox", p_config.font_focus_color);
			p_theme->set_color("font_pressed_color", "CheckBox", p_config.font_pressed_color);
			p_theme->set_color("font_disabled_color", "CheckBox", p_config.font_disabled_color);
			p_theme->set_color("font_outline_color", "CheckBox", p_config.font_outline_color);

			p_theme->set_color("icon_normal_color", "CheckBox", p_config.icon_normal_color);
			p_theme->set_color("icon_hover_color", "CheckBox", p_config.icon_hover_color);
			p_theme->set_color("icon_focus_color", "CheckBox", p_config.icon_focus_color);
			p_theme->set_color("icon_pressed_color", "CheckBox", p_config.icon_pressed_color);
			p_theme->set_color("icon_disabled_color", "CheckBox", p_config.icon_disabled_color);

			p_theme->set_constant("h_separation", "CheckBox", 8 * EDSCALE);
			p_theme->set_constant("check_v_offset", "CheckBox", 0);
			p_theme->set_constant("outline_size", "CheckBox", 0);
		}

		// LinkButton.

		p_theme->set_stylebox("focus", "LinkButton", p_config.base_empty_style);
		p_theme->set_color(SceneStringName(font_color), "LinkButton", p_config.font_color);
		p_theme->set_color("font_hover_color", "LinkButton", p_config.font_hover_color);
		p_theme->set_color("font_hover_pressed_color", "LinkButton", p_config.font_hover_pressed_color);
		p_theme->set_color("font_focus_color", "LinkButton", p_config.font_focus_color);
		p_theme->set_color("font_pressed_color", "LinkButton", p_config.font_pressed_color);
		p_theme->set_color("font_disabled_color", "LinkButton", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "LinkButton", p_config.font_outline_color);

		p_theme->set_constant("outline_size", "LinkButton", 0);
	}

	// Tree & ItemList.
	{
		Ref<StyleBoxFlat> style_tree_focus = p_config.base_style->duplicate();
		style_tree_focus->set_bg_color(p_config.highlight_color);
		style_tree_focus->set_border_width_all(0);

		Ref<StyleBoxFlat> style_tree_selected = style_tree_focus->duplicate();

		const Color guide_color = p_config.mono_color * Color(1, 1, 1, 0.05);

		// Tree.
		{
			p_theme->set_icon("checked", "Tree", p_theme->get_icon(SNAME("GuiChecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("checked_disabled", "Tree", p_theme->get_icon(SNAME("GuiCheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("indeterminate", "Tree", p_theme->get_icon(SNAME("GuiIndeterminate"), EditorStringName(EditorIcons)));
			p_theme->set_icon("indeterminate_disabled", "Tree", p_theme->get_icon(SNAME("GuiIndeterminateDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked", "Tree", p_theme->get_icon(SNAME("GuiUnchecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked_disabled", "Tree", p_theme->get_icon(SNAME("GuiUncheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("arrow", "Tree", p_theme->get_icon(SNAME("GuiTreeArrowDown"), EditorStringName(EditorIcons)));
			p_theme->set_icon("arrow_collapsed", "Tree", p_theme->get_icon(SNAME("GuiTreeArrowRight"), EditorStringName(EditorIcons)));
			p_theme->set_icon("arrow_collapsed_mirrored", "Tree", p_theme->get_icon(SNAME("GuiTreeArrowLeft"), EditorStringName(EditorIcons)));
			p_theme->set_icon("updown", "Tree", p_theme->get_icon(SNAME("GuiTreeUpdown"), EditorStringName(EditorIcons)));
			p_theme->set_icon("select_arrow", "Tree", p_theme->get_icon(SNAME("GuiDropdown"), EditorStringName(EditorIcons)));

			p_theme->set_stylebox(SceneStringName(panel), "Tree", p_config.tree_panel_style);
			p_theme->set_stylebox("focus", "Tree", p_config.button_style_focus);
			p_theme->set_stylebox("custom_button", "Tree", make_empty_stylebox());
			p_theme->set_stylebox("custom_button_pressed", "Tree", make_empty_stylebox());
			p_theme->set_stylebox("custom_button_hover", "Tree", p_config.button_style);

			p_theme->set_color("custom_button_font_highlight", "Tree", p_config.font_hover_color);
			p_theme->set_color(SceneStringName(font_color), "Tree", p_config.font_color);
			p_theme->set_color("font_hovered_color", "Tree", p_config.mono_color);
			p_theme->set_color("font_hovered_dimmed_color", "Tree", p_config.font_color);
			p_theme->set_color("font_selected_color", "Tree", p_config.mono_color);
			p_theme->set_color("font_disabled_color", "Tree", p_config.font_disabled_color);
			p_theme->set_color("font_outline_color", "Tree", p_config.font_outline_color);
			p_theme->set_color("title_button_color", "Tree", p_config.font_color);
			p_theme->set_color("drop_position_color", "Tree", p_config.accent_color);

			p_theme->set_constant("v_separation", "Tree", p_config.separation_margin);
			p_theme->set_constant("h_separation", "Tree", (p_config.increased_margin + 2) * EDSCALE);
			p_theme->set_constant("guide_width", "Tree", p_config.border_width);
			p_theme->set_constant("item_margin", "Tree", 3 * p_config.increased_margin * EDSCALE);
			p_theme->set_constant("inner_item_margin_top", "Tree", p_config.separation_margin);
			p_theme->set_constant("inner_item_margin_bottom", "Tree", p_config.separation_margin);
			p_theme->set_constant("inner_item_margin_left", "Tree", p_config.increased_margin * EDSCALE);
			p_theme->set_constant("inner_item_margin_right", "Tree", p_config.increased_margin * EDSCALE);
			p_theme->set_constant("button_margin", "Tree", p_config.base_margin * EDSCALE);
			p_theme->set_constant("scroll_border", "Tree", 40 * EDSCALE);
			p_theme->set_constant("scroll_speed", "Tree", 12);
			p_theme->set_constant("outline_size", "Tree", 0);
			p_theme->set_constant("scrollbar_margin_left", "Tree", 0);
			p_theme->set_constant("scrollbar_margin_top", "Tree", 0);
			p_theme->set_constant("scrollbar_margin_right", "Tree", 0);
			p_theme->set_constant("scrollbar_margin_bottom", "Tree", 0);
			p_theme->set_constant("scrollbar_h_separation", "Tree", 1 * EDSCALE);
			p_theme->set_constant("scrollbar_v_separation", "Tree", 1 * EDSCALE);

			Color relationship_line_color = p_config.mono_color * Color(1, 1, 1, p_config.relationship_line_opacity);

			p_theme->set_constant("draw_guides", "Tree", p_config.relationship_line_opacity < 0.01);
			p_theme->set_color("guide_color", "Tree", guide_color);

			int relationship_line_width = 1;
			Color parent_line_color = p_config.mono_color * Color(1, 1, 1, CLAMP(p_config.relationship_line_opacity + 0.45, 0.0, 1.0));
			Color children_line_color = p_config.mono_color * Color(1, 1, 1, CLAMP(p_config.relationship_line_opacity + 0.25, 0.0, 1.0));

			p_theme->set_constant("draw_relationship_lines", "Tree", p_config.relationship_line_opacity >= 0.01);
			p_theme->set_constant("relationship_line_width", "Tree", relationship_line_width);
			p_theme->set_constant("parent_hl_line_width", "Tree", relationship_line_width * 2);
			p_theme->set_constant("children_hl_line_width", "Tree", relationship_line_width);
			p_theme->set_constant("parent_hl_line_margin", "Tree", relationship_line_width * 3);
			p_theme->set_color("relationship_line_color", "Tree", relationship_line_color);
			p_theme->set_color("parent_hl_line_color", "Tree", parent_line_color);
			p_theme->set_color("children_hl_line_color", "Tree", children_line_color);
			p_theme->set_color("drop_position_color", "Tree", p_config.accent_color);

			Ref<StyleBoxFlat> style_tree_btn = p_config.base_style->duplicate();
			style_tree_btn->set_bg_color(p_config.highlight_color);
			style_tree_btn->set_border_width_all(0);
			p_theme->set_stylebox("button_pressed", "Tree", style_tree_btn);

			Ref<StyleBoxFlat> style_tree_hover = p_config.base_style->duplicate();
			style_tree_hover->set_bg_color(p_config.highlight_color * Color(1, 1, 1, 0.4));
			style_tree_hover->set_border_width_all(0);
			p_theme->set_stylebox("hovered", "Tree", style_tree_hover);
			p_theme->set_stylebox("button_hover", "Tree", style_tree_hover);

			Ref<StyleBoxFlat> style_tree_hover_dimmed = p_config.base_style->duplicate();
			style_tree_hover_dimmed->set_bg_color(p_config.highlight_color * Color(1, 1, 1, 0.2));
			style_tree_hover_dimmed->set_border_width_all(0);
			p_theme->set_stylebox("hovered_dimmed", "Tree", style_tree_hover_dimmed);

			p_theme->set_stylebox("selected_focus", "Tree", style_tree_focus);
			p_theme->set_stylebox("selected", "Tree", style_tree_selected);

			Ref<StyleBoxFlat> style_tree_cursor = p_config.base_style->duplicate();
			style_tree_cursor->set_draw_center(false);
			style_tree_cursor->set_border_width_all(MAX(1, p_config.border_width));
			style_tree_cursor->set_border_color(p_config.contrast_color_1);

			Ref<StyleBoxFlat> style_tree_title = p_config.base_style->duplicate();
			style_tree_title->set_bg_color(p_config.dark_color_3);
			style_tree_title->set_border_width_all(0);
			p_theme->set_stylebox("cursor", "Tree", style_tree_cursor);
			p_theme->set_stylebox("cursor_unfocused", "Tree", style_tree_cursor);
			p_theme->set_stylebox("title_button_normal", "Tree", style_tree_title);
			p_theme->set_stylebox("title_button_hover", "Tree", style_tree_title);
			p_theme->set_stylebox("title_button_pressed", "Tree", style_tree_title);
		}

		// ItemList.
		{
			Ref<StyleBoxFlat> style_itemlist_bg = p_config.base_style->duplicate();
			style_itemlist_bg->set_content_margin_all(p_config.separation_margin);
			style_itemlist_bg->set_bg_color(p_config.dark_color_1);

			if (p_config.draw_extra_borders) {
				style_itemlist_bg->set_border_width_all(Math::round(EDSCALE));
				style_itemlist_bg->set_border_color(p_config.extra_border_color_2);
			} else {
				style_itemlist_bg->set_border_width_all(p_config.border_width);
				style_itemlist_bg->set_border_color(p_config.dark_color_3);
			}

			Ref<StyleBoxFlat> style_itemlist_cursor = p_config.base_style->duplicate();
			style_itemlist_cursor->set_draw_center(false);
			style_itemlist_cursor->set_border_width_all(p_config.border_width);
			style_itemlist_cursor->set_border_color(p_config.highlight_color);

			Ref<StyleBoxFlat> style_itemlist_hover = style_tree_selected->duplicate();
			style_itemlist_hover->set_bg_color(p_config.highlight_color * Color(1, 1, 1, 0.3));
			style_itemlist_hover->set_border_width_all(0);

			p_theme->set_stylebox(SceneStringName(panel), "ItemList", style_itemlist_bg);
			p_theme->set_stylebox("focus", "ItemList", p_config.button_style_focus);
			p_theme->set_stylebox("cursor", "ItemList", style_itemlist_cursor);
			p_theme->set_stylebox("cursor_unfocused", "ItemList", style_itemlist_cursor);
			p_theme->set_stylebox("selected_focus", "ItemList", style_tree_focus);
			p_theme->set_stylebox("selected", "ItemList", style_tree_selected);
			p_theme->set_stylebox("hovered", "ItemList", style_itemlist_hover);
			p_theme->set_color(SceneStringName(font_color), "ItemList", p_config.font_color);
			p_theme->set_color("font_hovered_color", "ItemList", p_config.mono_color);
			p_theme->set_color("font_selected_color", "ItemList", p_config.mono_color);
			p_theme->set_color("font_outline_color", "ItemList", p_config.font_outline_color);
			p_theme->set_color("guide_color", "ItemList", Color(1, 1, 1, 0));
			p_theme->set_constant("v_separation", "ItemList", p_config.forced_even_separation * EDSCALE);
			p_theme->set_constant("h_separation", "ItemList", (p_config.increased_margin + 2) * EDSCALE);
			p_theme->set_constant("icon_margin", "ItemList", (p_config.increased_margin + 2) * EDSCALE);
			p_theme->set_constant(SceneStringName(line_separation), "ItemList", p_config.separation_margin);
			p_theme->set_constant("outline_size", "ItemList", 0);
		}
	}

	// TabBar & TabContainer.
	{
		Ref<StyleBoxFlat> style_tab_base = p_config.button_style->duplicate();

		style_tab_base->set_border_width_all(0);
		// Don't round the top corners to avoid creating a small blank space between the tabs and the main panel.
		// This also makes the top highlight look better.
		style_tab_base->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
		style_tab_base->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);

		// When using a border width greater than 0, visually line up the left of the selected tab with the underlying panel.
		style_tab_base->set_expand_margin(SIDE_LEFT, -p_config.border_width);

		style_tab_base->set_content_margin(SIDE_LEFT, p_config.widget_margin.x + 5 * EDSCALE);
		style_tab_base->set_content_margin(SIDE_RIGHT, p_config.widget_margin.x + 5 * EDSCALE);
		style_tab_base->set_content_margin(SIDE_BOTTOM, p_config.widget_margin.y);
		style_tab_base->set_content_margin(SIDE_TOP, p_config.widget_margin.y);

		Ref<StyleBoxFlat> style_tab_selected = style_tab_base->duplicate();

		style_tab_selected->set_bg_color(p_config.base_color);
		// Add a highlight line at the top of the selected tab.
		style_tab_selected->set_border_width(SIDE_TOP, Math::round(2 * EDSCALE));
		// Make the highlight line prominent, but not too prominent as to not be distracting.
		Color tab_highlight = p_config.dark_color_2.lerp(p_config.accent_color, 0.75);
		style_tab_selected->set_border_color(tab_highlight);
		style_tab_selected->set_corner_radius_all(0);

		Ref<StyleBoxFlat> style_tab_hovered = style_tab_base->duplicate();

		style_tab_hovered->set_bg_color(p_config.dark_color_1.lerp(p_config.base_color, 0.4));
		// Hovered tab has a subtle highlight between normal and selected states.
		style_tab_hovered->set_corner_radius_all(0);

		Ref<StyleBoxFlat> style_tab_unselected = style_tab_base->duplicate();
		style_tab_unselected->set_expand_margin(SIDE_BOTTOM, 0);
		style_tab_unselected->set_bg_color(p_config.dark_color_1);
		// Add some spacing between unselected tabs to make them easier to distinguish from each other
		style_tab_unselected->set_border_color(Color(0, 0, 0, 0));

		Ref<StyleBoxFlat> style_tab_disabled = style_tab_base->duplicate();
		style_tab_disabled->set_expand_margin(SIDE_BOTTOM, 0);
		style_tab_disabled->set_bg_color(p_config.disabled_bg_color);
		style_tab_disabled->set_border_color(p_config.disabled_bg_color);

		Ref<StyleBoxFlat> style_tab_focus = p_config.button_style_focus->duplicate();

		Ref<StyleBoxFlat> style_tabbar_background = make_flat_stylebox(p_config.dark_color_1, 0, 0, 0, 0, p_config.corner_radius * EDSCALE);
		style_tabbar_background->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
		style_tabbar_background->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);
		p_theme->set_stylebox("tabbar_background", "TabContainer", style_tabbar_background);
		p_theme->set_stylebox(SceneStringName(panel), "TabContainer", p_config.content_panel_style);

		p_theme->set_stylebox("tab_selected", "TabContainer", style_tab_selected);
		p_theme->set_stylebox("tab_hovered", "TabContainer", style_tab_hovered);
		p_theme->set_stylebox("tab_unselected", "TabContainer", style_tab_unselected);
		p_theme->set_stylebox("tab_disabled", "TabContainer", style_tab_disabled);
		p_theme->set_stylebox("tab_focus", "TabContainer", style_tab_focus);
		p_theme->set_stylebox("tab_selected", "TabBar", style_tab_selected);
		p_theme->set_stylebox("tab_hovered", "TabBar", style_tab_hovered);
		p_theme->set_stylebox("tab_unselected", "TabBar", style_tab_unselected);
		p_theme->set_stylebox("tab_disabled", "TabBar", style_tab_disabled);
		p_theme->set_stylebox("tab_focus", "TabBar", style_tab_focus);
		p_theme->set_stylebox("button_pressed", "TabBar", p_config.panel_container_style);
		p_theme->set_stylebox("button_highlight", "TabBar", p_config.panel_container_style);

		p_theme->set_color("font_selected_color", "TabContainer", p_config.font_color);
		p_theme->set_color("font_hovered_color", "TabContainer", p_config.font_color);
		p_theme->set_color("font_unselected_color", "TabContainer", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "TabContainer", p_config.font_outline_color);
		p_theme->set_color("font_selected_color", "TabBar", p_config.font_color);
		p_theme->set_color("font_hovered_color", "TabBar", p_config.font_color);
		p_theme->set_color("font_unselected_color", "TabBar", p_config.font_disabled_color);
		p_theme->set_color("font_outline_color", "TabBar", p_config.font_outline_color);
		p_theme->set_color("drop_mark_color", "TabContainer", tab_highlight);
		p_theme->set_color("drop_mark_color", "TabBar", tab_highlight);

		p_theme->set_icon("menu", "TabContainer", p_theme->get_icon(SNAME("GuiTabMenu"), EditorStringName(EditorIcons)));
		p_theme->set_icon("menu_highlight", "TabContainer", p_theme->get_icon(SNAME("GuiTabMenuHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("close", "TabBar", p_theme->get_icon(SNAME("GuiClose"), EditorStringName(EditorIcons)));
		p_theme->set_icon("increment", "TabContainer", p_theme->get_icon(SNAME("GuiScrollArrowRight"), EditorStringName(EditorIcons)));
		p_theme->set_icon("decrement", "TabContainer", p_theme->get_icon(SNAME("GuiScrollArrowLeft"), EditorStringName(EditorIcons)));
		p_theme->set_icon("increment", "TabBar", p_theme->get_icon(SNAME("GuiScrollArrowRight"), EditorStringName(EditorIcons)));
		p_theme->set_icon("decrement", "TabBar", p_theme->get_icon(SNAME("GuiScrollArrowLeft"), EditorStringName(EditorIcons)));
		p_theme->set_icon("increment_highlight", "TabBar", p_theme->get_icon(SNAME("GuiScrollArrowRightHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("decrement_highlight", "TabBar", p_theme->get_icon(SNAME("GuiScrollArrowLeftHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("increment_highlight", "TabContainer", p_theme->get_icon(SNAME("GuiScrollArrowRightHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("decrement_highlight", "TabContainer", p_theme->get_icon(SNAME("GuiScrollArrowLeftHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("drop_mark", "TabContainer", p_theme->get_icon(SNAME("GuiTabDropMark"), EditorStringName(EditorIcons)));
		p_theme->set_icon("drop_mark", "TabBar", p_theme->get_icon(SNAME("GuiTabDropMark"), EditorStringName(EditorIcons)));

		p_theme->set_constant("side_margin", "TabContainer", 0);
		p_theme->set_constant("outline_size", "TabContainer", 0);
		p_theme->set_constant("h_separation", "TabBar", 4 * EDSCALE);
		p_theme->set_constant("outline_size", "TabBar", 0);
	}

	// Separators.
	p_theme->set_stylebox("separator", "HSeparator", make_line_stylebox(p_config.separator_color, MAX(Math::round(EDSCALE), p_config.border_width)));
	p_theme->set_stylebox("separator", "VSeparator", make_line_stylebox(p_config.separator_color, MAX(Math::round(EDSCALE), p_config.border_width), 0, 0, true));

	// LineEdit & TextEdit.
	{
		Ref<StyleBoxFlat> text_editor_style = p_config.button_style->duplicate();

		// Don't round the bottom corners to make the line look sharper.
		text_editor_style->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
		text_editor_style->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);

		if (p_config.draw_extra_borders) {
			text_editor_style->set_border_width_all(Math::round(EDSCALE));
			text_editor_style->set_border_color(p_config.extra_border_color_1);
		} else {
			// Add a bottom line to make LineEdits more visible, especially in sectioned inspectors
			// such as the Project Settings.
			text_editor_style->set_border_width(SIDE_BOTTOM, Math::round(2 * EDSCALE));
			text_editor_style->set_border_color(p_config.dark_color_2);
		}

		Ref<StyleBoxFlat> text_editor_disabled_style = text_editor_style->duplicate();
		text_editor_disabled_style->set_border_color(p_config.disabled_border_color);
		text_editor_disabled_style->set_bg_color(p_config.disabled_bg_color);

		// LineEdit.

		p_theme->set_stylebox(CoreStringName(normal), "LineEdit", text_editor_style);
		p_theme->set_stylebox("focus", "LineEdit", p_config.button_style_focus);
		p_theme->set_stylebox("read_only", "LineEdit", text_editor_disabled_style);

		p_theme->set_icon("clear", "LineEdit", p_theme->get_icon(SNAME("GuiClose"), EditorStringName(EditorIcons)));

		p_theme->set_color(SceneStringName(font_color), "LineEdit", p_config.font_color);
		p_theme->set_color("font_selected_color", "LineEdit", p_config.mono_color);
		p_theme->set_color("font_uneditable_color", "LineEdit", p_config.font_readonly_color);
		p_theme->set_color("font_placeholder_color", "LineEdit", p_config.font_placeholder_color);
		p_theme->set_color("font_outline_color", "LineEdit", p_config.font_outline_color);
		p_theme->set_color("caret_color", "LineEdit", p_config.font_color);
		p_theme->set_color("selection_color", "LineEdit", p_config.selection_color);
		p_theme->set_color("clear_button_color", "LineEdit", p_config.font_color);
		p_theme->set_color("clear_button_color_pressed", "LineEdit", p_config.accent_color);

		p_theme->set_constant("minimum_character_width", "LineEdit", 4);
		p_theme->set_constant("outline_size", "LineEdit", 0);
		p_theme->set_constant("caret_width", "LineEdit", 1);

		// TextEdit.

		p_theme->set_stylebox(CoreStringName(normal), "TextEdit", text_editor_style);
		p_theme->set_stylebox("focus", "TextEdit", p_config.button_style_focus);
		p_theme->set_stylebox("read_only", "TextEdit", text_editor_disabled_style);

		p_theme->set_icon("tab", "TextEdit", p_theme->get_icon(SNAME("GuiTab"), EditorStringName(EditorIcons)));
		p_theme->set_icon("space", "TextEdit", p_theme->get_icon(SNAME("GuiSpace"), EditorStringName(EditorIcons)));

		p_theme->set_color(SceneStringName(font_color), "TextEdit", p_config.font_color);
		p_theme->set_color("font_readonly_color", "TextEdit", p_config.font_readonly_color);
		p_theme->set_color("font_placeholder_color", "TextEdit", p_config.font_placeholder_color);
		p_theme->set_color("font_outline_color", "TextEdit", p_config.font_outline_color);
		p_theme->set_color("caret_color", "TextEdit", p_config.font_color);
		p_theme->set_color("selection_color", "TextEdit", p_config.selection_color);
		p_theme->set_color("background_color", "TextEdit", Color(0, 0, 0, 0));

		p_theme->set_constant("line_spacing", "TextEdit", 4 * EDSCALE);
		p_theme->set_constant("outline_size", "TextEdit", 0);
		p_theme->set_constant("caret_width", "TextEdit", 1);
	}

	// Containers.
	{
		p_theme->set_constant("separation", "BoxContainer", p_config.separation_margin);
		p_theme->set_constant("separation", "HBoxContainer", p_config.separation_margin);
		p_theme->set_constant("separation", "VBoxContainer", p_config.separation_margin);
		p_theme->set_constant("margin_left", "MarginContainer", 0);
		p_theme->set_constant("margin_top", "MarginContainer", 0);
		p_theme->set_constant("margin_right", "MarginContainer", 0);
		p_theme->set_constant("margin_bottom", "MarginContainer", 0);
		p_theme->set_constant("h_separation", "GridContainer", p_config.separation_margin);
		p_theme->set_constant("v_separation", "GridContainer", p_config.separation_margin);
		p_theme->set_constant("h_separation", "FlowContainer", p_config.separation_margin);
		p_theme->set_constant("v_separation", "FlowContainer", p_config.separation_margin);
		p_theme->set_constant("h_separation", "HFlowContainer", p_config.separation_margin);
		p_theme->set_constant("v_separation", "HFlowContainer", p_config.separation_margin);
		p_theme->set_constant("h_separation", "VFlowContainer", p_config.separation_margin);
		p_theme->set_constant("v_separation", "VFlowContainer", p_config.separation_margin);

		// SplitContainer.

		p_theme->set_icon("h_grabber", "SplitContainer", p_theme->get_icon(SNAME("GuiHsplitter"), EditorStringName(EditorIcons)));
		p_theme->set_icon("v_grabber", "SplitContainer", p_theme->get_icon(SNAME("GuiVsplitter"), EditorStringName(EditorIcons)));
		p_theme->set_icon("grabber", "VSplitContainer", p_theme->get_icon(SNAME("GuiVsplitter"), EditorStringName(EditorIcons)));
		p_theme->set_icon("grabber", "HSplitContainer", p_theme->get_icon(SNAME("GuiHsplitter"), EditorStringName(EditorIcons)));

		p_theme->set_constant("separation", "SplitContainer", p_config.separation_margin);
		p_theme->set_constant("separation", "HSplitContainer", p_config.separation_margin);
		p_theme->set_constant("separation", "VSplitContainer", p_config.separation_margin);

		p_theme->set_constant("minimum_grab_thickness", "SplitContainer", p_config.increased_margin * EDSCALE);
		p_theme->set_constant("minimum_grab_thickness", "HSplitContainer", p_config.increased_margin * EDSCALE);
		p_theme->set_constant("minimum_grab_thickness", "VSplitContainer", p_config.increased_margin * EDSCALE);

		// GridContainer.
		p_theme->set_constant("v_separation", "GridContainer", Math::round(p_config.widget_margin.y - 2 * EDSCALE));
	}

	// Window and dialogs.
	{
		// Window.

		p_theme->set_stylebox("embedded_border", "Window", p_config.window_style);
		p_theme->set_stylebox("embedded_unfocused_border", "Window", p_config.window_style);

		p_theme->set_color("title_color", "Window", p_config.font_color);
		p_theme->set_icon("close", "Window", p_theme->get_icon(SNAME("GuiClose"), EditorStringName(EditorIcons)));
		p_theme->set_icon("close_pressed", "Window", p_theme->get_icon(SNAME("GuiClose"), EditorStringName(EditorIcons)));
		p_theme->set_constant("close_h_offset", "Window", 22 * EDSCALE);
		p_theme->set_constant("close_v_offset", "Window", 20 * EDSCALE);
		p_theme->set_constant("title_height", "Window", 24 * EDSCALE);
		p_theme->set_constant("resize_margin", "Window", 4 * EDSCALE);
		p_theme->set_font("title_font", "Window", p_theme->get_font(SNAME("title"), EditorStringName(EditorFonts)));
		p_theme->set_font_size("title_font_size", "Window", p_theme->get_font_size(SNAME("title_size"), EditorStringName(EditorFonts)));

		// AcceptDialog.
		p_theme->set_stylebox(SceneStringName(panel), "AcceptDialog", p_config.dialog_style);
		p_theme->set_constant("buttons_separation", "AcceptDialog", 8 * EDSCALE);
		// Make buttons with short texts such as "OK" easier to click/tap.
		p_theme->set_constant("buttons_min_width", "AcceptDialog", p_config.dialogs_buttons_min_size.x * EDSCALE);
		p_theme->set_constant("buttons_min_height", "AcceptDialog", p_config.dialogs_buttons_min_size.y * EDSCALE);

		// FileDialog.
		p_theme->set_icon("folder", "FileDialog", p_theme->get_icon(SNAME("Folder"), EditorStringName(EditorIcons)));
		p_theme->set_icon("parent_folder", "FileDialog", p_theme->get_icon(SNAME("ArrowUp"), EditorStringName(EditorIcons)));
		p_theme->set_icon("back_folder", "FileDialog", p_theme->get_icon(SNAME("Back"), EditorStringName(EditorIcons)));
		p_theme->set_icon("forward_folder", "FileDialog", p_theme->get_icon(SNAME("Forward"), EditorStringName(EditorIcons)));
		p_theme->set_icon("reload", "FileDialog", p_theme->get_icon(SNAME("Reload"), EditorStringName(EditorIcons)));
		p_theme->set_icon("toggle_hidden", "FileDialog", p_theme->get_icon(SNAME("GuiVisibilityVisible"), EditorStringName(EditorIcons)));
		p_theme->set_icon("create_folder", "FileDialog", p_theme->get_icon(SNAME("FolderCreate"), EditorStringName(EditorIcons)));
		// Use a different color for folder icons to make them easier to distinguish from files.
		// On a light theme, the icon will be dark, so we need to lighten it before blending it with the accent color.
		p_theme->set_color("folder_icon_color", "FileDialog", (p_config.dark_theme ? Color(1, 1, 1) : Color(4.25, 4.25, 4.25)).lerp(p_config.accent_color, 0.7));
		p_theme->set_color("file_disabled_color", "FileDialog", p_config.font_disabled_color);

		// PopupDialog.
		p_theme->set_stylebox(SceneStringName(panel), "PopupDialog", p_config.popup_style);

		// PopupMenu.
		{
			Ref<StyleBoxFlat> style_popup_menu = p_config.popup_border_style->duplicate();
			// Use 1 pixel for the sides, since if 0 is used, the highlight of hovered items is drawn
			// on top of the popup border. This causes a 'gap' in the panel border when an item is highlighted,
			// and it looks weird. 1px solves this.
			style_popup_menu->set_content_margin_individual(Math::round(EDSCALE), 2 * EDSCALE, Math::round(EDSCALE), 2 * EDSCALE);
			p_theme->set_stylebox(SceneStringName(panel), "PopupMenu", style_popup_menu);

			Ref<StyleBoxFlat> style_menu_hover = p_config.button_style_hover->duplicate();
			// Don't use rounded corners for hover highlights since the StyleBox touches the PopupMenu's edges.
			style_menu_hover->set_corner_radius_all(0);
			p_theme->set_stylebox(SceneStringName(hover), "PopupMenu", style_menu_hover);

			Ref<StyleBoxLine> style_popup_separator(memnew(StyleBoxLine));
			style_popup_separator->set_color(p_config.separator_color);
			style_popup_separator->set_grow_begin(Math::round(EDSCALE) - MAX(Math::round(EDSCALE), p_config.border_width));
			style_popup_separator->set_grow_end(Math::round(EDSCALE) - MAX(Math::round(EDSCALE), p_config.border_width));
			style_popup_separator->set_thickness(MAX(Math::round(EDSCALE), p_config.border_width));

			Ref<StyleBoxLine> style_popup_labeled_separator_left(memnew(StyleBoxLine));
			style_popup_labeled_separator_left->set_grow_begin(Math::round(EDSCALE) - MAX(Math::round(EDSCALE), p_config.border_width));
			style_popup_labeled_separator_left->set_color(p_config.separator_color);
			style_popup_labeled_separator_left->set_thickness(MAX(Math::round(EDSCALE), p_config.border_width));

			Ref<StyleBoxLine> style_popup_labeled_separator_right(memnew(StyleBoxLine));
			style_popup_labeled_separator_right->set_grow_end(Math::round(EDSCALE) - MAX(Math::round(EDSCALE), p_config.border_width));
			style_popup_labeled_separator_right->set_color(p_config.separator_color);
			style_popup_labeled_separator_right->set_thickness(MAX(Math::round(EDSCALE), p_config.border_width));

			p_theme->set_stylebox("separator", "PopupMenu", style_popup_separator);
			p_theme->set_stylebox("labeled_separator_left", "PopupMenu", style_popup_labeled_separator_left);
			p_theme->set_stylebox("labeled_separator_right", "PopupMenu", style_popup_labeled_separator_right);

			p_theme->set_color(SceneStringName(font_color), "PopupMenu", p_config.font_color);
			p_theme->set_color("font_hover_color", "PopupMenu", p_config.font_hover_color);
			p_theme->set_color("font_accelerator_color", "PopupMenu", p_config.font_disabled_color);
			p_theme->set_color("font_disabled_color", "PopupMenu", p_config.font_disabled_color);
			p_theme->set_color("font_separator_color", "PopupMenu", p_config.font_disabled_color);
			p_theme->set_color("font_outline_color", "PopupMenu", p_config.font_outline_color);

			p_theme->set_icon("checked", "PopupMenu", p_theme->get_icon(SNAME("GuiChecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked", "PopupMenu", p_theme->get_icon(SNAME("GuiUnchecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_checked", "PopupMenu", p_theme->get_icon(SNAME("GuiRadioChecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_unchecked", "PopupMenu", p_theme->get_icon(SNAME("GuiRadioUnchecked"), EditorStringName(EditorIcons)));
			p_theme->set_icon("checked_disabled", "PopupMenu", p_theme->get_icon(SNAME("GuiCheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("unchecked_disabled", "PopupMenu", p_theme->get_icon(SNAME("GuiUncheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_checked_disabled", "PopupMenu", p_theme->get_icon(SNAME("GuiRadioCheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("radio_unchecked_disabled", "PopupMenu", p_theme->get_icon(SNAME("GuiRadioUncheckedDisabled"), EditorStringName(EditorIcons)));
			p_theme->set_icon("submenu", "PopupMenu", p_theme->get_icon(SNAME("ArrowRight"), EditorStringName(EditorIcons)));
			p_theme->set_icon("submenu_mirrored", "PopupMenu", p_theme->get_icon(SNAME("ArrowLeft"), EditorStringName(EditorIcons)));
			p_theme->set_icon("visibility_hidden", "PopupMenu", p_theme->get_icon(SNAME("GuiVisibilityHidden"), EditorStringName(EditorIcons)));
			p_theme->set_icon("visibility_visible", "PopupMenu", p_theme->get_icon(SNAME("GuiVisibilityVisible"), EditorStringName(EditorIcons)));
			p_theme->set_icon("visibility_xray", "PopupMenu", p_theme->get_icon(SNAME("GuiVisibilityXray"), EditorStringName(EditorIcons)));

			p_theme->set_constant("v_separation", "PopupMenu", p_config.forced_even_separation * EDSCALE);
			p_theme->set_constant("outline_size", "PopupMenu", 0);
			p_theme->set_constant("item_start_padding", "PopupMenu", p_config.separation_margin);
			p_theme->set_constant("item_end_padding", "PopupMenu", p_config.separation_margin);
		}
	}

	// Sliders and scrollbars.
	{
		Ref<Texture2D> empty_icon = memnew(ImageTexture);

		// HScrollBar.

		if (p_config.increase_scrollbar_touch_area) {
			p_theme->set_stylebox("scroll", "HScrollBar", make_line_stylebox(p_config.separator_color, 50));
		} else {
			p_theme->set_stylebox("scroll", "HScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollBg"), EditorStringName(EditorIcons)), 5, 5, 5, 5, -5, 1, -5, 1));
		}
		p_theme->set_stylebox("scroll_focus", "HScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollBg"), EditorStringName(EditorIcons)), 5, 5, 5, 5, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber", "HScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabber"), EditorStringName(EditorIcons)), 6, 6, 6, 6, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber_highlight", "HScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabberHl"), EditorStringName(EditorIcons)), 5, 5, 5, 5, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber_pressed", "HScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabberPressed"), EditorStringName(EditorIcons)), 6, 6, 6, 6, 1, 1, 1, 1));

		p_theme->set_icon("increment", "HScrollBar", empty_icon);
		p_theme->set_icon("increment_highlight", "HScrollBar", empty_icon);
		p_theme->set_icon("increment_pressed", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement_highlight", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement_pressed", "HScrollBar", empty_icon);

		// VScrollBar.

		if (p_config.increase_scrollbar_touch_area) {
			p_theme->set_stylebox("scroll", "VScrollBar", make_line_stylebox(p_config.separator_color, 50, 1, 1, true));
		} else {
			p_theme->set_stylebox("scroll", "VScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollBg"), EditorStringName(EditorIcons)), 5, 5, 5, 5, 1, -5, 1, -5));
		}
		p_theme->set_stylebox("scroll_focus", "VScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollBg"), EditorStringName(EditorIcons)), 5, 5, 5, 5, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber", "VScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabber"), EditorStringName(EditorIcons)), 6, 6, 6, 6, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber_highlight", "VScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabberHl"), EditorStringName(EditorIcons)), 5, 5, 5, 5, 1, 1, 1, 1));
		p_theme->set_stylebox("grabber_pressed", "VScrollBar", make_stylebox(p_theme->get_icon(SNAME("GuiScrollGrabberPressed"), EditorStringName(EditorIcons)), 6, 6, 6, 6, 1, 1, 1, 1));

		p_theme->set_icon("increment", "VScrollBar", empty_icon);
		p_theme->set_icon("increment_highlight", "VScrollBar", empty_icon);
		p_theme->set_icon("increment_pressed", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement_highlight", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement_pressed", "VScrollBar", empty_icon);

		// Slider
		const int background_margin = MAX(2, p_config.base_margin / 2);

		// HSlider.
		p_theme->set_icon("grabber_highlight", "HSlider", p_theme->get_icon(SNAME("GuiSliderGrabberHl"), EditorStringName(EditorIcons)));
		p_theme->set_icon("grabber", "HSlider", p_theme->get_icon(SNAME("GuiSliderGrabber"), EditorStringName(EditorIcons)));
		p_theme->set_stylebox("slider", "HSlider", make_flat_stylebox(p_config.dark_color_3, 0, background_margin, 0, background_margin, p_config.corner_radius));
		p_theme->set_stylebox("grabber_area", "HSlider", make_flat_stylebox(p_config.contrast_color_1, 0, background_margin, 0, background_margin, p_config.corner_radius));
		p_theme->set_stylebox("grabber_area_highlight", "HSlider", make_flat_stylebox(p_config.contrast_color_1, 0, background_margin, 0, background_margin));
		p_theme->set_constant("center_grabber", "HSlider", 0);
		p_theme->set_constant("grabber_offset", "HSlider", 0);

		// VSlider.
		p_theme->set_icon("grabber", "VSlider", p_theme->get_icon(SNAME("GuiSliderGrabber"), EditorStringName(EditorIcons)));
		p_theme->set_icon("grabber_highlight", "VSlider", p_theme->get_icon(SNAME("GuiSliderGrabberHl"), EditorStringName(EditorIcons)));
		p_theme->set_stylebox("slider", "VSlider", make_flat_stylebox(p_config.dark_color_3, background_margin, 0, background_margin, 0, p_config.corner_radius));
		p_theme->set_stylebox("grabber_area", "VSlider", make_flat_stylebox(p_config.contrast_color_1, background_margin, 0, background_margin, 0, p_config.corner_radius));
		p_theme->set_stylebox("grabber_area_highlight", "VSlider", make_flat_stylebox(p_config.contrast_color_1, background_margin, 0, background_margin, 0));
		p_theme->set_constant("center_grabber", "VSlider", 0);
		p_theme->set_constant("grabber_offset", "VSlider", 0);
	}

	// Labels.
	{
		// RichTextLabel.

		p_theme->set_stylebox(CoreStringName(normal), "RichTextLabel", p_config.tree_panel_style);
		p_theme->set_stylebox("focus", "RichTextLabel", make_empty_stylebox());

		p_theme->set_color("default_color", "RichTextLabel", p_config.font_color);
		p_theme->set_color("font_shadow_color", "RichTextLabel", Color(0, 0, 0, 0));
		p_theme->set_color("font_outline_color", "RichTextLabel", p_config.font_outline_color);
		p_theme->set_color("selection_color", "RichTextLabel", p_config.selection_color);

		p_theme->set_constant("shadow_offset_x", "RichTextLabel", 1 * EDSCALE);
		p_theme->set_constant("shadow_offset_y", "RichTextLabel", 1 * EDSCALE);
		p_theme->set_constant("shadow_outline_size", "RichTextLabel", 1 * EDSCALE);
		p_theme->set_constant("outline_size", "RichTextLabel", 0);

		// Label.

		p_theme->set_stylebox(CoreStringName(normal), "Label", p_config.base_empty_style);

		p_theme->set_color(SceneStringName(font_color), "Label", p_config.font_color);
		p_theme->set_color("font_shadow_color", "Label", Color(0, 0, 0, 0));
		p_theme->set_color("font_outline_color", "Label", p_config.font_outline_color);

		p_theme->set_constant("shadow_offset_x", "Label", 1 * EDSCALE);
		p_theme->set_constant("shadow_offset_y", "Label", 1 * EDSCALE);
		p_theme->set_constant("shadow_outline_size", "Label", 1 * EDSCALE);
		p_theme->set_constant("line_spacing", "Label", 3 * EDSCALE);
		p_theme->set_constant("outline_size", "Label", 0);
	}

	// SpinBox.
	{
		Ref<Texture2D> empty_icon = memnew(ImageTexture);
		p_theme->set_icon("updown", "SpinBox", empty_icon);
		p_theme->set_icon("up", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxUp"), EditorStringName(EditorIcons)));
		p_theme->set_icon("up_hover", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxUp"), EditorStringName(EditorIcons)));
		p_theme->set_icon("up_pressed", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxUp"), EditorStringName(EditorIcons)));
		p_theme->set_icon("up_disabled", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxUp"), EditorStringName(EditorIcons)));
		p_theme->set_icon("down", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxDown"), EditorStringName(EditorIcons)));
		p_theme->set_icon("down_hover", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxDown"), EditorStringName(EditorIcons)));
		p_theme->set_icon("down_pressed", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxDown"), EditorStringName(EditorIcons)));
		p_theme->set_icon("down_disabled", "SpinBox", p_theme->get_icon(SNAME("GuiSpinboxDown"), EditorStringName(EditorIcons)));

		p_theme->set_stylebox("up_background", "SpinBox", make_empty_stylebox());
		p_theme->set_stylebox("up_background_hovered", "SpinBox", p_config.button_style_hover);
		p_theme->set_stylebox("up_background_pressed", "SpinBox", p_config.button_style_pressed);
		p_theme->set_stylebox("up_background_disabled", "SpinBox", make_empty_stylebox());
		p_theme->set_stylebox("down_background", "SpinBox", make_empty_stylebox());
		p_theme->set_stylebox("down_background_hovered", "SpinBox", p_config.button_style_hover);
		p_theme->set_stylebox("down_background_pressed", "SpinBox", p_config.button_style_pressed);
		p_theme->set_stylebox("down_background_disabled", "SpinBox", make_empty_stylebox());

		p_theme->set_color("up_icon_modulate", "SpinBox", p_config.font_color);
		p_theme->set_color("up_hover_icon_modulate", "SpinBox", p_config.font_hover_color);
		p_theme->set_color("up_pressed_icon_modulate", "SpinBox", p_config.font_pressed_color);
		p_theme->set_color("up_disabled_icon_modulate", "SpinBox", p_config.font_disabled_color);
		p_theme->set_color("down_icon_modulate", "SpinBox", p_config.font_color);
		p_theme->set_color("down_hover_icon_modulate", "SpinBox", p_config.font_hover_color);
		p_theme->set_color("down_pressed_icon_modulate", "SpinBox", p_config.font_pressed_color);
		p_theme->set_color("down_disabled_icon_modulate", "SpinBox", p_config.font_disabled_color);

		p_theme->set_stylebox("field_and_buttons_separator", "SpinBox", make_empty_stylebox());
		p_theme->set_stylebox("up_down_buttons_separator", "SpinBox", make_empty_stylebox());

		p_theme->set_constant("buttons_vertical_separation", "SpinBox", 0);
		p_theme->set_constant("field_and_buttons_separation", "SpinBox", 2);
		p_theme->set_constant("buttons_width", "SpinBox", 16);
#ifndef DISABLE_DEPRECATED
		p_theme->set_constant("set_min_buttons_width_from_icons", "SpinBox", 1);
#endif
	}

	// ProgressBar.
	p_theme->set_stylebox("background", "ProgressBar", make_stylebox(p_theme->get_icon(SNAME("GuiProgressBar"), EditorStringName(EditorIcons)), 4, 4, 4, 4, 0, 0, 0, 0));
	p_theme->set_stylebox("fill", "ProgressBar", make_stylebox(p_theme->get_icon(SNAME("GuiProgressFill"), EditorStringName(EditorIcons)), 6, 6, 6, 6, 2, 1, 2, 1));
	p_theme->set_color(SceneStringName(font_color), "ProgressBar", p_config.font_color);
	p_theme->set_color("font_outline_color", "ProgressBar", p_config.font_outline_color);
	p_theme->set_constant("outline_size", "ProgressBar", 0);

	// GraphEdit and related nodes.
	{
		// GraphEdit.

		p_theme->set_stylebox(SceneStringName(panel), "GraphEdit", p_config.tree_panel_style);
		p_theme->set_stylebox("menu_panel", "GraphEdit", make_flat_stylebox(p_config.dark_color_1 * Color(1, 1, 1, 0.6), 4, 2, 4, 2, 3));

		float grid_base_brightness = p_config.dark_theme ? 1.0 : 0.0;
		GraphEdit::GridPattern grid_pattern = (GraphEdit::GridPattern) int(EDITOR_GET("editors/visual_editors/grid_pattern"));
		switch (grid_pattern) {
			case GraphEdit::GRID_PATTERN_LINES:
				p_theme->set_color("grid_major", "GraphEdit", Color(grid_base_brightness, grid_base_brightness, grid_base_brightness, 0.10));
				p_theme->set_color("grid_minor", "GraphEdit", Color(grid_base_brightness, grid_base_brightness, grid_base_brightness, 0.05));
				break;
			case GraphEdit::GRID_PATTERN_DOTS:
				p_theme->set_color("grid_major", "GraphEdit", Color(grid_base_brightness, grid_base_brightness, grid_base_brightness, 0.07));
				p_theme->set_color("grid_minor", "GraphEdit", Color(grid_base_brightness, grid_base_brightness, grid_base_brightness, 0.07));
				break;
			default:
				WARN_PRINT("Unknown grid pattern.");
				break;
		}

		p_theme->set_color("selection_fill", "GraphEdit", p_theme->get_color(SNAME("box_selection_fill_color"), EditorStringName(Editor)));
		p_theme->set_color("selection_stroke", "GraphEdit", p_theme->get_color(SNAME("box_selection_stroke_color"), EditorStringName(Editor)));
		p_theme->set_color("activity", "GraphEdit", p_config.dark_theme ? Color(1, 1, 1) : Color(0, 0, 0));

		p_theme->set_color("connection_hover_tint_color", "GraphEdit", p_config.dark_theme ? Color(0, 0, 0, 0.3) : Color(1, 1, 1, 0.3));
		p_theme->set_color("connection_valid_target_tint_color", "GraphEdit", p_config.dark_theme ? Color(1, 1, 1, 0.4) : Color(0, 0, 0, 0.4));
		p_theme->set_color("connection_rim_color", "GraphEdit", p_config.tree_panel_style->get_bg_color());

		p_theme->set_icon("zoom_out", "GraphEdit", p_theme->get_icon(SNAME("ZoomLess"), EditorStringName(EditorIcons)));
		p_theme->set_icon("zoom_in", "GraphEdit", p_theme->get_icon(SNAME("ZoomMore"), EditorStringName(EditorIcons)));
		p_theme->set_icon("zoom_reset", "GraphEdit", p_theme->get_icon(SNAME("ZoomReset"), EditorStringName(EditorIcons)));
		p_theme->set_icon("grid_toggle", "GraphEdit", p_theme->get_icon(SNAME("GridToggle"), EditorStringName(EditorIcons)));
		p_theme->set_icon("minimap_toggle", "GraphEdit", p_theme->get_icon(SNAME("GridMinimap"), EditorStringName(EditorIcons)));
		p_theme->set_icon("snapping_toggle", "GraphEdit", p_theme->get_icon(SNAME("SnapGrid"), EditorStringName(EditorIcons)));
		p_theme->set_icon("layout", "GraphEdit", p_theme->get_icon(SNAME("GridLayout"), EditorStringName(EditorIcons)));

		// GraphEditMinimap.
		{
			Ref<StyleBoxFlat> style_minimap_bg = make_flat_stylebox(p_config.dark_color_1, 0, 0, 0, 0);
			style_minimap_bg->set_border_color(p_config.dark_color_3);
			style_minimap_bg->set_border_width_all(1);
			p_theme->set_stylebox(SceneStringName(panel), "GraphEditMinimap", style_minimap_bg);

			Ref<StyleBoxFlat> style_minimap_camera;
			Ref<StyleBoxFlat> style_minimap_node;
			if (p_config.dark_theme) {
				style_minimap_camera = make_flat_stylebox(Color(0.65, 0.65, 0.65, 0.2), 0, 0, 0, 0);
				style_minimap_camera->set_border_color(Color(0.65, 0.65, 0.65, 0.45));
				style_minimap_node = make_flat_stylebox(Color(1, 1, 1), 0, 0, 0, 0);
			} else {
				style_minimap_camera = make_flat_stylebox(Color(0.38, 0.38, 0.38, 0.2), 0, 0, 0, 0);
				style_minimap_camera->set_border_color(Color(0.38, 0.38, 0.38, 0.45));
				style_minimap_node = make_flat_stylebox(Color(0, 0, 0), 0, 0, 0, 0);
			}
			style_minimap_camera->set_border_width_all(1);
			style_minimap_node->set_anti_aliased(false);
			p_theme->set_stylebox("camera", "GraphEditMinimap", style_minimap_camera);
			p_theme->set_stylebox("node", "GraphEditMinimap", style_minimap_node);

			const Color minimap_resizer_color = p_config.dark_theme ? Color(1, 1, 1, 0.65) : Color(0, 0, 0, 0.65);
			p_theme->set_icon("resizer", "GraphEditMinimap", p_theme->get_icon(SNAME("GuiResizerTopLeft"), EditorStringName(EditorIcons)));
			p_theme->set_color("resizer_color", "GraphEditMinimap", minimap_resizer_color);
		}

		// GraphElement, GraphNode & GraphFrame.
		{
			const int gn_margin_top = 2;
			const int gn_margin_side = 2;
			const int gn_margin_bottom = 2;

			const int gn_corner_radius = 3;

			const Color gn_bg_color = p_config.dark_theme ? p_config.dark_color_3 : p_config.dark_color_1.lerp(p_config.mono_color, 0.09);
			const Color gn_selected_border_color = p_config.dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);
			const Color gn_frame_bg = gn_bg_color.lerp(p_config.tree_panel_style->get_bg_color(), 0.3);

			const bool high_contrast_borders = p_config.draw_extra_borders && p_config.dark_theme;

			Ref<StyleBoxFlat> gn_panel_style = make_flat_stylebox(gn_frame_bg, gn_margin_side, gn_margin_top, gn_margin_side, gn_margin_bottom, p_config.corner_radius);
			gn_panel_style->set_border_width(SIDE_BOTTOM, 2 * EDSCALE);
			gn_panel_style->set_border_width(SIDE_LEFT, 2 * EDSCALE);
			gn_panel_style->set_border_width(SIDE_RIGHT, 2 * EDSCALE);
			gn_panel_style->set_border_color(high_contrast_borders ? gn_bg_color.lightened(0.2) : gn_bg_color.darkened(0.3));
			gn_panel_style->set_corner_radius_individual(0, 0, gn_corner_radius * EDSCALE, gn_corner_radius * EDSCALE);
			gn_panel_style->set_anti_aliased(true);

			Ref<StyleBoxFlat> gn_panel_selected_style = gn_panel_style->duplicate();
			gn_panel_selected_style->set_bg_color(p_config.dark_theme ? gn_bg_color.lightened(0.15) : gn_bg_color.darkened(0.15));
			gn_panel_selected_style->set_border_width(SIDE_TOP, 0);
			gn_panel_selected_style->set_border_width(SIDE_BOTTOM, 2 * EDSCALE);
			gn_panel_selected_style->set_border_width(SIDE_LEFT, 2 * EDSCALE);
			gn_panel_selected_style->set_border_width(SIDE_RIGHT, 2 * EDSCALE);
			gn_panel_selected_style->set_border_color(gn_selected_border_color);

			const int gn_titlebar_margin_top = 8;
			const int gn_titlebar_margin_side = 12;
			const int gn_titlebar_margin_bottom = 8;

			Ref<StyleBoxFlat> gn_titlebar_style = make_flat_stylebox(gn_bg_color, gn_titlebar_margin_side, gn_titlebar_margin_top, gn_titlebar_margin_side, gn_titlebar_margin_bottom, p_config.corner_radius);
			gn_titlebar_style->set_border_width(SIDE_TOP, 2 * EDSCALE);
			gn_titlebar_style->set_border_width(SIDE_LEFT, 2 * EDSCALE);
			gn_titlebar_style->set_border_width(SIDE_RIGHT, 2 * EDSCALE);
			gn_titlebar_style->set_border_color(high_contrast_borders ? gn_bg_color.lightened(0.2) : gn_bg_color.darkened(0.3));
			gn_titlebar_style->set_expand_margin(SIDE_TOP, 2 * EDSCALE);
			gn_titlebar_style->set_corner_radius_individual(gn_corner_radius * EDSCALE, gn_corner_radius * EDSCALE, 0, 0);
			gn_titlebar_style->set_anti_aliased(true);

			Ref<StyleBoxFlat> gn_titlebar_selected_style = gn_titlebar_style->duplicate();
			gn_titlebar_selected_style->set_border_color(gn_selected_border_color);
			gn_titlebar_selected_style->set_border_width(SIDE_TOP, 2 * EDSCALE);
			gn_titlebar_selected_style->set_border_width(SIDE_LEFT, 2 * EDSCALE);
			gn_titlebar_selected_style->set_border_width(SIDE_RIGHT, 2 * EDSCALE);
			gn_titlebar_selected_style->set_expand_margin(SIDE_TOP, 2 * EDSCALE);

			Color gn_decoration_color = p_config.dark_color_1.inverted();

			// GraphElement.

			p_theme->set_stylebox(SceneStringName(panel), "GraphElement", gn_panel_style);
			p_theme->set_stylebox("panel_selected", "GraphElement", gn_panel_selected_style);
			p_theme->set_stylebox("titlebar", "GraphElement", gn_titlebar_style);
			p_theme->set_stylebox("titlebar_selected", "GraphElement", gn_titlebar_selected_style);

			p_theme->set_color("resizer_color", "GraphElement", gn_decoration_color);
			p_theme->set_icon("resizer", "GraphElement", p_theme->get_icon(SNAME("GuiResizer"), EditorStringName(EditorIcons)));

			// GraphNode.

			Ref<StyleBoxEmpty> gn_slot_style = make_empty_stylebox(12, 0, 12, 0);

			p_theme->set_stylebox(SceneStringName(panel), "GraphNode", gn_panel_style);
			p_theme->set_stylebox("panel_selected", "GraphNode", gn_panel_selected_style);
			p_theme->set_stylebox("titlebar", "GraphNode", gn_titlebar_style);
			p_theme->set_stylebox("titlebar_selected", "GraphNode", gn_titlebar_selected_style);
			p_theme->set_stylebox("slot", "GraphNode", gn_slot_style);

			p_theme->set_color("resizer_color", "GraphNode", gn_decoration_color);

			p_theme->set_constant("port_h_offset", "GraphNode", 1);
			p_theme->set_constant("separation", "GraphNode", 1 * EDSCALE);

			Ref<ImageTexture> port_icon = p_theme->get_icon(SNAME("GuiGraphNodePort"), EditorStringName(EditorIcons));
			// The true size is 24x24 This is necessary for sharp port icons at high zoom levels in GraphEdit (up to ~200%).
			port_icon->set_size_override(Size2(12, 12));
			p_theme->set_icon("port", "GraphNode", port_icon);

			// GraphNode's title Label.
			p_theme->set_type_variation("GraphNodeTitleLabel", "Label");
			p_theme->set_stylebox(CoreStringName(normal), "GraphNodeTitleLabel", make_empty_stylebox(0, 0, 0, 0));
			p_theme->set_color("font_shadow_color", "GraphNodeTitleLabel", p_config.shadow_color);
			p_theme->set_constant("shadow_outline_size", "GraphNodeTitleLabel", 4);
			p_theme->set_constant("shadow_offset_x", "GraphNodeTitleLabel", 0);
			p_theme->set_constant("shadow_offset_y", "GraphNodeTitleLabel", 1);
			p_theme->set_constant("line_spacing", "GraphNodeTitleLabel", 3 * EDSCALE);

			// GraphFrame.

			const int gf_corner_width = 7 * EDSCALE;
			const int gf_border_width = 2 * MAX(1, EDSCALE);

			Ref<StyleBoxFlat> graphframe_sb = make_flat_stylebox(Color(0.0, 0.0, 0.0, 0.2), gn_margin_side, gn_margin_side, gn_margin_side, gn_margin_bottom, gf_corner_width);
			graphframe_sb->set_expand_margin(SIDE_TOP, 38 * EDSCALE);
			graphframe_sb->set_border_width_all(gf_border_width);
			graphframe_sb->set_border_color(high_contrast_borders ? gn_bg_color.lightened(0.2) : gn_bg_color.darkened(0.3));
			graphframe_sb->set_shadow_size(8 * EDSCALE);
			graphframe_sb->set_shadow_color(Color(p_config.shadow_color, p_config.shadow_color.a * 0.25));
			graphframe_sb->set_anti_aliased(true);

			Ref<StyleBoxFlat> graphframe_sb_selected = graphframe_sb->duplicate();
			graphframe_sb_selected->set_border_color(gn_selected_border_color);

			p_theme->set_stylebox(SceneStringName(panel), "GraphFrame", graphframe_sb);
			p_theme->set_stylebox("panel_selected", "GraphFrame", graphframe_sb_selected);
			p_theme->set_stylebox("titlebar", "GraphFrame", make_empty_stylebox(4, 4, 4, 4));
			p_theme->set_stylebox("titlebar_selected", "GraphFrame", make_empty_stylebox(4, 4, 4, 4));
			p_theme->set_color("resizer_color", "GraphFrame", gn_decoration_color);

			// GraphFrame's title Label.
			p_theme->set_type_variation("GraphFrameTitleLabel", "Label");
			p_theme->set_stylebox(CoreStringName(normal), "GraphFrameTitleLabel", memnew(StyleBoxEmpty));
			p_theme->set_font_size(SceneStringName(font_size), "GraphFrameTitleLabel", 22 * EDSCALE);
			p_theme->set_color(SceneStringName(font_color), "GraphFrameTitleLabel", Color(1, 1, 1));
			p_theme->set_color("font_shadow_color", "GraphFrameTitleLabel", Color(0, 0, 0, 0));
			p_theme->set_color("font_outline_color", "GraphFrameTitleLabel", Color(1, 1, 1));
			p_theme->set_constant("shadow_offset_x", "GraphFrameTitleLabel", 1 * EDSCALE);
			p_theme->set_constant("shadow_offset_y", "GraphFrameTitleLabel", 1 * EDSCALE);
			p_theme->set_constant("outline_size", "GraphFrameTitleLabel", 0);
			p_theme->set_constant("shadow_outline_size", "GraphFrameTitleLabel", 1 * EDSCALE);
			p_theme->set_constant("line_spacing", "GraphFrameTitleLabel", 3 * EDSCALE);
		}

		// VisualShader reroute node.
		{
			Ref<StyleBox> vs_reroute_panel_style = make_empty_stylebox();
			Ref<StyleBox> vs_reroute_titlebar_style = vs_reroute_panel_style->duplicate();
			vs_reroute_titlebar_style->set_content_margin_all(16 * EDSCALE);
			p_theme->set_stylebox(SceneStringName(panel), "VSRerouteNode", vs_reroute_panel_style);
			p_theme->set_stylebox("panel_selected", "VSRerouteNode", vs_reroute_panel_style);
			p_theme->set_stylebox("titlebar", "VSRerouteNode", vs_reroute_titlebar_style);
			p_theme->set_stylebox("titlebar_selected", "VSRerouteNode", vs_reroute_titlebar_style);
			p_theme->set_stylebox("slot", "VSRerouteNode", make_empty_stylebox());

			p_theme->set_color("drag_background", "VSRerouteNode", p_config.dark_theme ? Color(0.19, 0.21, 0.24) : Color(0.8, 0.8, 0.8));
			p_theme->set_color("selected_rim_color", "VSRerouteNode", p_config.dark_theme ? Color(1, 1, 1) : Color(0, 0, 0));
		}
	}

	// ColorPicker and related nodes.
	{
		// ColorPicker.

		p_theme->set_constant("margin", "ColorPicker", p_config.base_margin);
		p_theme->set_constant("sv_width", "ColorPicker", 256 * EDSCALE);
		p_theme->set_constant("sv_height", "ColorPicker", 256 * EDSCALE);
		p_theme->set_constant("h_width", "ColorPicker", 30 * EDSCALE);
		p_theme->set_constant("label_width", "ColorPicker", 10 * EDSCALE);
		p_theme->set_constant("center_slider_grabbers", "ColorPicker", 1);

		p_theme->set_icon("screen_picker", "ColorPicker", p_theme->get_icon(SNAME("ColorPick"), EditorStringName(EditorIcons)));
		p_theme->set_icon("shape_circle", "ColorPicker", p_theme->get_icon(SNAME("PickerShapeCircle"), EditorStringName(EditorIcons)));
		p_theme->set_icon("shape_rect", "ColorPicker", p_theme->get_icon(SNAME("PickerShapeRectangle"), EditorStringName(EditorIcons)));
		p_theme->set_icon("shape_rect_wheel", "ColorPicker", p_theme->get_icon(SNAME("PickerShapeRectangleWheel"), EditorStringName(EditorIcons)));
		p_theme->set_icon("add_preset", "ColorPicker", p_theme->get_icon(SNAME("Add"), EditorStringName(EditorIcons)));
		p_theme->set_icon("sample_bg", "ColorPicker", p_theme->get_icon(SNAME("GuiMiniCheckerboard"), EditorStringName(EditorIcons)));
		p_theme->set_icon("sample_revert", "ColorPicker", p_theme->get_icon(SNAME("Reload"), EditorStringName(EditorIcons)));
		p_theme->set_icon("overbright_indicator", "ColorPicker", p_theme->get_icon(SNAME("OverbrightIndicator"), EditorStringName(EditorIcons)));
		p_theme->set_icon("bar_arrow", "ColorPicker", p_theme->get_icon(SNAME("ColorPickerBarArrow"), EditorStringName(EditorIcons)));
		p_theme->set_icon("picker_cursor", "ColorPicker", p_theme->get_icon(SNAME("PickerCursor"), EditorStringName(EditorIcons)));

		// ColorPickerButton.
		p_theme->set_icon("bg", "ColorPickerButton", p_theme->get_icon(SNAME("GuiMiniCheckerboard"), EditorStringName(EditorIcons)));

		// ColorPresetButton.
		p_theme->set_stylebox("preset_fg", "ColorPresetButton", make_flat_stylebox(Color(1, 1, 1), 2, 2, 2, 2, 2));
		p_theme->set_icon("preset_bg", "ColorPresetButton", p_theme->get_icon(SNAME("GuiMiniCheckerboard"), EditorStringName(EditorIcons)));
		p_theme->set_icon("overbright_indicator", "ColorPresetButton", p_theme->get_icon(SNAME("OverbrightIndicator"), EditorStringName(EditorIcons)));
	}
}

void EditorThemeManager::_populate_editor_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config) {
	// Project manager.
	{
		p_theme->set_stylebox("project_list", "ProjectManager", p_config.tree_panel_style);
		p_theme->set_constant("sidebar_button_icon_separation", "ProjectManager", int(6 * EDSCALE));
		p_theme->set_icon("browse_folder", "ProjectManager", p_theme->get_icon(SNAME("FolderBrowse"), EditorStringName(EditorIcons)));
		p_theme->set_icon("browse_file", "ProjectManager", p_theme->get_icon(SNAME("FileBrowse"), EditorStringName(EditorIcons)));

		// ProjectTag.
		{
			p_theme->set_type_variation("ProjectTag", "Button");

			Ref<StyleBoxFlat> tag = p_config.button_style->duplicate();
			tag->set_bg_color(p_config.dark_theme ? tag->get_bg_color().lightened(0.2) : tag->get_bg_color().darkened(0.2));
			tag->set_corner_radius(CORNER_TOP_LEFT, 0);
			tag->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
			tag->set_corner_radius(CORNER_TOP_RIGHT, 4);
			tag->set_corner_radius(CORNER_BOTTOM_RIGHT, 4);
			p_theme->set_stylebox(CoreStringName(normal), "ProjectTag", tag);

			tag = p_config.button_style_hover->duplicate();
			tag->set_corner_radius(CORNER_TOP_LEFT, 0);
			tag->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
			tag->set_corner_radius(CORNER_TOP_RIGHT, 4);
			tag->set_corner_radius(CORNER_BOTTOM_RIGHT, 4);
			p_theme->set_stylebox(SceneStringName(hover), "ProjectTag", tag);

			tag = p_config.button_style_pressed->duplicate();
			tag->set_corner_radius(CORNER_TOP_LEFT, 0);
			tag->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
			tag->set_corner_radius(CORNER_TOP_RIGHT, 4);
			tag->set_corner_radius(CORNER_BOTTOM_RIGHT, 4);
			p_theme->set_stylebox(SceneStringName(pressed), "ProjectTag", tag);
		}
	}

	// Editor and main screen.
	{
		// Editor background.
		Color background_color_opaque = p_config.dark_color_2;
		background_color_opaque.a = 1.0;
		p_theme->set_color("background", EditorStringName(Editor), background_color_opaque);
		p_theme->set_stylebox("Background", EditorStringName(EditorStyles), make_flat_stylebox(background_color_opaque, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin));

		Ref<StyleBoxFlat> editor_panel_foreground = p_config.base_style->duplicate();
		editor_panel_foreground->set_corner_radius_all(0);
		p_theme->set_stylebox("PanelForeground", EditorStringName(EditorStyles), editor_panel_foreground);

		// Editor focus.
		p_theme->set_stylebox("Focus", EditorStringName(EditorStyles), p_config.button_style_focus);

		Ref<StyleBoxFlat> style_widget_focus_viewport = p_config.button_style_focus->duplicate();
		// Make the focus outline appear to be flush with the buttons it's focusing, so not draw on top of the content.
		style_widget_focus_viewport->set_expand_margin_all(2);
		// Use a less opaque color to be less distracting for the 2D and 3D editor viewports.
		style_widget_focus_viewport->set_border_color(p_config.accent_color * Color(1, 1, 1, 0.5));
		p_theme->set_stylebox("FocusViewport", EditorStringName(EditorStyles), style_widget_focus_viewport);

		Ref<StyleBoxFlat> style_widget_scroll_container = p_config.button_style_focus->duplicate();
		// Make the focus outline appear to be flush with the buttons it's focusing, so not draw on top of the content.
		style_widget_scroll_container->set_expand_margin_all(4);
		p_theme->set_stylebox("focus", "ScrollContainer", style_widget_scroll_container);

		// This stylebox is used in 3d and 2d viewports (no borders).
		Ref<StyleBoxFlat> style_content_panel_vp = p_config.content_panel_style->duplicate();
		style_content_panel_vp->set_content_margin_individual(p_config.border_width * 2, p_config.base_margin * EDSCALE, p_config.border_width * 2, p_config.border_width * 2);
		p_theme->set_stylebox("Content", EditorStringName(EditorStyles), style_content_panel_vp);

		// 3D/Spatial editor.
		Ref<StyleBoxFlat> style_info_3d_viewport = p_config.base_style->duplicate();
		style_info_3d_viewport->set_bg_color(style_info_3d_viewport->get_bg_color() * Color(1, 1, 1, 0.5));
		style_info_3d_viewport->set_border_width_all(0);
		p_theme->set_stylebox("Information3dViewport", EditorStringName(EditorStyles), style_info_3d_viewport);

		// 2D and 3D contextual toolbar.
		// Use a custom stylebox to make contextual menu items stand out from the rest.
		// This helps with editor usability as contextual menu items change when selecting nodes,
		// even though it may not be immediately obvious at first.
		Ref<StyleBoxFlat> toolbar_stylebox = memnew(StyleBoxFlat);
		toolbar_stylebox->set_bg_color(p_config.accent_color * Color(1, 1, 1, 0.1));
		toolbar_stylebox->set_anti_aliased(false);
		// Add an underline to the StyleBox, but prevent its minimum vertical size from changing.
		toolbar_stylebox->set_border_color(p_config.accent_color);
		toolbar_stylebox->set_border_width(SIDE_BOTTOM, Math::round(2 * EDSCALE));
		toolbar_stylebox->set_content_margin(SIDE_BOTTOM, 0);
		toolbar_stylebox->set_expand_margin_individual(4 * EDSCALE, 2 * EDSCALE, 4 * EDSCALE, 4 * EDSCALE);
		p_theme->set_stylebox("ContextualToolbar", EditorStringName(EditorStyles), toolbar_stylebox);

		// Script editor.
		p_theme->set_stylebox("ScriptEditorPanel", EditorStringName(EditorStyles), make_empty_stylebox(p_config.base_margin, 0, p_config.base_margin, p_config.base_margin));
		p_theme->set_stylebox("ScriptEditorPanelFloating", EditorStringName(EditorStyles), make_empty_stylebox(0, 0, 0, 0));
		p_theme->set_stylebox("ScriptEditor", EditorStringName(EditorStyles), make_empty_stylebox(0, 0, 0, 0));

		// Game view.
		p_theme->set_type_variation("GamePanel", "Panel");
		Ref<StyleBoxFlat> game_panel = p_theme->get_stylebox(SceneStringName(panel), SNAME("Panel"))->duplicate();
		game_panel->set_corner_radius_all(0);
		p_theme->set_stylebox(SceneStringName(panel), "GamePanel", game_panel);

		// Main menu.
		Ref<StyleBoxFlat> menu_transparent_style = p_config.button_style->duplicate();
		menu_transparent_style->set_bg_color(Color(1, 1, 1, 0));
		menu_transparent_style->set_border_width_all(0);
		Ref<StyleBoxFlat> main_screen_button_hover = p_config.button_style_hover->duplicate();
		for (int i = 0; i < 4; i++) {
			menu_transparent_style->set_content_margin((Side)i, p_config.button_style->get_content_margin((Side)i));
			main_screen_button_hover->set_content_margin((Side)i, p_config.button_style_hover->get_content_margin((Side)i));
		}
		p_theme->set_stylebox(CoreStringName(normal), "MainScreenButton", menu_transparent_style);
		p_theme->set_stylebox("normal_mirrored", "MainScreenButton", menu_transparent_style);
		p_theme->set_stylebox(SceneStringName(pressed), "MainScreenButton", menu_transparent_style);
		p_theme->set_stylebox("pressed_mirrored", "MainScreenButton", menu_transparent_style);
		p_theme->set_stylebox(SceneStringName(hover), "MainScreenButton", main_screen_button_hover);
		p_theme->set_stylebox("hover_mirrored", "MainScreenButton", main_screen_button_hover);
		p_theme->set_stylebox("hover_pressed", "MainScreenButton", main_screen_button_hover);
		p_theme->set_stylebox("hover_pressed_mirrored", "MainScreenButton", main_screen_button_hover);

		p_theme->set_type_variation("MainMenuBar", "FlatMenuButton");
		p_theme->set_stylebox(CoreStringName(normal), "MainMenuBar", menu_transparent_style);
		p_theme->set_stylebox(SceneStringName(pressed), "MainMenuBar", main_screen_button_hover);
		p_theme->set_stylebox(SceneStringName(hover), "MainMenuBar", main_screen_button_hover);
		p_theme->set_stylebox("hover_pressed", "MainMenuBar", main_screen_button_hover);

		// Run bar.
		p_theme->set_type_variation("RunBarButton", "FlatMenuButton");
		p_theme->set_stylebox("disabled", "RunBarButton", menu_transparent_style);
		p_theme->set_stylebox(SceneStringName(pressed), "RunBarButton", menu_transparent_style);

		// Bottom panel.
		Ref<StyleBoxFlat> style_bottom_panel = p_config.content_panel_style->duplicate();
		style_bottom_panel->set_corner_radius_all(p_config.corner_radius * EDSCALE);
		p_theme->set_stylebox("BottomPanel", EditorStringName(EditorStyles), style_bottom_panel);
		p_theme->set_type_variation("BottomPanelButton", "FlatMenuButton");
		p_theme->set_stylebox(CoreStringName(normal), "BottomPanelButton", menu_transparent_style);
		p_theme->set_stylebox(SceneStringName(pressed), "BottomPanelButton", menu_transparent_style);
		p_theme->set_stylebox("hover_pressed", "BottomPanelButton", main_screen_button_hover);
		p_theme->set_stylebox(SceneStringName(hover), "BottomPanelButton", main_screen_button_hover);
		// Don't tint the icon even when in "pressed" state.
		p_theme->set_color("icon_pressed_color", "BottomPanelButton", Color(1, 1, 1, 1));
		Color icon_hover_color = p_config.icon_normal_color * (p_config.dark_theme ? 1.15 : 1.0);
		icon_hover_color.a = 1.0;
		p_theme->set_color("icon_hover_color", "BottomPanelButton", icon_hover_color);
		p_theme->set_color("icon_hover_pressed_color", "BottomPanelButton", icon_hover_color);
	}

	// Editor GUI widgets.
	{
		// EditorSpinSlider.
		p_theme->set_color("label_color", "EditorSpinSlider", p_config.font_color);
		p_theme->set_color("read_only_label_color", "EditorSpinSlider", p_config.font_readonly_color);

		Ref<StyleBoxFlat> editor_spin_label_bg = p_config.base_style->duplicate();
		editor_spin_label_bg->set_bg_color(p_config.dark_color_3);
		editor_spin_label_bg->set_border_width_all(0);
		p_theme->set_stylebox("label_bg", "EditorSpinSlider", editor_spin_label_bg);

		// TODO Use separate arrows instead like on SpinBox. Planned for a different PR.
		p_theme->set_icon("updown", "EditorSpinSlider", p_theme->get_icon(SNAME("GuiSpinboxUpdown"), EditorStringName(EditorIcons)));
		p_theme->set_icon("updown_disabled", "EditorSpinSlider", p_theme->get_icon(SNAME("GuiSpinboxUpdownDisabled"), EditorStringName(EditorIcons)));

		// Launch Pad and Play buttons.
		Ref<StyleBoxFlat> style_launch_pad = make_flat_stylebox(p_config.dark_color_1, 2 * EDSCALE, 0, 2 * EDSCALE, 0, p_config.corner_radius);
		style_launch_pad->set_corner_radius_all(p_config.corner_radius * EDSCALE);
		p_theme->set_stylebox("LaunchPadNormal", EditorStringName(EditorStyles), style_launch_pad);
		Ref<StyleBoxFlat> style_launch_pad_movie = style_launch_pad->duplicate();
		style_launch_pad_movie->set_bg_color(p_config.accent_color * Color(1, 1, 1, 0.1));
		style_launch_pad_movie->set_border_color(p_config.accent_color);
		style_launch_pad_movie->set_border_width_all(Math::round(2 * EDSCALE));
		p_theme->set_stylebox("LaunchPadMovieMode", EditorStringName(EditorStyles), style_launch_pad_movie);

		p_theme->set_stylebox("MovieWriterButtonNormal", EditorStringName(EditorStyles), make_empty_stylebox(0, 0, 0, 0));
		Ref<StyleBoxFlat> style_write_movie_button = p_config.button_style_pressed->duplicate();
		style_write_movie_button->set_bg_color(p_config.accent_color);
		style_write_movie_button->set_corner_radius_all(p_config.corner_radius * EDSCALE);
		style_write_movie_button->set_content_margin(SIDE_TOP, 0);
		style_write_movie_button->set_content_margin(SIDE_BOTTOM, 0);
		style_write_movie_button->set_content_margin(SIDE_LEFT, 0);
		style_write_movie_button->set_content_margin(SIDE_RIGHT, 0);
		style_write_movie_button->set_expand_margin(SIDE_RIGHT, 2 * EDSCALE);
		p_theme->set_stylebox("MovieWriterButtonPressed", EditorStringName(EditorStyles), style_write_movie_button);

		// Movie writer button colors.
		p_theme->set_color("movie_writer_icon_normal", EditorStringName(EditorStyles), Color(1, 1, 1, 0.7));
		p_theme->set_color("movie_writer_icon_pressed", EditorStringName(EditorStyles), Color(0, 0, 0, 0.84));
		p_theme->set_color("movie_writer_icon_hover", EditorStringName(EditorStyles), Color(1, 1, 1, 0.9));
		p_theme->set_color("movie_writer_icon_hover_pressed", EditorStringName(EditorStyles), Color(0, 0, 0, 0.84));
	}

	// Standard GUI variations.
	{
		// Custom theme type for MarginContainer with 4px margins.
		p_theme->set_type_variation("MarginContainer4px", "MarginContainer");
		p_theme->set_constant("margin_left", "MarginContainer4px", 4 * EDSCALE);
		p_theme->set_constant("margin_top", "MarginContainer4px", 4 * EDSCALE);
		p_theme->set_constant("margin_right", "MarginContainer4px", 4 * EDSCALE);
		p_theme->set_constant("margin_bottom", "MarginContainer4px", 4 * EDSCALE);

		// Header LinkButton variation.
		p_theme->set_type_variation("HeaderSmallLink", "LinkButton");
		p_theme->set_font(SceneStringName(font), "HeaderSmallLink", p_theme->get_font(SceneStringName(font), SNAME("HeaderSmall")));
		p_theme->set_font_size(SceneStringName(font_size), "HeaderSmallLink", p_theme->get_font_size(SceneStringName(font_size), SNAME("HeaderSmall")));

		// Flat button variations.
		{
			Ref<StyleBoxEmpty> style_flat_button = make_empty_stylebox();
			Ref<StyleBoxFlat> style_flat_button_hover = p_config.button_style_hover->duplicate();
			Ref<StyleBoxFlat> style_flat_button_pressed = p_config.button_style_pressed->duplicate();

			for (int i = 0; i < 4; i++) {
				style_flat_button->set_content_margin((Side)i, p_config.button_style->get_content_margin((Side)i));
				style_flat_button_hover->set_content_margin((Side)i, p_config.button_style->get_content_margin((Side)i));
				style_flat_button_pressed->set_content_margin((Side)i, p_config.button_style->get_content_margin((Side)i));
			}
			Color flat_pressed_color = p_config.dark_color_1.lightened(0.24).lerp(p_config.accent_color, 0.2) * Color(0.8, 0.8, 0.8, 0.85);
			if (p_config.dark_theme) {
				flat_pressed_color = p_config.dark_color_1.lerp(p_config.accent_color, 0.12) * Color(0.6, 0.6, 0.6, 0.85);
			}
			style_flat_button_pressed->set_bg_color(flat_pressed_color);

			p_theme->set_stylebox(CoreStringName(normal), SceneStringName(FlatButton), style_flat_button);
			p_theme->set_stylebox(SceneStringName(hover), SceneStringName(FlatButton), style_flat_button_hover);
			p_theme->set_stylebox(SceneStringName(pressed), SceneStringName(FlatButton), style_flat_button_pressed);
			p_theme->set_stylebox("disabled", SceneStringName(FlatButton), style_flat_button);

			p_theme->set_stylebox(CoreStringName(normal), "FlatMenuButton", style_flat_button);
			p_theme->set_stylebox(SceneStringName(hover), "FlatMenuButton", style_flat_button_hover);
			p_theme->set_stylebox(SceneStringName(pressed), "FlatMenuButton", style_flat_button_pressed);
			p_theme->set_stylebox("disabled", "FlatMenuButton", style_flat_button);

			// Variation for Editor Log filter buttons.

			p_theme->set_type_variation("EditorLogFilterButton", "Button");
			// When pressed, don't tint the icons with the accent color, just leave them normal.
			p_theme->set_color("icon_pressed_color", "EditorLogFilterButton", p_config.icon_normal_color);
			// When unpressed, dim the icons.
			Color icon_normal_color = Color(p_config.icon_normal_color, (p_config.dark_theme ? 0.4 : 0.8));
			p_theme->set_color("icon_normal_color", "EditorLogFilterButton", icon_normal_color);
			Color icon_hover_color = p_config.icon_normal_color * (p_config.dark_theme ? 1.15 : 1.0);
			icon_hover_color.a = 1.0;
			p_theme->set_color("icon_hover_color", "EditorLogFilterButton", icon_hover_color);
			p_theme->set_color("icon_hover_pressed_color", "EditorLogFilterButton", icon_hover_color);

			// When pressed, add a small bottom border to the buttons to better show their active state,
			// similar to active tabs.
			Ref<StyleBoxFlat> editor_log_button_pressed = style_flat_button_pressed->duplicate();
			editor_log_button_pressed->set_border_width(SIDE_BOTTOM, 2 * EDSCALE);
			editor_log_button_pressed->set_border_color(p_config.accent_color);
			if (!p_config.dark_theme) {
				editor_log_button_pressed->set_bg_color(flat_pressed_color.lightened(0.5));
			}
			p_theme->set_stylebox(CoreStringName(normal), "EditorLogFilterButton", style_flat_button);
			p_theme->set_stylebox(SceneStringName(hover), "EditorLogFilterButton", style_flat_button_hover);
			p_theme->set_stylebox(SceneStringName(pressed), "EditorLogFilterButton", editor_log_button_pressed);
		}

		// Buttons styles that stand out against the panel background (e.g. AssetLib).
		{
			p_theme->set_type_variation("PanelBackgroundButton", "Button");

			Ref<StyleBoxFlat> panel_button_style = p_config.button_style->duplicate();
			panel_button_style->set_bg_color(p_config.base_color.lerp(p_config.mono_color, 0.08));

			Ref<StyleBoxFlat> panel_button_style_hover = p_config.button_style_hover->duplicate();
			panel_button_style_hover->set_bg_color(p_config.base_color.lerp(p_config.mono_color, 0.16));

			Ref<StyleBoxFlat> panel_button_style_pressed = p_config.button_style_pressed->duplicate();
			panel_button_style_pressed->set_bg_color(p_config.base_color.lerp(p_config.mono_color, 0.20));

			Ref<StyleBoxFlat> panel_button_style_disabled = p_config.button_style_disabled->duplicate();
			panel_button_style_disabled->set_bg_color(p_config.disabled_bg_color);

			p_theme->set_stylebox(CoreStringName(normal), "PanelBackgroundButton", panel_button_style);
			p_theme->set_stylebox(SceneStringName(hover), "PanelBackgroundButton", panel_button_style_hover);
			p_theme->set_stylebox(SceneStringName(pressed), "PanelBackgroundButton", panel_button_style_pressed);
			p_theme->set_stylebox("disabled", "PanelBackgroundButton", panel_button_style_disabled);
		}

		// Top bar selectors.
		{
			p_theme->set_type_variation("TopBarOptionButton", "OptionButton");
			p_theme->set_font(SceneStringName(font), "TopBarOptionButton", p_theme->get_font(SNAME("bold"), EditorStringName(EditorFonts)));
			p_theme->set_font_size(SceneStringName(font_size), "TopBarOptionButton", p_theme->get_font_size(SNAME("bold_size"), EditorStringName(EditorFonts)));
		}

		// Complex editor windows.
		{
			Ref<StyleBoxFlat> style_complex_window = p_config.window_style->duplicate();
			style_complex_window->set_bg_color(p_config.dark_color_2);
			style_complex_window->set_border_color(p_config.dark_color_2);
			p_theme->set_stylebox(SceneStringName(panel), "EditorSettingsDialog", style_complex_window);
			p_theme->set_stylebox(SceneStringName(panel), "ProjectSettingsEditor", style_complex_window);
			p_theme->set_stylebox(SceneStringName(panel), "EditorAbout", style_complex_window);
		}

		// InspectorActionButton.
		{
			p_theme->set_type_variation("InspectorActionButton", "Button");

			const float action_extra_margin = 32 * EDSCALE;
			p_theme->set_constant("h_separation", "InspectorActionButton", action_extra_margin);

			Color color_inspector_action = p_config.dark_color_1.lerp(p_config.mono_color, 0.12);
			color_inspector_action.a = 0.5;
			Ref<StyleBoxFlat> style_inspector_action = p_config.button_style->duplicate();
			style_inspector_action->set_bg_color(color_inspector_action);
			style_inspector_action->set_content_margin(SIDE_RIGHT, action_extra_margin);
			p_theme->set_stylebox(CoreStringName(normal), "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style->duplicate();
			style_inspector_action->set_bg_color(color_inspector_action);
			style_inspector_action->set_content_margin(SIDE_LEFT, action_extra_margin);
			p_theme->set_stylebox("normal_mirrored", "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_hover->duplicate();
			style_inspector_action->set_content_margin(SIDE_RIGHT, action_extra_margin);
			p_theme->set_stylebox(SceneStringName(hover), "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_hover->duplicate();
			style_inspector_action->set_content_margin(SIDE_LEFT, action_extra_margin);
			p_theme->set_stylebox("hover_mirrored", "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_pressed->duplicate();
			style_inspector_action->set_content_margin(SIDE_RIGHT, action_extra_margin);
			p_theme->set_stylebox(SceneStringName(pressed), "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_pressed->duplicate();
			style_inspector_action->set_content_margin(SIDE_LEFT, action_extra_margin);
			p_theme->set_stylebox("pressed_mirrored", "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_disabled->duplicate();
			style_inspector_action->set_content_margin(SIDE_RIGHT, action_extra_margin);
			p_theme->set_stylebox("disabled", "InspectorActionButton", style_inspector_action);

			style_inspector_action = p_config.button_style_disabled->duplicate();
			style_inspector_action->set_content_margin(SIDE_LEFT, action_extra_margin);
			p_theme->set_stylebox("disabled_mirrored", "InspectorActionButton", style_inspector_action);
		}

		// Buttons in material previews.
		{
			const Color dim_light_color = p_config.icon_normal_color.darkened(0.24);
			const Color dim_light_highlighted_color = p_config.icon_normal_color.darkened(0.18);
			Ref<StyleBox> sb_empty_borderless = make_empty_stylebox();

			p_theme->set_type_variation("PreviewLightButton", "Button");
			// When pressed, don't use the accent color tint. When unpressed, dim the icon.
			p_theme->set_color("icon_normal_color", "PreviewLightButton", dim_light_color);
			p_theme->set_color("icon_focus_color", "PreviewLightButton", dim_light_color);
			p_theme->set_color("icon_pressed_color", "PreviewLightButton", p_config.icon_normal_color);
			p_theme->set_color("icon_hover_pressed_color", "PreviewLightButton", p_config.icon_normal_color);
			// Unpressed icon is dim, so use a dim highlight.
			p_theme->set_color("icon_hover_color", "PreviewLightButton", dim_light_highlighted_color);

			p_theme->set_stylebox(CoreStringName(normal), "PreviewLightButton", sb_empty_borderless);
			p_theme->set_stylebox(SceneStringName(hover), "PreviewLightButton", sb_empty_borderless);
			p_theme->set_stylebox("focus", "PreviewLightButton", sb_empty_borderless);
			p_theme->set_stylebox(SceneStringName(pressed), "PreviewLightButton", sb_empty_borderless);
		}

		// TabContainerOdd variation.
		{
			// Can be used on tabs against the base color background (e.g. nested tabs).
			p_theme->set_type_variation("TabContainerOdd", "TabContainer");

			Ref<StyleBoxFlat> style_tab_selected_odd = p_theme->get_stylebox(SNAME("tab_selected"), SNAME("TabContainer"))->duplicate();
			style_tab_selected_odd->set_bg_color(p_config.disabled_bg_color);
			p_theme->set_stylebox("tab_selected", "TabContainerOdd", style_tab_selected_odd);

			Ref<StyleBoxFlat> style_content_panel_odd = p_config.content_panel_style->duplicate();
			style_content_panel_odd->set_bg_color(p_config.disabled_bg_color);
			p_theme->set_stylebox(SceneStringName(panel), "TabContainerOdd", style_content_panel_odd);
		}

		// EditorValidationPanel.
		p_theme->set_stylebox(SceneStringName(panel), "EditorValidationPanel", p_config.tree_panel_style);

		// Secondary trees and item lists.
		p_theme->set_type_variation("TreeSecondary", "Tree");
		p_theme->set_type_variation("ItemListSecondary", "ItemList");
	}

	// Editor inspector.
	{
		// Vertical separation between inspector categories and sections.
		p_theme->set_constant("v_separation", "EditorInspector", 0);

		// EditorProperty.

		Ref<StyleBoxFlat> style_property_bg = p_config.base_style->duplicate();
		style_property_bg->set_bg_color(p_config.highlight_color);
		style_property_bg->set_border_width_all(0);

		Ref<StyleBoxFlat> style_property_child_bg = p_config.base_style->duplicate();
		style_property_child_bg->set_bg_color(p_config.dark_color_2);
		style_property_child_bg->set_border_width_all(0);

		p_theme->set_stylebox("bg", "EditorProperty", memnew(StyleBoxEmpty));
		p_theme->set_stylebox("bg_selected", "EditorProperty", style_property_bg);
		p_theme->set_stylebox("child_bg", "EditorProperty", style_property_child_bg);
		p_theme->set_constant("font_offset", "EditorProperty", 8 * EDSCALE);
		p_theme->set_constant("v_separation", "EditorProperty", p_config.increased_margin * EDSCALE);

		const Color property_color = p_config.font_color.lerp(Color(0.5, 0.5, 0.5), 0.5);
		const Color readonly_color = property_color.lerp(p_config.dark_theme ? Color(0, 0, 0) : Color(1, 1, 1), 0.25);
		const Color readonly_warning_color = p_config.error_color.lerp(p_config.dark_theme ? Color(0, 0, 0) : Color(1, 1, 1), 0.25);

		p_theme->set_color("property_color", "EditorProperty", property_color);
		p_theme->set_color("readonly_color", "EditorProperty", readonly_color);
		p_theme->set_color("warning_color", "EditorProperty", p_config.warning_color);
		p_theme->set_color("readonly_warning_color", "EditorProperty", readonly_warning_color);

		Ref<StyleBoxFlat> style_property_group_note = p_config.base_style->duplicate();
		Color property_group_note_color = p_config.accent_color;
		property_group_note_color.a = 0.1;
		style_property_group_note->set_bg_color(property_group_note_color);
		p_theme->set_stylebox("bg_group_note", "EditorProperty", style_property_group_note);

		// EditorInspectorSection.

		Color inspector_section_color = p_config.font_color.lerp(Color(0.5, 0.5, 0.5), 0.35);
		p_theme->set_color(SceneStringName(font_color), "EditorInspectorSection", inspector_section_color);

		Color inspector_indent_color = p_config.accent_color;
		inspector_indent_color.a = 0.2;
		Ref<StyleBoxFlat> inspector_indent_style = make_flat_stylebox(inspector_indent_color, 2.0 * EDSCALE, 0, 2.0 * EDSCALE, 0);
		p_theme->set_stylebox("indent_box", "EditorInspectorSection", inspector_indent_style);
		p_theme->set_constant("indent_size", "EditorInspectorSection", 6.0 * EDSCALE);
		p_theme->set_constant("h_separation", "EditorInspectorSection", 2.0 * EDSCALE);

		Color prop_category_color = p_config.dark_color_1.lerp(p_config.mono_color, 0.12);
		Color prop_section_color = p_config.dark_color_1.lerp(p_config.mono_color, 0.09);
		Color prop_subsection_color = p_config.dark_color_1.lerp(p_config.mono_color, 0.06);

		p_theme->set_color("prop_category", EditorStringName(Editor), prop_category_color);
		p_theme->set_color("prop_section", EditorStringName(Editor), prop_section_color);
		p_theme->set_color("prop_subsection", EditorStringName(Editor), prop_subsection_color);
#ifndef DISABLE_DEPRECATED // Used before 4.3.
		p_theme->set_color("property_color", EditorStringName(Editor), prop_category_color);
#endif

		// EditorInspectorCategory.

		Ref<StyleBoxFlat> category_bg = p_config.base_style->duplicate();
		category_bg->set_bg_color(prop_category_color);
		category_bg->set_border_color(prop_category_color);
		category_bg->set_content_margin_all(0);
		p_theme->set_stylebox("bg", "EditorInspectorCategory", category_bg);

		p_theme->set_constant("inspector_margin", EditorStringName(Editor), 12 * EDSCALE);

		// Colored EditorProperty.
		for (int i = 0; i < 16; i++) {
			Color si_base_color = p_config.accent_color;

			float hue_rotate = (i * 2 % 16) / 16.0;
			si_base_color.set_hsv(Math::fmod(float(si_base_color.get_h() + hue_rotate), float(1.0)), si_base_color.get_s(), si_base_color.get_v());
			si_base_color = p_config.accent_color.lerp(si_base_color, p_config.subresource_hue_tint);

			// Sub-inspector background.
			Ref<StyleBoxFlat> sub_inspector_bg = p_config.base_style->duplicate();
			sub_inspector_bg->set_bg_color(p_config.dark_color_1.lerp(si_base_color, 0.08));
			sub_inspector_bg->set_border_width_all(2 * EDSCALE);
			sub_inspector_bg->set_border_color(si_base_color * Color(0.7, 0.7, 0.7, 0.8));
			sub_inspector_bg->set_content_margin_all(4 * EDSCALE);
			sub_inspector_bg->set_corner_radius(CORNER_TOP_LEFT, 0);
			sub_inspector_bg->set_corner_radius(CORNER_TOP_RIGHT, 0);

			p_theme->set_stylebox("sub_inspector_bg" + itos(i + 1), EditorStringName(EditorStyles), sub_inspector_bg);

			// EditorProperty background while it has a sub-inspector open.
			Ref<StyleBoxFlat> bg_color = make_flat_stylebox(si_base_color * Color(0.7, 0.7, 0.7, 0.8), 0, 0, 0, 0, p_config.corner_radius);
			bg_color->set_anti_aliased(false);
			bg_color->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
			bg_color->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);

			p_theme->set_stylebox("sub_inspector_property_bg" + itos(i + 1), EditorStringName(EditorStyles), bg_color);

			// Dictionary editor add item.
			// Expand to the left and right by 4px to compensate for the dictionary editor margins.

			Color style_dictionary_bg_color = p_config.dark_color_3.lerp(si_base_color, 0.08);
			Ref<StyleBoxFlat> style_dictionary_add_item = make_flat_stylebox(style_dictionary_bg_color, 0, 4, 0, 4, p_config.corner_radius);
			style_dictionary_add_item->set_expand_margin(SIDE_LEFT, 2 * EDSCALE);
			style_dictionary_add_item->set_expand_margin(SIDE_RIGHT, 2 * EDSCALE);
			p_theme->set_stylebox("DictionaryAddItem" + itos(i + 1), EditorStringName(EditorStyles), style_dictionary_add_item);
		}
		Color si_base_color = p_config.accent_color;

		// Sub-inspector background.
		Ref<StyleBoxFlat> sub_inspector_bg = p_config.base_style->duplicate();
		sub_inspector_bg->set_bg_color(Color(1, 1, 1, 0));
		sub_inspector_bg->set_border_width_all(2 * EDSCALE);
		sub_inspector_bg->set_border_color(p_config.dark_color_1.lerp(si_base_color, 0.15));
		sub_inspector_bg->set_content_margin_all(4 * EDSCALE);
		sub_inspector_bg->set_corner_radius(CORNER_TOP_LEFT, 0);
		sub_inspector_bg->set_corner_radius(CORNER_TOP_RIGHT, 0);

		p_theme->set_stylebox("sub_inspector_bg0", EditorStringName(EditorStyles), sub_inspector_bg);

		// Sub-inspector background no border.

		Ref<StyleBoxFlat> sub_inspector_bg_no_border = p_config.base_style->duplicate();
		sub_inspector_bg_no_border->set_content_margin_all(2 * EDSCALE);
		sub_inspector_bg_no_border->set_bg_color(p_config.dark_color_2.lerp(p_config.dark_color_3, 0.15));
		p_theme->set_stylebox("sub_inspector_bg_no_border", EditorStringName(EditorStyles), sub_inspector_bg_no_border);

		// EditorProperty background while it has a sub-inspector open.
		Ref<StyleBoxFlat> bg_color = make_flat_stylebox(p_config.dark_color_1.lerp(si_base_color, 0.15), 0, 0, 0, 0, p_config.corner_radius);
		bg_color->set_anti_aliased(false);
		bg_color->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
		bg_color->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);

		p_theme->set_stylebox("sub_inspector_property_bg0", EditorStringName(EditorStyles), bg_color);

		p_theme->set_color("sub_inspector_property_color", EditorStringName(EditorStyles), p_config.dark_theme ? Color(1, 1, 1, 1) : Color(0, 0, 0, 1));

		// Dictionary editor.

		// Expand to the left and right by 4px to compensate for the dictionary editor margins.
		Ref<StyleBoxFlat> style_dictionary_add_item = make_flat_stylebox(prop_subsection_color, 0, 4, 0, 4, p_config.corner_radius);
		style_dictionary_add_item->set_expand_margin(SIDE_LEFT, 2 * EDSCALE);
		style_dictionary_add_item->set_expand_margin(SIDE_RIGHT, 2 * EDSCALE);
		p_theme->set_stylebox("DictionaryAddItem0", EditorStringName(EditorStyles), style_dictionary_add_item);
	}

	// Animation Editor.
	{
		// Timeline general.
		p_theme->set_constant("timeline_v_separation", "AnimationTrackEditor", 0);
		p_theme->set_constant("track_v_separation", "AnimationTrackEditor", 0);

		// AnimationTimelineEdit.
		// "primary" is used for integer timeline values, "secondary" for decimals.

		Ref<StyleBoxFlat> style_time_unavailable = make_flat_stylebox(p_config.dark_color_2, 0, 0, 0, 0, 0);
		Ref<StyleBoxFlat> style_time_available = make_flat_stylebox(p_config.font_color * Color(1, 1, 1, 0.2), 0, 0, 0, 0, 0);

		p_theme->set_stylebox("time_unavailable", "AnimationTimelineEdit", style_time_unavailable);
		p_theme->set_stylebox("time_available", "AnimationTimelineEdit", style_time_available);

		p_theme->set_color("v_line_primary_color", "AnimationTimelineEdit", p_config.font_color * Color(1, 1, 1, 0.2));
		p_theme->set_color("v_line_secondary_color", "AnimationTimelineEdit", p_config.font_color * Color(1, 1, 1, 0.2));
		p_theme->set_color("h_line_color", "AnimationTimelineEdit", p_config.font_color * Color(1, 1, 1, 0.2));
		p_theme->set_color("font_primary_color", "AnimationTimelineEdit", p_config.font_color);
		p_theme->set_color("font_secondary_color", "AnimationTimelineEdit", p_config.font_color * Color(1, 1, 1, 0.5));

		p_theme->set_constant("v_line_primary_margin", "AnimationTimelineEdit", 0);
		p_theme->set_constant("v_line_secondary_margin", "AnimationTimelineEdit", 0);
		p_theme->set_constant("v_line_primary_width", "AnimationTimelineEdit", 1 * EDSCALE);
		p_theme->set_constant("v_line_secondary_width", "AnimationTimelineEdit", 1 * EDSCALE);
		p_theme->set_constant("text_primary_margin", "AnimationTimelineEdit", 3 * EDSCALE);
		p_theme->set_constant("text_secondary_margin", "AnimationTimelineEdit", 3 * EDSCALE);

		// AnimationTrackEdit.

		Ref<StyleBoxFlat> style_animation_track_odd = make_flat_stylebox(Color(0.5, 0.5, 0.5, 0.05), 0, 0, 0, 0, p_config.corner_radius);
		Ref<StyleBoxFlat> style_animation_track_hover = make_flat_stylebox(Color(0.5, 0.5, 0.5, 0.1), 0, 0, 0, 0, p_config.corner_radius);

		p_theme->set_stylebox("odd", "AnimationTrackEdit", style_animation_track_odd);
		p_theme->set_stylebox(SceneStringName(hover), "AnimationTrackEdit", style_animation_track_hover);
		p_theme->set_stylebox("focus", "AnimationTrackEdit", p_config.button_style_focus);

		p_theme->set_color("h_line_color", "AnimationTrackEdit", p_config.font_color * Color(1, 1, 1, 0.2));

		p_theme->set_constant("h_separation", "AnimationTrackEdit", (p_config.increased_margin + 2) * EDSCALE);
		p_theme->set_constant("outer_margin", "AnimationTrackEdit", p_config.increased_margin * 6 * EDSCALE);

		// AnimationTrackEditGroup.

		Ref<StyleBoxFlat> style_animation_track_header = make_flat_stylebox(p_config.dark_color_2 * Color(1, 1, 1, 0.6), p_config.increased_margin * 3, 0, 0, 0, p_config.corner_radius);

		p_theme->set_stylebox("header", "AnimationTrackEditGroup", style_animation_track_header);

		p_theme->set_color("h_line_color", "AnimationTrackEditGroup", p_config.font_color * Color(1, 1, 1, 0.2));
		p_theme->set_color("v_line_color", "AnimationTrackEditGroup", p_config.font_color * Color(1, 1, 1, 0.2));

		p_theme->set_constant("h_separation", "AnimationTrackEditGroup", (p_config.increased_margin + 2) * EDSCALE);
		p_theme->set_constant("v_separation", "AnimationTrackEditGroup", 0);

		// AnimationBezierTrackEdit.

		p_theme->set_color("focus_color", "AnimationBezierTrackEdit", p_config.accent_color * Color(1, 1, 1, 0.7));
		p_theme->set_color("track_focus_color", "AnimationBezierTrackEdit", p_config.accent_color * Color(1, 1, 1, 0.5));
		p_theme->set_color("h_line_color", "AnimationBezierTrackEdit", p_config.font_color * Color(1, 1, 1, 0.2));
		p_theme->set_color("v_line_color", "AnimationBezierTrackEdit", p_config.font_color * Color(1, 1, 1, 0.2));

		p_theme->set_constant("h_separation", "AnimationBezierTrackEdit", (p_config.increased_margin + 2) * EDSCALE);
		p_theme->set_constant("v_separation", "AnimationBezierTrackEdit", p_config.forced_even_separation * EDSCALE);
	}

	// Editor help.
	{
		Ref<StyleBoxFlat> style_editor_help = p_config.base_style->duplicate();
		style_editor_help->set_bg_color(p_config.dark_color_2);
		style_editor_help->set_border_color(p_config.dark_color_3);
		p_theme->set_stylebox("background", "EditorHelp", style_editor_help);

		const Color kbd_color = p_config.font_color.lerp(Color(0.5, 0.5, 0.5), 0.5);

		p_theme->set_color("title_color", "EditorHelp", p_config.accent_color);
		p_theme->set_color("headline_color", "EditorHelp", p_config.mono_color);
		p_theme->set_color("text_color", "EditorHelp", p_config.font_color);
		p_theme->set_color("comment_color", "EditorHelp", p_config.font_color * Color(1, 1, 1, 0.6));
		p_theme->set_color("symbol_color", "EditorHelp", p_config.font_color * Color(1, 1, 1, 0.6));
		p_theme->set_color("value_color", "EditorHelp", p_config.font_color * Color(1, 1, 1, 0.6));
		p_theme->set_color("qualifier_color", "EditorHelp", p_config.font_color * Color(1, 1, 1, 0.8));
		p_theme->set_color("type_color", "EditorHelp", p_config.accent_color.lerp(p_config.font_color, 0.5));
		p_theme->set_color("override_color", "EditorHelp", p_config.warning_color);
		p_theme->set_color("selection_color", "EditorHelp", p_config.selection_color);
		p_theme->set_color("link_color", "EditorHelp", p_config.accent_color.lerp(p_config.mono_color, 0.8));
		p_theme->set_color("code_color", "EditorHelp", p_config.accent_color.lerp(p_config.mono_color, 0.6));
		p_theme->set_color("kbd_color", "EditorHelp", p_config.accent_color.lerp(kbd_color, 0.6));
		p_theme->set_color("code_bg_color", "EditorHelp", p_config.dark_color_3);
		p_theme->set_color("kbd_bg_color", "EditorHelp", p_config.dark_color_1);
		p_theme->set_color("param_bg_color", "EditorHelp", p_config.dark_color_1);
		p_theme->set_constant(SceneStringName(line_separation), "EditorHelp", Math::round(6 * EDSCALE));
		p_theme->set_constant("table_h_separation", "EditorHelp", 16 * EDSCALE);
		p_theme->set_constant("table_v_separation", "EditorHelp", 6 * EDSCALE);
		p_theme->set_constant("text_highlight_h_padding", "EditorHelp", 1 * EDSCALE);
		p_theme->set_constant("text_highlight_v_padding", "EditorHelp", 2 * EDSCALE);
	}

	// EditorHelpBitTitle.
	{
		Ref<StyleBoxFlat> style = p_config.tree_panel_style->duplicate();
		style->set_bg_color(p_config.dark_theme ? style->get_bg_color().lightened(0.04) : style->get_bg_color().darkened(0.04));
		style->set_border_color(p_config.dark_theme ? style->get_border_color().lightened(0.04) : style->get_border_color().darkened(0.04));
		style->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
		style->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);

		p_theme->set_type_variation("EditorHelpBitTitle", "RichTextLabel");
		p_theme->set_stylebox(CoreStringName(normal), "EditorHelpBitTitle", style);
	}

	// EditorHelpBitContent.
	{
		Ref<StyleBoxFlat> style = p_config.tree_panel_style->duplicate();
		style->set_corner_radius(CORNER_TOP_LEFT, 0);
		style->set_corner_radius(CORNER_TOP_RIGHT, 0);

		p_theme->set_type_variation("EditorHelpBitContent", "RichTextLabel");
		p_theme->set_stylebox(CoreStringName(normal), "EditorHelpBitContent", style);
	}

	// Asset Library.
	p_theme->set_stylebox("bg", "AssetLib", p_config.base_empty_style);
	p_theme->set_stylebox(SceneStringName(panel), "AssetLib", p_config.content_panel_style);
	p_theme->set_color("status_color", "AssetLib", Color(0.5, 0.5, 0.5)); // FIXME: Use a defined color instead.
	p_theme->set_icon("dismiss", "AssetLib", p_theme->get_icon(SNAME("Close"), EditorStringName(EditorIcons)));

	// Debugger.
	{
		Ref<StyleBoxFlat> debugger_panel_style = p_config.content_panel_style->duplicate();
		debugger_panel_style->set_border_width(SIDE_BOTTOM, 0);
		p_theme->set_stylebox("DebuggerPanel", EditorStringName(EditorStyles), debugger_panel_style);

		// This pattern of get_font()->get_height(get_font_size()) is used quite a lot and is very verbose.
		// FIXME: Introduce Theme::get_font_height() / Control::get_theme_font_height() / Window::get_theme_font_height().
		const int offset_i1 = p_theme->get_font(SNAME("tab_selected"), SNAME("TabContainer"))->get_height(p_theme->get_font_size(SNAME("tab_selected"), SNAME("TabContainer")));
		const int offset_i2 = p_theme->get_stylebox(SNAME("tab_selected"), SNAME("TabContainer"))->get_minimum_size().height;
		const int offset_i3 = p_theme->get_stylebox(SceneStringName(panel), SNAME("TabContainer"))->get_content_margin(SIDE_TOP);
		const int invisible_top_offset = offset_i1 + offset_i2 + offset_i3;

		Ref<StyleBoxFlat> invisible_top_panel_style = p_config.content_panel_style->duplicate();
		invisible_top_panel_style->set_expand_margin(SIDE_TOP, -invisible_top_offset);
		invisible_top_panel_style->set_content_margin(SIDE_TOP, 0);
		p_theme->set_stylebox("BottomPanelDebuggerOverride", EditorStringName(EditorStyles), invisible_top_panel_style);
	}

	// Resource and node editors.
	{
		// TextureRegion editor.
		Ref<StyleBoxFlat> style_texture_region_bg = p_config.tree_panel_style->duplicate();
		style_texture_region_bg->set_content_margin_all(0);
		p_theme->set_stylebox("TextureRegionPreviewBG", EditorStringName(EditorStyles), style_texture_region_bg);
		p_theme->set_stylebox("TextureRegionPreviewFG", EditorStringName(EditorStyles), make_empty_stylebox(0, 0, 0, 0));

		// Theme editor.
		{
			p_theme->set_color("preview_picker_overlay_color", "ThemeEditor", Color(0.1, 0.1, 0.1, 0.25));

			Color theme_preview_picker_bg_color = p_config.accent_color;
			theme_preview_picker_bg_color.a = 0.2;
			Ref<StyleBoxFlat> theme_preview_picker_sb = make_flat_stylebox(theme_preview_picker_bg_color, 0, 0, 0, 0);
			theme_preview_picker_sb->set_border_color(p_config.accent_color);
			theme_preview_picker_sb->set_border_width_all(1.0 * EDSCALE);
			p_theme->set_stylebox("preview_picker_overlay", "ThemeEditor", theme_preview_picker_sb);

			Color theme_preview_picker_label_bg_color = p_config.accent_color;
			theme_preview_picker_label_bg_color.set_v(0.5);
			Ref<StyleBoxFlat> theme_preview_picker_label_sb = make_flat_stylebox(theme_preview_picker_label_bg_color, 4.0, 1.0, 4.0, 3.0);
			p_theme->set_stylebox("preview_picker_label", "ThemeEditor", theme_preview_picker_label_sb);

			Ref<StyleBoxFlat> style_theme_preview_tab = p_theme->get_stylebox(SNAME("tab_selected"), SNAME("TabContainerOdd"))->duplicate();
			style_theme_preview_tab->set_expand_margin(SIDE_BOTTOM, 5 * EDSCALE);
			p_theme->set_stylebox("ThemeEditorPreviewFG", EditorStringName(EditorStyles), style_theme_preview_tab);

			Ref<StyleBoxFlat> style_theme_preview_bg_tab = p_theme->get_stylebox(SNAME("tab_unselected"), SNAME("TabContainer"))->duplicate();
			style_theme_preview_bg_tab->set_expand_margin(SIDE_BOTTOM, 2 * EDSCALE);
			p_theme->set_stylebox("ThemeEditorPreviewBG", EditorStringName(EditorStyles), style_theme_preview_bg_tab);
		}

		// VisualShader editor.
		p_theme->set_stylebox("label_style", "VShaderEditor", make_empty_stylebox(4, 6, 4, 6));

		// StateMachine graph.
		{
			p_theme->set_stylebox(SceneStringName(panel), "GraphStateMachine", p_config.tree_panel_style);
			p_theme->set_stylebox("error_panel", "GraphStateMachine", p_config.tree_panel_style);
			p_theme->set_color("error_color", "GraphStateMachine", p_config.error_color);

			const int sm_margin_side = 10 * EDSCALE;
			const int sm_margin_bottom = 2;
			const Color sm_bg_color = p_config.dark_theme ? p_config.dark_color_3 : p_config.dark_color_1.lerp(p_config.mono_color, 0.09);

			Ref<StyleBoxFlat> sm_node_style = make_flat_stylebox(p_config.dark_color_3 * Color(1, 1, 1, 0.7), sm_margin_side, 24 * EDSCALE, sm_margin_side, sm_margin_bottom, p_config.corner_radius);
			sm_node_style->set_border_width_all(p_config.border_width);
			sm_node_style->set_border_color(sm_bg_color);

			Ref<StyleBoxFlat> sm_node_selected_style = make_flat_stylebox(sm_bg_color * Color(1, 1, 1, 0.9), sm_margin_side, 24 * EDSCALE, sm_margin_side, sm_margin_bottom, p_config.corner_radius);
			sm_node_selected_style->set_border_width_all(2 * EDSCALE + p_config.border_width);
			sm_node_selected_style->set_border_color(p_config.accent_color * Color(1, 1, 1, 0.9));
			sm_node_selected_style->set_shadow_size(8 * EDSCALE);
			sm_node_selected_style->set_shadow_color(p_config.shadow_color);

			Ref<StyleBoxFlat> sm_node_playing_style = sm_node_selected_style->duplicate();
			sm_node_playing_style->set_border_color(p_config.warning_color);
			sm_node_playing_style->set_shadow_color(p_config.warning_color * Color(1, 1, 1, 0.2));
			sm_node_playing_style->set_draw_center(false);

			p_theme->set_stylebox("node_frame", "GraphStateMachine", sm_node_style);
			p_theme->set_stylebox("node_frame_selected", "GraphStateMachine", sm_node_selected_style);
			p_theme->set_stylebox("node_frame_playing", "GraphStateMachine", sm_node_playing_style);

			Ref<StyleBoxFlat> sm_node_start_style = sm_node_style->duplicate();
			sm_node_start_style->set_border_width_all(1 * EDSCALE);
			sm_node_start_style->set_border_color(p_config.success_color.lightened(0.24));
			p_theme->set_stylebox("node_frame_start", "GraphStateMachine", sm_node_start_style);

			Ref<StyleBoxFlat> sm_node_end_style = sm_node_style->duplicate();
			sm_node_end_style->set_border_width_all(1 * EDSCALE);
			sm_node_end_style->set_border_color(p_config.error_color);
			p_theme->set_stylebox("node_frame_end", "GraphStateMachine", sm_node_end_style);

			p_theme->set_font("node_title_font", "GraphStateMachine", p_theme->get_font(SceneStringName(font), SNAME("Label")));
			p_theme->set_font_size("node_title_font_size", "GraphStateMachine", p_theme->get_font_size(SceneStringName(font_size), SNAME("Label")));
			p_theme->set_color("node_title_font_color", "GraphStateMachine", p_config.font_color);

			p_theme->set_color("transition_color", "GraphStateMachine", p_config.font_color);
			p_theme->set_color("transition_disabled_color", "GraphStateMachine", p_config.font_color * Color(1, 1, 1, 0.2));
			p_theme->set_color("transition_icon_color", "GraphStateMachine", Color(1, 1, 1));
			p_theme->set_color("transition_icon_disabled_color", "GraphStateMachine", Color(1, 1, 1, 0.2));
			p_theme->set_color("highlight_color", "GraphStateMachine", p_config.accent_color);
			p_theme->set_color("highlight_disabled_color", "GraphStateMachine", p_config.accent_color * Color(1, 1, 1, 0.6));
			p_theme->set_color("focus_color", "GraphStateMachine", p_config.accent_color);
			p_theme->set_color("guideline_color", "GraphStateMachine", p_config.font_color * Color(1, 1, 1, 0.3));

			p_theme->set_color("playback_color", "GraphStateMachine", p_config.font_color);
			p_theme->set_color("playback_background_color", "GraphStateMachine", p_config.font_color * Color(1, 1, 1, 0.3));
		}
	}
}

void EditorThemeManager::_generate_text_editor_defaults(ThemeConfiguration &p_config) {
	// Adaptive colors for comments and elements with lower relevance.
	const Color dim_color = Color(p_config.font_color, 0.5);
	const float mono_value = p_config.mono_color.r;
	const Color alpha1 = Color(mono_value, mono_value, mono_value, 0.07);
	const Color alpha2 = Color(mono_value, mono_value, mono_value, 0.14);
	const Color alpha3 = Color(mono_value, mono_value, mono_value, 0.27);

	/* clang-format off */
	// Syntax highlight token colors.
	const Color symbol_color =               p_config.dark_theme ? Color(0.67, 0.79, 1)      : Color(0, 0, 0.61);
	const Color keyword_color =              p_config.dark_theme ? Color(1.0, 0.44, 0.52)    : Color(0.9, 0.135, 0.51);
	const Color control_flow_keyword_color = p_config.dark_theme ? Color(1.0, 0.55, 0.8)     : Color(0.743, 0.12, 0.8);
	const Color base_type_color =            p_config.dark_theme ? Color(0.26, 1.0, 0.76)    : Color(0, 0.6, 0.2);
	const Color engine_type_color =          p_config.dark_theme ? Color(0.56, 1, 0.86)      : Color(0.11, 0.55, 0.4);
	const Color user_type_color =            p_config.dark_theme ? Color(0.78, 1, 0.93)      : Color(0.18, 0.45, 0.4);
	const Color comment_color =              p_config.dark_theme ? dim_color                 : Color(0.08, 0.08, 0.08, 0.5);
	const Color doc_comment_color =          p_config.dark_theme ? Color(0.6, 0.7, 0.8, 0.8) : Color(0.15, 0.15, 0.4, 0.7);
	const Color string_color =               p_config.dark_theme ? Color(1, 0.93, 0.63)      : Color(0.6, 0.42, 0);

	// Use the brightest background color on a light theme (which generally uses a negative contrast rate).
	const Color te_background_color =             p_config.dark_theme ? p_config.dark_color_2 : p_config.dark_color_3;
	const Color completion_background_color =     p_config.dark_theme ? p_config.base_color : p_config.dark_color_2;
	const Color completion_selected_color =       alpha1;
	const Color completion_existing_color =       alpha2;
	// Same opacity as the scroll grabber editor icon.
	const Color completion_scroll_color =         Color(mono_value, mono_value, mono_value, 0.29);
	const Color completion_scroll_hovered_color = Color(mono_value, mono_value, mono_value, 0.4);
	const Color completion_font_color =           p_config.font_color;
	const Color text_color =                      p_config.font_color;
	const Color line_number_color =               dim_color;
	const Color safe_line_number_color =          p_config.dark_theme ? (dim_color * Color(1, 1.2, 1, 1.5)) : Color(0, 0.4, 0, 0.75);
	const Color caret_color =                     p_config.mono_color;
	const Color caret_background_color =          p_config.mono_color.inverted();
	const Color text_selected_color =             Color(0, 0, 0, 0);
	const Color selection_color =                 p_config.selection_color;
	const Color brace_mismatch_color =            p_config.dark_theme ? p_config.error_color : Color(1, 0.08, 0, 1);
	const Color current_line_color =              alpha1;
	const Color line_length_guideline_color =     p_config.dark_theme ? p_config.base_color : p_config.dark_color_2;
	const Color word_highlighted_color =          alpha1;
	const Color number_color =                    p_config.dark_theme ? Color(0.63, 1, 0.88) : Color(0, 0.55, 0.28, 1);
	const Color function_color =                  p_config.dark_theme ? Color(0.34, 0.7, 1.0) : Color(0, 0.225, 0.9, 1);
	const Color member_variable_color =           p_config.dark_theme ? Color(0.34, 0.7, 1.0).lerp(p_config.mono_color, 0.6) : Color(0, 0.4, 0.68, 1);
	const Color mark_color =                      Color(p_config.error_color.r, p_config.error_color.g, p_config.error_color.b, 0.3);
	const Color bookmark_color =                  Color(0.08, 0.49, 0.98);
	const Color breakpoint_color =                p_config.dark_theme ? p_config.error_color : Color(1, 0.27, 0.2, 1);
	const Color executing_line_color =            Color(0.98, 0.89, 0.27);
	const Color code_folding_color =              alpha3;
	const Color folded_code_region_color =        Color(0.68, 0.46, 0.77, 0.2);
	const Color search_result_color =             alpha1;
	const Color search_result_border_color =      p_config.dark_theme ? Color(0.41, 0.61, 0.91, 0.38) : Color(0, 0.4, 1, 0.38);
	/* clang-format on */

	EditorSettings *setting = EditorSettings::get_singleton();

	/* clang-format off */
	setting->set_initial_value("text_editor/theme/highlighting/symbol_color",                    symbol_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/keyword_color",                   keyword_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/control_flow_keyword_color",      control_flow_keyword_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/base_type_color",                 base_type_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/engine_type_color",               engine_type_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/user_type_color",                 user_type_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/comment_color",                   comment_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/doc_comment_color",               doc_comment_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/string_color",                    string_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/background_color",                te_background_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_background_color",     completion_background_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_selected_color",       completion_selected_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_existing_color",       completion_existing_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_scroll_color",         completion_scroll_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_scroll_hovered_color", completion_scroll_hovered_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/completion_font_color",           completion_font_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/text_color",                      text_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/line_number_color",               line_number_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/safe_line_number_color",          safe_line_number_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/caret_color",                     caret_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/caret_background_color",          caret_background_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/text_selected_color",             text_selected_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/selection_color",                 selection_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/brace_mismatch_color",            brace_mismatch_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/current_line_color",              current_line_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/line_length_guideline_color",     line_length_guideline_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/word_highlighted_color",          word_highlighted_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/number_color",                    number_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/function_color",                  function_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/member_variable_color",           member_variable_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/mark_color",                      mark_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/bookmark_color",                  bookmark_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/breakpoint_color",                breakpoint_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/executing_line_color",            executing_line_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/code_folding_color",              code_folding_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/folded_code_region_color",        folded_code_region_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/search_result_color",             search_result_color, true);
	setting->set_initial_value("text_editor/theme/highlighting/search_result_border_color",      search_result_border_color, true);
	/* clang-format on */
}

void EditorThemeManager::_populate_text_editor_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config) {
	String text_editor_color_theme = EditorSettings::get_singleton()->get("text_editor/theme/color_theme");
	if (text_editor_color_theme == "Default") {
		_generate_text_editor_defaults(p_config);
	} else if (text_editor_color_theme == "Godot 2") {
		EditorSettings::get_singleton()->load_text_editor_theme();
	}

	// Now theme is loaded, apply it to CodeEdit.
	p_theme->set_font(SceneStringName(font), "CodeEdit", p_theme->get_font(SNAME("source"), EditorStringName(EditorFonts)));
	p_theme->set_font_size(SceneStringName(font_size), "CodeEdit", p_theme->get_font_size(SNAME("source_size"), EditorStringName(EditorFonts)));

	/* clang-format off */
	p_theme->set_icon("tab",                  "CodeEdit", p_theme->get_icon(SNAME("GuiTab"), EditorStringName(EditorIcons)));
	p_theme->set_icon("space",                "CodeEdit", p_theme->get_icon(SNAME("GuiSpace"), EditorStringName(EditorIcons)));
	p_theme->set_icon("folded",               "CodeEdit", p_theme->get_icon(SNAME("CodeFoldedRightArrow"), EditorStringName(EditorIcons)));
	p_theme->set_icon("can_fold",             "CodeEdit", p_theme->get_icon(SNAME("CodeFoldDownArrow"), EditorStringName(EditorIcons)));
	p_theme->set_icon("folded_code_region",   "CodeEdit", p_theme->get_icon(SNAME("CodeRegionFoldedRightArrow"), EditorStringName(EditorIcons)));
	p_theme->set_icon("can_fold_code_region", "CodeEdit", p_theme->get_icon(SNAME("CodeRegionFoldDownArrow"), EditorStringName(EditorIcons)));
	p_theme->set_icon("executing_line",       "CodeEdit", p_theme->get_icon(SNAME("TextEditorPlay"), EditorStringName(EditorIcons)));
	p_theme->set_icon("breakpoint",           "CodeEdit", p_theme->get_icon(SNAME("Breakpoint"), EditorStringName(EditorIcons)));
	/* clang-format on */

	p_theme->set_constant("line_spacing", "CodeEdit", EDITOR_GET("text_editor/appearance/whitespace/line_spacing"));

	const Color background_color = EDITOR_GET("text_editor/theme/highlighting/background_color");
	Ref<StyleBoxFlat> code_edit_stylebox = make_flat_stylebox(background_color, p_config.widget_margin.x, p_config.widget_margin.y, p_config.widget_margin.x, p_config.widget_margin.y, p_config.corner_radius);
	p_theme->set_stylebox(CoreStringName(normal), "CodeEdit", code_edit_stylebox);
	p_theme->set_stylebox("read_only", "CodeEdit", code_edit_stylebox);
	p_theme->set_stylebox("focus", "CodeEdit", memnew(StyleBoxEmpty));

	p_theme->set_color("background_color", "CodeEdit", Color(0, 0, 0, 0)); // Unset any color, we use a stylebox.

	/* clang-format off */
	p_theme->set_color("completion_background_color",     "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/completion_background_color"));
	p_theme->set_color("completion_selected_color",       "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/completion_selected_color"));
	p_theme->set_color("completion_existing_color",       "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/completion_existing_color"));
	p_theme->set_color("completion_scroll_color",         "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/completion_scroll_color"));
	p_theme->set_color("completion_scroll_hovered_color", "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/completion_scroll_hovered_color"));
	p_theme->set_color(SceneStringName(font_color),                      "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/text_color"));
	p_theme->set_color("line_number_color",               "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/line_number_color"));
	p_theme->set_color("caret_color",                     "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/caret_color"));
	p_theme->set_color("font_selected_color",             "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/text_selected_color"));
	p_theme->set_color("selection_color",                 "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/selection_color"));
	p_theme->set_color("brace_mismatch_color",            "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/brace_mismatch_color"));
	p_theme->set_color("current_line_color",              "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/current_line_color"));
	p_theme->set_color("line_length_guideline_color",     "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/line_length_guideline_color"));
	p_theme->set_color("word_highlighted_color",          "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/word_highlighted_color"));
	p_theme->set_color("bookmark_color",                  "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/bookmark_color"));
	p_theme->set_color("breakpoint_color",                "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/breakpoint_color"));
	p_theme->set_color("executing_line_color",            "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/executing_line_color"));
	p_theme->set_color("code_folding_color",              "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/code_folding_color"));
	p_theme->set_color("folded_code_region_color",        "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/folded_code_region_color"));
	p_theme->set_color("search_result_color",             "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/search_result_color"));
	p_theme->set_color("search_result_border_color",      "CodeEdit", EDITOR_GET("text_editor/theme/highlighting/search_result_border_color"));
	/* clang-format on */
}

void EditorThemeManager::_populate_visual_shader_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config) {
	EditorSettings *ed_settings = EditorSettings::get_singleton();
	String visual_shader_color_theme = ed_settings->get("editors/visual_editors/color_theme");
	if (visual_shader_color_theme == "Default") {
		// Connection type colors
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/scalar_color", Color(0.55, 0.55, 0.55), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector2_color", Color(0.44, 0.43, 0.64), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector3_color", Color(0.337, 0.314, 0.71), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector4_color", Color(0.7, 0.65, 0.147), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/boolean_color", Color(0.243, 0.612, 0.349), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/transform_color", Color(0.71, 0.357, 0.64), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/sampler_color", Color(0.659, 0.4, 0.137), true);

		// Node category colors (used for the node headers)
		ed_settings->set_initial_value("editors/visual_editors/category_colors/output_color", Color(0.26, 0.10, 0.15), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/color_color", Color(0.5, 0.5, 0.1), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/conditional_color", Color(0.208, 0.522, 0.298), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/input_color", Color(0.502, 0.2, 0.204), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/scalar_color", Color(0.1, 0.5, 0.6), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/textures_color", Color(0.5, 0.3, 0.1), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/transform_color", Color(0.5, 0.3, 0.5), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/utility_color", Color(0.2, 0.2, 0.2), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/vector_color", Color(0.2, 0.2, 0.5), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/special_color", Color(0.098, 0.361, 0.294), true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/particle_color", Color(0.12, 0.358, 0.8), true);

	} else if (visual_shader_color_theme == "Legacy") {
		// Connection type colors
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/scalar_color", Color(0.38, 0.85, 0.96), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector2_color", Color(0.74, 0.57, 0.95), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector3_color", Color(0.84, 0.49, 0.93), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/vector4_color", Color(1.0, 0.125, 0.95), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/boolean_color", Color(0.55, 0.65, 0.94), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/transform_color", Color(0.96, 0.66, 0.43), true);
		ed_settings->set_initial_value("editors/visual_editors/connection_colors/sampler_color", Color(1.0, 1.0, 0.0), true);

		// Node category colors (used for the node headers)
		Ref<StyleBoxFlat> gn_panel_style = p_theme->get_stylebox(SceneStringName(panel), "GraphNode");
		Color gn_bg_color = gn_panel_style->get_bg_color();
		ed_settings->set_initial_value("editors/visual_editors/category_colors/output_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/color_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/conditional_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/input_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/scalar_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/textures_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/transform_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/utility_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/vector_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/special_color", gn_bg_color, true);
		ed_settings->set_initial_value("editors/visual_editors/category_colors/particle_color", gn_bg_color, true);
	}
}

void EditorThemeManager::_reset_dirty_flag() {
	outdated_cache_dirty = true;
}

// Public interface for theme generation.

Ref<EditorTheme> EditorThemeManager::generate_theme(const Ref<EditorTheme> &p_old_theme) {
	OS::get_singleton()->benchmark_begin_measure(get_benchmark_key(), "Generate Theme");

	Ref<EditorTheme> theme = _create_base_theme(p_old_theme);

	OS::get_singleton()->benchmark_begin_measure(get_benchmark_key(), "Merge Custom Theme");

	const String custom_theme_path = EDITOR_GET("interface/theme/custom_theme");
	if (!custom_theme_path.is_empty()) {
		Ref<Theme> custom_theme = ResourceLoader::load(custom_theme_path);
		if (custom_theme.is_valid()) {
			theme->merge_with(custom_theme);
		}
	}

	OS::get_singleton()->benchmark_end_measure(get_benchmark_key(), "Merge Custom Theme");

	OS::get_singleton()->benchmark_end_measure(get_benchmark_key(), "Generate Theme");
	benchmark_run++;

	return theme;
}

bool EditorThemeManager::is_generated_theme_outdated() {
	// This list includes settings used by files in the editor/themes folder.
	// Note that the editor scale is purposefully omitted because it cannot be changed
	// without a restart, so there is no point regenerating the theme.

	if (outdated_cache_dirty) {
		// TODO: We can use this information more intelligently to do partial theme updates and speed things up.
		outdated_cache = EditorSettings::get_singleton()->check_changed_settings_in_group("interface/theme") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/font") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/main_font") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/code_font") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("editors/visual_editors") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/theme") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/help/help") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("docks/property_editor/subresource_hue_tint") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("filesystem/file_dialog/thumbnail_size") ||
				EditorSettings::get_singleton()->check_changed_settings_in_group("run/output/font_size");

		// The outdated flag is relevant at the moment of changing editor settings.
		callable_mp_static(&EditorThemeManager::_reset_dirty_flag).call_deferred();
		outdated_cache_dirty = false;
	}

	return outdated_cache;
}

bool EditorThemeManager::is_dark_theme() {
	// Light color mode for icons and fonts means it's a dark theme, and vice versa.
	int icon_font_color_setting = EDITOR_GET("interface/theme/icon_and_font_color");

	if (icon_font_color_setting == ColorMode::AUTO_COLOR) {
		Color base_color = EDITOR_GET("interface/theme/base_color");
		return base_color.get_luminance() < 0.5;
	}

	return icon_font_color_setting == ColorMode::LIGHT_COLOR;
}

void EditorThemeManager::initialize() {
	EditorColorMap::create();
	EditorTheme::initialize();
}

void EditorThemeManager::finalize() {
	EditorColorMap::finish();
	EditorTheme::finalize();
}
