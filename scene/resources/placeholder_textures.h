/**************************************************************************/
/*  placeholder_textures.h                                                */
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

#include "scene/resources/texture.h"

class PlaceholderTexture2D : public Texture2D {
	GDCLASS(PlaceholderTexture2D, Texture2D)

	mutable RID rid;
	Size2 size = Size2(1, 1);

protected:
	static void _bind_methods();

public:
	void set_size(Size2 p_size);

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;
	virtual bool has_alpha() const override;

	virtual Ref<Image> get_image() const override;

	PlaceholderTexture2D();
	~PlaceholderTexture2D();
};

class PlaceholderTexture3D : public Texture3D {
	GDCLASS(PlaceholderTexture3D, Texture3D)

	mutable RID rid;
	Vector3i size = Vector3i(1, 1, 1);

protected:
	static void _bind_methods();

public:
	void set_size(const Vector3i &p_size);
	Vector3i get_size() const;
	virtual Image::Format get_format() const override;
	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual int get_depth() const override;
	virtual bool has_mipmaps() const override;
	virtual Vector<Ref<Image>> get_data() const override;
	virtual RID get_rid() const override;

	PlaceholderTexture3D();
	~PlaceholderTexture3D();
};

class PlaceholderTextureLayered : public TextureLayered {
	GDCLASS(PlaceholderTextureLayered, TextureLayered)

	mutable RID rid;
	Size2i size = Size2i(1, 1);
	int layers = 1;
	LayeredType layered_type = LAYERED_TYPE_2D_ARRAY;

protected:
	static void _bind_methods();

public:
	void set_size(const Size2i &p_size);
	Size2i get_size() const;
	void set_layers(int p_layers);
	virtual Image::Format get_format() const override;
	virtual LayeredType get_layered_type() const override;
	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual int get_layers() const override;
	virtual bool has_mipmaps() const override;
	virtual Ref<Image> get_layer_data(int p_layer) const override;
	virtual RID get_rid() const override;

	PlaceholderTextureLayered(LayeredType p_type);
	~PlaceholderTextureLayered();
};

class PlaceholderTexture2DArray : public PlaceholderTextureLayered {
	GDCLASS(PlaceholderTexture2DArray, PlaceholderTextureLayered)
public:
	PlaceholderTexture2DArray() :
			PlaceholderTextureLayered(LAYERED_TYPE_2D_ARRAY) {}
};

class PlaceholderCubemap : public PlaceholderTextureLayered {
	GDCLASS(PlaceholderCubemap, PlaceholderTextureLayered)
public:
	PlaceholderCubemap() :
			PlaceholderTextureLayered(LAYERED_TYPE_CUBEMAP) {}
};

class PlaceholderCubemapArray : public PlaceholderTextureLayered {
	GDCLASS(PlaceholderCubemapArray, PlaceholderTextureLayered)
public:
	PlaceholderCubemapArray() :
			PlaceholderTextureLayered(LAYERED_TYPE_CUBEMAP_ARRAY) {}
};
