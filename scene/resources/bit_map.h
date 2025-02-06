/**************************************************************************/
/*  bit_map.h                                                             */
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
#include "core/io/resource.h"

template <typename T>
class TypedArray;

class BitMap : public Resource {
	GDCLASS(BitMap, Resource);
	OBJ_SAVE_TYPE(BitMap);

	Vector<uint8_t> bitmask;
	int width = 0;
	int height = 0;

	Vector<Vector<Vector2>> _march_square(const Rect2i &p_rect, const Point2i &p_start) const;

	TypedArray<PackedVector2Array> _opaque_to_polygons_bind(const Rect2i &p_rect, float p_epsilon) const;

protected:
	void _set_data(const Dictionary &p_d);
	Dictionary _get_data() const;

	static void _bind_methods();

public:
	void create(const Size2i &p_size);
	void create_from_image_alpha(const Ref<Image> &p_image, float p_threshold = 0.1);

	void set_bitv(const Point2i &p_pos, bool p_value);
	void set_bit(int p_x, int p_y, bool p_value);
	void set_bit_rect(const Rect2i &p_rect, bool p_value);
	bool get_bitv(const Point2i &p_pos) const;
	bool get_bit(int p_x, int p_y) const;

	int get_true_bit_count() const;

	Size2i get_size() const;
	void resize(const Size2i &p_new_size);

	void grow_mask(int p_pixels, const Rect2i &p_rect);
	void shrink_mask(int p_pixels, const Rect2i &p_rect);

	void blit(const Vector2i &p_pos, const Ref<BitMap> &p_bitmap);
	Ref<Image> convert_to_image() const;

	Vector<Vector<Vector2>> clip_opaque_to_polygons(const Rect2i &p_rect, float p_epsilon = 2.0) const;

	BitMap();
};
