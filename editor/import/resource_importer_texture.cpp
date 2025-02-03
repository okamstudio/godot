/**************************************************************************/
/*  resource_importer_texture.cpp                                         */
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

#include "resource_importer_texture.h"

#include "core/io/config_file.h"
#include "core/io/image_loader.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "scene/resources/texture.h"

void ResourceImporterTexture::_texture_reimport_srgb(const Ref<StreamTexture> &p_tex) {
	singleton->mutex.lock();
	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = 0;
	}

	singleton->make_flags[path] |= MAKE_SRGB_FLAG;

	singleton->mutex.unlock();
}

void ResourceImporterTexture::_texture_reimport_3d(const Ref<StreamTexture> &p_tex) {
	singleton->mutex.lock();
	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = 0;
	}

	singleton->make_flags[path] |= MAKE_3D_FLAG;

	singleton->mutex.unlock();
}

void ResourceImporterTexture::_texture_reimport_normal(const Ref<StreamTexture> &p_tex) {
	singleton->mutex.lock();
	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = 0;
	}

	singleton->make_flags[path] |= MAKE_NORMAL_FLAG;

	singleton->mutex.unlock();
}

void ResourceImporterTexture::update_imports() {
	if (EditorFileSystem::get_singleton()->is_scanning() || EditorFileSystem::get_singleton()->is_importing()) {
		return; // do nothing for now
	}
	mutex.lock();

	if (make_flags.empty()) {
		mutex.unlock();
		return;
	}

	Vector<String> to_reimport;
	for (Map<StringName, int>::Element *E = make_flags.front(); E; E = E->next()) {
		Ref<ConfigFile> cf;
		cf.instance();
		String src_path = String(E->key()) + ".import";

		Error err = cf->load(src_path);
		ERR_CONTINUE(err != OK);

		bool changed = false;
		if (E->get() & MAKE_SRGB_FLAG && int(cf->get_value("params", "flags/srgb")) == 2) {
			cf->set_value("params", "flags/srgb", 1);
			changed = true;
		}

		if (E->get() & MAKE_NORMAL_FLAG && int(cf->get_value("params", "compress/normal_map")) == 0) {
			print_line(vformat(TTR("%s: Texture detected as used as a normal map in 3D. Enabling red-green texture compression to reduce memory usage (blue channel is discarded)."), String(E->key())));
			cf->set_value("params", "compress/normal_map", 1);
			changed = true;
		}

		if (E->get() & MAKE_3D_FLAG && bool(cf->get_value("params", "detect_3d"))) {
			print_line(vformat(TTR("%s: Texture detected as used in 3D. Enabling filter, repeat, mipmap generation and VRAM texture compression."), String(E->key())));
			cf->set_value("params", "detect_3d", false);
			cf->set_value("params", "compress/mode", 2);
			cf->set_value("params", "flags/repeat", true);
			cf->set_value("params", "flags/filter", true);
			cf->set_value("params", "flags/mipmaps", true);
			changed = true;
		}

		if (changed) {
			cf->save(src_path);
			to_reimport.push_back(E->key());
		}
	}

	make_flags.clear();

	mutex.unlock();

	if (to_reimport.size()) {
		EditorFileSystem::get_singleton()->reimport_files(to_reimport);
	}
}

String ResourceImporterTexture::get_importer_name() const {
	return "texture";
}

String ResourceImporterTexture::get_visible_name() const {
	return "Texture";
}
void ResourceImporterTexture::get_recognized_extensions(List<String> *p_extensions) const {
	ImageLoader::get_recognized_extensions(p_extensions);
}
String ResourceImporterTexture::get_save_extension() const {
	return "stex";
}

String ResourceImporterTexture::get_resource_type() const {
	return "StreamTexture";
}

bool ResourceImporterTexture::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	if (p_option == "compress/lossy_quality") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != COMPRESS_LOSSY && compress_mode != COMPRESS_VIDEO_RAM) {
			return false;
		}
	} else if (p_option == "compress/hdr_mode") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != COMPRESS_VIDEO_RAM) {
			return false;
		}
	} else if (p_option == "compress/normal_map") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode == COMPRESS_LOSSLESS) {
			return false;
		}
	} else if (p_option == "compress/bptc_ldr") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != COMPRESS_VIDEO_RAM) {
			return false;
		}
		if (!ProjectSettings::get_singleton()->get("rendering/vram_compression/import_bptc")) {
			return false;
		}
	}

	return true;
}

