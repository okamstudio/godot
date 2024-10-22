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

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/image_loader.h"
#include "core/version.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_toaster.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/resources/compressed_texture.h"

void ResourceImporterTexture::_texture_reimport_roughness(const Ref<CompressedTexture2D> &p_tex, const String &p_normal_path, RS::TextureDetectRoughnessChannel p_channel) {
	ERR_FAIL_COND(p_tex.is_null());

	MutexLock lock(singleton->mutex);

	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = MakeInfo();
	}

	singleton->make_flags[path].flags |= MAKE_ROUGHNESS_FLAG;
	singleton->make_flags[path].channel_for_roughness = p_channel;
	singleton->make_flags[path].normal_path_for_roughness = p_normal_path;
}

void ResourceImporterTexture::_texture_reimport_3d(const Ref<CompressedTexture2D> &p_tex) {
	ERR_FAIL_COND(p_tex.is_null());

	MutexLock lock(singleton->mutex);

	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = MakeInfo();
	}

	singleton->make_flags[path].flags |= MAKE_3D_FLAG;
}

void ResourceImporterTexture::_texture_reimport_normal(const Ref<CompressedTexture2D> &p_tex) {
	ERR_FAIL_COND(p_tex.is_null());

	MutexLock lock(singleton->mutex);

	StringName path = p_tex->get_path();

	if (!singleton->make_flags.has(path)) {
		singleton->make_flags[path] = MakeInfo();
	}

	singleton->make_flags[path].flags |= MAKE_NORMAL_FLAG;
}

void ResourceImporterTexture::update_imports() {
	if (EditorFileSystem::get_singleton()->is_scanning() || EditorFileSystem::get_singleton()->is_importing()) {
		return; // do nothing for now
	}

	MutexLock lock(mutex);
	Vector<String> to_reimport;
	{
		if (make_flags.is_empty()) {
			return;
		}

		for (const KeyValue<StringName, MakeInfo> &E : make_flags) {
			Ref<ConfigFile> cf;
			cf.instantiate();
			String src_path = String(E.key) + ".import";

			Error err = cf->load(src_path);
			ERR_CONTINUE(err != OK);

			bool changed = false;

			if (E.value.flags & MAKE_NORMAL_FLAG && int(cf->get_value("params", "compress/normal_map")) == 0) {
				String message = vformat(TTR("%s: Texture detected as used as a normal map in 3D. Enabling red-green texture compression to reduce memory usage (blue channel is discarded)."), String(E.key));
#ifdef TOOLS_ENABLED
				EditorToaster::get_singleton()->popup_str(message);
#endif
				print_line(message);
				cf->set_value("params", "compress/normal_map", 1);
				changed = true;
			}

			if (E.value.flags & MAKE_ROUGHNESS_FLAG && int(cf->get_value("params", "roughness/mode")) == 0) {
				String message = vformat(TTR("%s: Texture detected as used as a roughness map in 3D. Enabling roughness limiter based on the detected associated normal map at %s."), String(E.key), E.value.normal_path_for_roughness);
#ifdef TOOLS_ENABLED
				EditorToaster::get_singleton()->popup_str(message);
#endif
				print_line(message);
				cf->set_value("params", "roughness/mode", E.value.channel_for_roughness + 2);
				cf->set_value("params", "roughness/src_normal", E.value.normal_path_for_roughness);
				changed = true;
			}

			if (E.value.flags & MAKE_3D_FLAG && bool(cf->get_value("params", "detect_3d/compress_to"))) {
				const int compress_to = cf->get_value("params", "detect_3d/compress_to");
				String compress_string;
				cf->set_value("params", "detect_3d/compress_to", 0);
				if (compress_to == 1) {
					cf->set_value("params", "compress/mode", COMPRESS_VRAM_COMPRESSED);
					compress_string = "VRAM Compressed (S3TC/ETC/BPTC)";
				} else if (compress_to == 2) {
					cf->set_value("params", "compress/mode", COMPRESS_BASIS_UNIVERSAL);
					compress_string = "Basis Universal";
				}
				String message = vformat(TTR("%s: Texture detected as used in 3D. Enabling mipmap generation and setting the texture compression mode to %s."), String(E.key), compress_string);
#ifdef TOOLS_ENABLED
				EditorToaster::get_singleton()->popup_str(message);
#endif
				print_line(message);
				cf->set_value("params", "mipmaps/generate", true);
				changed = true;
			}

			if (changed) {
				cf->save(src_path);
				to_reimport.push_back(E.key);
			}
		}

		make_flags.clear();
	}

	if (to_reimport.size()) {
		EditorFileSystem::get_singleton()->reimport_files(to_reimport);
	}
}

