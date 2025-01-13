/**************************************************************************/
/*  resource_importer_dynamic_font.cpp                                    */
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

#include "resource_importer_dynamic_font.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "editor/import/dynamic_font_import_settings.h"
#include "scene/resources/font.h"
#include "servers/text_server.h"

#include "modules/modules_enabled.gen.h" // For freetype.

String ResourceImporterDynamicFont::get_importer_name() const {
	return "font_data_dynamic";
}

String ResourceImporterDynamicFont::get_visible_name() const {
	return "Font Data (Dynamic Font)";
}

void ResourceImporterDynamicFont::get_recognized_extensions(List<String> *p_extensions) const {
	if (p_extensions) {
#ifdef MODULE_FREETYPE_ENABLED
		p_extensions->push_back("ttf");
		p_extensions->push_back("ttc");
		p_extensions->push_back("otf");
		p_extensions->push_back("otc");
		p_extensions->push_back("woff");
		p_extensions->push_back("woff2");
		p_extensions->push_back("pfb");
		p_extensions->push_back("pfm");
#endif
	}
}

String ResourceImporterDynamicFont::get_save_extension() const {
	return "fontdata";
}

String ResourceImporterDynamicFont::get_resource_type() const {
	return "FontFile";
}

bool ResourceImporterDynamicFont::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "msdf_pixel_range" && !bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	if (p_option == "msdf_size" && !bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	if (p_option == "antialiasing" && bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	if (p_option == "oversampling" && bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	if (p_option == "subpixel_positioning" && bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	if (p_option == "keep_rounding_remainders" && bool(p_options["multichannel_signed_distance_field"])) {
		return false;
	}
	return true;
}

int ResourceImporterDynamicFont::get_preset_count() const {
	return PRESET_MAX;
}

String ResourceImporterDynamicFont::get_preset_name(int p_idx) const {
	switch (p_idx) {
		case PRESET_DYNAMIC:
			return TTR("Dynamically rendered TrueType/OpenType font");
		case PRESET_MSDF:
			return TTR("Prerendered multichannel(+true) signed distance field");
		default:
			return String();
	}
}

void ResourceImporterDynamicFont::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	bool msdf = p_preset == PRESET_MSDF;

	r_options->push_back(ImportOption(PropertyInfo(Variant::NIL, "Rendering", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));

	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "antialiasing", PROPERTY_HINT_ENUM, "None,Grayscale,LCD Subpixel"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "disable_embedded_bitmaps"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "multichannel_signed_distance_field", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), (msdf) ? true : false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "msdf_pixel_range", PROPERTY_HINT_RANGE, "1,100,1"), 8));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "msdf_size", PROPERTY_HINT_RANGE, "1,250,1"), 48));

	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "allow_system_fallback"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force_autohinter"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "hinting", PROPERTY_HINT_ENUM, "None,Light,Normal"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "subpixel_positioning", PROPERTY_HINT_ENUM, "Disabled,Auto,One Half of a Pixel,One Quarter of a Pixel,Auto (Except Pixel Fonts)"), 4));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "keep_rounding_remainders"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "oversampling", PROPERTY_HINT_RANGE, "0,10,0.1"), 0.0));

	r_options->push_back(ImportOption(PropertyInfo(Variant::NIL, "Fallbacks", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));

	r_options->push_back(ImportOption(PropertyInfo(Variant::NIL, "Compress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress"), true));

	// Hide from the main UI, only for advanced import dialog.
	r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "preload", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Array()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "language_support", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "script_support", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));
}

bool ResourceImporterDynamicFont::has_advanced_options() const {
	return true;
}
void ResourceImporterDynamicFont::show_advanced_options(const String &p_path) {
	DynamicFontImportSettingsDialog::get_singleton()->open_settings(p_path);
}