int ResourceImporterTexture::get_preset_count() const {
	return 4;
}
String ResourceImporterTexture::get_preset_name(int p_idx) const {
	static const char *preset_names[] = {
		TTRC("2D, Detect 3D"),
		TTRC("2D"),
		TTRC("2D Pixel"),
		TTRC("3D"),
	};

	return TTRGET(preset_names[p_idx]);
}

void ResourceImporterTexture::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,Video RAM,Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == PRESET_3D ? 2 : 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/hdr_mode", PROPERTY_HINT_ENUM, "Enabled,Force RGBE"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/bptc_ldr", PROPERTY_HINT_ENUM, "Enabled,RGBA Only"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "flags/repeat", PROPERTY_HINT_ENUM, "Disabled,Enabled,Mirrored"), p_preset == PRESET_3D ? 1 : 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "flags/filter"), p_preset != PRESET_2D_PIXEL));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "flags/mipmaps"), p_preset == PRESET_3D));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "flags/anisotropic"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "flags/srgb", PROPERTY_HINT_ENUM, "Disable,Enable,Detect"), 2));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), p_preset != PRESET_3D));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/HDR_as_SRGB"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/invert_color"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/normal_map_invert_y"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "stream"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "detect_3d"), p_preset == PRESET_DETECT));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "svg/scale", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 1.0));
}