String ResourceImporterTexture::get_importer_name() const {
	return "texture";
}

String ResourceImporterTexture::get_visible_name() const {
	return "Texture2D";
}

void ResourceImporterTexture::get_recognized_extensions(List<String> *p_extensions) const {
	ImageLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterTexture::get_save_extension() const {
	return "ctex";
}

String ResourceImporterTexture::get_resource_type() const {
	return "CompressedTexture2D";
}

bool ResourceImporterTexture::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "compress/high_quality" || p_option == "compress/hdr_compression") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != COMPRESS_VRAM_COMPRESSED) {
			return false;
		}
	} else if (p_option == "compress/lossy_quality") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != COMPRESS_LOSSY) {
			return false;
		}
	} else if (p_option == "compress/hdr_mode") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode < COMPRESS_VRAM_COMPRESSED) {
			return false;
		}
	} else if (p_option == "compress/normal_map") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode == COMPRESS_LOSSLESS) {
			return false;
		}
	} else if (p_option == "mipmaps/limit") {
		return p_options["mipmaps/generate"];
	}

	return true;
}

int ResourceImporterTexture::get_preset_count() const {
	return 3;
}

String ResourceImporterTexture::get_preset_name(int p_idx) const {
	static const char *preset_names[] = {
		TTRC("2D/3D (Auto-Detect)"),
		TTRC("2D"),
		TTRC("3D"),
	};

	return TTRGET(preset_names[p_idx]);
}

void ResourceImporterTexture::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,VRAM Compressed,VRAM Uncompressed,Basis Universal", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == PRESET_3D ? 2 : 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress/high_quality"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/hdr_compression", PROPERTY_HINT_ENUM, "Disabled,Opaque Only,Always"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/channel_pack", PROPERTY_HINT_ENUM, "sRGB Friendly,Optimized"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "mipmaps/generate"), (p_preset == PRESET_3D ? true : false)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "mipmaps/limit", PROPERTY_HINT_RANGE, "-1,256"), -1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "roughness/mode", PROPERTY_HINT_ENUM, "Detect,Disabled,Red,Green,Blue,Alpha,Gray"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "roughness/src_normal", PROPERTY_HINT_FILE, "*.bmp,*.dds,*.exr,*.jpeg,*.jpg,*.hdr,*.png,*.svg,*.tga,*.webp"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), p_preset != PRESET_3D));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/normal_map_invert_y"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_as_srgb"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_clamp_exposure"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "detect_3d/compress_to", PROPERTY_HINT_ENUM, "Disabled,VRAM Compressed,Basis Universal"), (p_preset == PRESET_DETECT) ? 1 : 0));

	// Do path based customization only if a path was passed.
	if (p_path.is_empty() || p_path.get_extension() == "svg") {
		r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "svg/scale", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 1.0));

		// Editor use only, applies to SVG.
		r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "editor/scale_with_editor_scale"), false));
		r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "editor/convert_colors_with_editor_theme"), false));
	}
}

