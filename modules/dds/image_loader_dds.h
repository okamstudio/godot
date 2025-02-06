/**************************************************************************/
/*  image_loader_dds.h                                                    */
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

#ifndef IMAGE_LOADER_DDS_H
#define IMAGE_LOADER_DDS_H

#include "core/io/image_loader.h"

class ImageLoaderDDS : public ImageFormatLoader {
public:
	// The legacy bitmasked format names here represent the actual data layout in the files,
	// while their official names are flipped (e.g. RGBA8 layout is officially called ABGR8).
	enum DDSFormat {
		DDS_DXT1,
		DDS_DXT3,
		DDS_DXT5,
		DDS_ATI1,
		DDS_ATI2,
		DDS_BC6U,
		DDS_BC6S,
		DDS_BC7,
		DDS_R16F,
		DDS_RG16F,
		DDS_RGBA16F,
		DDS_R32F,
		DDS_RG32F,
		DDS_RGB32F,
		DDS_RGBA32F,
		DDS_RGB9E5,
		DDS_RGB8,
		DDS_RGBA8,
		DDS_BGR8,
		DDS_BGRA8,
		DDS_BGR5A1,
		DDS_BGR565,
		DDS_B2GR3,
		DDS_B2GR3A8,
		DDS_BGR10A2,
		DDS_RGB10A2,
		DDS_BGRA4,
		DDS_LUMINANCE,
		DDS_LUMINANCE_ALPHA,
		DDS_LUMINANCE_ALPHA_4,
		DDS_MAX
	};

	enum DDSType {
		DDST_2D = 1,
		DDST_CUBEMAP,
		DDST_3D,

		DDST_TYPE_MASK = 0x7F,
		DDST_ARRAY = 0x80,
	};

	static Error load_layer(Ref<Image> p_layer, Ref<FileAccess> p_file, DDSFormat p_dds_format, uint32_t p_width, uint32_t p_height, uint32_t p_mipmaps, uint32_t p_pitch, uint32_t p_flags, Vector<uint8_t> &r_src_data);
	static Error load_image_layers(Vector<Ref<Image>> &p_layers, Ref<FileAccess> p_file, uint32_t *r_dds_type = nullptr);

	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool should_import(const String &p_resource_type) const;
	ImageLoaderDDS();
};

#endif // IMAGE_LOADER_DDS_H