void ResourceImporterTexture::_save_stex(const Ref<Image> &p_image, const String &p_to_path, int p_compress_mode, float p_lossy_quality, Image::CompressMode p_vram_compression, bool p_mipmaps, int p_texture_flags, bool p_streamable, bool p_detect_3d, bool p_detect_srgb, bool p_force_rgbe, bool p_detect_normal, bool p_force_normal, bool p_force_po2_for_compressed) {
	FileAccess *f = FileAccess::open(p_to_path, FileAccess::WRITE);
	ERR_FAIL_NULL(f);
	f->store_8('G');
	f->store_8('D');
	f->store_8('S');
	f->store_8('T'); //godot streamable texture

	bool resize_to_po2 = false;

	if (p_compress_mode == COMPRESS_VIDEO_RAM && p_force_po2_for_compressed && (p_mipmaps || p_texture_flags & Texture::FLAG_REPEAT)) {
		resize_to_po2 = true;
		f->store_16(next_power_of_2(p_image->get_width()));
		f->store_16(p_image->get_width());
		f->store_16(next_power_of_2(p_image->get_height()));
		f->store_16(p_image->get_height());
	} else {
		f->store_16(p_image->get_width());
		f->store_16(0);
		f->store_16(p_image->get_height());
		f->store_16(0);
	}
	f->store_32(p_texture_flags);

	uint32_t format = 0;

	if (p_streamable) {
		format |= StreamTexture::FORMAT_BIT_STREAM;
	}
	if (p_mipmaps) {
		format |= StreamTexture::FORMAT_BIT_HAS_MIPMAPS; //mipmaps bit
	}
	if (p_detect_3d) {
		format |= StreamTexture::FORMAT_BIT_DETECT_3D;
	}
	if (p_detect_srgb) {
		format |= StreamTexture::FORMAT_BIT_DETECT_SRGB;
	}
	if (p_detect_normal) {
		format |= StreamTexture::FORMAT_BIT_DETECT_NORMAL;
	}

	if ((p_compress_mode == COMPRESS_LOSSLESS || p_compress_mode == COMPRESS_LOSSY) && p_image->get_format() > Image::FORMAT_RGBA8) {
		p_compress_mode = COMPRESS_UNCOMPRESSED; //these can't go as lossy
	}

	switch (p_compress_mode) {
		case COMPRESS_LOSSLESS: {
			bool lossless_force_png = ProjectSettings::get_singleton()->get("rendering/misc/lossless_compression/force_png") ||
					!Image::_webp_mem_loader_func; // WebP module disabled.
			bool use_webp = !lossless_force_png && p_image->get_width() <= 16383 && p_image->get_height() <= 16383; // WebP has a size limit
			Ref<Image> image = p_image->duplicate();
			if (p_texture_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
				// Only RGB8 and RGBA8 (widely) support sRGB decoding. This is just an optimization, so we don't have to convert inside the driver code.
				if (image->detect_alpha() == Image::ALPHA_NONE) {
					image->convert(Image::FORMAT_RGB8);
				} else {
					image->convert(Image::FORMAT_RGBA8);
				}
			} else {
				image->optimize_channels();
			}

			if (p_mipmaps) {
				image->generate_mipmaps();
			} else {
				image->clear_mipmaps();
			}

			int mmc = image->get_mipmap_count() + 1;

			format |= StreamTexture::FORMAT_BIT_HAS_IMAGE_FORMAT_HINT;
			format |= image->get_format();

			if (use_webp) {
				format |= StreamTexture::FORMAT_BIT_WEBP;
			} else {
				format |= StreamTexture::FORMAT_BIT_PNG;
			}
			f->store_32(format);
			f->store_32(mmc);

			for (int i = 0; i < mmc; i++) {
				if (i > 0) {
					image->shrink_x2();
				}

				PoolVector<uint8_t> data;
				if (use_webp) {
					data = Image::webp_lossless_packer(image);
				} else {
					data = Image::png_packer(image);
				}
				int data_len = data.size();
				f->store_32(data_len);

				PoolVector<uint8_t>::Read r = data.read();
				f->store_buffer(r.ptr(), data_len);
			}

		} break;
		case COMPRESS_LOSSY: {
			Ref<Image> image = p_image->duplicate();
			if (p_texture_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
				// Only RGB8 and RGBA8 (widely) support sRGB decoding. This is just an optimization, so we don't have to convert inside the driver code.
				if (image->detect_alpha() == Image::ALPHA_NONE) {
					image->convert(Image::FORMAT_RGB8);
				} else {
					image->convert(Image::FORMAT_RGBA8);
				}
			} else {
				image->optimize_channels();
			}

			if (p_mipmaps) {
				image->generate_mipmaps();
			} else {
				image->clear_mipmaps();
			}

			int mmc = image->get_mipmap_count() + 1;

			format |= StreamTexture::FORMAT_BIT_HAS_IMAGE_FORMAT_HINT;
			format |= image->get_format();

			format |= StreamTexture::FORMAT_BIT_WEBP;
			f->store_32(format);
			f->store_32(mmc);

			for (int i = 0; i < mmc; i++) {
				if (i > 0) {
					image->shrink_x2();
				}

				PoolVector<uint8_t> data = Image::webp_lossy_packer(image, p_lossy_quality);
				int data_len = data.size();
				f->store_32(data_len);

				PoolVector<uint8_t>::Read r = data.read();
				f->store_buffer(r.ptr(), data_len);
			}
		} break;
		case COMPRESS_VIDEO_RAM: {
			Ref<Image> image = p_image->duplicate();
			if (resize_to_po2) {
				image->resize_to_po2();
			}
			if (p_mipmaps) {
				image->generate_mipmaps(p_force_normal);
			}

			if (p_force_rgbe && image->get_format() >= Image::FORMAT_R8 && image->get_format() <= Image::FORMAT_RGBE9995) {
				image->convert(Image::FORMAT_RGBE9995);
			} else {
				Image::CompressSource csource = Image::COMPRESS_SOURCE_GENERIC;
				if (p_force_normal) {
					csource = Image::COMPRESS_SOURCE_NORMAL;
				} else if (p_texture_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
					csource = Image::COMPRESS_SOURCE_SRGB;
				}

				image->compress(p_vram_compression, csource, p_lossy_quality);
			}

			format |= image->get_format();

			f->store_32(format);

			PoolVector<uint8_t> data = image->get_data();
			int dl = data.size();
			PoolVector<uint8_t>::Read r = data.read();
			f->store_buffer(r.ptr(), dl);
		} break;
		case COMPRESS_UNCOMPRESSED: {
			Ref<Image> image = p_image->duplicate();
			if (p_mipmaps) {
				image->generate_mipmaps();
			} else {
				image->clear_mipmaps();
			}

			format |= image->get_format();
			f->store_32(format);

			PoolVector<uint8_t> data = image->get_data();
			int dl = data.size();
			PoolVector<uint8_t>::Read r = data.read();

			f->store_buffer(r.ptr(), dl);

		} break;
	}

	memdelete(f);
}

