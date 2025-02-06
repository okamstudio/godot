/**************************************************************************/
/*  resource_importer_lottie.cpp                                          */
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

#include "resource_importer_lottie.h"

#include "core/io/dir_access.h"
#include "modules/zip/zip_reader.h"

#include <thorvg.h>

Ref<JSON> read_lottie_json(const String &p_path) {
	Error err = OK;
	Ref<JSON> lottie_json;
	lottie_json.instantiate();
	String lottie_str = FileAccess::get_file_as_string(p_path, &err);
	if (err == OK) {
		err = lottie_json->parse(lottie_str, true);
	}
	if (err != OK) {
		Ref<ZIPReader> zip_reader;
		zip_reader.instantiate();
		err = zip_reader->open(p_path);
		ERR_FAIL_COND_V_MSG(err != OK, nullptr, vformat("Failed to open dotLottie: %s.", error_names[err]));
		String manifest_str;
		PackedByteArray manifest_data = zip_reader->read_file("manifest.json", true);
		err = manifest_str.parse_utf8(reinterpret_cast<const char *>(manifest_data.ptr()), manifest_data.size());
		ERR_FAIL_COND_V_MSG(err != OK, nullptr, vformat("Failed to parse dotLottie manifest: %s.", error_names[err]));
		Ref<JSON> manifest;
		manifest.instantiate();
		err = manifest->parse(manifest_str, true);
		ERR_FAIL_COND_V_MSG(err != OK, nullptr, vformat("Failed to parse dotLottie manifest: %s.", error_names[err]));
		Array animations = ((Dictionary)manifest->get_data())["animations"];
		String anim_file;
		for (Dictionary anim : animations) {
			String file = "animations/" + (String)(anim["id"]) + ".json";
			if (zip_reader->file_exists(file, true)) {
				anim_file = file;
				break;
			}
		}
		ERR_FAIL_COND_V_MSG(anim_file.is_empty(), nullptr, "Animations in dotLottie manifest don't exist.");
		PackedByteArray lottie_data = zip_reader->read_file(anim_file, true);
		lottie_str.clear();
		err = lottie_str.parse_utf8(reinterpret_cast<const char *>(lottie_data.ptr()), lottie_data.size());
		ERR_FAIL_COND_V_MSG(err != OK, nullptr, vformat("Failed to parse lottie animation %s: %s.", anim_file, error_names[err]));
		err = lottie_json->parse(lottie_str, true);
		ERR_FAIL_COND_V_MSG(err != OK, nullptr, vformat("Failed to parse lottie animation %s: %s.", anim_file, error_names[err]));
	}
	return lottie_json;
}

Ref<Image> lottie_to_sprite_sheet(Ref<JSON> p_json, float p_begin, float p_end, float p_fps, int p_columns, float p_scale, int p_size_limit, Size2i *r_sprite_size, int *r_columns, int *r_frame_count) {
	std::unique_ptr<tvg::SwCanvas> sw_canvas = tvg::SwCanvas::gen();
	std::unique_ptr<tvg::Animation> animation = tvg::Animation::gen();
	tvg::Picture *picture = animation->picture();
	tvg::Result res = sw_canvas->push(tvg::cast(picture));
	ERR_FAIL_COND_V(res != tvg::Result::Success, Ref<Image>());

	String lottie_str = p_json->get_parsed_text();
	if (lottie_str.is_empty()) {
		// Set p_sort_keys to false, otherwise ThorVG can't load it.
		lottie_str = JSON::stringify(p_json->get_data(), "", false);
	}

	res = picture->load(lottie_str.utf8(), lottie_str.utf8().size(), "lottie", true);
	ERR_FAIL_COND_V_MSG(res != tvg::Result::Success, Ref<Image>(), "Failed to load Lottie.");

	float origin_width, origin_height;
	picture->size(&origin_width, &origin_height);

	p_end = CLAMP(p_end, p_begin, 1);
	int total_frame_count = animation->totalFrame();
	int frame_count = MAX(1, animation->duration() * CLAMP(p_end - p_begin, 0, 1) * p_fps);
	int sheet_columns = p_columns <= 0 ? Math::ceil(Math::sqrt((float)frame_count)) : p_columns;
	int sheet_rows = Math::ceil(((float)frame_count) / sheet_columns);
	Vector2 texture_size = Vector2(origin_width * sheet_columns * p_scale, origin_height * sheet_rows * p_scale);

	const uint32_t max_dimension = 16384;
	if (p_size_limit <= 0) {
		p_size_limit = max_dimension;
	}
	if (texture_size[texture_size.max_axis_index()] > p_size_limit) {
		p_scale = p_size_limit / MAX(origin_width * sheet_columns, origin_height * sheet_rows);
	}
	uint32_t width = MAX(1, Math::round(origin_width * p_scale));
	uint32_t height = MAX(1, Math::round(origin_height * p_scale));
	picture->size(width, height);

	uint32_t *buffer = (uint32_t *)memalloc(sizeof(uint32_t) * width * height);
	memset(buffer, 0, sizeof(uint32_t) * width * height);

	sw_canvas->sync();
	res = sw_canvas->target(buffer, width, width, height, tvg::SwCanvas::ARGB8888S);
	if (res != tvg::Result::Success) {
		memfree(buffer);
		ERR_FAIL_V_MSG(Ref<Image>(), "Couldn't set target on ThorVG canvas.");
	}

	Ref<Image> image = Image::create_empty(width * sheet_columns, height * sheet_rows, false, Image::FORMAT_RGBA8);

	for (int row = 0; row < sheet_rows; row++) {
		for (int column = 0; column < sheet_columns; column++) {
			if (row * sheet_columns + column >= frame_count) {
				break;
			}
			float progress = ((float)(row * sheet_columns + column)) / frame_count;
			float current_frame = total_frame_count * (p_begin + (p_end - p_begin) * progress);

			animation->frame(current_frame);
			res = sw_canvas->update(picture);
			if (res != tvg::Result::Success) {
				memfree(buffer);
				ERR_FAIL_V_MSG(Ref<Image>(), "Couldn't update ThorVG pictures on canvas.");
			}
			res = sw_canvas->draw();
			if (res != tvg::Result::Success) {
				WARN_PRINT_ONCE("Couldn't draw ThorVG pictures on canvas.");
			}
			res = sw_canvas->sync();
			if (res != tvg::Result::Success) {
				memfree(buffer);
				ERR_FAIL_V_MSG(Ref<Image>(), "Couldn't sync ThorVG canvas.");
			}

			for (uint32_t y = 0; y < height; y++) {
				for (uint32_t x = 0; x < width; x++) {
					uint32_t n = buffer[y * width + x];
					Color color;
					color.set_r8((n >> 16) & 0xff);
					color.set_g8((n >> 8) & 0xff);
					color.set_b8(n & 0xff);
					color.set_a8((n >> 24) & 0xff);
					image->set_pixel(x + width * column, y + height * row, color);
				}
			}
			sw_canvas->clear(false);
		}
	}
	memfree(buffer);
	if (r_sprite_size) {
		*r_sprite_size = Size2i(width, height);
	}
	if (r_columns) {
		*r_columns = sheet_columns;
	}
	if (r_frame_count) {
		*r_frame_count = frame_count;
	}
	return image;
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie_compressed_texture_2d";
}

