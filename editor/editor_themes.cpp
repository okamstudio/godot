/**************************************************************************/
/*  editor_themes.cpp                                                     */
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

#include "editor_themes.h"

#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "editor_fonts.h"
#include "editor_icons.gen.h"
#include "editor_scale.h"
#include "editor_settings.h"

#include "modules/modules_enabled.gen.h" // For svg.
#ifdef MODULE_SVG_ENABLED
#include "modules/svg/image_loader_svg.h"
#endif

static Ref<StyleBoxTexture> make_stylebox(Ref<Texture> p_texture, float p_left, float p_top, float p_right, float p_bottom, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1, bool p_draw_center = true) {
	Ref<StyleBoxTexture> style(memnew(StyleBoxTexture));
	style->set_texture(p_texture);
	style->set_margin_size(MARGIN_LEFT, p_left * EDSCALE);
	style->set_margin_size(MARGIN_RIGHT, p_right * EDSCALE);
	style->set_margin_size(MARGIN_BOTTOM, p_bottom * EDSCALE);
	style->set_margin_size(MARGIN_TOP, p_top * EDSCALE);
	style->set_default_margin(MARGIN_LEFT, p_margin_left * EDSCALE);
	style->set_default_margin(MARGIN_RIGHT, p_margin_right * EDSCALE);
	style->set_default_margin(MARGIN_BOTTOM, p_margin_bottom * EDSCALE);
	style->set_default_margin(MARGIN_TOP, p_margin_top * EDSCALE);
	style->set_draw_center(p_draw_center);
	return style;
}

static Ref<StyleBoxEmpty> make_empty_stylebox(float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1) {
	Ref<StyleBoxEmpty> style(memnew(StyleBoxEmpty));
	style->set_default_margin(MARGIN_LEFT, p_margin_left * EDSCALE);
	style->set_default_margin(MARGIN_RIGHT, p_margin_right * EDSCALE);
	style->set_default_margin(MARGIN_BOTTOM, p_margin_bottom * EDSCALE);
	style->set_default_margin(MARGIN_TOP, p_margin_top * EDSCALE);
	return style;
}

static Ref<StyleBoxFlat> make_flat_stylebox(Color p_color, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1) {
	Ref<StyleBoxFlat> style(memnew(StyleBoxFlat));
	style->set_bg_color(p_color);
	style->set_default_margin(MARGIN_LEFT, p_margin_left * EDSCALE);
	style->set_default_margin(MARGIN_RIGHT, p_margin_right * EDSCALE);
	style->set_default_margin(MARGIN_BOTTOM, p_margin_bottom * EDSCALE);
	style->set_default_margin(MARGIN_TOP, p_margin_top * EDSCALE);
	return style;
}

static Ref<StyleBoxLine> make_line_stylebox(Color p_color, int p_thickness = 1, float p_grow_begin = 1, float p_grow_end = 1, bool p_vertical = false) {
	Ref<StyleBoxLine> style(memnew(StyleBoxLine));
	style->set_color(p_color);
	style->set_grow_begin(p_grow_begin);
	style->set_grow_end(p_grow_end);
	style->set_thickness(p_thickness);
	style->set_vertical(p_vertical);
	return style;
}

static Ref<Texture> flip_icon(Ref<Texture> p_texture, bool p_flip_y = false, bool p_flip_x = false) {
	if (!p_flip_y && !p_flip_x) {
		return p_texture;
	}

	Ref<ImageTexture> texture(memnew(ImageTexture));
	Ref<Image> img = p_texture->get_data();
	img = img->duplicate();

	if (p_flip_y) {
		img->flip_y();
	}
	if (p_flip_x) {
		img->flip_x();
	}

	texture->create_from_image(img);
	return texture;
}

#ifdef MODULE_SVG_ENABLED
static Ref<ImageTexture> editor_generate_icon(int p_index, bool p_convert_color, float p_scale = EDSCALE, bool p_force_filter = false) {
	Ref<ImageTexture> icon = memnew(ImageTexture);
	Ref<Image> img = memnew(Image);

	// dumb gizmo check
	bool is_gizmo = String(editor_icons_names[p_index]).begins_with("Gizmo");

	// Upsample icon generation only if the editor scale isn't an integer multiplier.
	// Generating upsampled icons is slower, and the benefit is hardly visible
	// with integer editor scales.
	const bool upsample = !Math::is_equal_approx(Math::round(p_scale), p_scale);
	ImageLoaderSVG::create_image_from_string(img, editor_icons_sources[p_index], p_scale, upsample, p_convert_color);

	if ((p_scale - (float)((int)p_scale)) > 0.0 || is_gizmo || p_force_filter) {
		icon->create_from_image(img); // in this case filter really helps
	} else {
		icon->create_from_image(img, 0);
	}

	return icon;
}
#endif

#ifndef ADD_CONVERT_COLOR
#define ADD_CONVERT_COLOR(dictionary, old_color, new_color) dictionary[Color::html(old_color)] = Color::html(new_color)
#endif

float get_gizmo_handle_scale(const String &gizmo_handle_name = "") {
	const float scale_gizmo_handles_for_touch = EDITOR_GET("interface/touchscreen/scale_gizmo_handles");
	if (scale_gizmo_handles_for_touch > 1.0f) {
		// The names of the icons that require custom scaling.
		static Set<StringName> gizmo_to_scale;
		if (gizmo_to_scale.empty()) {
			gizmo_to_scale.insert("EditorHandle");
			gizmo_to_scale.insert("EditorHandleAdd");
			gizmo_to_scale.insert("EditorCurveHandle");
			gizmo_to_scale.insert("EditorPathSharpHandle");
			gizmo_to_scale.insert("EditorPathSmoothHandle");
		}

		if (gizmo_to_scale.has(gizmo_handle_name)) {
			return EDSCALE * scale_gizmo_handles_for_touch;
		}
	}

	return EDSCALE;
}

void editor_register_and_generate_icons(Ref<Theme> p_theme, bool p_dark_theme = true, int p_thumb_size = 32, bool p_only_thumbs = false) {
	OS::get_singleton()->benchmark_begin_measure("editor_register_and_generate_icons_" + String((p_only_thumbs ? "with_only_thumbs" : "all")));
#ifdef MODULE_SVG_ENABLED
	// The default icon theme is designed to be used for a dark theme.
	// This dictionary stores color codes to convert to other colors
	// for better readability on a light theme.
	Dictionary dark_icon_color_dictionary;

	// The names of the icons to never convert, even if one of their colors
	// are contained in the dictionary above.
	Set<StringName> exceptions;

	if (!p_dark_theme) {
		// convert color:                             FROM       TO
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e0e0e0", "#5a5a5a"); // common icon color
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffffff", "#414141"); // white
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#b4b4b4", "#363636"); // script darker color
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f9f9f9", "#606060"); // scrollbar grabber highlight color

		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#cea4f1", "#a85de9"); // animation
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#fc9c9c", "#cd3838"); // spatial
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#a5b7f3", "#3d64dd"); // 2d
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#708cea", "#1a3eac"); // 2d dark
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#a5efac", "#2fa139"); // control
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffdd65", "#ca8a04"); // node warning

		// rainbow
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff7070", "#ff2929"); // red
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffeb70", "#ffe337"); // yellow
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#9dff70", "#74ff34"); // green
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#70ffb9", "#2cff98"); // aqua
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#70deff", "#22ccff"); // blue
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#9f70ff", "#702aff"); // purple
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff70ac", "#ff2781"); // pink

		// audio gradient
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff8484", "#ff4040"); // red
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e1dc7a", "#d6cf4b"); // yellow
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84ffb1", "#00f010"); // green

		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffd684", "#fea900"); // mesh (orange)
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#40a2ff", "#68b6ff"); // shape (blue)

		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff8484", "#ff3333"); // remove (red)
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84ffb1", "#00db50"); // add (green)
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84c2ff", "#5caeff"); // selection (blue)

		// Animation editor tracks
		// The property track icon color is set by the common icon color
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ea9568", "#bd5e2c"); // 3D Transform track
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#66f376", "#16a827"); // Call Method track
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#5792f6", "#236be6"); // Bezier Curve track
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#eae668", "#9f9722"); // Audio Playback track
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#b76ef0", "#9853ce"); // Animation Playback track

		// TileSet editor icons
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#fce844", "#aa8d24"); // New Single Tile
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#4490fc", "#0350bd"); // New Autotile
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#c9cfd4", "#828f9b"); // New Atlas

		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#69ecbd", "#25e3a0"); // VS variant
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#8da6f0", "#6d8eeb"); // VS bool
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#7dc6ef", "#4fb2e9"); // VS int
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#61daf4", "#27ccf0"); // VS float
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#6ba7ec", "#4690e7"); // VS string
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#bd91f1", "#ad76ee"); // VS vector2
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f191a5", "#ee758e"); // VS rect
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e286f0", "#dc6aed"); // VS vector3
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#c4ec69", "#96ce1a"); // VS transform2D
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f77070", "#f77070"); // VS plane
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ec69a3", "#ec69a3"); // VS quat
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ee7991", "#ee7991"); // VS aabb
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e3ec69", "#b2bb19"); // VS basis
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f6a86e", "#f49047"); // VS transform
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#6993ec", "#6993ec"); // VS path
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#69ec9a", "#2ce573"); // VS rid
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#79f3e8", "#12d5c3"); // VS object
		ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#77edb1", "#57e99f"); // VS dict

		exceptions.insert("EditorPivot");
		exceptions.insert("EditorHandle");
		exceptions.insert("Editor3DHandle");
		exceptions.insert("Godot");
		exceptions.insert("PanoramaSky");
		exceptions.insert("ProceduralSky");
		exceptions.insert("EditorControlAnchor");
		exceptions.insert("DefaultProjectIcon");
		exceptions.insert("GuiCloseCustomizable");
		exceptions.insert("GuiGraphNodePort");
		exceptions.insert("GuiResizer");
		exceptions.insert("ZoomMore");
		exceptions.insert("ZoomLess");
		exceptions.insert("ZoomReset");
		exceptions.insert("LockViewport");
		exceptions.insert("GroupViewport");
		exceptions.insert("StatusError");
		exceptions.insert("StatusSuccess");
		exceptions.insert("StatusWarning");
		exceptions.insert("OverbrightIndicator");

		// Prevents Code Editor icons from changing
		exceptions.insert("GuiTab");
		exceptions.insert("GuiSpace");
		exceptions.insert("GuiEllipsis");
		exceptions.insert("TextEditFold");
		exceptions.insert("TextEditFolded");
		exceptions.insert("TextEditorPlay");
	}

	// These ones should be converted even if we are using a dark theme.
	const Color accent_color = EDITOR_GET("interface/theme/accent_color");
	const Color error_color = p_theme->get_color("error_color", "Editor");
	const Color success_color = p_theme->get_color("success_color", "Editor");
	const Color warning_color = p_theme->get_color("warning_color", "Editor");
	dark_icon_color_dictionary[Color::html("#699ce8")] = accent_color;
	dark_icon_color_dictionary[Color::html("#ff0000")] = error_color;
	dark_icon_color_dictionary[Color::html("#45ff8b")] = success_color;
	dark_icon_color_dictionary[Color::html("#dbab09")] = warning_color;

	ImageLoaderSVG::set_convert_colors(&dark_icon_color_dictionary);

	// Generate icons.
	if (!p_only_thumbs) {
		for (int i = 0; i < editor_icons_count; i++) {
			const String &editor_icon_name = editor_icons_names[i];
			const int is_exception = exceptions.has(editor_icon_name);
			const Ref<ImageTexture> icon = editor_generate_icon(i, !is_exception, get_gizmo_handle_scale(editor_icon_name));

			p_theme->set_icon(editor_icon_name, "EditorIcons", icon);
		}
	}

	// Generate thumbnail icons with the given thumbnail size.
	// We don't need filtering when generating at one of the default resolutions.
	const bool force_filter = p_thumb_size != 64 && p_thumb_size != 32;
	if (p_thumb_size >= 64) {
		const float scale = (float)p_thumb_size / 64.0 * EDSCALE;
		for (int i = 0; i < editor_bg_thumbs_count; i++) {
			const int index = editor_bg_thumbs_indices[i];
			const int is_exception = exceptions.has(editor_icons_names[index]);
			const Ref<ImageTexture> icon = editor_generate_icon(index, !p_dark_theme && !is_exception, scale, force_filter);

			p_theme->set_icon(editor_icons_names[index], "EditorIcons", icon);
		}
	} else {
		const float scale = (float)p_thumb_size / 32.0 * EDSCALE;
		for (int i = 0; i < editor_md_thumbs_count; i++) {
			const int index = editor_md_thumbs_indices[i];
			const bool is_exception = exceptions.has(editor_icons_names[index]);
			const Ref<ImageTexture> icon = editor_generate_icon(index, !p_dark_theme && !is_exception, scale, force_filter);

			p_theme->set_icon(editor_icons_names[index], "EditorIcons", icon);
		}
	}

	ImageLoaderSVG::set_convert_colors(nullptr);
#else
	WARN_PRINT("SVG support disabled, editor icons won't be rendered.");
#endif
	OS::get_singleton()->benchmark_end_measure("editor_register_and_generate_icons_" + String((p_only_thumbs ? "with_only_thumbs" : "all")));
}

