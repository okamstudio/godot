/**************************************************************************/
/*  resource_importer_layered_texture.cpp                                 */
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

#include "resource_importer_layered_texture.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/image_loader.h"
#include "core/object/ref_counted.h"
#include "editor/editor_file_system.h"
#include "editor/import/resource_importer_texture.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "scene/resources/compressed_texture.h"

String ResourceImporterLayeredTexture::get_importer_name() const {
	switch (mode) {
		case MODE_CUBEMAP: {
			return "cubemap_texture";
		} break;
		case MODE_2D_ARRAY: {
			return "2d_array_texture";
		} break;
		case MODE_CUBEMAP_ARRAY: {
			return "cubemap_array_texture";
		} break;
		case MODE_3D: {
			return "3d_texture";
		} break;
	}

	ERR_FAIL_V("");
}

String ResourceImporterLayeredTexture::get_visible_name() const {
	switch (mode) {
		case MODE_CUBEMAP: {
			return "Cubemap";
		} break;
		case MODE_2D_ARRAY: {
			return "Texture2DArray";
		} break;
		case MODE_CUBEMAP_ARRAY: {
			return "CubemapArray";
		} break;
		case MODE_3D: {
			return "Texture3D";
		} break;
	}

	ERR_FAIL_V("");
}

void ResourceImporterLayeredTexture::get_recognized_extensions(List<String> *p_extensions) const {
	ImageLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterLayeredTexture::get_save_extension() const {
	switch (mode) {
		case MODE_CUBEMAP: {
			return "ccube";
		} break;
		case MODE_2D_ARRAY: {
			return "ctexarray";
		} break;
		case MODE_CUBEMAP_ARRAY: {
			return "ccubearray";
		} break;
		case MODE_3D: {
			return "ctex3d";
		} break;
	}

	ERR_FAIL_V(String());
}

String ResourceImporterLayeredTexture::get_resource_type() const {
	switch (mode) {
		case MODE_CUBEMAP: {
			return "CompressedCubemap";
		} break;
		case MODE_2D_ARRAY: {
			return "CompressedTexture2DArray";
		} break;
		case MODE_CUBEMAP_ARRAY: {
			return "CompressedCubemapArray";
		} break;
		case MODE_3D: {
			return "CompressedTexture3D";
		} break;
	}
	ERR_FAIL_V(String());
}

bool ResourceImporterLayeredTexture::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "compress/lossy_quality" && p_options.has("compress/mode")) {
		return int(p_options["compress/mode"]) == COMPRESS_LOSSY;
	}
	if ((p_option == "compress/high_quality" || p_option == "compress/hdr_compression") && p_options.has("compress/mode")) {
		return int(p_options["compress/mode"]) == COMPRESS_VRAM_COMPRESSED;
	}
	return true;
}

int ResourceImporterLayeredTexture::get_preset_count() const {
	return 0;
}

String ResourceImporterLayeredTexture::get_preset_name(int p_idx) const {
	return "";
}

void ResourceImporterLayeredTexture::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,VRAM Compressed,VRAM Uncompressed,Basis Universal", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress/high_quality"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/hdr_compression", PROPERTY_HINT_ENUM, "Disabled,Opaque Only,Always"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/channel_pack", PROPERTY_HINT_ENUM, "sRGB Friendly,Optimized,Normal Map (RG Channels)"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "mipmaps/generate"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "mipmaps/limit", PROPERTY_HINT_RANGE, "-1,256"), -1));

	if (mode == MODE_2D_ARRAY || mode == MODE_3D) {
		r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "slices/horizontal", PROPERTY_HINT_RANGE, "1,256,1"), 8));
		r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "slices/vertical", PROPERTY_HINT_RANGE, "1,256,1"), 8));
	}
	if (mode == MODE_CUBEMAP || mode == MODE_CUBEMAP_ARRAY) {
		r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "slices/arrangement", PROPERTY_HINT_ENUM, "1x6,2x3,3x2,6x1"), 1));
		if (mode == MODE_CUBEMAP_ARRAY) {
			r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "slices/layout", PROPERTY_HINT_ENUM, "Horizontal,Vertical"), 1));
			r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "slices/amount", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 1));
		}
	}
}