void ResourceImporterTexture::save_to_ctex_format(Ref<FileAccess> f, const Ref<Image> &p_image, CompressMode p_compress_mode, Image::UsedChannels p_channels, Image::CompressMode p_compress_format, float p_lossy_quality) {
	switch (p_compress_mode) {
		case COMPRESS_LOSSLESS: {
			bool lossless_force_png = GLOBAL_GET("rendering/textures/lossless_compression/force_png") ||
					!Image::_webp_mem_loader_func; // WebP module disabled.
			bool use_webp = !lossless_force_png && p_image->get_width() <= 16383 && p_image->get_height() <= 16383; // WebP has a size limit
			f->store_32(use_webp ? CompressedTexture2D::DATA_FORMAT_WEBP : CompressedTexture2D::DATA_FORMAT_PNG);
			f->store_16(p_image->get_width());
			f->store_16(p_image->get_height());
			f->store_32(p_image->get_mipmap_count());
			f->store_32(p_image->get_format());

			for (int i = 0; i < p_image->get_mipmap_count() + 1; i++) {
				Vector<uint8_t> data;
				if (use_webp) {
					data = Image::webp_lossless_packer(i ? p_image->get_image_from_mipmap(i) : p_image);
				} else {
					data = Image::png_packer(i ? p_image->get_image_from_mipmap(i) : p_image);
				}
				int data_len = data.size();
				f->store_32(data_len);

				const uint8_t *r = data.ptr();
				f->store_buffer(r, data_len);
			}

		} break;
		case COMPRESS_LOSSY: {
			f->store_32(CompressedTexture2D::DATA_FORMAT_WEBP);
			f->store_16(p_image->get_width());
			f->store_16(p_image->get_height());
			f->store_32(p_image->get_mipmap_count());
			f->store_32(p_image->get_format());

			for (int i = 0; i < p_image->get_mipmap_count() + 1; i++) {
				Vector<uint8_t> data = Image::webp_lossy_packer(i ? p_image->get_image_from_mipmap(i) : p_image, p_lossy_quality);
				int data_len = data.size();
				f->store_32(data_len);

				const uint8_t *r = data.ptr();
				f->store_buffer(r, data_len);
			}
		} break;
		case COMPRESS_VRAM_COMPRESSED: {
			Ref<Image> image = p_image->duplicate();

			image->compress_from_channels(p_compress_format, p_channels);

			f->store_32(CompressedTexture2D::DATA_FORMAT_IMAGE);
			f->store_16(image->get_width());
			f->store_16(image->get_height());
			f->store_32(image->get_mipmap_count());
			f->store_32(image->get_format());

			Vector<uint8_t> data = image->get_data();
			int dl = data.size();
			const uint8_t *r = data.ptr();
			f->store_buffer(r, dl);
		} break;
		case COMPRESS_VRAM_UNCOMPRESSED: {
			f->store_32(CompressedTexture2D::DATA_FORMAT_IMAGE);
			f->store_16(p_image->get_width());
			f->store_16(p_image->get_height());
			f->store_32(p_image->get_mipmap_count());
			f->store_32(p_image->get_format());

			Vector<uint8_t> data = p_image->get_data();
			int dl = data.size();
			const uint8_t *r = data.ptr();

			f->store_buffer(r, dl);

		} break;
		case COMPRESS_BASIS_UNIVERSAL: {
			f->store_32(CompressedTexture2D::DATA_FORMAT_BASIS_UNIVERSAL);
			f->store_16(p_image->get_width());
			f->store_16(p_image->get_height());
			f->store_32(p_image->get_mipmap_count());
			f->store_32(p_image->get_format());
			Vector<uint8_t> data = Image::basis_universal_packer(p_image, p_channels);
			int data_len = data.size();
			f->store_32(data_len);
			const uint8_t *r = data.ptr();
			f->store_buffer(r, data_len);
		} break;
	}
}

