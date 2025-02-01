/**************************************************************************/
/*  resource_importer_layered_texture.h                                   */
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

#include "core/io/image.h"
#include "core/io/resource_importer.h"
#include "core/object/ref_counted.h"

class CompressedTexture2D;

class LayeredTextureImport : public RefCounted {
	GDCLASS(LayeredTextureImport, RefCounted);

public:
	Image::CompressSource *csource = nullptr;
	String save_path;
	HashMap<StringName, Variant> options;
	List<String> *platform_variants = nullptr;
	Ref<Image> image = nullptr;
	Array formats_imported;
	Vector<Ref<Image>> *slices = nullptr;
	int compress_mode = 0;
	float lossy = 1.0;
	int hdr_compression = 0;
	bool mipmaps = true;
	bool high_quality = false;
	Image::UsedChannels used_channels = Image::USED_CHANNELS_RGBA;
	virtual ~LayeredTextureImport() {}
};

class ResourceImporterLayeredTexture : public ResourceImporter {
	GDCLASS(ResourceImporterLayeredTexture, ResourceImporter);

public:
	enum Mode {
		MODE_2D_ARRAY,
		MODE_CUBEMAP,
		MODE_CUBEMAP_ARRAY,
		MODE_3D,
	};

	enum CubemapFormat {
		CUBEMAP_FORMAT_1X6,
		CUBEMAP_FORMAT_2X3,
		CUBEMAP_FORMAT_3X2,
		CUBEMAP_FORMAT_6X1,
	};

	enum TextureFlags {
		TEXTURE_FLAGS_MIPMAPS = 1
	};

private:
	Mode mode;
	static const char *compression_formats[];

protected:
	static ResourceImporterLayeredTexture *singleton;

public:
	void _check_compress_ctex(const String &p_source_file, Ref<LayeredTextureImport> r_texture_import);

	static ResourceImporterLayeredTexture *get_singleton() { return singleton; }
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;

	enum CompressMode {
		COMPRESS_LOSSLESS,
		COMPRESS_LOSSY,
		COMPRESS_VRAM_COMPRESSED,
		COMPRESS_VRAM_UNCOMPRESSED,
		COMPRESS_BASIS_UNIVERSAL
	};

	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_idx) const override;

	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	void _save_tex(Vector<Ref<Image>> p_images, const String &p_to_path, int p_compress_mode, float p_lossy, Image::CompressMode p_vram_compression, Image::CompressSource p_csource, Image::UsedChannels used_channels, bool p_mipmaps, bool p_force_po2);

	virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

	virtual bool are_import_settings_valid(const String &p_path, const Dictionary &p_meta) const override;
	virtual String get_import_settings_string() const override;

	virtual bool can_import_threaded() const override { return true; }

	void set_mode(Mode p_mode) { mode = p_mode; }

	ResourceImporterLayeredTexture(bool p_singleton = false);
	~ResourceImporterLayeredTexture();
};