void ResourceImporterLayeredTexture::_save_tex(Vector<Ref<Image>> p_images, const String &p_to_path, int p_compress_mode, float p_lossy, Image::CompressMode p_vram_compression, Image::CompressSource p_csource, Image::UsedChannels used_channels, bool p_mipmaps, bool p_force_po2) {
	Vector<Ref<Image>> mipmap_images; //for 3D

	if (mode == MODE_3D) {
		//3D saves in its own way

		for (int i = 0; i < p_images.size(); i++) {
			if (p_images.write[i]->has_mipmaps()) {
				p_images.write[i]->clear_mipmaps();
			}

			if (p_force_po2) {
				p_images.write[i]->resize_to_po2();
			}
		}

		if (p_mipmaps) {
			Vector<Ref<Image>> parent_images = p_images;
			//create 3D mipmaps, this is horrible, though not used very often
			int w = p_images[0]->get_width();
			int h = p_images[0]->get_height();
			int d = p_images.size();

			while (w > 1 || h > 1 || d > 1) {
				Vector<Ref<Image>> mipmaps;
				int mm_w = MAX(1, w >> 1);
				int mm_h = MAX(1, h >> 1);
				int mm_d = MAX(1, d >> 1);

				for (int i = 0; i < mm_d; i++) {
					Ref<Image> mm = Image::create_empty(mm_w, mm_h, false, p_images[0]->get_format());
					Vector3 pos;
					pos.z = float(i) * float(d) / float(mm_d) + 0.5;
					for (int x = 0; x < mm_w; x++) {
						for (int y = 0; y < mm_h; y++) {
							pos.x = float(x) * float(w) / float(mm_w) + 0.5;
							pos.y = float(y) * float(h) / float(mm_h) + 0.5;

							Vector3i posi = Vector3i(pos);
							Vector3 fract = pos - Vector3(posi);
							Vector3i posi_n = posi;
							if (posi_n.x < w - 1) {
								posi_n.x++;
							}
							if (posi_n.y < h - 1) {
								posi_n.y++;
							}
							if (posi_n.z < d - 1) {
								posi_n.z++;
							}

							Color c000 = parent_images[posi.z]->get_pixel(posi.x, posi.y);
							Color c100 = parent_images[posi.z]->get_pixel(posi_n.x, posi.y);
							Color c010 = parent_images[posi.z]->get_pixel(posi.x, posi_n.y);
							Color c110 = parent_images[posi.z]->get_pixel(posi_n.x, posi_n.y);
							Color c001 = parent_images[posi_n.z]->get_pixel(posi.x, posi.y);
							Color c101 = parent_images[posi_n.z]->get_pixel(posi_n.x, posi.y);
							Color c011 = parent_images[posi_n.z]->get_pixel(posi.x, posi_n.y);
							Color c111 = parent_images[posi_n.z]->get_pixel(posi_n.x, posi_n.y);

							Color cx00 = c000.lerp(c100, fract.x);
							Color cx01 = c001.lerp(c101, fract.x);
							Color cx10 = c010.lerp(c110, fract.x);
							Color cx11 = c011.lerp(c111, fract.x);

							Color cy0 = cx00.lerp(cx10, fract.y);
							Color cy1 = cx01.lerp(cx11, fract.y);

							Color cz = cy0.lerp(cy1, fract.z);

							mm->set_pixel(x, y, cz);
						}
					}

					mipmaps.push_back(mm);
				}

				w = mm_w;
				h = mm_h;
				d = mm_d;

				mipmap_images.append_array(mipmaps);
				parent_images = mipmaps;
			}
		}
	} else {
		for (int i = 0; i < p_images.size(); i++) {
			if (p_force_po2) {
				p_images.write[i]->resize_to_po2();
			}

			if (p_mipmaps) {
				p_images.write[i]->generate_mipmaps(p_csource == Image::COMPRESS_SOURCE_NORMAL);
			} else {
				p_images.write[i]->clear_mipmaps();
			}
		}
	}

	Ref<FileAccess> f = FileAccess::open(p_to_path, FileAccess::WRITE);
	f->store_8('G');
	f->store_8('S');
	f->store_8('T');
	f->store_8('L');

	f->store_32(CompressedTextureLayered::FORMAT_VERSION);
	f->store_32(p_images.size()); // For 2d layers or 3d depth.
	f->store_32(mode);
	f->store_32(0);

	f->store_32(0);
	f->store_32(mipmap_images.size()); // Adjust the amount of mipmaps.
	f->store_32(0);
	f->store_32(0);

	if ((p_compress_mode == COMPRESS_LOSSLESS || p_compress_mode == COMPRESS_LOSSY) && p_images[0]->get_format() >= Image::FORMAT_RF) {
		p_compress_mode = COMPRESS_VRAM_UNCOMPRESSED; // These can't go as lossy.
	}

	for (int i = 0; i < p_images.size(); i++) {
		ResourceImporterTexture::save_to_ctex_format(f, p_images[i], ResourceImporterTexture::CompressMode(p_compress_mode), used_channels, p_vram_compression, p_lossy);
	}

	for (int i = 0; i < mipmap_images.size(); i++) {
		ResourceImporterTexture::save_to_ctex_format(f, mipmap_images[i], ResourceImporterTexture::CompressMode(p_compress_mode), used_channels, p_vram_compression, p_lossy);
	}
}