String ResourceImporterLottie::get_visible_name() const {
	return "CompressedTexture2D";
}

int ResourceImporterLottie::get_preset_count() const {
	return 1;
}

String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return p_idx == 0 ? importer_ctex->get_preset_name(ResourceImporterTexture::PRESET_2D) : "";
}

void ResourceImporterLottie::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "lottie/size_limit", PROPERTY_HINT_RANGE, "0,4096,1,or_greater"), 2048));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "lottie/scale", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "lottie/begin", PROPERTY_HINT_RANGE, "0,1,0.001"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "lottie/end", PROPERTY_HINT_RANGE, "0,1,0.001"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "lottie/fps", PROPERTY_HINT_RANGE, "0,60,0.1,or_greater"), 30));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "lottie/columns", PROPERTY_HINT_RANGE, "0,16,1,or_greater"), 0));
	if (r_options->is_empty()) {
		return;
	}
	importer_ctex->get_import_options(p_path, r_options, p_preset);
}

bool ResourceImporterLottie::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return importer_ctex->get_option_visibility(p_path, p_option, p_options);
}

void ResourceImporterLottie::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("lottie");
}

String ResourceImporterLottie::get_save_extension() const {
	return importer_ctex->get_save_extension();
}

String ResourceImporterLottie::get_resource_type() const {
	return importer_ctex->get_resource_type();
}

Error ResourceImporterLottie::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Error err = OK;
	Ref<JSON> lottie_json = read_lottie_json(p_source_file);

	ERR_FAIL_COND_V(lottie_json.is_null(), ERR_INVALID_DATA);

	const int size_limit = p_options["lottie/size_limit"];
	const float scale = p_options["lottie/scale"];
	const float begin = p_options["lottie/begin"];
	const float end = p_options["lottie/end"];
	const float fps = p_options["lottie/fps"];
	const int columns = p_options["lottie/columns"];

	Size2i sprite_size;
	int column_r;
	int frame_count;
	Ref<Image> image = lottie_to_sprite_sheet(lottie_json, begin, end, fps, columns, scale, size_limit, &sprite_size, &column_r, &frame_count);
	ERR_FAIL_COND_V(image.is_null(), ERR_INVALID_DATA);
	String tmp_image = p_save_path + ".tmp.png";
	err = image->save_png(tmp_image);
	if (err == OK) {
		err = importer_ctex->import(p_source_id, tmp_image, p_save_path, p_options, r_platform_variants, r_gen_files, r_metadata);
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		err = d->remove(tmp_image);
		if (r_metadata) {
			Dictionary meta;
			meta["sprite_size"] = sprite_size;
			meta["columns"] = column_r;
			meta["frame_count"] = frame_count;
			meta["fps"] = fps;
			*r_metadata = meta;
		}
	}
	return err;
}

ResourceImporterLottie::ResourceImporterLottie() {
	importer_ctex.instantiate();
}
