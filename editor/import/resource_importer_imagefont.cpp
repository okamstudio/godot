/**************************************************************************/
/*  resource_importer_imagefont.cpp                                       */
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

#include "resource_importer_imagefont.h"

#include "core/io/image_loader.h"
#include "core/io/resource_saver.h"

String ResourceImporterImageFont::get_importer_name() const {
	return "font_data_image";
}

String ResourceImporterImageFont::get_visible_name() const {
	return "Font Data (Image Font)";
}

void ResourceImporterImageFont::get_recognized_extensions(List<String> *p_extensions) const {
	if (p_extensions) {
		ImageLoader::get_recognized_extensions(p_extensions);
	}
}

String ResourceImporterImageFont::get_save_extension() const {
	return "fontdata";
}

String ResourceImporterImageFont::get_resource_type() const {
	return "FontFile";
}

bool ResourceImporterImageFont::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

void ResourceImporterImageFont::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "character_ranges"), Vector<String>()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "kerning_pairs"), Vector<String>()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "columns", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "rows", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::RECT2I, "image_margin"), Rect2i()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::RECT2I, "character_margin"), Rect2i()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "ascent"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "descent"), 0));

	r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));

	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "scaling_mode", PROPERTY_HINT_ENUM, "Disabled,Enabled (Integer),Enabled (Fractional)"), TextServer::FIXED_SIZE_SCALE_ENABLED));
}