Error ResourceImporterTexture::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	int compress_mode = p_options["compress/mode"];
	float lossy = p_options["compress/lossy_quality"];
	int repeat = p_options["flags/repeat"];
	bool filter = p_options["flags/filter"];
	bool mipmaps = p_options["flags/mipmaps"];
	bool anisotropic = p_options["flags/anisotropic"];
	int srgb = p_options["flags/srgb"];
	bool fix_alpha_border = p_options["process/fix_alpha_border"];
	bool premult_alpha = p_options["process/premult_alpha"];
	bool invert_color = p_options["process/invert_color"];
	bool normal_map_invert_y = p_options["process/normal_map_invert_y"];
	bool stream = p_options["stream"];
	int size_limit = p_options["size_limit"];
	bool hdr_as_srgb = p_options["process/HDR_as_SRGB"];
	int normal = p_options["compress/normal_map"];
	float scale = p_options["svg/scale"];
	bool force_rgbe = p_options["compress/hdr_mode"];
	int bptc_ldr = p_options["compress/bptc_ldr"];

	Ref<Image> image;
	image.instance();
	Error err = ImageLoader::load_image(p_source_file, image, nullptr, hdr_as_srgb, scale);
	if (err != OK) {
		return err;
	}

	Array formats_imported;

	int tex_flags = 0;
	if (repeat > 0) {
		tex_flags |= Texture::FLAG_REPEAT;

		const bool min_gles3 = GLOBAL_GET("rendering/quality/driver/driver_name") == "GLES3" &&
				!GLOBAL_GET("rendering/quality/driver/fallback_to_gles2");
		if (!min_gles3 && !image->is_size_po2()) {
			// The project can be run using GLES2. GLES2 does not guarantee that
			// repeating textures with a non-power-of-two size will be displayed
			// without artifacts (due to upscaling to the nearest power of 2).
			if (GLOBAL_GET("rendering/quality/driver/fallback_to_gles2")) {
				WARN_PRINT(vformat("%s: Imported a repeating texture with a size of %dx%d, but the project is configured to allow falling back to GLES2.\nNon-power-of-2 repeating textures may not display correctly on some platforms such as HTML5. This is because GLES2 does not mandate support for non-power-of-2 repeating textures.",
						p_source_file, image->get_width(), image->get_height()));
			} else {
				WARN_PRINT(vformat("%s: Imported a repeating texture with a size of %dx%d, but the project is configured to use GLES2.\nNon-power-of-2 repeating textures may not display correctly on some platforms such as HTML5. This is because GLES2 does not mandate support for non-power-of-2 repeating textures.",
						p_source_file, image->get_width(), image->get_height()));
			}
		}
	}
	if (repeat == 2) {
		tex_flags |= Texture::FLAG_MIRRORED_REPEAT;
	}
	if (filter) {
		tex_flags |= Texture::FLAG_FILTER;
	}
	if (mipmaps || compress_mode == COMPRESS_VIDEO_RAM) {
		tex_flags |= Texture::FLAG_MIPMAPS;
	}
	if (anisotropic) {
		tex_flags |= Texture::FLAG_ANISOTROPIC_FILTER;
	}
	if (srgb == 1) {
		tex_flags |= Texture::FLAG_CONVERT_TO_LINEAR;
	}

	if (size_limit > 0 && (image->get_width() > size_limit || image->get_height() > size_limit)) {
		//limit size
		if (image->get_width() >= image->get_height()) {
			int new_width = size_limit;
			int new_height = image->get_height() * new_width / image->get_width();

			image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
		} else {
			int new_height = size_limit;
			int new_width = image->get_width() * new_height / image->get_height();

			image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
		}

		if (normal == 1) {
			image->normalize();
		}
	}

	if (fix_alpha_border) {
		image->fix_alpha_edges();
	}

	if (premult_alpha) {
		image->premultiply_alpha();
	}

	if (invert_color) {
		int height = image->get_height();
		int width = image->get_width();

		image->lock();
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				image->set_pixel(i, j, image->get_pixel(i, j).inverted());
			}
		}
		image->unlock();
	}

	if (normal_map_invert_y) {
		// Inverting the green channel can be used to flip a normal map's direction.
		// There's no standard when it comes to normal map Y direction, so this is
		// sometimes needed when using a normal map exported from another program.
		// See <http://wiki.polycount.com/wiki/Normal_Map_Technical_Details#Common_Swizzle_Coordinates>.
		const int height = image->get_height();
		const int width = image->get_width();

		image->lock();
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				const Color color = image->get_pixel(i, j);
				image->set_pixel(i, j, Color(color.r, 1 - color.g, color.b));
			}
		}
		image->unlock();
	}

	bool detect_3d = p_options["detect_3d"];
	bool detect_srgb = srgb == 2;
	bool detect_normal = normal == 0;
	bool force_normal = normal == 1;

	if (compress_mode == COMPRESS_VIDEO_RAM) {
		//must import in all formats, in order of priority (so platform choses the best supported one. IE, etc2 over etc).
		//Android, GLES 2.x

		bool ok_on_pc = false;
		bool is_hdr = (image->get_format() >= Image::FORMAT_RF && image->get_format() <= Image::FORMAT_RGBE9995);
		bool is_ldr = (image->get_format() >= Image::FORMAT_L8 && image->get_format() <= Image::FORMAT_RGBA5551);
		bool can_bptc = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_bptc");
		bool can_s3tc = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_s3tc");

		if (can_bptc) {
			Image::DetectChannels channels = image->get_detected_channels();
			if (is_hdr) {
				if (channels == Image::DETECTED_LA || channels == Image::DETECTED_RGBA) {
					can_bptc = false;
				}
			} else if (is_ldr) {
				//handle "RGBA Only" setting
				if (bptc_ldr == 1 && channels != Image::DETECTED_LA && channels != Image::DETECTED_RGBA) {
					can_bptc = false;
				}
			}

			formats_imported.push_back("bptc");
		}

		if (!can_bptc && is_hdr && !force_rgbe) {
			//convert to ldr if this can't be stored hdr
			image->convert(Image::FORMAT_RGBA8);
		}

		if (can_bptc || can_s3tc) {
			_save_stex(image, p_save_path + ".s3tc.stex", compress_mode, lossy, can_bptc ? Image::COMPRESS_BPTC : Image::COMPRESS_S3TC, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, false);
			r_platform_variants->push_back("s3tc");
			formats_imported.push_back("s3tc");
			ok_on_pc = true;
		}

		if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc2")) {
			_save_stex(image, p_save_path + ".etc2.stex", compress_mode, lossy, Image::COMPRESS_ETC2, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, true);
			r_platform_variants->push_back("etc2");
			formats_imported.push_back("etc2");
		}

		if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc")) {
			_save_stex(image, p_save_path + ".etc.stex", compress_mode, lossy, Image::COMPRESS_ETC, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, true);
			r_platform_variants->push_back("etc");
			formats_imported.push_back("etc");
		}

		if (ProjectSettings::get_singleton()->get("rendering/vram_compression/import_pvrtc")) {
			_save_stex(image, p_save_path + ".pvrtc.stex", compress_mode, lossy, Image::COMPRESS_PVRTC4, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, true);
			r_platform_variants->push_back("pvrtc");
			formats_imported.push_back("pvrtc");
		}

		if (!ok_on_pc) {
			EditorNode::add_io_error(TTR("Warning, no suitable PC VRAM compression enabled in Project Settings. This texture will not display correctly on PC."));
		}
	} else {
		//import normally
		_save_stex(image, p_save_path + ".stex", compress_mode, lossy, Image::COMPRESS_S3TC /*this is ignored */, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, false);
	}

	if (r_metadata) {
		Dictionary metadata;
		metadata["vram_texture"] = compress_mode == COMPRESS_VIDEO_RAM;
		if (formats_imported.size()) {
			metadata["imported_formats"] = formats_imported;
		}
		*r_metadata = metadata;
	}
	return OK;
}

