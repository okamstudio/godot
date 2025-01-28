/**************************************************************************/
/*  texture.h                                                             */
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

#ifndef TEXTURE_H
#define TEXTURE_H

#include "core/io/resource.h"
#include "core/math/rect2.h"
#include "scene/resources/curve.h"
#include "scene/resources/gradient.h"
#include "servers/camera_server.h"
#include "servers/rendering_server.h"

class Texture : public Resource {
	GDCLASS(Texture, Resource);

public:
	Texture() {}
};

class Texture2D : public Texture {
	GDCLASS(Texture2D, Texture);
	OBJ_SAVE_TYPE(Texture2D); // Saves derived classes with common type so they can be interchanged.

protected:
	static void _bind_methods();

	GDVIRTUAL0RC_REQUIRED(int, _get_width)
	GDVIRTUAL0RC_REQUIRED(int, _get_height)
	GDVIRTUAL2RC(bool, _is_pixel_opaque, int, int)
	GDVIRTUAL0RC(bool, _has_alpha)

	GDVIRTUAL4C(_draw, RID, Point2, Color, bool)
	GDVIRTUAL5C(_draw_rect, RID, Rect2, bool, Color, bool)
	GDVIRTUAL6C(_draw_rect_region, RID, Rect2, Rect2, Color, bool, bool)

public:
	virtual int get_width() const;
	virtual int get_height() const;
	virtual Size2 get_size() const;

	virtual bool is_pixel_opaque(int p_x, int p_y) const;

	virtual bool has_alpha() const;

	virtual void draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false) const;
	virtual void draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false) const;
	virtual void draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = true) const;
	virtual bool get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const;

	virtual Ref<Image> get_image() const { return Ref<Image>(); }

	virtual Ref<Resource> create_placeholder() const;

	Texture2D();
};

class TextureLayered : public Texture {
	GDCLASS(TextureLayered, Texture);

protected:
	static void _bind_methods();

	GDVIRTUAL0RC_REQUIRED(Image::Format, _get_format)
	GDVIRTUAL0RC_REQUIRED(uint32_t, _get_layered_type)
	GDVIRTUAL0RC_REQUIRED(int, _get_width)
	GDVIRTUAL0RC_REQUIRED(int, _get_height)
	GDVIRTUAL0RC_REQUIRED(int, _get_layers)
	GDVIRTUAL0RC_REQUIRED(bool, _has_mipmaps)
	GDVIRTUAL1RC_REQUIRED(Ref<Image>, _get_layer_data, int)
public:
	enum LayeredType {
		LAYERED_TYPE_2D_ARRAY,
		LAYERED_TYPE_CUBEMAP,
		LAYERED_TYPE_CUBEMAP_ARRAY
	};

	virtual Image::Format get_format() const;
	virtual LayeredType get_layered_type() const;
	virtual int get_width() const;
	virtual int get_height() const;
	virtual int get_layers() const;
	virtual bool has_mipmaps() const;
	virtual Ref<Image> get_layer_data(int p_layer) const;

	TextureLayered() {}
};

VARIANT_ENUM_CAST(TextureLayered::LayeredType)

class Texture3D : public Texture {
	GDCLASS(Texture3D, Texture);

protected:
	static void _bind_methods();

	TypedArray<Image> _get_datai() const;

	GDVIRTUAL0RC_REQUIRED(Image::Format, _get_format)
	GDVIRTUAL0RC_REQUIRED(int, _get_width)
	GDVIRTUAL0RC_REQUIRED(int, _get_height)
	GDVIRTUAL0RC_REQUIRED(int, _get_depth)
	GDVIRTUAL0RC_REQUIRED(bool, _has_mipmaps)
	GDVIRTUAL0RC_REQUIRED(TypedArray<Image>, _get_data)
public:
	virtual Image::Format get_format() const;
	virtual int get_width() const;
	virtual int get_height() const;
	virtual int get_depth() const;
	virtual bool has_mipmaps() const;
	virtual Vector<Ref<Image>> get_data() const;
	virtual Ref<Resource> create_placeholder() const;
};

#endif // TEXTURE_H