Error ResourceImporterImageFont::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	print_verbose("Importing image font from: " + p_source_file);

	int columns = p_options["columns"];
	int rows = p_options["rows"];
	int ascent = p_options["ascent"];
	int descent = p_options["descent"];
	Vector<String> ranges = p_options["character_ranges"];
	Vector<String> kern = p_options["kerning_pairs"];
	Array fallbacks = p_options["fallbacks"];
	Rect2i img_margin = p_options["image_margin"];
	Rect2i char_margin = p_options["character_margin"];
	TextServer::FixedSizeScaleMode smode = (TextServer::FixedSizeScaleMode)p_options["scaling_mode"].operator int();

	Ref<Image> img;
	img.instantiate();
	Error err = ImageLoader::load_image(p_source_file, img);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_READ, vformat("Can't load font texture: \"%s\".", p_source_file));

	ERR_FAIL_COND_V_MSG(columns <= 0, ERR_FILE_CANT_READ, vformat("Columns (%d) must be positive.", columns));
	ERR_FAIL_COND_V_MSG(rows <= 0, ERR_FILE_CANT_READ, vformat("Rows (%d) must be positive.", rows));
	int count = columns * rows;
	int chr_cell_width = (img->get_width() - img_margin.position.x - img_margin.size.x) / columns;
	int chr_cell_height = (img->get_height() - img_margin.position.y - img_margin.size.y) / rows;
	ERR_FAIL_COND_V_MSG(chr_cell_width <= 0 || chr_cell_height <= 0, ERR_FILE_CANT_READ, "Image margin too big.");

	int chr_width = chr_cell_width - char_margin.position.x - char_margin.size.x;
	int chr_height = chr_cell_height - char_margin.position.y - char_margin.size.y;
	ERR_FAIL_COND_V_MSG(chr_width <= 0 || chr_height <= 0, ERR_FILE_CANT_READ, "Character margin too big.");

	Ref<FontFile> font;
	font.instantiate();
	font->set_antialiasing(TextServer::FONT_ANTIALIASING_NONE);
	font->set_generate_mipmaps(false);
	font->set_multichannel_signed_distance_field(false);
	font->set_fixed_size(chr_height);
	font->set_subpixel_positioning(TextServer::SUBPIXEL_POSITIONING_DISABLED);
	font->set_keep_rounding_remainders(true);
	font->set_force_autohinter(false);
	font->set_allow_system_fallback(false);
	font->set_hinting(TextServer::HINTING_NONE);
	font->set_oversampling(1.0f);
	font->set_fallbacks(fallbacks);
	font->set_texture_image(0, Vector2i(chr_height, 0), 0, img);
	font->set_fixed_size_scale_mode(smode);

	int32_t pos = 0;
	for (const String &range : ranges) {
		int32_t start = -1;
		int32_t end = -1;
		int chr_adv = 0;
		Vector2i chr_off;

		{
			enum RangeParseStep {
				STEP_START_BEGIN,
				STEP_START_READ_HEX,
				STEP_START_READ_DEC,
				STEP_END_BEGIN,
				STEP_END_READ_HEX,
				STEP_END_READ_DEC,
				STEP_ADVANCE_BEGIN,
				STEP_OFF_X_BEGIN,
				STEP_OFF_Y_BEGIN,
				STEP_FINISHED,
			};
			RangeParseStep step = STEP_START_BEGIN;
			String token;
			for (int c = 0; c < range.length(); c++) {
				switch (step) {
					case STEP_START_BEGIN:
					case STEP_END_BEGIN: {
						// Read range start/end first symbol.
						if (range[c] == 'U' || range[c] == 'u') {
							if ((c <= range.length() - 2) && range[c + 1] == '+') {
								token = String();
								if (step == STEP_START_BEGIN) {
									step = STEP_START_READ_HEX;
								} else {
									step = STEP_END_READ_HEX;
								}
								c++; // Skip "+".
								continue;
							}
						} else if (range[c] == '0' && (c <= range.length() - 2) && range[c + 1] == 'x') {
							// Read hexadecimal value, start.
							token = String();
							if (step == STEP_START_BEGIN) {
								step = STEP_START_READ_HEX;
							} else {
								step = STEP_END_READ_HEX;
							}
							c++; // Skip "x".
							continue;
						} else if (range[c] == '\'' || range[c] == '\"') {
							if ((c <= range.length() - 3) && (range[c + 2] == '\'' || range[c + 2] == '\"')) {
								token = String();
								if (step == STEP_START_BEGIN) {
									start = range.unicode_at(c + 1);
									step = STEP_END_BEGIN;
								} else {
									end = range.unicode_at(c + 1);
									step = STEP_ADVANCE_BEGIN;
								}
								c = c + 2; // Skip the rest or token.
								continue;
							}
						} else if (is_digit(range[c])) {
							// Read decimal value, start.
							token = String();
							token += range[c];
							if (step == STEP_START_BEGIN) {
								step = STEP_START_READ_DEC;
							} else {
								step = STEP_END_READ_DEC;
							}
							continue;
						}
						[[fallthrough]];
					}
					case STEP_ADVANCE_BEGIN:
					case STEP_OFF_X_BEGIN:
					case STEP_OFF_Y_BEGIN: {
						// Read advance and offset.
						if (range[c] == ' ') {
							int next = range.find_char(' ', c + 1);
							if (next < c) {
								next = range.length();
							}
							if (step == STEP_OFF_X_BEGIN) {
								chr_off.x = range.substr(c + 1, next - (c + 1)).to_int();
								step = STEP_OFF_Y_BEGIN;
							} else if (step == STEP_OFF_Y_BEGIN) {
								chr_off.y = range.substr(c + 1, next - (c + 1)).to_int();
								step = STEP_FINISHED;
							} else {
								chr_adv = range.substr(c + 1, next - (c + 1)).to_int();
								step = STEP_OFF_X_BEGIN;
							}
							c = next - 1;
							continue;
						}
					} break;
					case STEP_START_READ_HEX:
					case STEP_END_READ_HEX: {
						// Read hexadecimal value.
						if (is_hex_digit(range[c])) {
							token += range[c];
						} else {
							if (step == STEP_START_READ_HEX) {
								start = token.hex_to_int();
								step = STEP_END_BEGIN;
							} else {
								end = token.hex_to_int();
								step = STEP_ADVANCE_BEGIN;
								c--;
							}
						}
					} break;
					case STEP_START_READ_DEC:
					case STEP_END_READ_DEC: {
						// Read decimal value.
						if (is_digit(range[c])) {
							token += range[c];
						} else {
							if (step == STEP_START_READ_DEC) {
								start = token.to_int();
								step = STEP_END_BEGIN;
							} else {
								end = token.to_int();
								step = STEP_ADVANCE_BEGIN;
								c--;
							}
						}
					} break;
					default: {
						WARN_PRINT(vformat("Invalid character \"%d\" in the range: \"%s\"", c, range));
					} break;
				}
			}
			if (step == STEP_START_READ_HEX) {
				start = token.hex_to_int();
			} else if (step == STEP_START_READ_DEC) {
				start = token.to_int();
			} else if (step == STEP_END_READ_HEX) {
				end = token.hex_to_int();
			} else if (step == STEP_END_READ_DEC) {
				end = token.to_int();
			}
			if (end == -1) {
				end = start;
			}

			if (start == -1) {
				WARN_PRINT(vformat("Invalid range: \"%s\"", range));
				continue;
			}
		}

		for (int32_t idx = MIN(start, end); idx <= MAX(start, end); idx++) {
			ERR_FAIL_COND_V_MSG(pos >= count, ERR_CANT_CREATE, "Too many characters in range, should be " + itos(columns * rows));
			int x = pos % columns;
			int y = pos / columns;
			font->set_glyph_advance(0, chr_height, idx, Vector2(chr_width + chr_adv, 0));
			font->set_glyph_offset(0, Vector2i(chr_height, 0), idx, Vector2i(0, -0.5 * chr_height) + chr_off);
			font->set_glyph_size(0, Vector2i(chr_height, 0), idx, Vector2(chr_width, chr_height));
			font->set_glyph_uv_rect(0, Vector2i(chr_height, 0), idx, Rect2(img_margin.position.x + chr_cell_width * x + char_margin.position.x, img_margin.position.y + chr_cell_height * y + char_margin.position.y, chr_width, chr_height));
			font->set_glyph_texture_idx(0, Vector2i(chr_height, 0), idx, 0);
			pos++;
		}
	}
	for (const String &kp : kern) {
		const Vector<String> &kp_tokens = kp.split(" ");
		if (kp_tokens.size() != 3) {
			WARN_PRINT(vformat("Invalid kerning pairs string: \"%s\"", kp));
			continue;
		}
		String from_tokens;
		for (int i = 0; i < kp_tokens[0].length(); i++) {
			if (i <= kp_tokens[0].length() - 6 && kp_tokens[0][i] == '\\' && kp_tokens[0][i + 1] == 'u' && is_hex_digit(kp_tokens[0][i + 2]) && is_hex_digit(kp_tokens[0][i + 3]) && is_hex_digit(kp_tokens[0][i + 4]) && is_hex_digit(kp_tokens[0][i + 5])) {
				char32_t charcode = kp_tokens[0].substr(i + 2, 4).hex_to_int();
				from_tokens += charcode;
				i += 5;
			} else {
				from_tokens += kp_tokens[0][i];
			}
		}
		String to_tokens;
		for (int i = 0; i < kp_tokens[1].length(); i++) {
			if (i <= kp_tokens[1].length() - 6 && kp_tokens[1][i] == '\\' && kp_tokens[1][i + 1] == 'u' && is_hex_digit(kp_tokens[1][i + 2]) && is_hex_digit(kp_tokens[1][i + 3]) && is_hex_digit(kp_tokens[1][i + 4]) && is_hex_digit(kp_tokens[1][i + 5])) {
				char32_t charcode = kp_tokens[1].substr(i + 2, 4).hex_to_int();
				to_tokens += charcode;
				i += 5;
			} else {
				to_tokens += kp_tokens[1][i];
			}
		}
		int offset = kp_tokens[2].to_int();

		for (int a = 0; a < from_tokens.length(); a++) {
			for (int b = 0; b < to_tokens.length(); b++) {
				font->set_kerning(0, chr_height, Vector2i(from_tokens.unicode_at(a), to_tokens.unicode_at(b)), Vector2(offset, 0));
			}
		}
	}

	if (ascent > 0) {
		font->set_cache_ascent(0, chr_height, ascent);
	} else {
		font->set_cache_ascent(0, chr_height, 0.5 * chr_height);
	}

	if (descent > 0) {
		font->set_cache_descent(0, chr_height, descent);
	} else {
		font->set_cache_descent(0, chr_height, 0.5 * chr_height);
	}

	int flg = 0;
	if ((bool)p_options["compress"]) {
		flg |= ResourceSaver::SaverFlags::FLAG_COMPRESS;
	}

	print_verbose("Saving to: " + p_save_path + ".fontdata");
	err = ResourceSaver::save(font, p_save_path + ".fontdata", flg);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save font to file \"" + p_save_path + ".res\".");
	print_verbose("Done saving to: " + p_save_path + ".fontdata");
	return OK;
}

ResourceImporterImageFont::ResourceImporterImageFont() {
}