Error ResourceImporterLayeredTexture::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	int compress_mode = p_options["compress/mode"];
	float lossy = p_options["compress/lossy_quality"];
	bool high_quality = p_options["compress/high_quality"];
	int hdr_compression = p_options["compress/hdr_compression"];
	bool mipmaps = p_options["mipmaps/generate"];

	int channel_pack = p_options["compress/channel_pack"];
	int hslices = (p_options.has("slices/horizontal")) ? int(p_options["slices/horizontal"]) : 0;
	int vslices = (p_options.has("slices/vertical")) ? int(p_options["slices/vertical"]) : 0;
	int arrangement = (p_options.has("slices/arrangement")) ? int(p_options["slices/arrangement"]) : 0;
	int layout = (p_options.has("slices/layout")) ? int(p_options["slices/layout"]) : 0;
	int amount = (p_options.has("slices/amount")) ? int(p_options["slices/amount"]) : 0;

	if (mode == MODE_CUBEMAP || mode == MODE_CUBEMAP_ARRAY) {
		switch (arrangement) {
			case CUBEMAP_FORMAT_1X6: {
				hslices = 1;
				vslices = 6;
			} break;
			case CUBEMAP_FORMAT_2X3: {
				hslices = 2;
				vslices = 3;
			} break;
			case CUBEMAP_FORMAT_3X2: {
				hslices = 3;
				vslices = 2;
			} break;
			case CUBEMAP_FORMAT_6X1: {
				hslices = 6;
				vslices = 1;
			} break;
		}

		if (mode == MODE_CUBEMAP_ARRAY) {
			if (layout == 0) {
				hslices *= amount;
			} else {
				vslices *= amount;
			}
		}
	}

	Ref<Image> image;
	image.instantiate();
	Error err = ImageLoader::load_image(p_source_file, image);
	if (err != OK) {
		return err;
	}

	if (compress_mode == COMPRESS_VRAM_COMPRESSED) {
		//if using video ram, optimize
		if (channel_pack == 0) {
			//remove alpha if not needed, so compression is more efficient
			if (image->get_format() == Image::FORMAT_RGBA8 && !image->detect_alpha()) {
				image->convert(Image::FORMAT_RGB8);
			}
		} else if (image->get_format() < Image::FORMAT_RGBA8) {
			image->optimize_channels();
		}
	}

	Image::CompressSource csource = Image::COMPRESS_SOURCE_GENERIC;
	if (channel_pack == 0) {
		csource = Image::COMPRESS_SOURCE_SRGB;
	} else if (channel_pack == 2) {
		// force normal
		csource = Image::COMPRESS_SOURCE_NORMAL;
	}

	Image::UsedChannels used_channels = image->detect_used_channels(csource);

	Vector<Ref<Image>> slices;

	int slice_w = image->get_width() / hslices;
	int slice_h = image->get_height() / vslices;

	for (int i = 0; i < vslices; i++) {
		for (int j = 0; j < hslices; j++) {
			int x = slice_w * j;
			int y = slice_h * i;
			Ref<Image> slice = image->get_region(Rect2i(x, y, slice_w, slice_h));
			ERR_CONTINUE(slice.is_null() || slice->is_empty());
			if (slice->get_width() != slice_w || slice->get_height() != slice_h) {
				slice->resize(slice_w, slice_h);
			}
			slices.push_back(slice);
		}
	}
	Array formats_imported;
	Ref<LayeredTextureImport> texture_import;
	texture_import.instantiate();
	texture_import->csource = &csource;
	texture_import->save_path = p_save_path;
	texture_import->options = p_options;
	texture_import->platform_variants = r_platform_variants;
	texture_import->image = image;
	texture_import->formats_imported = formats_imported;
	texture_import->slices = &slices;
	texture_import->compress_mode = compress_mode;
	texture_import->lossy = lossy;
	texture_import->hdr_compression = hdr_compression;
	texture_import->mipmaps = mipmaps;
	texture_import->used_channels = used_channels;
	texture_import->high_quality = high_quality;

	_check_compress_ctex(p_source_file, texture_import);
	if (r_metadata) {
		Dictionary meta;
		meta["vram_texture"] = compress_mode == COMPRESS_VRAM_COMPRESSED;
		if (formats_imported.size()) {
			meta["imported_formats"] = formats_imported;
		}
		*r_metadata = meta;
	}

	return OK;
}

