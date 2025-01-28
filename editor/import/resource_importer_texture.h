/**************************************************************************/
/*  resource_importer_texture.h                                           */
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

#ifndef RESOURCE_IMPORTER_TEXTURE_H
#define RESOURCE_IMPORTER_TEXTURE_H

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_importer.h"
#include "servers/rendering_server.h"

class CompressedTexture2D;

class ResourceImporterTexture : public ResourceImporter {
	GDCLASS(ResourceImporterTexture, ResourceImporter);

public:
	enum CompressMode {
		COMPRESS_LOSSLESS,
		COMPRESS_LOSSY,
		COMPRESS_VRAM_COMPRESSED,
		COMPRESS_VRAM_UNCOMPRESSED,
		COMPRESS_BASIS_UNIVERSAL
	};

protected:
	enum {
		MAKE_3D_FLAG = 1,
		MAKE_ROUGHNESS_FLAG = 2,
		MAKE_NORMAL_FLAG = 4
	};

	Mutex mutex;
	struct MakeInfo {
		int flags = 0;
		String normal_path_for_roughness;
		RS::TextureDetectRoughnessChannel channel_for_roughness = RS::TEXTURE_DETECT_ROUGHNESS_R;
	};

	HashMap<StringName, MakeInfo> make_flags;

	static void _texture_reimport_roughness(const Ref<CompressedTexture2D> &p_tex, const String &p_normal_path, RenderingServer::TextureDetectRoughnessChannel p_channel);
	static void _texture_reimport_3d(const Ref<CompressedTexture2D> &p_tex);
	static void _texture_reimport_normal(const Ref<CompressedTexture2D> &p_tex);

	static ResourceImporterTexture *singleton;
	static const char *compression_formats[];

	void _save_ctex(const Ref<Image> &p_image, const String &p_to_path, CompressMode p_compress_mode, float p_lossy_quality, Image::CompressMode p_vram_compression, bool p_mipmaps, bool p_streamable, bool p_detect_3d, bool p_detect_srgb, bool p_detect_normal, bool p_force_normal, bool p_srgb_friendly, bool p_force_po2_for_compressed, uint32_t p_limit_mipmap, const Ref<Image> &p_normal, Image::RoughnessChannel p_roughness_channel);
	void _save_editor_meta(const Dictionary &p_metadata, const String &p_to_path);
	Dictionary _load_editor_meta(const String &p_to_path) const;

	static inline void _clamp_hdr_exposure(Ref<Image> &r_image);
	static inline void _invert_y_channel(Ref<Image> &r_image);
	static inline void _print_callback_message(const String &p_message);

public:
	static void save_to_ctex_format(Ref<FileAccess> f, const Ref<Image> &p_image, CompressMode p_compress_mode, Image::UsedChannels p_channels, Image::CompressMode p_compress_format, float p_lossy_quality);

	static ResourceImporterTexture *get_singleton() { return singleton; }
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;

	enum Preset {
		PRESET_DETECT,
		PRESET_2D,
		PRESET_3D,
	};

	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_idx) const override;

	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

	virtual bool can_import_threaded() const override { return true; }

	void update_imports();

	virtual bool are_import_settings_valid(const String &p_path, const Dictionary &p_meta) const override;
	virtual String get_import_settings_string() const override;

	ResourceImporterTexture(bool p_singleton = false);
	~ResourceImporterTexture();
};

#endif // RESOURCE_IMPORTER_TEXTURE_H