void ResourceImporterTexture::_save_ctex(const Ref<Image> &p_image, const String &p_to_path, CompressMode p_compress_mode, float p_lossy_quality, Image::CompressMode p_vram_compression, bool p_mipmaps, bool p_streamable, bool p_detect_3d, bool p_detect_roughness, bool p_detect_normal, bool p_force_normal, bool p_srgb_friendly, bool p_force_po2_for_compressed, uint32_t p_limit_mipmap, const Ref<Image> &p_normal, Image::RoughnessChannel p_roughness_channel) {
	Ref<FileAccess> f = FileAccess::open(p_to_path, FileAccess::WRITE);
	ERR_FAIL_COND(f.is_null());
	f->store_8('G');
	f->store_8('S');
	f->store_8('T');
	f->store_8('2'); //godot streamable texture 2D

	//format version
	f->store_32(CompressedTexture2D::FORMAT_VERSION);
	//texture may be resized later, so original size must be saved first
	f->store_32(p_image->get_width());
	f->store_32(p_image->get_height());

	uint32_t flags = 0;
	if (p_streamable) {
		flags |= CompressedTexture2D::FORMAT_BIT_STREAM;
	}
	if (p_mipmaps) {
		flags |= CompressedTexture2D::FORMAT_BIT_HAS_MIPMAPS; //mipmaps bit
	}
	if (p_detect_3d) {
		flags |= CompressedTexture2D::FORMAT_BIT_DETECT_3D;
	}
	if (p_detect_roughness) {
		flags |= CompressedTexture2D::FORMAT_BIT_DETECT_ROUGNESS;
	}
	if (p_detect_normal) {
		flags |= CompressedTexture2D::FORMAT_BIT_DETECT_NORMAL;
	}

	f->store_32(flags);
	f->store_32(p_limit_mipmap);
	//reserved for future use
	f->store_32(0);
	f->store_32(0);
	f->store_32(0);

	if ((p_compress_mode == COMPRESS_LOSSLESS || p_compress_mode == COMPRESS_LOSSY) && p_image->get_format() >= Image::FORMAT_RF) {
		p_compress_mode = COMPRESS_VRAM_UNCOMPRESSED; //these can't go as lossy
	}

	Ref<Image> image = p_image->duplicate();

	if (p_force_po2_for_compressed && p_mipmaps && ((p_compress_mode == COMPRESS_BASIS_UNIVERSAL) || (p_compress_mode == COMPRESS_VRAM_COMPRESSED))) {
		image->resize_to_po2();
	}

	if (p_mipmaps && (!image->has_mipmaps() || p_force_normal)) {
		image->generate_mipmaps(p_force_normal);
	}

	if (!p_mipmaps) {
		image->clear_mipmaps();
	}

	if (image->has_mipmaps() && p_normal.is_valid()) {
		image->generate_mipmap_roughness(p_roughness_channel, p_normal);
	}

	Image::CompressSource csource = Image::COMPRESS_SOURCE_GENERIC;
	if (p_force_normal) {
		csource = Image::COMPRESS_SOURCE_NORMAL;
	} else if (p_srgb_friendly) {
		csource = Image::COMPRESS_SOURCE_SRGB;
	}

	Image::UsedChannels used_channels = image->detect_used_channels(csource);

	save_to_ctex_format(f, image, p_compress_mode, used_channels, p_vram_compression, p_lossy_quality);
}

void ResourceImporterTexture::_save_editor_meta(const Dictionary &p_metadata, const String &p_to_path) {
	Ref<FileAccess> f = FileAccess::open(p_to_path, FileAccess::WRITE);
	ERR_FAIL_COND(f.is_null());

	f->store_var(p_metadata);
}

Dictionary ResourceImporterTexture::_load_editor_meta(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), Dictionary(), vformat("Missing required editor-specific import metadata for a texture (please reimport it using the 'Import' tab): '%s'", p_path));

	return f->get_var();
}

