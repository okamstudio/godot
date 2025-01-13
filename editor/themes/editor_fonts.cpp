/**************************************************************************/
/*  editor_fonts.cpp                                                      */
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

#include "editor_fonts.h"

#include "core/io/dir_access.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/builtin_fonts.gen.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/font.h"
#include "scene/scene_string_names.h"

Ref<FontFile> load_external_font(const String &p_path, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_font_disable_embedded_bitmaps, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<FontFile> font;
	font.instantiate();

	Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path);

	font->set_data(data);
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);
	font->set_disable_embedded_bitmaps(p_font_disable_embedded_bitmaps);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<SystemFont> load_system_font(const PackedStringArray &p_names, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_font_disable_embedded_bitmaps, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<SystemFont> font;
	font.instantiate();

	font->set_font_names(p_names);
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);
	font->set_disable_embedded_bitmaps(p_font_disable_embedded_bitmaps);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<FontFile> load_internal_font(const uint8_t *p_data, size_t p_size, TextServer::Hinting p_hinting, TextServer::FontAntialiasing p_aa, bool p_autohint, TextServer::SubpixelPositioning p_font_subpixel_positioning, bool p_font_disable_embedded_bitmaps, bool p_msdf = false, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<FontFile> font;
	font.instantiate();

	font->set_data_ptr(p_data, p_size);
	font->set_multichannel_signed_distance_field(p_msdf);
	font->set_antialiasing(p_aa);
	font->set_hinting(p_hinting);
	font->set_force_autohinter(p_autohint);
	font->set_subpixel_positioning(p_font_subpixel_positioning);
	font->set_disable_embedded_bitmaps(p_font_disable_embedded_bitmaps);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font);
	}

	return font;
}

Ref<FontVariation> make_bold_font(const Ref<Font> &p_font, double p_embolden, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<FontVariation> font_var;
	font_var.instantiate();
	font_var->set_base_font(p_font);
	font_var->set_variation_embolden(p_embolden);

	if (r_fallbacks != nullptr) {
		r_fallbacks->push_back(font_var);
	}

	return font_var;
}

