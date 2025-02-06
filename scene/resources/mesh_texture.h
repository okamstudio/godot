/**************************************************************************/
/*  mesh_texture.h                                                        */
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

#ifndef MESH_TEXTURE_H
#define MESH_TEXTURE_H

#include "scene/resources/texture.h"

class Mesh;

class MeshTexture : public Texture2D {
	GDCLASS(MeshTexture, Texture2D);
	RES_BASE_EXTENSION("meshtex");

	Ref<Texture2D> base_texture;
	Ref<Mesh> mesh;
	RID texture;
	RID viewport;
	RID canvas;
	RID canvas_item;
	Size2 size = Size2(512, 512);
	Size2 scale = Size2(1, 1);
	Size2 offset = Size2(0, 0);
	Color background = Color(0, 0, 0, 0);

	bool update_pending = false;
	void _queue_update();
	void _update_viewport_texture();

protected:
	static void _bind_methods();

public:
	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;

	virtual bool has_alpha() const override;

	virtual bool is_pixel_opaque(int p_x, int p_y) const override;

	virtual Ref<Image> get_image() const override;

	virtual void set_path(const String &p_path, bool p_take_over = false) override;

	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_image_size(const Size2 &p_size);
	Size2 get_image_size() const;

	void set_base_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_base_texture() const;

	void set_scale(const Size2 &p_scale);
	Size2 get_scale() const;

	void set_offset(const Size2 &p_offset);
	Size2 get_offset() const;

	void set_background(const Color &p_color);
	Color get_background() const;

	MeshTexture();
	~MeshTexture();
};

#endif // MESH_TEXTURE_H