Error ResourceImporterTexture::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	// Parse import options.
	int32_t loader_flags = ImageFormatLoader::FLAG_NONE;

	// Compression.
	CompressMode compress_mode = CompressMode(int(p_options["compress/mode"]));
	const float lossy = p_options["compress/lossy_quality"];
	const int pack_channels = p_options["compress/channel_pack"];
	const int normal = p_options["compress/normal_map"];
	const int hdr_compression = p_options["compress/hdr_compression"];
	const int high_quality = p_options["compress/high_quality"];

	// Mipmaps.
	const bool mipmaps = p_options["mipmaps/generate"];
	const uint32_t mipmap_limit = mipmaps ? uint32_t(p_options["mipmaps/limit"]) : uint32_t(-1);

	// Roughness.
	const int roughness = p_options["roughness/mode"];
	const String normal_map = p_options["roughness/src_normal"];

	// Processing.
	const bool fix_alpha_border = p_options["process/fix_alpha_border"];
	const bool premult_alpha = p_options["process/premult_alpha"];
	const bool normal_map_invert_y = p_options["process/normal_map_invert_y"];
	// Support for texture streaming is not implemented yet.
	const bool stream = false;
	const int size_limit = p_options["process/size_limit"];
	const bool hdr_as_srgb = p_options["process/hdr_as_srgb"];
	if (hdr_as_srgb) {
		loader_flags |= ImageFormatLoader::FLAG_FORCE_LINEAR;
	}
	const bool hdr_clamp_exposure = p_options["process/hdr_clamp_exposure"];

	float scale = 1.0;
	// SVG-specific options.
	if (p_options.has("svg/scale")) {
		scale = p_options["svg/scale"];
	}

	// Editor-specific options.
	bool use_editor_scale = p_options.has("editor/scale_with_editor_scale") && p_options["editor/scale_with_editor_scale"];
	bool convert_editor_colors = p_options.has("editor/convert_colors_with_editor_theme") && p_options["editor/convert_colors_with_editor_theme"];

	// Start importing images.
	List<Ref<Image>> images_imported;

	// Load the normal image.
	Ref<Image> normal_image;
	Image::RoughnessChannel roughness_channel = Image::ROUGHNESS_CHANNEL_R;
	if (mipmaps && roughness > 1 && FileAccess::exists(normal_map)) {
		normal_image.instantiate();
		if (ImageLoader::load_image(normal_map, normal_image) == OK) {
			roughness_channel = Image::RoughnessChannel(roughness - 2);
		}
	}

	// Load the main image.
	Ref<Image> image;
	image.instantiate();
	Error err = ImageLoader::load_image(p_source_file, image, nullptr, loader_flags, scale);
	if (err != OK) {
		return err;
	}
	images_imported.push_back(image);

	// Load the editor-only image.
	Ref<Image> editor_image;
	bool import_editor_image = use_editor_scale || convert_editor_colors;
	if (import_editor_image) {
		float editor_scale = scale;
		if (use_editor_scale) {
			editor_scale = scale * EDSCALE;
		}

		int32_t editor_loader_flags = loader_flags;
		if (convert_editor_colors) {
			editor_loader_flags |= ImageFormatLoader::FLAG_CONVERT_COLORS;
		}

		editor_image.instantiate();
		err = ImageLoader::load_image(p_source_file, editor_image, nullptr, editor_loader_flags, editor_scale);
		if (err != OK) {
			WARN_PRINT("Failed to import an image resource for editor use from '" + p_source_file + "'");
		} else {
			images_imported.push_back(editor_image);
		}
	}

	for (Ref<Image> &target_image : images_imported) {
		// Apply the size limit.
		if (size_limit > 0 && (target_image->get_width() > size_limit || target_image->get_height() > size_limit)) {
			if (target_image->get_width() >= target_image->get_height()) {
				int new_width = size_limit;
				int new_height = target_image->get_height() * new_width / target_image->get_width();

				target_image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
			} else {
				int new_height = size_limit;
				int new_width = target_image->get_width() * new_height / target_image->get_height();

				target_image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
			}

			if (normal == 1) {
				target_image->normalize();
			}
		}

		// Fix alpha border.
		if (fix_alpha_border) {
			target_image->fix_alpha_edges();
		}

		// Premultiply the alpha.
		if (premult_alpha) {
			target_image->premultiply_alpha();
		}

		// Invert the green channel of the image to flip the normal map it contains.
		if (normal_map_invert_y) {
			// Inverting the green channel can be used to flip a normal map's direction.
			// There's no standard when it comes to normal map Y direction, so this is
			// sometimes needed when using a normal map exported from another program.
			// See <http://wiki.polycount.com/wiki/Normal_Map_Technical_Details#Common_Swizzle_Coordinates>.
			const int height = target_image->get_height();
			const int width = target_image->get_width();

			for (int i = 0; i < width; i++) {
				for (int j = 0; j < height; j++) {
					const Color color = target_image->get_pixel(i, j);
					target_image->set_pixel(i, j, Color(color.r, 1 - color.g, color.b));
				}
			}
		}

		// Clamp HDR exposure.
		if (hdr_clamp_exposure) {
			// Clamp HDR exposure following Filament's tonemapping formula.
			// This can be used to reduce fireflies in environment maps or reduce the influence
			// of the sun from an HDRI panorama on environment lighting (when a DirectionalLight3D is used instead).
			const int height = target_image->get_height();
			const int width = target_image->get_width();

			// These values are chosen arbitrarily and seem to produce good results with 4,096 samples.
			const float linear = 4096.0;
			const float compressed = 16384.0;

			for (int i = 0; i < width; i++) {
				for (int j = 0; j < height; j++) {
					const Color color = target_image->get_pixel(i, j);
					const float luma = color.get_luminance();

					Color clamped_color;
					if (luma <= linear) {
						clamped_color = color;
					} else {
						clamped_color = (color / luma) * ((linear * linear - compressed * luma) / (2 * linear - compressed - luma));
					}

					target_image->set_pixel(i, j, clamped_color);
				}
			}
		}
	}

	bool detect_3d = int(p_options["detect_3d/compress_to"]) > 0;
	bool detect_roughness = roughness == 0;
	bool detect_normal = normal == 0;
	bool force_normal = normal == 1;
	bool srgb_friendly_pack = pack_channels == 0;

	Array formats_imported;

	if (compress_mode == COMPRESS_VRAM_COMPRESSED) {
		// Must import in all formats, in order of priority (so platform choses the best supported one. IE, etc2 over etc).
		// Android, GLES 2.x

		const bool is_hdr = (image->get_format() >= Image::FORMAT_RF && image->get_format() <= Image::FORMAT_RGBE9995);
		const bool can_s3tc_bptc = ResourceImporterTextureSettings::should_import_s3tc_bptc();
		const bool can_etc2_astc = ResourceImporterTextureSettings::should_import_etc2_astc();

		// Add list of formats imported
		if (can_s3tc_bptc) {
			formats_imported.push_back("s3tc_bptc");
		}
		if (can_etc2_astc) {
			formats_imported.push_back("etc2_astc");
		}

		bool can_compress_hdr = hdr_compression > 0;
		bool has_alpha = image->detect_alpha() != Image::ALPHA_NONE;
		bool use_uncompressed = false;

		if (is_hdr) {
			if (has_alpha) {
				// Can compress HDR, but HDR with alpha is not compressible.
				if (hdr_compression == 2) {
					// But user selected to compress HDR anyway, so force an alpha-less format.
					if (image->get_format() == Image::FORMAT_RGBAF) {
						image->convert(Image::FORMAT_RGBF);
					} else if (image->get_format() == Image::FORMAT_RGBAH) {
						image->convert(Image::FORMAT_RGBH);
					}
				} else {
					can_compress_hdr = false;
				}
			}

			if (!can_compress_hdr) {
				// Fallback to RGBE99995.
				if (image->get_format() != Image::FORMAT_RGBE9995) {
					image->convert(Image::FORMAT_RGBE9995);
					use_uncompressed = true;
				}
			}
		}

		if (use_uncompressed) {
			_save_ctex(image, p_save_path + ".ctex", COMPRESS_VRAM_UNCOMPRESSED, lossy, Image::COMPRESS_S3TC /*this is ignored */, mipmaps, stream, detect_3d, detect_roughness, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit, normal_image, roughness_channel);
		} else {
			if (can_s3tc_bptc) {
				Image::CompressMode image_compress_mode;
				String image_compress_format;
				if (high_quality || is_hdr) {
					image_compress_mode = Image::COMPRESS_BPTC;
					image_compress_format = "bptc";
				} else {
					image_compress_mode = Image::COMPRESS_S3TC;
					image_compress_format = "s3tc";
				}
				_save_ctex(image, p_save_path + "." + image_compress_format + ".ctex", compress_mode, lossy, image_compress_mode, mipmaps, stream, detect_3d, detect_roughness, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit, normal_image, roughness_channel);
				r_platform_variants->push_back(image_compress_format);
			}

			if (can_etc2_astc) {
				Image::CompressMode image_compress_mode;
				String image_compress_format;
				if (high_quality || is_hdr) {
					image_compress_mode = Image::COMPRESS_ASTC;
					image_compress_format = "astc";
				} else {
					image_compress_mode = Image::COMPRESS_ETC2;
					image_compress_format = "etc2";
				}
				_save_ctex(image, p_save_path + "." + image_compress_format + ".ctex", compress_mode, lossy, image_compress_mode, mipmaps, stream, detect_3d, detect_roughness, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit, normal_image, roughness_channel);
				r_platform_variants->push_back(image_compress_format);
			}
		}
	} else {
		// Import normally.
		_save_ctex(image, p_save_path + ".ctex", compress_mode, lossy, Image::COMPRESS_S3TC /*this is ignored */, mipmaps, stream, detect_3d, detect_roughness, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit, normal_image, roughness_channel);
	}

	if (editor_image.is_valid()) {
		_save_ctex(editor_image, p_save_path + ".editor.ctex", compress_mode, lossy, Image::COMPRESS_S3TC /*this is ignored */, mipmaps, stream, detect_3d, detect_roughness, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit, normal_image, roughness_channel);

		// Generate and save editor-specific metadata, which we cannot save to the .import file.
		Dictionary editor_meta;

		if (use_editor_scale) {
			editor_meta["editor_scale"] = EDSCALE;
		}
		if (convert_editor_colors) {
			editor_meta["editor_dark_theme"] = EditorThemeManager::is_dark_theme();
		}

		_save_editor_meta(editor_meta, p_save_path + ".editor.meta");
	}

	if (r_metadata) {
		Dictionary meta;
		meta["vram_texture"] = compress_mode == COMPRESS_VRAM_COMPRESSED;
		if (formats_imported.size()) {
			meta["imported_formats"] = formats_imported;
		}

		if (editor_image.is_valid()) {
			meta["has_editor_variant"] = true;
		}

		*r_metadata = meta;
	}

	return OK;
}

