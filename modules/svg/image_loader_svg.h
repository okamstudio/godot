/**************************************************************************/
/*  image_loader_svg.h                                                    */
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

#include "core/io/image_loader.h"

class ImageLoaderSVG : public ImageFormatLoader {
	static HashMap<Color, Color> forced_color_map;

	static void _replace_color_property(const HashMap<Color, Color> &p_color_map, const String &p_prefix, String &r_string);

	static Ref<Image> load_mem_svg(const uint8_t *p_svg, int p_size, float p_scale);

public:
	static void set_forced_color_map(const HashMap<Color, Color> &p_color_map);

	static Error create_image_from_utf8_buffer(Ref<Image> p_image, const uint8_t *p_buffer, int p_buffer_size, float p_scale, bool p_upsample);
	static Error create_image_from_utf8_buffer(Ref<Image> p_image, const PackedByteArray &p_buffer, float p_scale, bool p_upsample);

	static Error create_image_from_string(Ref<Image> p_image, String p_string, float p_scale, bool p_upsample, const HashMap<Color, Color> &p_color_map);

	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;

	ImageLoaderSVG();
};
