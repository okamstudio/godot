/**************************************************************************/
/*  image_loader.h                                                        */
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

#include "core/core_bind.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/object/gdvirtual.gen.inc"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/variant/binder_common.h"

class ImageLoader;

class ImageFormatLoader : public RefCounted {
	GDCLASS(ImageFormatLoader, RefCounted);

	friend class ImageLoader;
	friend class ResourceFormatLoaderImage;

public:
	enum LoaderFlags {
		FLAG_NONE = 0,
		FLAG_FORCE_LINEAR = 1,
		FLAG_CONVERT_COLORS = 2,
	};

protected:
	static void _bind_methods();

	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFormatLoader::LoaderFlags> p_flags = FLAG_NONE, float p_scale = 1.0) = 0;
	virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;
	bool recognize(const String &p_extension) const;

public:
	virtual ~ImageFormatLoader() {}
};

VARIANT_BITFIELD_CAST(ImageFormatLoader::LoaderFlags);

class ImageFormatLoaderExtension : public ImageFormatLoader {
	GDCLASS(ImageFormatLoaderExtension, ImageFormatLoader);

protected:
	static void _bind_methods();

public:
	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFormatLoader::LoaderFlags> p_flags = FLAG_NONE, float p_scale = 1.0) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;

	void add_format_loader();
	void remove_format_loader();

	GDVIRTUAL0RC(PackedStringArray, _get_recognized_extensions);
	GDVIRTUAL4R(Error, _load_image, Ref<Image>, Ref<FileAccess>, BitField<ImageFormatLoader::LoaderFlags>, float);
};

class ImageLoader {
	static Vector<Ref<ImageFormatLoader>> loader;
	friend class ResourceFormatLoaderImage;

protected:
public:
	static Error load_image(const String &p_file, Ref<Image> p_image, Ref<FileAccess> p_custom = Ref<FileAccess>(), BitField<ImageFormatLoader::LoaderFlags> p_flags = ImageFormatLoader::FLAG_NONE, float p_scale = 1.0);
	static void get_recognized_extensions(List<String> *p_extensions);
	static Ref<ImageFormatLoader> recognize(const String &p_extension);

	static void add_image_format_loader(Ref<ImageFormatLoader> p_loader);
	static void remove_image_format_loader(Ref<ImageFormatLoader> p_loader);

	static void cleanup();
};

class ResourceFormatLoaderImage : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};