const char *ResourceImporterLayeredTexture::compression_formats[] = {
	"s3tc_bptc",
	"etc2_astc",
	nullptr
};

String ResourceImporterLayeredTexture::get_import_settings_string() const {
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

bool ResourceImporterLayeredTexture::are_import_settings_valid(const String &p_path, const Dictionary &p_meta) const {
	//will become invalid if formats are missing to import
	if (!p_meta.has("vram_texture")) {
		return false;
	}

	bool vram = p_meta["vram_texture"];
	if (!vram) {
		return true; //do not care about non vram
	}

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

ResourceImporterLayeredTexture *ResourceImporterLayeredTexture::singleton = nullptr;

ResourceImporterLayeredTexture::ResourceImporterLayeredTexture(bool p_singleton) {
	// This should only be set through the EditorNode.
	if (p_singleton) {
		singleton = this;
	}

	mode = MODE_CUBEMAP;
}

ResourceImporterLayeredTexture::~ResourceImporterLayeredTexture() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ResourceImporterLayeredTexture::_check_compress_ctex(const String &p_source_file, Ref<LayeredTextureImport> r_texture_import) {
	String extension = get_save_extension();
	ERR_FAIL_NULL(r_texture_import->csource);
	if (r_texture_import->compress_mode != COMPRESS_VRAM_COMPRESSED) {
		// Import normally.
		_save_tex(*r_texture_import->slices, r_texture_import->save_path + "." + extension, r_texture_import->compress_mode, r_texture_import->lossy, Image::COMPRESS_S3TC /* IGNORED */, *r_texture_import->csource, r_texture_import->used_channels, r_texture_import->mipmaps, false);
		return;
	}
	// Must import in all formats, in order of priority (so platform chooses the best supported one. IE, etc2 over etc).
	// Android, GLES 2.x

	const bool can_s3tc_bptc = ResourceImporterTextureSettings::should_import_s3tc_bptc();
	const bool can_etc2_astc = ResourceImporterTextureSettings::should_import_etc2_astc();

	// Add list of formats imported
	if (can_s3tc_bptc) {
		r_texture_import->formats_imported.push_back("s3tc_bptc");
	}
	if (can_etc2_astc) {
		r_texture_import->formats_imported.push_back("etc2_astc");
	}

	bool can_compress_hdr = r_texture_import->hdr_compression > 0;
	ERR_FAIL_COND(r_texture_import->image.is_null());
	bool is_hdr = (r_texture_import->image->get_format() >= Image::FORMAT_RF && r_texture_import->image->get_format() <= Image::FORMAT_RGBE9995);
	ERR_FAIL_NULL(r_texture_import->slices);
	// Can compress hdr, but hdr with alpha is not compressible.
	bool use_uncompressed = false;

	if (is_hdr) {
		if (r_texture_import->used_channels == Image::USED_CHANNELS_LA || r_texture_import->used_channels == Image::USED_CHANNELS_RGBA) {
			if (r_texture_import->hdr_compression == 2) {
				// The user selected to compress hdr anyway, so force an alpha-less format.
				if (r_texture_import->image->get_format() == Image::FORMAT_RGBAF) {
					for (int i = 0; i < r_texture_import->slices->size(); i++) {
						r_texture_import->slices->write[i]->convert(Image::FORMAT_RGBF);
					}

				} else if (r_texture_import->image->get_format() == Image::FORMAT_RGBAH) {
					for (int i = 0; i < r_texture_import->slices->size(); i++) {
						r_texture_import->slices->write[i]->convert(Image::FORMAT_RGBH);
					}
				}
			} else {
				can_compress_hdr = false;
			}
		}

		if (!can_compress_hdr) {
			//default to rgbe
			if (r_texture_import->image->get_format() != Image::FORMAT_RGBE9995) {
				for (int i = 0; i < r_texture_import->slices->size(); i++) {
					r_texture_import->slices->write[i]->convert(Image::FORMAT_RGBE9995);
				}
			}
			use_uncompressed = true;
		}
	}

	if (use_uncompressed) {
		_save_tex(*r_texture_import->slices, r_texture_import->save_path + "." + extension, COMPRESS_VRAM_UNCOMPRESSED, r_texture_import->lossy, Image::COMPRESS_S3TC /* IGNORED */, *r_texture_import->csource, r_texture_import->used_channels, r_texture_import->mipmaps, false);
	} else {
		if (can_s3tc_bptc) {
			Image::CompressMode image_compress_mode;
			String image_compress_format;
			if (r_texture_import->high_quality || is_hdr) {
				image_compress_mode = Image::COMPRESS_BPTC;
				image_compress_format = "bptc";
			} else {
				image_compress_mode = Image::COMPRESS_S3TC;
				image_compress_format = "s3tc";
			}
			_save_tex(*r_texture_import->slices, r_texture_import->save_path + "." + image_compress_format + "." + extension, r_texture_import->compress_mode, r_texture_import->lossy, image_compress_mode, *r_texture_import->csource, r_texture_import->used_channels, r_texture_import->mipmaps, true);
			r_texture_import->platform_variants->push_back(image_compress_format);
		}

		if (can_etc2_astc) {
			Image::CompressMode image_compress_mode;
			String image_compress_format;
			if (r_texture_import->high_quality || is_hdr) {
				image_compress_mode = Image::COMPRESS_ASTC;
				image_compress_format = "astc";
			} else {
				image_compress_mode = Image::COMPRESS_ETC2;
				image_compress_format = "etc2";
			}
			_save_tex(*r_texture_import->slices, r_texture_import->save_path + "." + image_compress_format + "." + extension, r_texture_import->compress_mode, r_texture_import->lossy, image_compress_mode, *r_texture_import->csource, r_texture_import->used_channels, r_texture_import->mipmaps, true);
			r_texture_import->platform_variants->push_back(image_compress_format);
		}
	}
}