const char *ResourceImporterTexture::compression_formats[] = {
	"s3tc_bptc",
	"etc2_astc",
	nullptr
};
String ResourceImporterTexture::get_import_settings_string() const {
	String s;

	int index = 0;
	while (compression_formats[index]) {
		String setting_path = "rendering/textures/vram_compression/import_" + String(compression_formats[index]);
		bool test = GLOBAL_GET(setting_path);
		if (test) {
			s += String(compression_formats[index]);
		}
		index++;
	}

	return s;
}

bool ResourceImporterTexture::are_import_settings_valid(const String &p_path, const Dictionary &p_meta) const {
	if (p_meta.has("has_editor_variant")) {
		String imported_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
		if (!FileAccess::exists(imported_path)) {
			return false;
		}

		String editor_meta_path = imported_path.replace(".editor.ctex", ".editor.meta");
		Dictionary editor_meta = _load_editor_meta(editor_meta_path);

		if (editor_meta.has("editor_scale") && (float)editor_meta["editor_scale"] != EDSCALE) {
			return false;
		}
		if (editor_meta.has("editor_dark_theme") && (bool)editor_meta["editor_dark_theme"] != EditorThemeManager::is_dark_theme()) {
			return false;
		}
	}

	if (!p_meta.has("vram_texture")) {
		return false;
	}

	bool vram = p_meta["vram_texture"];
	if (!vram) {
		return true; // Do not care about non-VRAM.
	}

	// Will become invalid if formats are missing to import.
	Vector<String> formats_imported;
	if (p_meta.has("imported_formats")) {
		formats_imported = p_meta["imported_formats"];
	}

	int index = 0;
	bool valid = true;
	while (compression_formats[index]) {
		String setting_path = "rendering/textures/vram_compression/import_" + String(compression_formats[index]);
		if (ProjectSettings::get_singleton()->has_setting(setting_path)) {
			bool test = GLOBAL_GET(setting_path);
			if (test) {
				if (!formats_imported.has(compression_formats[index])) {
					valid = false;
					break;
				}
			}
		} else {
			WARN_PRINT("Setting for imported format not found: " + setting_path);
		}
		index++;
	}

	return valid;
}

ResourceImporterTexture *ResourceImporterTexture::singleton = nullptr;

ResourceImporterTexture::ResourceImporterTexture(bool p_singleton) {
	// This should only be set through the EditorNode.
	if (p_singleton) {
		singleton = this;
	}

	CompressedTexture2D::request_3d_callback = _texture_reimport_3d;
	CompressedTexture2D::request_roughness_callback = _texture_reimport_roughness;
	CompressedTexture2D::request_normal_callback = _texture_reimport_normal;
}

ResourceImporterTexture::~ResourceImporterTexture() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