const char *ResourceImporterTexture::compression_formats[] = {
	"bptc",
	"s3tc",
	"etc",
	"etc2",
	"pvrtc",
	nullptr
};
String ResourceImporterTexture::get_import_settings_string() const {
	String s;

	int index = 0;
	while (compression_formats[index]) {
		String setting_path = "rendering/vram_compression/import_" + String(compression_formats[index]);
		bool test = ProjectSettings::get_singleton()->get(setting_path);
		if (test) {
			s += String(compression_formats[index]);
		}
		index++;
	}

	return s;
}

bool ResourceImporterTexture::are_import_settings_valid(const String &p_path) const {
	//will become invalid if formats are missing to import
	Dictionary metadata = ResourceFormatImporter::get_singleton()->get_resource_metadata(p_path);

	if (!metadata.has("vram_texture")) {
		return false;
	}

	bool vram = metadata["vram_texture"];
	if (!vram) {
		return true; //do not care about non vram
	}

	Vector<String> formats_imported;
	if (metadata.has("imported_formats")) {
		formats_imported = metadata["imported_formats"];
	}

	int index = 0;
	bool valid = true;
	while (compression_formats[index]) {
		String setting_path = "rendering/vram_compression/import_" + String(compression_formats[index]);
		bool test = ProjectSettings::get_singleton()->get(setting_path);
		if (test) {
			if (formats_imported.find(compression_formats[index]) == -1) {
				valid = false;
				break;
			}
		}
		index++;
	}

	return valid;
}

ResourceImporterTexture *ResourceImporterTexture::singleton = nullptr;

ResourceImporterTexture::ResourceImporterTexture() {
	singleton = this;
	StreamTexture::request_3d_callback = _texture_reimport_3d;
	StreamTexture::request_srgb_callback = _texture_reimport_srgb;
	StreamTexture::request_normal_callback = _texture_reimport_normal;
}
