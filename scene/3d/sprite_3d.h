/**************************************************************************/
/*  sprite_3d.h                                                           */
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

#ifndef SPRITE_3D_H
#define SPRITE_3D_H

#include "scene/2d/animated_sprite.h"
#include "scene/3d/visual_instance.h"

class SpriteBase3D : public GeometryInstance {
	GDCLASS(SpriteBase3D, GeometryInstance);

	mutable Ref<TriangleMesh> triangle_mesh; //cached

public:
	enum DrawFlags {
		FLAG_TRANSPARENT,
		FLAG_SHADED,
		FLAG_DOUBLE_SIDED,
		FLAG_DISABLE_DEPTH_TEST,
		FLAG_FIXED_SIZE,
		FLAG_MAX

	};

	enum AlphaCutMode {
		ALPHA_CUT_DISABLED,
		ALPHA_CUT_DISCARD,
		ALPHA_CUT_OPAQUE_PREPASS
	};

private:
	bool color_dirty;
	Color color_accum;

	SpriteBase3D *parent_sprite;
	List<SpriteBase3D *> children;
	List<SpriteBase3D *>::Element *pI;

	bool centered;
	Point2 offset;

	bool hflip;
	bool vflip;

	Color modulate;
	int render_priority = 0;
	float opacity;

	Vector3::Axis axis;
	float pixel_size;
	AABB aabb;

	RID mesh;
	RID material;

	bool flags[FLAG_MAX];
	AlphaCutMode alpha_cut;
	Material3D::BillboardMode billboard_mode;
	bool pending_update;
	void _im_update();

	void _propagate_color_changed();

protected:
	Color _get_color_accum();
	void _notification(int p_what);
	static void _bind_methods();
	virtual void _draw(){};
	void draw_texture_rect(Ref<Texture> p_texture, Rect2 p_dst_rect, Rect2 p_src_rect);
	_FORCE_INLINE_ void set_aabb(const AABB &p_aabb) { aabb = p_aabb; }
	_FORCE_INLINE_ RID &get_mesh() { return mesh; }
	_FORCE_INLINE_ RID &get_material() { return material; }

	uint32_t mesh_surface_offsets[VS::ARRAY_MAX];
	PoolByteArray mesh_buffer;
	uint32_t mesh_stride[VS::ARRAY_MAX];
	uint32_t mesh_surface_format;

	void _queue_update();

public:
	void set_centered(bool p_center);
	bool is_centered() const;

	void set_offset(const Point2 &p_offset);
	Point2 get_offset() const;

	void set_flip_h(bool p_flip);
	bool is_flipped_h() const;

	void set_flip_v(bool p_flip);
	bool is_flipped_v() const;

	void set_modulate(const Color &p_color);
	Color get_modulate() const;

	void set_opacity(float p_amount);
	float get_opacity() const;

	void set_render_priority(int p_priority);
	int get_render_priority() const;

	void set_pixel_size(float p_amount);
	float get_pixel_size() const;

	void set_axis(Vector3::Axis p_axis);
	Vector3::Axis get_axis() const;

	void set_draw_flag(DrawFlags p_flag, bool p_enable);
	bool get_draw_flag(DrawFlags p_flag) const;

	void set_alpha_cut_mode(AlphaCutMode p_mode);
	AlphaCutMode get_alpha_cut_mode() const;
	void set_billboard_mode(Material3D::BillboardMode p_mode);
	Material3D::BillboardMode get_billboard_mode() const;

	virtual Rect2 get_item_rect() const;

	virtual AABB get_aabb() const;
	virtual PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;
	Ref<TriangleMesh> generate_triangle_mesh() const;

	SpriteBase3D();
	~SpriteBase3D();
};

class Sprite3D : public SpriteBase3D {
	GDCLASS(Sprite3D, SpriteBase3D);
	Ref<Texture> texture;

	bool region;
	Rect2 region_rect;

	int frame;

	int vframes;
	int hframes;

protected:
	virtual void _draw();
	static void _bind_methods();

	virtual void _validate_property(PropertyInfo &property) const;

public:
	void set_texture(const Ref<Texture> &p_texture);
	Ref<Texture> get_texture() const;

	void set_region(bool p_region);
	bool is_region() const;

	void set_region_rect(const Rect2 &p_region_rect);
	Rect2 get_region_rect() const;

	void set_frame(int p_frame);
	int get_frame() const;

	void set_frame_coords(const Vector2 &p_coord);
	Vector2 get_frame_coords() const;

	void set_vframes(int p_amount);
	int get_vframes() const;

	void set_hframes(int p_amount);
	int get_hframes() const;

	virtual Rect2 get_item_rect() const;

	Sprite3D();
	//~Sprite3D();
};

class AnimatedSprite3D : public SpriteBase3D {
	GDCLASS(AnimatedSprite3D, SpriteBase3D);

	Ref<SpriteFrames> frames;
	bool playing;
	StringName animation;
	int frame;

	float timeout;

	bool hflip;
	bool vflip;

	Color modulate;

	void _res_changed();

	void _reset_timeout();
	void _set_playing(bool p_playing);
	bool _is_playing() const;

protected:
	virtual void _draw();
	static void _bind_methods();
	void _notification(int p_what);
	virtual void _validate_property(PropertyInfo &property) const;

public:
	void set_sprite_frames(const Ref<SpriteFrames> &p_frames);
	Ref<SpriteFrames> get_sprite_frames() const;

	void play(const StringName &p_animation = StringName());
	void stop();
	bool is_playing() const;

	void set_animation(const StringName &p_animation);
	StringName get_animation() const;

	void set_frame(int p_frame);
	int get_frame() const;

	virtual Rect2 get_item_rect() const;

	virtual String get_configuration_warning() const;
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const;

	AnimatedSprite3D();
};

VARIANT_ENUM_CAST(SpriteBase3D::DrawFlags);
VARIANT_ENUM_CAST(SpriteBase3D::AlphaCutMode);

#endif // SPRITE_3D_H