Error ResourceImporterDynamicFont::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	print_verbose("Importing dynamic font from: " + p_source_file);

	int antialiasing = p_options["antialiasing"];
	bool generate_mipmaps = p_options["generate_mipmaps"];
	bool disable_embedded_bitmaps = p_options["disable_embedded_bitmaps"];
	bool msdf = p_options["multichannel_signed_distance_field"];
	int px_range = p_options["msdf_pixel_range"];
	int px_size = p_options["msdf_size"];
	Dictionary ot_ov = p_options["opentype_features"];

	bool autohinter = p_options["force_autohinter"];
	bool allow_system_fallback = p_options["allow_system_fallback"];
	int hinting = p_options["hinting"];
	int subpixel_positioning = p_options["subpixel_positioning"];
	bool keep_rounding_remainders = p_options["keep_rounding_remainders"];
	real_t oversampling = p_options["oversampling"];
	Array fallbacks = p_options["fallbacks"];

	// Load base font data.
	Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_source_file);

	// Create font.
	Ref<FontFile> font;
	font.instantiate();
	font->set_data(data);
	font->set_antialiasing((TextServer::FontAntialiasing)antialiasing);
	font->set_disable_embedded_bitmaps(disable_embedded_bitmaps);
	font->set_generate_mipmaps(generate_mipmaps);
	font->set_multichannel_signed_distance_field(msdf);
	font->set_msdf_pixel_range(px_range);
	font->set_msdf_size(px_size);
	font->set_opentype_feature_overrides(ot_ov);
	font->set_fixed_size(0);
	font->set_force_autohinter(autohinter);
	font->set_allow_system_fallback(allow_system_fallback);
	font->set_hinting((TextServer::Hinting)hinting);
	font->set_oversampling(oversampling);
	font->set_fallbacks(fallbacks);

	if (subpixel_positioning == 4 /* Auto (Except Pixel Fonts) */) {
		PackedInt32Array glyphs = TS->font_get_supported_glyphs(font->get_rids()[0]);
		bool is_pixel = true;
		for (int32_t gl : glyphs) {
			Dictionary ct = TS->font_get_glyph_contours(font->get_rids()[0], 16, gl);
			PackedInt32Array contours = ct["contours"];
			PackedVector3Array points = ct["points"];
			int prev_start = 0;
			for (int i = 0; i < contours.size(); i++) {
				for (int j = prev_start; j <= contours[i]; j++) {
					int next_point = (j < contours[i]) ? (j + 1) : prev_start;
					if ((points[j].z != TextServer::CONTOUR_CURVE_TAG_ON) || (!Math::is_equal_approx(points[j].x, points[next_point].x) && !Math::is_equal_approx(points[j].y, points[next_point].y))) {
						is_pixel = false;
						break;
					}
				}
				prev_start = contours[i] + 1;
				if (!is_pixel) {
					break;
				}
			}
			if (!is_pixel) {
				break;
			}
		}
		if (is_pixel && !glyphs.is_empty()) {
			print_line(vformat("%s: Pixel font detected, disabling subpixel positioning.", p_source_file));
			subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_DISABLED;
		} else {
			subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_AUTO;
		}
	}
	font->set_subpixel_positioning((TextServer::SubpixelPositioning)subpixel_positioning);
	font->set_keep_rounding_remainders(keep_rounding_remainders);

	Dictionary langs = p_options["language_support"];
	for (int i = 0; i < langs.size(); i++) {
		String key = langs.get_key_at_index(i);
		bool enabled = langs.get_value_at_index(i);
		font->set_language_support_override(key, enabled);
	}

	Dictionary scripts = p_options["script_support"];
	for (int i = 0; i < scripts.size(); i++) {
		String key = scripts.get_key_at_index(i);
		bool enabled = scripts.get_value_at_index(i);
		font->set_script_support_override(key, enabled);
	}

	Array preload_configurations = p_options["preload"];

	for (int i = 0; i < preload_configurations.size(); i++) {
		Dictionary preload_config = preload_configurations[i];

		Dictionary variation = preload_config.has("variation_opentype") ? preload_config["variation_opentype"].operator Dictionary() : Dictionary();
		double embolden = preload_config.has("variation_embolden") ? preload_config["variation_embolden"].operator double() : 0;
		int face_index = preload_config.has("variation_face_index") ? preload_config["variation_face_index"].operator int() : 0;
		Transform2D transform = preload_config.has("variation_transform") ? preload_config["variation_transform"].operator Transform2D() : Transform2D();
		Vector2i size = preload_config.has("size") ? preload_config["size"].operator Vector2i() : Vector2i(16, 0);
		String name = preload_config.has("name") ? preload_config["name"].operator String() : vformat("Configuration %d", i);

		RID conf_rid = font->find_variation(variation, face_index, embolden, transform);

		Array chars = preload_config["chars"];
		for (int j = 0; j < chars.size(); j++) {
			char32_t c = chars[j].operator int();
			TS->font_render_range(conf_rid, size, c, c);
		}

		Array glyphs = preload_config["glyphs"];
		for (int j = 0; j < glyphs.size(); j++) {
			int32_t c = glyphs[j];
			TS->font_render_glyph(conf_rid, size, c);
		}
	}

	int flg = 0;
	if ((bool)p_options["compress"]) {
		flg |= ResourceSaver::SaverFlags::FLAG_COMPRESS;
	}

	print_verbose("Saving to: " + p_save_path + ".fontdata");
	Error err = ResourceSaver::save(font, p_save_path + ".fontdata", flg);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save font to file \"" + p_save_path + ".res\".");
	print_verbose("Done saving to: " + p_save_path + ".fontdata");
	return OK;
}

ResourceImporterDynamicFont::ResourceImporterDynamicFont() {
}