Ref<Theme> create_editor_theme(const Ref<Theme> p_theme) {
	OS::get_singleton()->benchmark_begin_measure("create_editor_theme");
	Ref<Theme> theme = Ref<Theme>(memnew(Theme));

	const float default_contrast = 0.25;

	//Theme settings
	Color accent_color = EDITOR_GET("interface/theme/accent_color");
	Color base_color = EDITOR_GET("interface/theme/base_color");
	float contrast = EDITOR_GET("interface/theme/contrast");
	float relationship_line_opacity = EDITOR_GET("interface/theme/relationship_line_opacity");

	String preset = EDITOR_GET("interface/theme/preset");

	bool increase_scrollbar_touch_area = EDITOR_GET("interface/touchscreen/increase_scrollbar_touch_area");
	const float gizmo_handle_scale = EDITOR_GET("interface/touchscreen/scale_gizmo_handles");
	bool highlight_tabs = EDITOR_GET("interface/theme/highlight_tabs");
	int border_size = EDITOR_GET("interface/theme/border_size");

	bool use_gn_headers = EDITOR_GET("interface/theme/use_graph_node_headers");

	Color preset_accent_color;
	Color preset_base_color;
	float preset_contrast = 0;

	// Please, use alphabet order if you've added new theme here(After "Default" and "Custom")

	if (preset == "Custom") {
		accent_color = EDITOR_GET("interface/theme/accent_color");
		base_color = EDITOR_GET("interface/theme/base_color");
		contrast = EDITOR_GET("interface/theme/contrast");
	} else if (preset == "Alien") {
		preset_accent_color = Color(0.11, 1.0, 0.6);
		preset_base_color = Color(0.18, 0.22, 0.25);
		preset_contrast = 0.25;
	} else if (preset == "Arc") {
		preset_accent_color = Color(0.32, 0.58, 0.89);
		preset_base_color = Color(0.22, 0.24, 0.29);
		preset_contrast = 0.25;
	} else if (preset == "Godot 2") {
		preset_accent_color = Color(0.53, 0.67, 0.89);
		preset_base_color = Color(0.24, 0.23, 0.27);
		preset_contrast = 0.25;
	} else if (preset == "Grey") {
		preset_accent_color = Color(0.44, 0.73, 0.98);
		preset_base_color = Color(0.24, 0.24, 0.24);
		preset_contrast = 0.2;
	} else if (preset == "Light") {
		preset_accent_color = Color(0.13, 0.44, 1.0);
		preset_base_color = Color(1, 1, 1);
		preset_contrast = 0.08;
	} else if (preset == "Solarized (Dark)") {
		preset_accent_color = Color(0.15, 0.55, 0.82);
		preset_base_color = Color(0.03, 0.21, 0.26);
		preset_contrast = 0.23;
	} else if (preset == "Solarized (Light)") {
		preset_accent_color = Color(0.15, 0.55, 0.82);
		preset_base_color = Color(0.99, 0.96, 0.89);
		preset_contrast = 0.06;
	} else { // Default
		preset_accent_color = Color(0.41, 0.61, 0.91);
		preset_base_color = Color(0.2, 0.23, 0.31);
		preset_contrast = default_contrast;
	}

	if (preset != "Custom") {
		accent_color = preset_accent_color;
		base_color = preset_base_color;
		contrast = preset_contrast;
		EditorSettings::get_singleton()->set_initial_value("interface/theme/accent_color", accent_color);
		EditorSettings::get_singleton()->set_initial_value("interface/theme/base_color", base_color);
		EditorSettings::get_singleton()->set_initial_value("interface/theme/contrast", contrast);
	}
	EditorSettings::get_singleton()->set_manually("interface/theme/preset", preset);
	EditorSettings::get_singleton()->set_manually("interface/theme/accent_color", accent_color);
	EditorSettings::get_singleton()->set_manually("interface/theme/base_color", base_color);
	EditorSettings::get_singleton()->set_manually("interface/theme/contrast", contrast);

	//Colors
	bool dark_theme = EditorSettings::get_singleton()->is_dark_theme();

	const Color dark_color_1 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast);
	const Color dark_color_2 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast * 1.5);
	const Color dark_color_3 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast * 2);

	const Color background_color = dark_color_2;

	// white (dark theme) or black (light theme), will be used to generate the rest of the colors
	const Color mono_color = dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);

	const Color contrast_color_1 = base_color.linear_interpolate(mono_color, MAX(contrast, default_contrast));
	const Color contrast_color_2 = base_color.linear_interpolate(mono_color, MAX(contrast * 1.5, default_contrast * 1.5));

	const Color font_color = mono_color.linear_interpolate(base_color, 0.25);
	const Color font_color_hl = mono_color.linear_interpolate(base_color, 0.15);
	const Color font_color_focus = mono_color.linear_interpolate(base_color, 0.15);
	const Color font_color_disabled = Color(mono_color.r, mono_color.g, mono_color.b, 0.3);
	const Color font_color_readonly = Color(mono_color.r, mono_color.g, mono_color.b, 0.65);
	const Color font_color_selection = accent_color * Color(1, 1, 1, 0.4);
	const Color color_disabled = mono_color.inverted().linear_interpolate(base_color, 0.7);
	const Color color_disabled_bg = mono_color.inverted().linear_interpolate(base_color, 0.9);

	const Color icon_color_normal = Color(1, 1, 1);
	Color icon_color_hover = icon_color_normal * (dark_theme ? 1.15 : 1.45);
	icon_color_hover.a = 1.0;
	Color icon_color_disabled = icon_color_normal;
	icon_color_disabled.a = 0.4;
	// Make the pressed icon color overbright because icons are not completely white on a dark theme.
	// On a light theme, icons are dark, so we need to modulate them with an even brighter color.
	Color icon_color_pressed = accent_color * (dark_theme ? 1.15 : 3.5);
	icon_color_pressed.a = 1.0;

	const Color separator_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.1);

	const Color highlight_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.2);

	theme->set_color("accent_color", "Editor", accent_color);
	theme->set_color("highlight_color", "Editor", highlight_color);
	theme->set_color("base_color", "Editor", base_color);
	theme->set_color("dark_color_1", "Editor", dark_color_1);
	theme->set_color("dark_color_2", "Editor", dark_color_2);
	theme->set_color("dark_color_3", "Editor", dark_color_3);
	theme->set_color("contrast_color_1", "Editor", contrast_color_1);
	theme->set_color("contrast_color_2", "Editor", contrast_color_2);
	theme->set_color("box_selection_fill_color", "Editor", accent_color * Color(1, 1, 1, 0.3));
	theme->set_color("box_selection_stroke_color", "Editor", accent_color * Color(1, 1, 1, 0.8));

	theme->set_color("axis_x_color", "Editor", Color(0.96, 0.20, 0.32));
	theme->set_color("axis_y_color", "Editor", Color(0.53, 0.84, 0.01));
	theme->set_color("axis_z_color", "Editor", Color(0.16, 0.55, 0.96));

	theme->set_color("font_color", "Editor", font_color);
	theme->set_color("highlighted_font_color", "Editor", font_color_hl);
	theme->set_color("disabled_font_color", "Editor", font_color_disabled);

	theme->set_color("mono_color", "Editor", mono_color);

	Color success_color = Color(0.45, 0.95, 0.5);
	Color warning_color = Color(1, 0.87, 0.4);
	Color error_color = Color(1, 0.47, 0.42);
	Color property_color = font_color.linear_interpolate(Color(0.5, 0.5, 0.5), 0.5);

	if (!dark_theme) {
		// Darken some colors to be readable on a light background
		success_color = success_color.linear_interpolate(mono_color, 0.35);
		warning_color = warning_color.linear_interpolate(mono_color, 0.35);
		error_color = error_color.linear_interpolate(mono_color, 0.25);
	}

	theme->set_color("success_color", "Editor", success_color);
	theme->set_color("warning_color", "Editor", warning_color);
	theme->set_color("error_color", "Editor", error_color);
	theme->set_color("property_color", "Editor", property_color);

	const int thumb_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
	theme->set_constant("scale", "Editor", EDSCALE);
	theme->set_constant("thumb_size", "Editor", thumb_size);
	theme->set_constant("dark_theme", "Editor", dark_theme);
	theme->set_constant("gizmo_handle_scale", "Editor", gizmo_handle_scale);

	//Register icons + font
	bool keep_old_icons = false;
	if (p_theme != nullptr) {
		keep_old_icons = fabs(p_theme->get_constant("scale", "Editor") - EDSCALE) < 0.00001 &&
				fabs(p_theme->get_constant("gizmo_handle_scale", "Editor") - gizmo_handle_scale) < 0.00001 &&
				(bool)p_theme->get_constant("dark_theme", "Editor") == dark_theme;
	}

	// the resolution and the icon color (dark_theme bool) has not changed, so we do not regenerate the icons
	if (keep_old_icons) {
		// register already generated icons
		for (int i = 0; i < editor_icons_count; i++) {
			theme->set_icon(editor_icons_names[i], "EditorIcons", p_theme->get_icon(editor_icons_names[i], "EditorIcons"));
		}
	} else {
		editor_register_and_generate_icons(theme, dark_theme, thumb_size);
	}
	// thumbnail size has changed, so we regenerate the medium sizes
	if (p_theme != nullptr && fabs((double)p_theme->get_constant("thumb_size", "Editor") - thumb_size) > 0.00001) {
		editor_register_and_generate_icons(p_theme, dark_theme, thumb_size, true);
	}

	editor_register_fonts(theme);

	// Highlighted tabs and border width
	Color tab_color = highlight_tabs ? base_color.linear_interpolate(font_color, contrast) : base_color;
	// Ensure borders are visible when using an editor scale below 100%.
	const int border_width = CLAMP(border_size, 0, 3) * MAX(1, EDSCALE);

	const int default_margin_size = 4;
	const int margin_size_extra = default_margin_size + CLAMP(border_size, 0, 3);

	// styleboxes
	// this is the most commonly used stylebox, variations should be made as duplicate of this
	Ref<StyleBoxFlat> style_default = make_flat_stylebox(base_color, default_margin_size, default_margin_size, default_margin_size, default_margin_size);
	style_default->set_border_width_all(border_width);
	style_default->set_border_color(base_color);
	style_default->set_draw_center(true);

	// Button and widgets
	const float extra_spacing = EDITOR_GET("interface/theme/additional_spacing");

	Ref<StyleBoxFlat> style_widget = style_default->duplicate();
	style_widget->set_default_margin(MARGIN_LEFT, (extra_spacing + 6) * EDSCALE);
	style_widget->set_default_margin(MARGIN_TOP, (extra_spacing + default_margin_size) * EDSCALE);
	style_widget->set_default_margin(MARGIN_RIGHT, (extra_spacing + 6) * EDSCALE);
	style_widget->set_default_margin(MARGIN_BOTTOM, (extra_spacing + default_margin_size) * EDSCALE);
	style_widget->set_bg_color(dark_color_1);
	style_widget->set_border_color(dark_color_2);

	Ref<StyleBoxFlat> style_widget_disabled = style_widget->duplicate();
	style_widget_disabled->set_border_color(color_disabled);
	style_widget_disabled->set_bg_color(color_disabled_bg);

	Ref<StyleBoxFlat> style_widget_focus = style_widget->duplicate();
	style_widget_focus->set_border_color(accent_color);

	Ref<StyleBoxFlat> style_widget_pressed = style_widget->duplicate();
	style_widget_pressed->set_border_color(accent_color);

	Ref<StyleBoxFlat> style_widget_hover = style_widget->duplicate();
	style_widget_hover->set_border_color(contrast_color_1);

	// style for windows, popups, etc..
	Ref<StyleBoxFlat> style_popup = style_default->duplicate();
	const int popup_margin_size = default_margin_size * EDSCALE * 2;
	style_popup->set_default_margin(MARGIN_LEFT, popup_margin_size);
	style_popup->set_default_margin(MARGIN_TOP, popup_margin_size);
	style_popup->set_default_margin(MARGIN_RIGHT, popup_margin_size);
	style_popup->set_default_margin(MARGIN_BOTTOM, popup_margin_size);
	style_popup->set_border_color(contrast_color_1);
	style_popup->set_border_width_all(MAX(EDSCALE, border_width));
	const Color shadow_color = Color(0, 0, 0, dark_theme ? 0.3 : 0.1);
	style_popup->set_shadow_color(shadow_color);
	style_popup->set_shadow_size(4 * EDSCALE);

	Ref<StyleBoxLine> style_popup_separator(memnew(StyleBoxLine));
	style_popup_separator->set_color(separator_color);
	style_popup_separator->set_grow_begin(popup_margin_size - MAX(EDSCALE, border_width));
	style_popup_separator->set_grow_end(popup_margin_size - MAX(EDSCALE, border_width));
	style_popup_separator->set_thickness(MAX(EDSCALE, border_width));

	Ref<StyleBoxLine> style_popup_labeled_separator_left(memnew(StyleBoxLine));
	style_popup_labeled_separator_left->set_grow_begin(popup_margin_size - MAX(EDSCALE, border_width));
	style_popup_labeled_separator_left->set_color(separator_color);
	style_popup_labeled_separator_left->set_thickness(MAX(EDSCALE, border_width));

	Ref<StyleBoxLine> style_popup_labeled_separator_right(memnew(StyleBoxLine));
	style_popup_labeled_separator_right->set_grow_end(popup_margin_size - MAX(EDSCALE, border_width));
	style_popup_labeled_separator_right->set_color(separator_color);
	style_popup_labeled_separator_right->set_thickness(MAX(EDSCALE, border_width));

	Ref<StyleBoxEmpty> style_empty = make_empty_stylebox(default_margin_size, default_margin_size, default_margin_size, default_margin_size);

	// Tabs

	const int tab_default_margin_side = 10 * EDSCALE + extra_spacing * EDSCALE;
	const int tab_default_margin_vertical = 5 * EDSCALE + extra_spacing * EDSCALE;

	Ref<StyleBoxFlat> style_tab_selected = style_widget->duplicate();

	style_tab_selected->set_border_width_all(border_width);
	style_tab_selected->set_border_width(MARGIN_BOTTOM, 0);
	style_tab_selected->set_border_color(dark_color_3);
	style_tab_selected->set_expand_margin_size(MARGIN_BOTTOM, border_width);
	style_tab_selected->set_default_margin(MARGIN_LEFT, tab_default_margin_side);
	style_tab_selected->set_default_margin(MARGIN_RIGHT, tab_default_margin_side);
	style_tab_selected->set_default_margin(MARGIN_BOTTOM, tab_default_margin_vertical);
	style_tab_selected->set_default_margin(MARGIN_TOP, tab_default_margin_vertical);
	style_tab_selected->set_bg_color(tab_color);

	Ref<StyleBoxFlat> style_tab_unselected = style_tab_selected->duplicate();
	style_tab_unselected->set_bg_color(dark_color_1);
	style_tab_unselected->set_border_color(dark_color_2);

	Ref<StyleBoxFlat> style_tab_disabled = style_tab_selected->duplicate();
	style_tab_disabled->set_bg_color(color_disabled_bg);
	style_tab_disabled->set_border_color(color_disabled);

	// Editor background
	Color background_color_opaque = background_color;
	background_color_opaque.a = 1.0;
	theme->set_stylebox("Background", "EditorStyles", make_flat_stylebox(background_color_opaque, default_margin_size, default_margin_size, default_margin_size, default_margin_size));

	// Focus
	Ref<StyleBoxFlat> style_focus = style_default->duplicate();
	style_focus->set_draw_center(false);
	style_focus->set_border_color(contrast_color_2);
	theme->set_stylebox("Focus", "EditorStyles", style_focus);

	// Menu
	Ref<StyleBoxFlat> style_menu = style_widget->duplicate();
	style_menu->set_draw_center(false);
	style_menu->set_border_width_all(0);
	theme->set_stylebox("panel", "PanelContainer", style_menu);
	theme->set_stylebox("MenuPanel", "EditorStyles", style_menu);

	// CanvasItem Editor
	Ref<StyleBoxFlat> style_canvas_editor_info = make_flat_stylebox(Color(0.0, 0.0, 0.0, 0.2));
	style_canvas_editor_info->set_expand_margin_size_all(4 * EDSCALE);
	theme->set_stylebox("CanvasItemInfoOverlay", "EditorStyles", style_canvas_editor_info);

	// Script Editor
	theme->set_stylebox("ScriptEditorPanel", "EditorStyles", make_empty_stylebox(default_margin_size, 0, default_margin_size, default_margin_size));
	theme->set_stylebox("ScriptEditor", "EditorStyles", make_empty_stylebox(0, 0, 0, 0));

	// Play button group
	theme->set_stylebox("PlayButtonPanel", "EditorStyles", style_empty);

	//MenuButton
	Ref<StyleBoxFlat> style_menu_hover_border = style_widget->duplicate();
	style_menu_hover_border->set_draw_center(false);
	style_menu_hover_border->set_border_width_all(0);
	style_menu_hover_border->set_border_width(MARGIN_BOTTOM, border_width);
	style_menu_hover_border->set_border_color(accent_color);

	Ref<StyleBoxFlat> style_menu_hover_bg = style_widget->duplicate();
	style_menu_hover_bg->set_border_width_all(0);
	style_menu_hover_bg->set_bg_color(dark_color_1);

	theme->set_stylebox("normal", "MenuButton", style_menu);
	theme->set_stylebox("hover", "MenuButton", style_menu);
	theme->set_stylebox("pressed", "MenuButton", style_menu);
	theme->set_stylebox("focus", "MenuButton", style_menu);
	theme->set_stylebox("disabled", "MenuButton", style_menu);

	theme->set_stylebox("normal", "PopupMenu", style_menu);
	theme->set_stylebox("hover", "PopupMenu", style_menu_hover_bg);
	theme->set_stylebox("pressed", "PopupMenu", style_menu);
	theme->set_stylebox("focus", "PopupMenu", style_menu);
	theme->set_stylebox("disabled", "PopupMenu", style_menu);

	theme->set_stylebox("normal", "ToolButton", style_menu);
	theme->set_stylebox("hover", "ToolButton", style_menu);
	theme->set_stylebox("pressed", "ToolButton", style_menu);
	theme->set_stylebox("focus", "ToolButton", style_menu);
	theme->set_stylebox("disabled", "ToolButton", style_menu);

	theme->set_color("font_color", "MenuButton", font_color);
	theme->set_color("font_color_hover", "MenuButton", font_color_hl);
	theme->set_color("font_color_focus", "MenuButton", font_color_focus);
	theme->set_color("font_color", "ToolButton", font_color);
	theme->set_color("font_color_hover", "ToolButton", font_color_hl);
	theme->set_color("font_color_focus", "ToolButton", font_color_focus);
	theme->set_color("font_color_pressed", "ToolButton", accent_color);

	theme->set_stylebox("MenuHover", "EditorStyles", style_menu_hover_border);

	// Buttons
	theme->set_stylebox("normal", "Button", style_widget);
	theme->set_stylebox("hover", "Button", style_widget_hover);
	theme->set_stylebox("pressed", "Button", style_widget_pressed);
	theme->set_stylebox("focus", "Button", style_widget_focus);
	theme->set_stylebox("disabled", "Button", style_widget_disabled);

	theme->set_color("font_color", "Button", font_color);
	theme->set_color("font_color_hover", "Button", font_color_hl);
	theme->set_color("font_color_focus", "Button", font_color_focus);
	theme->set_color("font_color_pressed", "Button", accent_color);
	theme->set_color("font_color_disabled", "Button", font_color_disabled);

	theme->set_color("icon_color_normal", "Button", icon_color_normal);
	theme->set_color("icon_color_hover", "Button", icon_color_hover);
	theme->set_color("icon_color_pressed", "Button", icon_color_pressed);
	theme->set_color("icon_color_disabled", "Button", icon_color_disabled);

	// OptionButton
	theme->set_stylebox("normal", "OptionButton", style_widget);
	theme->set_stylebox("hover", "OptionButton", style_widget_hover);
	theme->set_stylebox("pressed", "OptionButton", style_widget_pressed);
	theme->set_stylebox("focus", "OptionButton", style_widget_focus);
	theme->set_stylebox("disabled", "OptionButton", style_widget_disabled);

	theme->set_color("font_color", "OptionButton", font_color);
	theme->set_color("font_color_hover", "OptionButton", font_color_hl);
	theme->set_color("font_color_focus", "OptionButton", font_color_focus);
	theme->set_color("font_color_pressed", "OptionButton", accent_color);
	theme->set_color("font_color_disabled", "OptionButton", font_color_disabled);

	theme->set_color("icon_color_hover", "OptionButton", icon_color_hover);
	theme->set_color("icon_color_pressed", "OptionButton", icon_color_pressed);
	theme->set_color("icon_color_disabled", "OptionButton", icon_color_disabled);

	theme->set_icon("arrow", "OptionButton", theme->get_icon("GuiOptionArrow", "EditorIcons"));
	theme->set_constant("arrow_margin", "OptionButton", default_margin_size * EDSCALE);
	theme->set_constant("modulate_arrow", "OptionButton", true);
	theme->set_constant("hseparation", "OptionButton", 4 * EDSCALE);

	// CheckButton
	theme->set_stylebox("normal", "CheckButton", style_menu);
	theme->set_stylebox("pressed", "CheckButton", style_menu);
	theme->set_stylebox("disabled", "CheckButton", style_menu);
	theme->set_stylebox("hover", "CheckButton", style_menu);

	theme->set_icon("on", "CheckButton", theme->get_icon("GuiToggleOn", "EditorIcons"));
	theme->set_icon("on_disabled", "CheckButton", theme->get_icon("GuiToggleOnDisabled", "EditorIcons"));
	theme->set_icon("off", "CheckButton", theme->get_icon("GuiToggleOff", "EditorIcons"));
	theme->set_icon("off_disabled", "CheckButton", theme->get_icon("GuiToggleOffDisabled", "EditorIcons"));

	theme->set_color("font_color", "CheckButton", font_color);
	theme->set_color("font_color_hover", "CheckButton", font_color_hl);
	theme->set_color("font_color_focus", "CheckButton", font_color_focus);
	theme->set_color("font_color_pressed", "CheckButton", accent_color);
	theme->set_color("font_color_disabled", "CheckButton", font_color_disabled);

	theme->set_color("icon_color_hover", "CheckButton", icon_color_hover);
	theme->set_color("icon_color_pressed", "CheckButton", icon_color_pressed);
	theme->set_color("icon_color_disabled", "CheckButton", icon_color_disabled);

	theme->set_constant("hseparation", "CheckButton", 4 * EDSCALE);
	theme->set_constant("check_vadjust", "CheckButton", 0);

	// Checkbox
	Ref<StyleBoxFlat> sb_checkbox = style_menu->duplicate();
	sb_checkbox->set_default_margin(MARGIN_LEFT, default_margin_size * EDSCALE);
	sb_checkbox->set_default_margin(MARGIN_RIGHT, default_margin_size * EDSCALE);
	sb_checkbox->set_default_margin(MARGIN_TOP, default_margin_size * EDSCALE);
	sb_checkbox->set_default_margin(MARGIN_BOTTOM, default_margin_size * EDSCALE);

	theme->set_stylebox("normal", "CheckBox", sb_checkbox);
	theme->set_stylebox("pressed", "CheckBox", sb_checkbox);
	theme->set_stylebox("disabled", "CheckBox", sb_checkbox);
	theme->set_stylebox("hover", "CheckBox", sb_checkbox);
	theme->set_icon("checked", "CheckBox", theme->get_icon("GuiChecked", "EditorIcons"));
	theme->set_icon("unchecked", "CheckBox", theme->get_icon("GuiUnchecked", "EditorIcons"));
	theme->set_icon("checked_disabled", "CheckBox", theme->get_icon("GuiCheckedDisabled", "EditorIcons"));
	theme->set_icon("unchecked_disabled", "CheckBox", theme->get_icon("GuiUncheckedDisabled", "EditorIcons"));
	theme->set_icon("radio_checked", "CheckBox", theme->get_icon("GuiRadioChecked", "EditorIcons"));
	theme->set_icon("radio_unchecked", "CheckBox", theme->get_icon("GuiRadioUnchecked", "EditorIcons"));
	theme->set_icon("radio_checked_disabled", "CheckBox", theme->get_icon("GuiRadioCheckedDisabled", "EditorIcons"));
	theme->set_icon("radio_unchecked_disabled", "CheckBox", theme->get_icon("GuiRadioUncheckedDisabled", "EditorIcons"));

	theme->set_color("font_color", "CheckBox", font_color);
	theme->set_color("font_color_hover", "CheckBox", font_color_hl);
	theme->set_color("font_color_focus", "CheckBox", font_color_focus);
	theme->set_color("font_color_pressed", "CheckBox", accent_color);
	theme->set_color("font_color_disabled", "CheckBox", font_color_disabled);

	theme->set_color("icon_color_hover", "CheckBox", icon_color_hover);
	theme->set_color("icon_color_pressed", "CheckBox", icon_color_pressed);
	theme->set_color("icon_color_disabled", "CheckBox", icon_color_disabled);

	theme->set_constant("hseparation", "CheckBox", 4 * EDSCALE);
	theme->set_constant("check_vadjust", "CheckBox", 0);

	// PopupDialog
	theme->set_stylebox("panel", "PopupDialog", style_popup);

	// PopupMenu
	theme->set_stylebox("panel", "PopupMenu", style_popup);
	theme->set_stylebox("separator", "PopupMenu", style_popup_separator);
	theme->set_stylebox("labeled_separator_left", "PopupMenu", style_popup_labeled_separator_left);
	theme->set_stylebox("labeled_separator_right", "PopupMenu", style_popup_labeled_separator_right);

	theme->set_color("font_color", "PopupMenu", font_color);
	theme->set_color("font_color_hover", "PopupMenu", font_color_hl);
	theme->set_color("font_color_accel", "PopupMenu", font_color_disabled);
	theme->set_color("font_color_disabled", "PopupMenu", font_color_disabled);
	theme->set_color("font_color_separator", "PopupMenu", font_color_disabled);
	theme->set_icon("checked", "PopupMenu", theme->get_icon("GuiChecked", "EditorIcons"));
	theme->set_icon("unchecked", "PopupMenu", theme->get_icon("GuiUnchecked", "EditorIcons"));
	theme->set_icon("radio_checked", "PopupMenu", theme->get_icon("GuiRadioChecked", "EditorIcons"));
	theme->set_icon("radio_unchecked", "PopupMenu", theme->get_icon("GuiRadioUnchecked", "EditorIcons"));
	theme->set_icon("submenu", "PopupMenu", theme->get_icon("ArrowRight", "EditorIcons"));
	theme->set_icon("visibility_hidden", "PopupMenu", theme->get_icon("GuiVisibilityHidden", "EditorIcons"));
	theme->set_icon("visibility_visible", "PopupMenu", theme->get_icon("GuiVisibilityVisible", "EditorIcons"));
	theme->set_icon("visibility_xray", "PopupMenu", theme->get_icon("GuiVisibilityXray", "EditorIcons"));
	theme->set_constant("vseparation", "PopupMenu", (extra_spacing + default_margin_size + 1) * EDSCALE);

	for (int i = 0; i < 16; i++) {
		Color si_base_color = accent_color;

		float hue_rotate = (i * 2 % 16) / 16.0;
		si_base_color.set_hsv(Math::fmod(float(si_base_color.get_h() + hue_rotate), float(1.0)), si_base_color.get_s(), si_base_color.get_v());
		si_base_color = accent_color.linear_interpolate(si_base_color, float(EDITOR_GET("docks/property_editor/subresource_hue_tint")));

		Ref<StyleBoxFlat> sub_inspector_bg;

		sub_inspector_bg = make_flat_stylebox(dark_color_1.linear_interpolate(si_base_color, 0.08), 2, 0, 2, 2);

		sub_inspector_bg->set_border_width_all(2);
		sub_inspector_bg->set_default_margin(MARGIN_LEFT, 4 * EDSCALE);
		sub_inspector_bg->set_default_margin(MARGIN_RIGHT, 4 * EDSCALE);
		sub_inspector_bg->set_default_margin(MARGIN_BOTTOM, 4 * EDSCALE);
		sub_inspector_bg->set_default_margin(MARGIN_TOP, 4 * EDSCALE);
		sub_inspector_bg->set_border_color(si_base_color * Color(0.7, 0.7, 0.7, 0.8));
		sub_inspector_bg->set_draw_center(true);

		theme->set_stylebox("sub_inspector_bg" + itos(i), "Editor", sub_inspector_bg);

		Ref<StyleBoxFlat> bg_color;
		bg_color.instance();
		bg_color->set_bg_color(si_base_color * Color(0.7, 0.7, 0.7, 0.8));
		bg_color->set_border_width_all(0);

		Ref<StyleBoxFlat> bg_color_selected;
		bg_color_selected.instance();
		bg_color_selected->set_border_width_all(0);
		bg_color_selected->set_bg_color(si_base_color * Color(0.8, 0.8, 0.8, 0.8));

		theme->set_stylebox("sub_inspector_property_bg" + itos(i), "Editor", bg_color);
		theme->set_stylebox("sub_inspector_property_bg_selected" + itos(i), "Editor", bg_color_selected);
	}

	theme->set_color("sub_inspector_property_color", "Editor", dark_theme ? Color(1, 1, 1, 1) : Color(0, 0, 0, 1));
	theme->set_constant("sub_inspector_font_offset", "Editor", 4 * EDSCALE);

	Ref<StyleBoxFlat> style_property_bg = style_default->duplicate();
	style_property_bg->set_bg_color(highlight_color);
	style_property_bg->set_border_width_all(0);

	theme->set_constant("font_offset", "EditorProperty", 1 * EDSCALE);
	theme->set_stylebox("bg_selected", "EditorProperty", style_property_bg);
	theme->set_stylebox("bg", "EditorProperty", Ref<StyleBoxEmpty>(memnew(StyleBoxEmpty)));
	theme->set_constant("vseparation", "EditorProperty", (extra_spacing + default_margin_size) * EDSCALE);
	theme->set_color("warning_color", "EditorProperty", warning_color);
	theme->set_color("property_color", "EditorProperty", property_color);

	theme->set_constant("inspector_margin", "Editor", 8 * EDSCALE);

	// Tree & ItemList background
	Ref<StyleBoxFlat> style_tree_bg = style_default->duplicate();
	style_tree_bg->set_bg_color(dark_color_1);
	style_tree_bg->set_border_color(dark_color_3);
	theme->set_stylebox("bg", "Tree", style_tree_bg);

	const Color guide_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.05);
	Color relationship_line_color = Color(mono_color.r, mono_color.g, mono_color.b, relationship_line_opacity);
	// Tree
	theme->set_icon("checked", "Tree", theme->get_icon("GuiChecked", "EditorIcons"));
	theme->set_icon("unchecked", "Tree", theme->get_icon("GuiUnchecked", "EditorIcons"));
	theme->set_icon("arrow", "Tree", theme->get_icon("GuiTreeArrowDown", "EditorIcons"));
	theme->set_icon("arrow_collapsed", "Tree", theme->get_icon("GuiTreeArrowRight", "EditorIcons"));
	theme->set_icon("updown", "Tree", theme->get_icon("GuiTreeUpdown", "EditorIcons"));
	theme->set_icon("select_arrow", "Tree", theme->get_icon("GuiDropdown", "EditorIcons"));
	theme->set_stylebox("bg_focus", "Tree", style_focus);
	theme->set_stylebox("custom_button", "Tree", make_empty_stylebox());
	theme->set_stylebox("custom_button_pressed", "Tree", make_empty_stylebox());
	theme->set_stylebox("custom_button_hover", "Tree", style_widget);
	theme->set_color("custom_button_font_highlight", "Tree", font_color_hl);
	theme->set_color("font_color", "Tree", font_color);
	theme->set_color("font_color_selected", "Tree", mono_color);
	theme->set_color("title_button_color", "Tree", font_color);
	theme->set_color("guide_color", "Tree", guide_color);
	theme->set_color("relationship_line_color", "Tree", relationship_line_color);
	theme->set_color("drop_position_color", "Tree", accent_color);
	theme->set_constant("vseparation", "Tree", (extra_spacing + default_margin_size) * EDSCALE);
	theme->set_constant("hseparation", "Tree", (extra_spacing + default_margin_size) * EDSCALE);
	theme->set_constant("item_margin", "Tree", 3 * default_margin_size * EDSCALE);
	theme->set_constant("button_margin", "Tree", default_margin_size * EDSCALE);
	theme->set_constant("draw_relationship_lines", "Tree", relationship_line_opacity >= 0.01);
	theme->set_constant("draw_guides", "Tree", relationship_line_opacity < 0.01);
	theme->set_constant("scroll_border", "Tree", 40 * EDSCALE);
	theme->set_constant("scroll_speed", "Tree", 12);

	Ref<StyleBoxFlat> style_tree_btn = style_default->duplicate();
	style_tree_btn->set_bg_color(contrast_color_1);
	style_tree_btn->set_border_width_all(0);
	theme->set_stylebox("button_pressed", "Tree", style_tree_btn);

	Ref<StyleBoxFlat> style_tree_hover = style_default->duplicate();
	style_tree_hover->set_bg_color(highlight_color * Color(1, 1, 1, 0.4));
	style_tree_hover->set_border_width_all(0);
	theme->set_stylebox("hover", "Tree", style_tree_hover);

	Ref<StyleBoxFlat> style_tree_focus = style_default->duplicate();
	style_tree_focus->set_bg_color(highlight_color);
	style_tree_focus->set_border_width_all(0);
	theme->set_stylebox("selected_focus", "Tree", style_tree_focus);

	Ref<StyleBoxFlat> style_tree_selected = style_tree_focus->duplicate();
	theme->set_stylebox("selected", "Tree", style_tree_selected);

	Ref<StyleBoxFlat> style_tree_cursor = style_default->duplicate();
	style_tree_cursor->set_draw_center(false);
	style_tree_cursor->set_border_width_all(MAX(1, border_width));
	style_tree_cursor->set_border_color(contrast_color_1);

	Ref<StyleBoxFlat> style_tree_title = style_default->duplicate();
	style_tree_title->set_bg_color(dark_color_3);
	style_tree_title->set_border_width_all(0);
	theme->set_stylebox("cursor", "Tree", style_tree_cursor);
	theme->set_stylebox("cursor_unfocused", "Tree", style_tree_cursor);
	theme->set_stylebox("title_button_normal", "Tree", style_tree_title);
	theme->set_stylebox("title_button_hover", "Tree", style_tree_title);
	theme->set_stylebox("title_button_pressed", "Tree", style_tree_title);

	Color prop_category_color = dark_color_1.linear_interpolate(mono_color, 0.12);
	Color prop_section_color = dark_color_1.linear_interpolate(mono_color, 0.09);
	Color prop_subsection_color = dark_color_1.linear_interpolate(mono_color, 0.06);
	theme->set_color("prop_category", "Editor", prop_category_color);
	theme->set_color("prop_section", "Editor", prop_section_color);
	theme->set_color("prop_subsection", "Editor", prop_subsection_color);
	theme->set_color("drop_position_color", "Tree", accent_color);

	// ItemList
	Ref<StyleBoxFlat> style_itemlist_bg = style_default->duplicate();
	style_itemlist_bg->set_bg_color(dark_color_1);
	style_itemlist_bg->set_border_width_all(border_width);
	style_itemlist_bg->set_border_color(dark_color_3);

	Ref<StyleBoxFlat> style_itemlist_cursor = style_default->duplicate();
	style_itemlist_cursor->set_draw_center(false);
	style_itemlist_cursor->set_border_width_all(border_width);
	style_itemlist_cursor->set_border_color(highlight_color);
	theme->set_stylebox("cursor", "ItemList", style_itemlist_cursor);
	theme->set_stylebox("cursor_unfocused", "ItemList", style_itemlist_cursor);
	theme->set_stylebox("selected_focus", "ItemList", style_tree_focus);
	theme->set_stylebox("selected", "ItemList", style_tree_selected);
	theme->set_stylebox("bg_focus", "ItemList", style_focus);
	theme->set_stylebox("bg", "ItemList", style_itemlist_bg);
	theme->set_color("font_color", "ItemList", font_color);
	theme->set_color("font_color_selected", "ItemList", mono_color);
	theme->set_color("guide_color", "ItemList", guide_color);
	theme->set_constant("vseparation", "ItemList", 3 * EDSCALE);
	theme->set_constant("hseparation", "ItemList", 3 * EDSCALE);
	theme->set_constant("icon_margin", "ItemList", default_margin_size * EDSCALE);
	theme->set_constant("line_separation", "ItemList", 3 * EDSCALE);

	// Tabs & TabContainer
	theme->set_stylebox("tab_fg", "TabContainer", style_tab_selected);
	theme->set_stylebox("tab_bg", "TabContainer", style_tab_unselected);
	theme->set_stylebox("tab_disabled", "TabContainer", style_tab_disabled);
	theme->set_stylebox("tab_fg", "Tabs", style_tab_selected);
	theme->set_stylebox("tab_bg", "Tabs", style_tab_unselected);
	theme->set_stylebox("tab_disabled", "Tabs", style_tab_disabled);
	theme->set_color("font_color_fg", "TabContainer", font_color);
	theme->set_color("font_color_bg", "TabContainer", font_color_disabled);
	theme->set_color("font_color_fg", "Tabs", font_color);
	theme->set_color("font_color_bg", "Tabs", font_color_disabled);
	theme->set_icon("menu", "TabContainer", theme->get_icon("GuiTabMenu", "EditorIcons"));
	theme->set_icon("menu_highlight", "TabContainer", theme->get_icon("GuiTabMenuHl", "EditorIcons"));
	theme->set_stylebox("SceneTabFG", "EditorStyles", style_tab_selected);
	theme->set_stylebox("SceneTabBG", "EditorStyles", style_tab_unselected);
	theme->set_icon("close", "Tabs", theme->get_icon("GuiClose", "EditorIcons"));
	theme->set_stylebox("button_pressed", "Tabs", style_menu);
	theme->set_stylebox("button", "Tabs", style_menu);
	theme->set_icon("increment", "TabContainer", theme->get_icon("GuiScrollArrowRight", "EditorIcons"));
	theme->set_icon("decrement", "TabContainer", theme->get_icon("GuiScrollArrowLeft", "EditorIcons"));
	theme->set_icon("increment", "Tabs", theme->get_icon("GuiScrollArrowRight", "EditorIcons"));
	theme->set_icon("decrement", "Tabs", theme->get_icon("GuiScrollArrowLeft", "EditorIcons"));
	theme->set_icon("increment_highlight", "Tabs", theme->get_icon("GuiScrollArrowRightHl", "EditorIcons"));
	theme->set_icon("decrement_highlight", "Tabs", theme->get_icon("GuiScrollArrowLeftHl", "EditorIcons"));
	theme->set_icon("increment_highlight", "TabContainer", theme->get_icon("GuiScrollArrowRightHl", "EditorIcons"));
	theme->set_icon("decrement_highlight", "TabContainer", theme->get_icon("GuiScrollArrowLeftHl", "EditorIcons"));
	theme->set_constant("hseparation", "Tabs", 4 * EDSCALE);

	// Content of each tab
	Ref<StyleBoxFlat> style_content_panel = style_default->duplicate();
	style_content_panel->set_border_color(dark_color_3);
	style_content_panel->set_border_width_all(border_width);
	// compensate the border
	style_content_panel->set_default_margin(MARGIN_TOP, margin_size_extra * EDSCALE);
	style_content_panel->set_default_margin(MARGIN_RIGHT, margin_size_extra * EDSCALE);
	style_content_panel->set_default_margin(MARGIN_BOTTOM, margin_size_extra * EDSCALE);
	style_content_panel->set_default_margin(MARGIN_LEFT, margin_size_extra * EDSCALE);

	// These styleboxes can be used on tabs against the base color background (e.g. nested tabs).
	Ref<StyleBoxFlat> style_tab_selected_odd = style_tab_selected->duplicate();
	style_tab_selected_odd->set_bg_color(color_disabled_bg);
	theme->set_stylebox("tab_selected_odd", "TabContainer", style_tab_selected_odd);

	Ref<StyleBoxFlat> style_content_panel_odd = style_content_panel->duplicate();
	style_content_panel_odd->set_bg_color(color_disabled_bg);
	theme->set_stylebox("panel_odd", "TabContainer", style_content_panel_odd);

	// This stylebox is used in 3d and 2d viewports (no borders).
	Ref<StyleBoxFlat> style_content_panel_vp = style_content_panel->duplicate();
	style_content_panel_vp->set_default_margin(MARGIN_LEFT, border_width * 2);
	style_content_panel_vp->set_default_margin(MARGIN_TOP, default_margin_size * EDSCALE);
	style_content_panel_vp->set_default_margin(MARGIN_RIGHT, border_width * 2);
	style_content_panel_vp->set_default_margin(MARGIN_BOTTOM, border_width * 2);
	theme->set_stylebox("panel", "TabContainer", style_content_panel);
	theme->set_stylebox("Content", "EditorStyles", style_content_panel_vp);

	// This stylebox is used by preview tabs in the Theme Editor.
	Ref<StyleBoxFlat> style_theme_preview_tab = style_tab_selected_odd->duplicate();
	style_theme_preview_tab->set_expand_margin_size(MARGIN_BOTTOM, 3 * EDSCALE);
	theme->set_stylebox("ThemeEditorPreviewFG", "EditorStyles", style_theme_preview_tab);
	Ref<StyleBoxFlat> style_theme_preview_bg_tab = style_tab_unselected->duplicate();
	style_theme_preview_bg_tab->set_expand_margin_size(MARGIN_BOTTOM, 2 * EDSCALE);
	theme->set_stylebox("ThemeEditorPreviewBG", "EditorStyles", style_theme_preview_bg_tab);

	// Separators
	theme->set_stylebox("separator", "HSeparator", make_line_stylebox(separator_color, border_width));
	theme->set_stylebox("separator", "VSeparator", make_line_stylebox(separator_color, border_width, 0, 0, true));

	// Debugger

	Ref<StyleBoxFlat> style_panel_debugger = style_content_panel->duplicate();
	style_panel_debugger->set_border_width(MARGIN_BOTTOM, 0);
	theme->set_stylebox("DebuggerPanel", "EditorStyles", style_panel_debugger);
	theme->set_stylebox("DebuggerTabFG", "EditorStyles", style_tab_selected);
	theme->set_stylebox("DebuggerTabBG", "EditorStyles", style_tab_unselected);

	Ref<StyleBoxFlat> style_panel_invisible_top = style_content_panel->duplicate();
	int stylebox_offset = theme->get_font("tab_fg", "TabContainer")->get_height() + theme->get_stylebox("tab_fg", "TabContainer")->get_minimum_size().height + theme->get_stylebox("panel", "TabContainer")->get_default_margin(MARGIN_TOP);
	style_panel_invisible_top->set_expand_margin_size(MARGIN_TOP, -stylebox_offset);
	style_panel_invisible_top->set_default_margin(MARGIN_TOP, 0);
	theme->set_stylebox("BottomPanelDebuggerOverride", "EditorStyles", style_panel_invisible_top);

	// LineEdit
	theme->set_stylebox("normal", "LineEdit", style_widget);
	theme->set_stylebox("focus", "LineEdit", style_widget_focus);
	theme->set_stylebox("read_only", "LineEdit", style_widget_disabled);
	theme->set_icon("clear", "LineEdit", theme->get_icon("GuiClose", "EditorIcons"));
	theme->set_color("read_only", "LineEdit", font_color_disabled);
	theme->set_color("font_color", "LineEdit", font_color);
	theme->set_color("font_color_selected", "LineEdit", mono_color);
	theme->set_color("font_color_uneditable", "LineEdit", font_color_readonly);
	theme->set_color("cursor_color", "LineEdit", font_color);
	theme->set_color("selection_color", "LineEdit", font_color_selection);
	theme->set_color("clear_button_color", "LineEdit", font_color);
	theme->set_color("clear_button_color_pressed", "LineEdit", accent_color);

	// TextEdit
	theme->set_stylebox("normal", "TextEdit", style_widget);
	theme->set_stylebox("focus", "TextEdit", style_widget_hover);
	theme->set_stylebox("read_only", "TextEdit", style_widget_disabled);
	theme->set_constant("side_margin", "TabContainer", 0);
	theme->set_icon("tab", "TextEdit", theme->get_icon("GuiTab", "EditorIcons"));
	theme->set_icon("space", "TextEdit", theme->get_icon("GuiSpace", "EditorIcons"));
	theme->set_icon("folded", "TextEdit", theme->get_icon("TextEditFolded", "EditorIcons"));
	theme->set_icon("fold", "TextEdit", theme->get_icon("TextEditFold", "EditorIcons"));
	theme->set_color("font_color", "TextEdit", font_color);
	theme->set_color("font_color_readonly", "TextEdit", font_color_readonly);
	theme->set_color("caret_color", "TextEdit", font_color);
	theme->set_color("selection_color", "TextEdit", font_color_selection);

	// H/VSplitContainer
	theme->set_stylebox("bg", "VSplitContainer", make_stylebox(theme->get_icon("GuiVsplitBg", "EditorIcons"), 1, 1, 1, 1));
	theme->set_stylebox("bg", "HSplitContainer", make_stylebox(theme->get_icon("GuiHsplitBg", "EditorIcons"), 1, 1, 1, 1));

	theme->set_icon("grabber", "VSplitContainer", theme->get_icon("GuiVsplitter", "EditorIcons"));
	theme->set_icon("grabber", "HSplitContainer", theme->get_icon("GuiHsplitter", "EditorIcons"));

	theme->set_constant("separation", "HSplitContainer", default_margin_size * 2 * EDSCALE);
	theme->set_constant("separation", "VSplitContainer", default_margin_size * 2 * EDSCALE);

	// Containers
	theme->set_constant("separation", "BoxContainer", default_margin_size * EDSCALE);
	theme->set_constant("separation", "HBoxContainer", default_margin_size * EDSCALE);
	theme->set_constant("separation", "VBoxContainer", default_margin_size * EDSCALE);
	theme->set_constant("margin_left", "MarginContainer", 0);
	theme->set_constant("margin_top", "MarginContainer", 0);
	theme->set_constant("margin_right", "MarginContainer", 0);
	theme->set_constant("margin_bottom", "MarginContainer", 0);
	theme->set_constant("hseparation", "GridContainer", default_margin_size * EDSCALE);
	theme->set_constant("vseparation", "GridContainer", default_margin_size * EDSCALE);
	theme->set_constant("hseparation", "FlowContainer", default_margin_size * EDSCALE);
	theme->set_constant("vseparation", "FlowContainer", default_margin_size * EDSCALE);
	theme->set_constant("hseparation", "HFlowContainer", default_margin_size * EDSCALE);
	theme->set_constant("vseparation", "HFlowContainer", default_margin_size * EDSCALE);
	theme->set_constant("hseparation", "VFlowContainer", default_margin_size * EDSCALE);
	theme->set_constant("vseparation", "VFlowContainer", default_margin_size * EDSCALE);

	// WindowDialog
	Ref<StyleBoxFlat> style_window = style_popup->duplicate();
	style_window->set_border_color(tab_color);
	style_window->set_border_width(MARGIN_TOP, 24 * EDSCALE);
	style_window->set_expand_margin_size(MARGIN_TOP, 24 * EDSCALE);
	theme->set_stylebox("panel", "WindowDialog", style_window);
	theme->set_color("title_color", "WindowDialog", font_color);
	theme->set_icon("close", "WindowDialog", theme->get_icon("GuiClose", "EditorIcons"));
	theme->set_icon("close_highlight", "WindowDialog", theme->get_icon("GuiClose", "EditorIcons"));
	theme->set_constant("close_h_ofs", "WindowDialog", 22 * EDSCALE);
	theme->set_constant("close_v_ofs", "WindowDialog", 20 * EDSCALE);
	theme->set_constant("title_height", "WindowDialog", 24 * EDSCALE);
	theme->set_font("title_font", "WindowDialog", theme->get_font("title", "EditorFonts"));

	// complex window, for now only Editor settings and Project settings
	Ref<StyleBoxFlat> style_complex_window = style_window->duplicate();
	style_complex_window->set_bg_color(dark_color_2);
	style_complex_window->set_border_color(highlight_tabs ? tab_color : dark_color_2);
	theme->set_stylebox("panel", "EditorSettingsDialog", style_complex_window);
	theme->set_stylebox("panel", "ProjectSettingsEditor", style_complex_window);
	theme->set_stylebox("panel", "EditorAbout", style_complex_window);

	// HScrollBar
	Ref<Texture> empty_icon = memnew(ImageTexture);

	if (increase_scrollbar_touch_area) {
		theme->set_stylebox("scroll", "HScrollBar", make_line_stylebox(separator_color, 50));
	} else {
		theme->set_stylebox("scroll", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
	}
	theme->set_stylebox("scroll_focus", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
	theme->set_stylebox("grabber", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabber", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));
	theme->set_stylebox("grabber_highlight", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberHl", "EditorIcons"), 5, 5, 5, 5, 2, 2, 2, 2));
	theme->set_stylebox("grabber_pressed", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberPressed", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));

	theme->set_icon("increment", "HScrollBar", empty_icon);
	theme->set_icon("increment_highlight", "HScrollBar", empty_icon);
	theme->set_icon("increment_pressed", "HScrollBar", empty_icon);
	theme->set_icon("decrement", "HScrollBar", empty_icon);
	theme->set_icon("decrement_highlight", "HScrollBar", empty_icon);
	theme->set_icon("decrement_pressed", "HScrollBar", empty_icon);

	// VScrollBar
	if (increase_scrollbar_touch_area) {
		theme->set_stylebox("scroll", "VScrollBar", make_line_stylebox(separator_color, 50, 1, 1, true));
	} else {
		theme->set_stylebox("scroll", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
	}
	theme->set_stylebox("scroll_focus", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
	theme->set_stylebox("grabber", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabber", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));
	theme->set_stylebox("grabber_highlight", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberHl", "EditorIcons"), 5, 5, 5, 5, 2, 2, 2, 2));
	theme->set_stylebox("grabber_pressed", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberPressed", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));

	theme->set_icon("increment", "VScrollBar", empty_icon);
	theme->set_icon("increment_highlight", "VScrollBar", empty_icon);
	theme->set_icon("increment_pressed", "VScrollBar", empty_icon);
	theme->set_icon("decrement", "VScrollBar", empty_icon);
	theme->set_icon("decrement_highlight", "VScrollBar", empty_icon);
	theme->set_icon("decrement_pressed", "VScrollBar", empty_icon);

	// HSlider
	theme->set_icon("grabber_highlight", "HSlider", theme->get_icon("GuiSliderGrabberHl", "EditorIcons"));
	theme->set_icon("grabber", "HSlider", theme->get_icon("GuiSliderGrabber", "EditorIcons"));
	theme->set_stylebox("slider", "HSlider", make_flat_stylebox(dark_color_3, 0, default_margin_size / 2, 0, default_margin_size / 2));
	theme->set_stylebox("grabber_area", "HSlider", make_flat_stylebox(contrast_color_1, 0, default_margin_size / 2, 0, default_margin_size / 2));
	theme->set_stylebox("grabber_area_highlight", "HSlider", make_flat_stylebox(contrast_color_1, 0, default_margin_size / 2, 0, default_margin_size / 2));

	// VSlider
	theme->set_icon("grabber", "VSlider", theme->get_icon("GuiSliderGrabber", "EditorIcons"));
	theme->set_icon("grabber_highlight", "VSlider", theme->get_icon("GuiSliderGrabberHl", "EditorIcons"));
	theme->set_stylebox("slider", "VSlider", make_flat_stylebox(dark_color_3, default_margin_size / 2, 0, default_margin_size / 2, 0));
	theme->set_stylebox("grabber_area", "VSlider", make_flat_stylebox(contrast_color_1, default_margin_size / 2, 0, default_margin_size / 2, 0));
	theme->set_stylebox("grabber_area_highlight", "VSlider", make_flat_stylebox(contrast_color_1, default_margin_size / 2, 0, default_margin_size / 2, 0));

	//RichTextLabel
	theme->set_color("default_color", "RichTextLabel", font_color);
	theme->set_color("font_color_shadow", "RichTextLabel", Color(0, 0, 0, 0));
	theme->set_constant("shadow_offset_x", "RichTextLabel", 1 * EDSCALE);
	theme->set_constant("shadow_offset_y", "RichTextLabel", 1 * EDSCALE);
	theme->set_constant("shadow_as_outline", "RichTextLabel", 0);
	theme->set_stylebox("focus", "RichTextLabel", make_empty_stylebox());
	theme->set_stylebox("normal", "RichTextLabel", style_tree_bg);

	theme->set_color("headline_color", "EditorHelp", mono_color);

	// Panel
	theme->set_stylebox("panel", "Panel", make_flat_stylebox(dark_color_1, 6, 4, 6, 4));

	// Label
	theme->set_stylebox("normal", "Label", style_empty);
	theme->set_color("font_color", "Label", font_color);
	theme->set_color("font_color_shadow", "Label", Color(0, 0, 0, 0));
	theme->set_constant("shadow_offset_x", "Label", 1 * EDSCALE);
	theme->set_constant("shadow_offset_y", "Label", 1 * EDSCALE);
	theme->set_constant("shadow_as_outline", "Label", 0);
	theme->set_constant("line_spacing", "Label", 3 * EDSCALE);

	// LinkButton
	theme->set_stylebox("focus", "LinkButton", style_empty);
	theme->set_color("font_color", "LinkButton", font_color);
	theme->set_color("font_color_hover", "LinkButton", font_color_hl);
	theme->set_color("font_color_focus", "LinkButton", font_color_focus);
	theme->set_color("font_color_pressed", "LinkButton", accent_color);
	theme->set_color("font_color_disabled", "LinkButton", font_color_disabled);

	// TooltipPanel
	Ref<StyleBoxFlat> style_tooltip = style_popup->duplicate();
	float v = MAX(border_size * EDSCALE, 1.0);
	style_tooltip->set_default_margin(MARGIN_LEFT, v);
	style_tooltip->set_default_margin(MARGIN_TOP, v);
	style_tooltip->set_default_margin(MARGIN_RIGHT, v);
	style_tooltip->set_default_margin(MARGIN_BOTTOM, v);
	style_tooltip->set_bg_color(Color(mono_color.r, mono_color.g, mono_color.b, 0.9));
	style_tooltip->set_border_width_all(border_width);
	style_tooltip->set_border_color(mono_color);
	theme->set_color("font_color", "TooltipLabel", font_color.inverted());
	theme->set_color("font_color_shadow", "TooltipLabel", mono_color.inverted() * Color(1, 1, 1, 0.1));
	theme->set_stylebox("panel", "TooltipPanel", style_tooltip);

	// PopupPanel
	theme->set_stylebox("panel", "PopupPanel", style_popup);

	// SpinBox
	theme->set_icon("updown", "SpinBox", theme->get_icon("GuiSpinboxUpdown", "EditorIcons"));

	// ProgressBar
	theme->set_stylebox("bg", "ProgressBar", make_stylebox(theme->get_icon("GuiProgressBar", "EditorIcons"), 4, 4, 4, 4, 0, 0, 0, 0));
	theme->set_stylebox("fg", "ProgressBar", make_stylebox(theme->get_icon("GuiProgressFill", "EditorIcons"), 6, 6, 6, 6, 2, 1, 2, 1));
	theme->set_color("font_color", "ProgressBar", font_color);

	// GraphEdit
	theme->set_stylebox("bg", "GraphEdit", style_tree_bg);
	if (dark_theme) {
		theme->set_color("grid_major", "GraphEdit", Color(1.0, 1.0, 1.0, 0.15));
		theme->set_color("grid_minor", "GraphEdit", Color(1.0, 1.0, 1.0, 0.07));
	} else {
		theme->set_color("grid_major", "GraphEdit", Color(0.0, 0.0, 0.0, 0.15));
		theme->set_color("grid_minor", "GraphEdit", Color(0.0, 0.0, 0.0, 0.07));
	}
	theme->set_color("selection_fill", "GraphEdit", theme->get_color("box_selection_fill_color", "Editor"));
	theme->set_color("selection_stroke", "GraphEdit", theme->get_color("box_selection_stroke_color", "Editor"));
	theme->set_color("activity", "GraphEdit", accent_color);
	theme->set_icon("minus", "GraphEdit", theme->get_icon("ZoomLess", "EditorIcons"));
	theme->set_icon("more", "GraphEdit", theme->get_icon("ZoomMore", "EditorIcons"));
	theme->set_icon("reset", "GraphEdit", theme->get_icon("ZoomReset", "EditorIcons"));
	theme->set_icon("snap", "GraphEdit", theme->get_icon("SnapGrid", "EditorIcons"));
	theme->set_icon("minimap", "GraphEdit", theme->get_icon("GridMinimap", "EditorIcons"));
	theme->set_constant("bezier_len_pos", "GraphEdit", 80 * EDSCALE);
	theme->set_constant("bezier_len_neg", "GraphEdit", 160 * EDSCALE);

	// GraphEditMinimap
	Ref<StyleBoxFlat> style_minimap_bg = make_flat_stylebox(dark_color_1, 0, 0, 0, 0);
	style_minimap_bg->set_border_color(dark_color_3);
	style_minimap_bg->set_border_width_all(1);
	theme->set_stylebox("bg", "GraphEditMinimap", style_minimap_bg);

	Ref<StyleBoxFlat> style_minimap_camera;
	Ref<StyleBoxFlat> style_minimap_node;
	if (dark_theme) {
		style_minimap_camera = make_flat_stylebox(Color(0.65, 0.65, 0.65, 0.2), 0, 0, 0, 0);
		style_minimap_camera->set_border_color(Color(0.65, 0.65, 0.65, 0.45));
		style_minimap_node = make_flat_stylebox(Color(1, 1, 1), 0, 0, 0, 0);
	} else {
		style_minimap_camera = make_flat_stylebox(Color(0.38, 0.38, 0.38, 0.2), 0, 0, 0, 0);
		style_minimap_camera->set_border_color(Color(0.38, 0.38, 0.38, 0.45));
		style_minimap_node = make_flat_stylebox(Color(0, 0, 0), 0, 0, 0, 0);
	}
	style_minimap_camera->set_border_width_all(1);
	style_minimap_node->set_corner_radius_all(1);
	theme->set_stylebox("camera", "GraphEditMinimap", style_minimap_camera);
	theme->set_stylebox("node", "GraphEditMinimap", style_minimap_node);

	Ref<Texture> minimap_resizer_icon = theme->get_icon("GuiResizer", "EditorIcons");
	Color minimap_resizer_color;
	if (dark_theme) {
		minimap_resizer_color = Color(1, 1, 1, 0.65);
	} else {
		minimap_resizer_color = Color(0, 0, 0, 0.65);
	}
	theme->set_icon("resizer", "GraphEditMinimap", flip_icon(minimap_resizer_icon, true, true));
	theme->set_color("resizer_color", "GraphEditMinimap", minimap_resizer_color);

	// GraphNode
	const float mv = dark_theme ? 0.0 : 1.0;
	const float mv2 = 1.0 - mv;
	const int gn_margin_side = 28;
	Ref<StyleBoxFlat> graphsb = make_flat_stylebox(Color(mv, mv, mv, 0.7), gn_margin_side, 24, gn_margin_side, 5);
	graphsb->set_border_width_all(border_width);
	graphsb->set_border_color(Color(mv2, mv2, mv2, 0.9));
	Ref<StyleBoxFlat> graphsbselected = make_flat_stylebox(Color(mv, mv, mv, 0.9), gn_margin_side, 24, gn_margin_side, 5);
	graphsbselected->set_border_width_all(border_width);
	graphsbselected->set_border_color(Color(accent_color.r, accent_color.g, accent_color.b, 0.9));
	graphsbselected->set_shadow_size(8 * EDSCALE);
	graphsbselected->set_shadow_color(shadow_color);
	Ref<StyleBoxFlat> graphsbcomment = make_flat_stylebox(Color(mv, mv, mv, 0.3), gn_margin_side, 24, gn_margin_side, 5);
	graphsbcomment->set_border_width_all(border_width);
	graphsbcomment->set_border_color(Color(mv2, mv2, mv2, 0.9));
	Ref<StyleBoxFlat> graphsbcommentselected = make_flat_stylebox(Color(mv, mv, mv, 0.4), gn_margin_side, 24, gn_margin_side, 5);
	graphsbcommentselected->set_border_width_all(border_width);
	graphsbcommentselected->set_border_color(Color(mv2, mv2, mv2, 0.9));
	Ref<StyleBoxFlat> graphsbbreakpoint = graphsbselected->duplicate();
	graphsbbreakpoint->set_draw_center(false);
	graphsbbreakpoint->set_border_color(warning_color);
	graphsbbreakpoint->set_shadow_color(warning_color * Color(1.0, 1.0, 1.0, 0.1));
	Ref<StyleBoxFlat> graphsbposition = graphsbselected->duplicate();
	graphsbposition->set_draw_center(false);
	graphsbposition->set_border_color(error_color);
	graphsbposition->set_shadow_color(error_color * Color(1.0, 1.0, 1.0, 0.2));
	Ref<StyleBoxFlat> smgraphsb = make_flat_stylebox(Color(mv, mv, mv, 0.7), gn_margin_side, 24, gn_margin_side, 5);
	smgraphsb->set_border_width_all(border_width);
	smgraphsb->set_border_color(Color(mv2, mv2, mv2, 0.9));
	Ref<StyleBoxFlat> smgraphsbselected = make_flat_stylebox(Color(mv, mv, mv, 0.9), gn_margin_side, 24, gn_margin_side, 5);
	smgraphsbselected->set_border_width_all(border_width);
	smgraphsbselected->set_border_color(Color(accent_color.r, accent_color.g, accent_color.b, 0.9));
	smgraphsbselected->set_shadow_size(8 * EDSCALE);
	smgraphsbselected->set_shadow_color(shadow_color);

	if (use_gn_headers) {
		graphsb->set_border_width(MARGIN_TOP, 24 * EDSCALE);
		graphsbselected->set_border_width(MARGIN_TOP, 24 * EDSCALE);
		graphsbcomment->set_border_width(MARGIN_TOP, 24 * EDSCALE);
		graphsbcommentselected->set_border_width(MARGIN_TOP, 24 * EDSCALE);
	}

	theme->set_stylebox("frame", "GraphNode", graphsb);
	theme->set_stylebox("selectedframe", "GraphNode", graphsbselected);
	theme->set_stylebox("comment", "GraphNode", graphsbcomment);
	theme->set_stylebox("commentfocus", "GraphNode", graphsbcommentselected);
	theme->set_stylebox("breakpoint", "GraphNode", graphsbbreakpoint);
	theme->set_stylebox("position", "GraphNode", graphsbposition);
	theme->set_stylebox("state_machine_frame", "GraphNode", smgraphsb);
	theme->set_stylebox("state_machine_selectedframe", "GraphNode", smgraphsbselected);

	Color default_node_color = Color(mv2, mv2, mv2);
	theme->set_color("title_color", "GraphNode", default_node_color);
	default_node_color.a = 0.7;
	theme->set_color("close_color", "GraphNode", default_node_color);
	theme->set_color("resizer_color", "GraphNode", default_node_color);

	theme->set_constant("port_offset", "GraphNode", 14 * EDSCALE);
	theme->set_constant("title_h_offset", "GraphNode", -16 * EDSCALE);
	theme->set_constant("title_offset", "GraphNode", 20 * EDSCALE);
	theme->set_constant("close_h_offset", "GraphNode", 20 * EDSCALE);
	theme->set_constant("close_offset", "GraphNode", 20 * EDSCALE);
	theme->set_constant("separation", "GraphNode", 1 * EDSCALE);

	theme->set_icon("close", "GraphNode", theme->get_icon("GuiCloseCustomizable", "EditorIcons"));
	theme->set_icon("resizer", "GraphNode", theme->get_icon("GuiResizer", "EditorIcons"));
	theme->set_icon("port", "GraphNode", theme->get_icon("GuiGraphNodePort", "EditorIcons"));

	// GridContainer
	theme->set_constant("vseparation", "GridContainer", (extra_spacing + default_margin_size) * EDSCALE);

	// FileDialog
	theme->set_icon("folder", "FileDialog", theme->get_icon("Folder", "EditorIcons"));
	theme->set_icon("parent_folder", "FileDialog", theme->get_icon("ArrowUp", "EditorIcons"));
	theme->set_icon("reload", "FileDialog", theme->get_icon("Reload", "EditorIcons"));
	theme->set_icon("toggle_hidden", "FileDialog", theme->get_icon("GuiVisibilityVisible", "EditorIcons"));
	// Use a different color for folder icons to make them easier to distinguish from files.
	// On a light theme, the icon will be dark, so we need to lighten it before blending it with the accent color.
	theme->set_color("folder_icon_modulate", "FileDialog", (dark_theme ? Color(1, 1, 1) : Color(4.25, 4.25, 4.25)).linear_interpolate(accent_color, 0.7));
	theme->set_color("files_disabled", "FileDialog", font_color_disabled);

	// color picker
	theme->set_constant("margin", "ColorPicker", popup_margin_size);
	theme->set_constant("sv_width", "ColorPicker", 256 * EDSCALE);
	theme->set_constant("sv_height", "ColorPicker", 256 * EDSCALE);
	theme->set_constant("h_width", "ColorPicker", 30 * EDSCALE);
	theme->set_constant("label_width", "ColorPicker", 10 * EDSCALE);
	theme->set_icon("screen_picker", "ColorPicker", theme->get_icon("ColorPick", "EditorIcons"));
	theme->set_icon("add_preset", "ColorPicker", theme->get_icon("Add", "EditorIcons"));
	theme->set_icon("preset_bg", "ColorPicker", theme->get_icon("GuiMiniCheckerboard", "EditorIcons"));
	theme->set_icon("overbright_indicator", "ColorPicker", theme->get_icon("OverbrightIndicator", "EditorIcons"));

	theme->set_icon("bg", "ColorPickerButton", theme->get_icon("GuiMiniCheckerboard", "EditorIcons"));

	// ColorPresetButton
	Ref<StyleBoxFlat> preset_sb = make_flat_stylebox(Color(1, 1, 1), 2, 2, 2, 2);

	preset_sb->set_anti_aliased(false);
	theme->set_stylebox("preset_fg", "ColorPresetButton", preset_sb);
	theme->set_icon("preset_bg_icon", "ColorPresetButton", theme->get_icon("GuiMiniCheckerboard", "EditorIcons"));
	theme->set_icon("overbright_indicator", "ColorPresetButton", theme->get_icon("OverbrightIndicator", "EditorIcons"));

	// Information on 3D viewport
	Ref<StyleBoxFlat> style_info_3d_viewport = style_default->duplicate();
	style_info_3d_viewport->set_bg_color(style_info_3d_viewport->get_bg_color() * Color(1, 1, 1, 0.5));
	style_info_3d_viewport->set_border_width_all(0);
	theme->set_stylebox("Information3dViewport", "EditorStyles", style_info_3d_viewport);

	// Theme editor.
	theme->set_color("preview_picker_overlay_color", "ThemeEditor", Color(0.1, 0.1, 0.1, 0.25));
	Color theme_preview_picker_bg_color = accent_color;
	theme_preview_picker_bg_color.a = 0.2;
	Ref<StyleBoxFlat> theme_preview_picker_sb = make_flat_stylebox(theme_preview_picker_bg_color, 0, 0, 0, 0);
	theme_preview_picker_sb->set_border_color(accent_color);
	theme_preview_picker_sb->set_border_width_all(1.0 * EDSCALE);
	theme->set_stylebox("preview_picker_overlay", "ThemeEditor", theme_preview_picker_sb);
	Color theme_preview_picker_label_bg_color = accent_color;
	theme_preview_picker_label_bg_color.set_hsv(theme_preview_picker_label_bg_color.get_h(), theme_preview_picker_label_bg_color.get_s(), 0.5);
	Ref<StyleBoxFlat> theme_preview_picker_label_sb = make_flat_stylebox(theme_preview_picker_label_bg_color, 4.0, 1.0, 4.0, 3.0);
	theme->set_stylebox("preview_picker_label", "ThemeEditor", theme_preview_picker_label_sb);

	// adaptive script theme constants
	// for comments and elements with lower relevance
	const Color dim_color = Color(font_color.r, font_color.g, font_color.b, 0.5);

	const float mono_value = mono_color.r;
	const Color alpha1 = Color(mono_value, mono_value, mono_value, 0.07);
	const Color alpha2 = Color(mono_value, mono_value, mono_value, 0.14);
	const Color alpha3 = Color(mono_value, mono_value, mono_value, 0.27);

	// editor main color
	const Color main_color = dark_theme ? Color(0.34, 0.7, 1.0) : Color(0.02, 0.5, 1.0);

	const Color symbol_color = Color(0.34, 0.57, 1.0).linear_interpolate(mono_color, dark_theme ? 0.5 : 0.3);
	const Color keyword_color = Color(1.0, 0.44, 0.52);
	const Color control_flow_keyword_color = dark_theme ? Color(1.0, 0.55, 0.8) : Color(0.8, 0.4, 0.6);
	const Color basetype_color = dark_theme ? Color(0.26, 1.0, 0.76) : Color(0.0, 0.76, 0.38);
	const Color type_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.4 : 0.3);
	const Color usertype_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.7 : 0.5);
	const Color comment_color = dim_color;
	const Color string_color = (dark_theme ? Color(1.0, 0.85, 0.26) : Color(1.0, 0.82, 0.09)).linear_interpolate(mono_color, dark_theme ? 0.5 : 0.3);

	const Color te_background_color = dark_theme ? background_color : base_color;
	const Color completion_background_color = dark_theme ? base_color : background_color;
	const Color completion_selected_color = alpha1;
	const Color completion_existing_color = alpha2;
	const Color completion_scroll_color = Color(mono_value, mono_value, mono_value, 0.29);
	const Color completion_font_color = font_color;
	const Color text_color = font_color;
	const Color line_number_color = dim_color;
	const Color safe_line_number_color = dim_color * Color(1, 1.2, 1, 1.5);
	const Color caret_color = mono_color;
	const Color caret_background_color = mono_color.inverted();
	const Color text_selected_color = dark_color_3;
	const Color selection_color = accent_color * Color(1, 1, 1, 0.35);
	const Color brace_mismatch_color = error_color;
	const Color current_line_color = alpha1;
	const Color line_length_guideline_color = dark_theme ? base_color : background_color;
	const Color word_highlighted_color = alpha1;
	const Color number_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.5 : 0.3);
	const Color function_color = main_color;
	const Color member_variable_color = main_color.linear_interpolate(mono_color, 0.6);
	const Color mark_color = Color(error_color.r, error_color.g, error_color.b, 0.3);
	const Color bookmark_color = Color(0.08, 0.49, 0.98);
	const Color breakpoint_color = error_color;
	const Color executing_line_color = Color(0.2, 0.8, 0.2, 0.4);
	const Color code_folding_color = alpha3;
	const Color search_result_color = alpha1;
	const Color search_result_border_color = Color(0.41, 0.61, 0.91, 0.38);

	EditorSettings *setting = EditorSettings::get_singleton();
	String text_editor_color_theme = setting->get("text_editor/theme/color_theme");
	if (text_editor_color_theme == "Adaptive") {
		setting->set_initial_value("text_editor/highlighting/symbol_color", symbol_color, true);
		setting->set_initial_value("text_editor/highlighting/keyword_color", keyword_color, true);
		setting->set_initial_value("text_editor/highlighting/control_flow_keyword_color", control_flow_keyword_color, true);
		setting->set_initial_value("text_editor/highlighting/base_type_color", basetype_color, true);
		setting->set_initial_value("text_editor/highlighting/engine_type_color", type_color, true);
		setting->set_initial_value("text_editor/highlighting/user_type_color", usertype_color, true);
		setting->set_initial_value("text_editor/highlighting/comment_color", comment_color, true);
		setting->set_initial_value("text_editor/highlighting/string_color", string_color, true);
		setting->set_initial_value("text_editor/highlighting/background_color", te_background_color, true);
		setting->set_initial_value("text_editor/highlighting/completion_background_color", completion_background_color, true);
		setting->set_initial_value("text_editor/highlighting/completion_selected_color", completion_selected_color, true);
		setting->set_initial_value("text_editor/highlighting/completion_existing_color", completion_existing_color, true);
		setting->set_initial_value("text_editor/highlighting/completion_scroll_color", completion_scroll_color, true);
		setting->set_initial_value("text_editor/highlighting/completion_font_color", completion_font_color, true);
		setting->set_initial_value("text_editor/highlighting/text_color", text_color, true);
		setting->set_initial_value("text_editor/highlighting/line_number_color", line_number_color, true);
		setting->set_initial_value("text_editor/highlighting/safe_line_number_color", safe_line_number_color, true);
		setting->set_initial_value("text_editor/highlighting/caret_color", caret_color, true);
		setting->set_initial_value("text_editor/highlighting/caret_background_color", caret_background_color, true);
		setting->set_initial_value("text_editor/highlighting/text_selected_color", text_selected_color, true);
		setting->set_initial_value("text_editor/highlighting/selection_color", selection_color, true);
		setting->set_initial_value("text_editor/highlighting/brace_mismatch_color", brace_mismatch_color, true);
		setting->set_initial_value("text_editor/highlighting/current_line_color", current_line_color, true);
		setting->set_initial_value("text_editor/highlighting/line_length_guideline_color", line_length_guideline_color, true);
		setting->set_initial_value("text_editor/highlighting/word_highlighted_color", word_highlighted_color, true);
		setting->set_initial_value("text_editor/highlighting/number_color", number_color, true);
		setting->set_initial_value("text_editor/highlighting/function_color", function_color, true);
		setting->set_initial_value("text_editor/highlighting/member_variable_color", member_variable_color, true);
		setting->set_initial_value("text_editor/highlighting/mark_color", mark_color, true);
		setting->set_initial_value("text_editor/highlighting/bookmark_color", bookmark_color, true);
		setting->set_initial_value("text_editor/highlighting/breakpoint_color", breakpoint_color, true);
		setting->set_initial_value("text_editor/highlighting/executing_line_color", executing_line_color, true);
		setting->set_initial_value("text_editor/highlighting/code_folding_color", code_folding_color, true);
		setting->set_initial_value("text_editor/highlighting/search_result_color", search_result_color, true);
		setting->set_initial_value("text_editor/highlighting/search_result_border_color", search_result_border_color, true);
	} else if (text_editor_color_theme == "Default") {
		setting->load_text_editor_theme();
	}

	OS::get_singleton()->benchmark_end_measure("create_editor_theme");

	return theme;
}

Ref<Theme> create_custom_theme(const Ref<Theme> p_theme) {
	OS::get_singleton()->benchmark_begin_measure("create_custom_theme");
	Ref<Theme> theme = create_editor_theme(p_theme);

	const String custom_theme_path = EditorSettings::get_singleton()->get("interface/theme/custom_theme");
	if (custom_theme_path != "") {
		Ref<Theme> custom_theme = ResourceLoader::load(custom_theme_path);
		if (custom_theme.is_valid()) {
			theme->merge_with(custom_theme);
		}
	}

	OS::get_singleton()->benchmark_end_measure("create_custom_theme");
	return theme;
}

Ref<ImageTexture> create_unscaled_default_project_icon() {
#ifdef MODULE_SVG_ENABLED
	for (int i = 0; i < editor_icons_count; i++) {
		// ESCALE should never affect size of the icon
		if (strcmp(editor_icons_names[i], "DefaultProjectIcon") == 0) {
			return editor_generate_icon(i, false, 1.0);
		}
	}
#endif
	return Ref<ImageTexture>(memnew(ImageTexture));
}