void editor_register_fonts(const Ref<Theme> &p_theme) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	TextServer::FontAntialiasing font_antialiasing = (TextServer::FontAntialiasing)(int)EDITOR_GET("interface/editor/font_antialiasing");
	int font_hinting_setting = (int)EDITOR_GET("interface/editor/font_hinting");
	TextServer::SubpixelPositioning font_subpixel_positioning = (TextServer::SubpixelPositioning)(int)EDITOR_GET("interface/editor/font_subpixel_positioning");
	bool font_disable_embedded_bitmaps = (bool)EDITOR_GET("interface/editor/font_disable_embedded_bitmaps");
	bool font_allow_msdf = (bool)EDITOR_GET("interface/editor/font_allow_msdf");

	TextServer::Hinting font_hinting;
	TextServer::Hinting font_mono_hinting;
	switch (font_hinting_setting) {
		case 0:
			// The "Auto" setting uses the setting that best matches the OS' font rendering:
			// - macOS doesn't use font hinting.
			// - Windows uses ClearType, which is in between "Light" and "Normal" hinting.
			// - Linux has configurable font hinting, but most distributions including Ubuntu default to "Light".
#ifdef MACOS_ENABLED
			font_hinting = TextServer::HINTING_NONE;
			font_mono_hinting = TextServer::HINTING_NONE;
#else
			font_hinting = TextServer::HINTING_LIGHT;
			font_mono_hinting = TextServer::HINTING_LIGHT;
#endif
			break;
		case 1:
			font_hinting = TextServer::HINTING_NONE;
			font_mono_hinting = TextServer::HINTING_NONE;
			break;
		case 2:
			font_hinting = TextServer::HINTING_LIGHT;
			font_mono_hinting = TextServer::HINTING_LIGHT;
			break;
		default:
			font_hinting = TextServer::HINTING_NORMAL;
			font_mono_hinting = TextServer::HINTING_LIGHT;
			break;
	}

	// Load built-in fonts.
	const int default_font_size = int(EDITOR_GET("interface/editor/main_font_size")) * EDSCALE;
	const float embolden_strength = 0.6;

	Ref<Font> default_font = load_internal_font(_font_NotoSans_Regular, _font_NotoSans_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false);
	Ref<Font> default_font_msdf = load_internal_font(_font_NotoSans_Regular, _font_NotoSans_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, font_allow_msdf);

	String noto_cjk_path;
	String noto_cjk_bold_path;
	String var_suffix[] = { "HK", "KR", "SC", "TC", "JP" }; // Note: All Noto Sans CJK versions support all glyph variations, it should not match current locale.
	for (size_t i = 0; i < sizeof(var_suffix) / sizeof(String); i++) {
		if (noto_cjk_path.is_empty()) {
			noto_cjk_path = OS::get_singleton()->get_system_font_path("Noto Sans CJK " + var_suffix[i], 400, 100);
		}
		if (noto_cjk_bold_path.is_empty()) {
			noto_cjk_bold_path = OS::get_singleton()->get_system_font_path("Noto Sans CJK " + var_suffix[i], 800, 100);
		}
	}

	TypedArray<Font> fallbacks;
	Ref<FontFile> arabic_font = load_internal_font(_font_Vazirmatn_Regular, _font_Vazirmatn_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> bengali_font = load_internal_font(_font_NotoSansBengaliUI_Regular, _font_NotoSansBengaliUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> devanagari_font = load_internal_font(_font_NotoSansDevanagariUI_Regular, _font_NotoSansDevanagariUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> georgian_font = load_internal_font(_font_NotoSansGeorgian_Regular, _font_NotoSansGeorgian_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> hebrew_font = load_internal_font(_font_NotoSansHebrew_Regular, _font_NotoSansHebrew_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> malayalam_font = load_internal_font(_font_NotoSansMalayalamUI_Regular, _font_NotoSansMalayalamUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> oriya_font = load_internal_font(_font_NotoSansOriya_Regular, _font_NotoSansOriya_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> sinhala_font = load_internal_font(_font_NotoSansSinhalaUI_Regular, _font_NotoSansSinhalaUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> tamil_font = load_internal_font(_font_NotoSansTamilUI_Regular, _font_NotoSansTamilUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> telugu_font = load_internal_font(_font_NotoSansTeluguUI_Regular, _font_NotoSansTeluguUI_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> thai_font = load_internal_font(_font_NotoSansThai_Regular, _font_NotoSansThai_Regular_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	if (!noto_cjk_path.is_empty()) {
		load_external_font(noto_cjk_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	}
	Ref<FontFile> fallback_font = load_internal_font(_font_DroidSansFallback, _font_DroidSansFallback_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	Ref<FontFile> japanese_font = load_internal_font(_font_DroidSansJapanese, _font_DroidSansJapanese_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks);
	default_font->set_fallbacks(fallbacks);
	default_font_msdf->set_fallbacks(fallbacks);

	Ref<FontFile> default_font_bold = load_internal_font(_font_NotoSans_Bold, _font_NotoSans_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false);
	Ref<FontFile> default_font_bold_msdf = load_internal_font(_font_NotoSans_Bold, _font_NotoSans_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, font_allow_msdf);

	TypedArray<Font> fallbacks_bold;
	Ref<FontFile> arabic_font_bold = load_internal_font(_font_Vazirmatn_Bold, _font_Vazirmatn_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> bengali_font_bold = load_internal_font(_font_NotoSansBengaliUI_Bold, _font_NotoSansBengaliUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> devanagari_font_bold = load_internal_font(_font_NotoSansDevanagariUI_Bold, _font_NotoSansDevanagariUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> georgian_font_bold = load_internal_font(_font_NotoSansGeorgian_Bold, _font_NotoSansGeorgian_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> hebrew_font_bold = load_internal_font(_font_NotoSansHebrew_Bold, _font_NotoSansHebrew_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> malayalam_font_bold = load_internal_font(_font_NotoSansMalayalamUI_Bold, _font_NotoSansMalayalamUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> oriya_font_bold = load_internal_font(_font_NotoSansOriya_Bold, _font_NotoSansOriya_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> sinhala_font_bold = load_internal_font(_font_NotoSansSinhalaUI_Bold, _font_NotoSansSinhalaUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> tamil_font_bold = load_internal_font(_font_NotoSansTamilUI_Bold, _font_NotoSansTamilUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> telugu_font_bold = load_internal_font(_font_NotoSansTeluguUI_Bold, _font_NotoSansTeluguUI_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	Ref<FontFile> thai_font_bold = load_internal_font(_font_NotoSansThai_Bold, _font_NotoSansThai_Bold_size, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	if (!noto_cjk_bold_path.is_empty()) {
		load_external_font(noto_cjk_bold_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false, &fallbacks_bold);
	}
	Ref<FontVariation> fallback_font_bold = make_bold_font(fallback_font, embolden_strength, &fallbacks_bold);
	Ref<FontVariation> japanese_font_bold = make_bold_font(japanese_font, embolden_strength, &fallbacks_bold);

	if (OS::get_singleton()->has_feature("system_fonts")) {
		PackedStringArray emoji_font_names;
		emoji_font_names.push_back("Apple Color Emoji");
		emoji_font_names.push_back("Segoe UI Emoji");
		emoji_font_names.push_back("Noto Color Emoji");
		emoji_font_names.push_back("Twitter Color Emoji");
		emoji_font_names.push_back("OpenMoji");
		emoji_font_names.push_back("EmojiOne Color");
		Ref<SystemFont> emoji_font = load_system_font(emoji_font_names, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, false);
		fallbacks.push_back(emoji_font);
		fallbacks_bold.push_back(emoji_font);
	}

	default_font_bold->set_fallbacks(fallbacks_bold);
	default_font_bold_msdf->set_fallbacks(fallbacks_bold);

	Ref<FontFile> default_font_mono = load_internal_font(_font_JetBrainsMono_Regular, _font_JetBrainsMono_Regular_size, font_mono_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps);
	default_font_mono->set_fallbacks(fallbacks);

	// Init base font configs and load custom fonts.
	String custom_font_path = EDITOR_GET("interface/editor/main_font");
	String custom_font_path_bold = EDITOR_GET("interface/editor/main_font_bold");
	String custom_font_path_source = EDITOR_GET("interface/editor/code_font");

	Ref<FontVariation> default_fc;
	default_fc.instantiate();
	if (custom_font_path.length() > 0 && dir->file_exists(custom_font_path)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font);
			custom_font->set_fallbacks(fallback_custom);
		}
		default_fc->set_base_font(custom_font);
	} else {
		EditorSettings::get_singleton()->set_manually("interface/editor/main_font", "");
		default_fc->set_base_font(default_font);
	}
	default_fc->set_spacing(TextServer::SPACING_TOP, -EDSCALE);
	default_fc->set_spacing(TextServer::SPACING_BOTTOM, -EDSCALE);

	Ref<FontVariation> default_fc_msdf;
	default_fc_msdf.instantiate();
	if (custom_font_path.length() > 0 && dir->file_exists(custom_font_path)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, font_allow_msdf);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_msdf);
			custom_font->set_fallbacks(fallback_custom);
		}
		default_fc_msdf->set_base_font(custom_font);
	} else {
		EditorSettings::get_singleton()->set_manually("interface/editor/main_font", "");
		default_fc_msdf->set_base_font(default_font_msdf);
	}
	default_fc_msdf->set_spacing(TextServer::SPACING_TOP, -EDSCALE);
	default_fc_msdf->set_spacing(TextServer::SPACING_BOTTOM, -EDSCALE);

	Ref<FontVariation> bold_fc;
	bold_fc.instantiate();
	if (custom_font_path_bold.length() > 0 && dir->file_exists(custom_font_path_bold)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path_bold, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_bold);
			custom_font->set_fallbacks(fallback_custom);
		}
		bold_fc->set_base_font(custom_font);
	} else if (custom_font_path.length() > 0 && dir->file_exists(custom_font_path)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_bold);
			custom_font->set_fallbacks(fallback_custom);
		}
		bold_fc->set_base_font(custom_font);
		bold_fc->set_variation_embolden(embolden_strength);
	} else {
		EditorSettings::get_singleton()->set_manually("interface/editor/main_font_bold", "");
		bold_fc->set_base_font(default_font_bold);
	}
	bold_fc->set_spacing(TextServer::SPACING_TOP, -EDSCALE);
	bold_fc->set_spacing(TextServer::SPACING_BOTTOM, -EDSCALE);

	Ref<FontVariation> bold_fc_msdf;
	bold_fc_msdf.instantiate();
	if (custom_font_path_bold.length() > 0 && dir->file_exists(custom_font_path_bold)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path_bold, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, font_allow_msdf);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_bold_msdf);
			custom_font->set_fallbacks(fallback_custom);
		}
		bold_fc_msdf->set_base_font(custom_font);
	} else if (custom_font_path.length() > 0 && dir->file_exists(custom_font_path)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path, font_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps, font_allow_msdf);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_bold_msdf);
			custom_font->set_fallbacks(fallback_custom);
		}
		bold_fc_msdf->set_base_font(custom_font);
		bold_fc_msdf->set_variation_embolden(embolden_strength);
	} else {
		EditorSettings::get_singleton()->set_manually("interface/editor/main_font_bold", "");
		bold_fc_msdf->set_base_font(default_font_bold_msdf);
	}
	bold_fc_msdf->set_spacing(TextServer::SPACING_TOP, -EDSCALE);
	bold_fc_msdf->set_spacing(TextServer::SPACING_BOTTOM, -EDSCALE);

	Ref<FontVariation> mono_fc;
	mono_fc.instantiate();
	if (custom_font_path_source.length() > 0 && dir->file_exists(custom_font_path_source)) {
		Ref<FontFile> custom_font = load_external_font(custom_font_path_source, font_mono_hinting, font_antialiasing, true, font_subpixel_positioning, font_disable_embedded_bitmaps);
		{
			TypedArray<Font> fallback_custom;
			fallback_custom.push_back(default_font_mono);
			custom_font->set_fallbacks(fallback_custom);
		}
		mono_fc->set_base_font(custom_font);
	} else {
		EditorSettings::get_singleton()->set_manually("interface/editor/code_font", "");
		mono_fc->set_base_font(default_font_mono);
	}
	mono_fc->set_spacing(TextServer::SPACING_TOP, -EDSCALE);
	mono_fc->set_spacing(TextServer::SPACING_BOTTOM, -EDSCALE);

	Ref<FontVariation> mono_other_fc = mono_fc->duplicate();

	// Enable contextual alternates (coding ligatures) and custom features for the source editor font.
	int ot_mode = EDITOR_GET("interface/editor/code_font_contextual_ligatures");
	switch (ot_mode) {
		case 1: { // Disable ligatures.
			Dictionary ftrs;
			ftrs[TS->name_to_tag("calt")] = 0;
			mono_fc->set_opentype_features(ftrs);
		} break;
		case 2: { // Custom.
			Vector<String> subtag = String(EDITOR_GET("interface/editor/code_font_custom_opentype_features")).split(",");
			Dictionary ftrs;
			for (int i = 0; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=");
				if (subtag_a.size() == 2) {
					ftrs[TS->name_to_tag(subtag_a[0])] = subtag_a[1].to_int();
				} else if (subtag_a.size() == 1) {
					ftrs[TS->name_to_tag(subtag_a[0])] = 1;
				}
			}
			mono_fc->set_opentype_features(ftrs);
		} break;
		default: { // Enabled.
			Dictionary ftrs;
			ftrs[TS->name_to_tag("calt")] = 1;
			mono_fc->set_opentype_features(ftrs);
		} break;
	}

	{
		// Disable contextual alternates (coding ligatures).
		Dictionary ftrs;
		ftrs[TS->name_to_tag("calt")] = 0;
		mono_other_fc->set_opentype_features(ftrs);
	}

	// Use fake bold/italics to style the editor log's `print_rich()` output.
	// Use stronger embolden strength to make bold easier to distinguish from regular text.
	Ref<FontVariation> mono_other_fc_bold = mono_other_fc->duplicate();
	mono_other_fc_bold->set_variation_embolden(0.8);

	Ref<FontVariation> mono_other_fc_italic = mono_other_fc->duplicate();
	mono_other_fc_italic->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	Ref<FontVariation> mono_other_fc_bold_italic = mono_other_fc->duplicate();
	mono_other_fc_bold_italic->set_variation_embolden(0.8);
	mono_other_fc_bold_italic->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	Ref<FontVariation> mono_other_fc_mono = mono_other_fc->duplicate();
	// Use a different font style to distinguish `[code]` in rich prints.
	// This emulates the "faint" styling used in ANSI escape codes by using a slightly thinner font.
	mono_other_fc_mono->set_variation_embolden(-0.25);
	mono_other_fc_mono->set_variation_transform(Transform2D(1.0, 0.1, 0.0, 1.0, 0.0, 0.0));

	Ref<FontVariation> italic_fc = default_fc->duplicate();
	italic_fc->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	Ref<FontVariation> bold_italic_fc = bold_fc->duplicate();
	bold_italic_fc->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));

	// Setup theme.

	p_theme->set_default_font(default_fc); // Default theme font config.
	p_theme->set_default_font_size(default_font_size);

	// Main font.

	p_theme->set_font("main", EditorStringName(EditorFonts), default_fc);
	p_theme->set_font("main_msdf", EditorStringName(EditorFonts), default_fc_msdf);
	p_theme->set_font_size("main_size", EditorStringName(EditorFonts), default_font_size);

	p_theme->set_font("bold", EditorStringName(EditorFonts), bold_fc);
	p_theme->set_font("main_bold_msdf", EditorStringName(EditorFonts), bold_fc_msdf);
	p_theme->set_font_size("bold_size", EditorStringName(EditorFonts), default_font_size);

	p_theme->set_font("italic", EditorStringName(EditorFonts), italic_fc);
	p_theme->set_font_size("italic_size", EditorStringName(EditorFonts), default_font_size);

	// Title font.

	p_theme->set_font("title", EditorStringName(EditorFonts), bold_fc);
	p_theme->set_font_size("title_size", EditorStringName(EditorFonts), default_font_size + 1 * EDSCALE);

	p_theme->set_type_variation("MainScreenButton", "Button");
	p_theme->set_font(SceneStringName(font), "MainScreenButton", bold_fc);
	p_theme->set_font_size(SceneStringName(font_size), "MainScreenButton", default_font_size + 2 * EDSCALE);

	// Labels.

	p_theme->set_font(SceneStringName(font), "Label", default_fc);

	p_theme->set_type_variation("HeaderSmall", "Label");
	p_theme->set_font(SceneStringName(font), "HeaderSmall", bold_fc);
	p_theme->set_font_size(SceneStringName(font_size), "HeaderSmall", default_font_size);

	p_theme->set_type_variation("HeaderMedium", "Label");
	p_theme->set_font(SceneStringName(font), "HeaderMedium", bold_fc);
	p_theme->set_font_size(SceneStringName(font_size), "HeaderMedium", default_font_size + 1 * EDSCALE);

	p_theme->set_type_variation("HeaderLarge", "Label");
	p_theme->set_font(SceneStringName(font), "HeaderLarge", bold_fc);
	p_theme->set_font_size(SceneStringName(font_size), "HeaderLarge", default_font_size + 3 * EDSCALE);

	p_theme->set_font("normal_font", "RichTextLabel", default_fc);
	p_theme->set_font("bold_font", "RichTextLabel", bold_fc);
	p_theme->set_font("italics_font", "RichTextLabel", italic_fc);
	p_theme->set_font("bold_italics_font", "RichTextLabel", bold_italic_fc);

	// Documentation fonts
	p_theme->set_font_size("doc_size", EditorStringName(EditorFonts), int(EDITOR_GET("text_editor/help/help_font_size")) * EDSCALE);
	p_theme->set_font("doc", EditorStringName(EditorFonts), default_fc);
	p_theme->set_font("doc_bold", EditorStringName(EditorFonts), bold_fc);
	p_theme->set_font("doc_italic", EditorStringName(EditorFonts), italic_fc);
	p_theme->set_font_size("doc_title_size", EditorStringName(EditorFonts), int(EDITOR_GET("text_editor/help/help_title_font_size")) * EDSCALE);
	p_theme->set_font("doc_title", EditorStringName(EditorFonts), bold_fc);
	p_theme->set_font_size("doc_source_size", EditorStringName(EditorFonts), int(EDITOR_GET("text_editor/help/help_source_font_size")) * EDSCALE);
	p_theme->set_font("doc_source", EditorStringName(EditorFonts), mono_fc);
	p_theme->set_font_size("doc_keyboard_size", EditorStringName(EditorFonts), (int(EDITOR_GET("text_editor/help/help_source_font_size")) - 1) * EDSCALE);
	p_theme->set_font("doc_keyboard", EditorStringName(EditorFonts), mono_fc);

	// Ruler font
	p_theme->set_font_size("rulers_size", EditorStringName(EditorFonts), 8 * EDSCALE);
	p_theme->set_font("rulers", EditorStringName(EditorFonts), default_fc);

	// Rotation widget font
	p_theme->set_font_size("rotation_control_size", EditorStringName(EditorFonts), 13 * EDSCALE);
	p_theme->set_font("rotation_control", EditorStringName(EditorFonts), default_fc);

	// Code font
	p_theme->set_font_size("source_size", EditorStringName(EditorFonts), int(EDITOR_GET("interface/editor/code_font_size")) * EDSCALE);
	p_theme->set_font("source", EditorStringName(EditorFonts), mono_fc);

	p_theme->set_font_size("expression_size", EditorStringName(EditorFonts), (int(EDITOR_GET("interface/editor/code_font_size")) - 1) * EDSCALE);
	p_theme->set_font("expression", EditorStringName(EditorFonts), mono_other_fc);

	p_theme->set_font_size("output_source_size", EditorStringName(EditorFonts), int(EDITOR_GET("run/output/font_size")) * EDSCALE);
	p_theme->set_font("output_source", EditorStringName(EditorFonts), mono_other_fc);
	p_theme->set_font("output_source_bold", EditorStringName(EditorFonts), mono_other_fc_bold);
	p_theme->set_font("output_source_italic", EditorStringName(EditorFonts), mono_other_fc_italic);
	p_theme->set_font("output_source_bold_italic", EditorStringName(EditorFonts), mono_other_fc_bold_italic);
	p_theme->set_font("output_source_mono", EditorStringName(EditorFonts), mono_other_fc_mono);

	p_theme->set_font_size("status_source_size", EditorStringName(EditorFonts), default_font_size);
	p_theme->set_font("status_source", EditorStringName(EditorFonts), mono_other_fc);
}
