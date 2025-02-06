/**************************************************************************/
/*  primitive_meshes.h                                                    */
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

#include "scene/resources/font.h"
#include "scene/resources/mesh.h"
#include "servers/text_server.h"

///@TODO probably should change a few integers to unsigned integers...

/**
	Base class for all the classes in this file, handles a number of code functions that are shared among all meshes.
	This class is set apart that it assumes a single surface is always generated for our mesh.
*/

class PrimitiveMesh : public Mesh {
	GDCLASS(PrimitiveMesh, Mesh);

private:
	RID mesh;
	mutable AABB aabb;
	AABB custom_aabb;

	mutable int array_len = 0;
	mutable int index_array_len = 0;

	Ref<Material> material;
	bool flip_faces = false;

	bool add_uv2 = false;
	float uv2_padding = 2.0;

	// make sure we do an update after we've finished constructing our object
	mutable bool pending_request = true;
	void _update() const;

protected:
	// assume primitive triangles as the type, correct for all but one and it will change this :)
	Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;

	// Copy of our texel_size project setting.
	float texel_size = 0.2;

	static void _bind_methods();

	virtual void _create_mesh_array(Array &p_arr) const {}
	GDVIRTUAL0RC(Array, _create_mesh_array)

	Vector2 get_uv2_scale(Vector2 p_margin_scale = Vector2(1.0, 1.0)) const;
	float get_lightmap_texel_size() const;
	virtual void _update_lightmap_size() {}

	void _on_settings_changed();

public:
	virtual int get_surface_count() const override;
	virtual int surface_get_array_len(int p_idx) const override;
	virtual int surface_get_array_index_len(int p_idx) const override;
	virtual Array surface_get_arrays(int p_surface) const override;
	virtual TypedArray<Array> surface_get_blend_shape_arrays(int p_surface) const override;
	virtual Dictionary surface_get_lods(int p_surface) const override;
	virtual BitField<ArrayFormat> surface_get_format(int p_idx) const override;
	virtual Mesh::PrimitiveType surface_get_primitive_type(int p_idx) const override;
	virtual void surface_set_material(int p_idx, const Ref<Material> &p_material) override;
	virtual Ref<Material> surface_get_material(int p_idx) const override;
	virtual int get_blend_shape_count() const override;
	virtual StringName get_blend_shape_name(int p_index) const override;
	virtual void set_blend_shape_name(int p_index, const StringName &p_name) override;
	virtual AABB get_aabb() const override;
	virtual RID get_rid() const override;

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const;

	Array get_mesh_arrays() const;

	void set_custom_aabb(const AABB &p_custom);
	AABB get_custom_aabb() const;

	void set_flip_faces(bool p_enable);
	bool get_flip_faces() const;

	void set_add_uv2(bool p_enable);
	bool get_add_uv2() const { return add_uv2; }

	void set_uv2_padding(float p_padding);
	float get_uv2_padding() const { return uv2_padding; }

	void request_update();

	PrimitiveMesh();
	~PrimitiveMesh();
};

/**
	Mesh for a simple capsule
*/
class CapsuleMesh : public PrimitiveMesh {
	GDCLASS(CapsuleMesh, PrimitiveMesh);

private:
	float radius = 0.5;
	float height = 2.0;
	int radial_segments = 64;
	int rings = 8;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	static void create_mesh_array(Array &p_arr, float radius, float height, int radial_segments = 64, int rings = 8, bool p_add_uv2 = false, const float p_uv2_padding = 1.0);

	void set_radius(const float p_radius);
	float get_radius() const;

	void set_height(const float p_height);
	float get_height() const;

	void set_radial_segments(const int p_segments);
	int get_radial_segments() const;

	void set_rings(const int p_rings);
	int get_rings() const;

	CapsuleMesh();
};

/**
	A box
*/
class BoxMesh : public PrimitiveMesh {
	GDCLASS(BoxMesh, PrimitiveMesh);

private:
	Vector3 size = Vector3(1, 1, 1);
	int subdivide_w = 0;
	int subdivide_h = 0;
	int subdivide_d = 0;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	static void create_mesh_array(Array &p_arr, Vector3 size, int subdivide_w = 0, int subdivide_h = 0, int subdivide_d = 0, bool p_add_uv2 = false, const float p_uv2_padding = 1.0);

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_subdivide_width(const int p_divisions);
	int get_subdivide_width() const;

	void set_subdivide_height(const int p_divisions);
	int get_subdivide_height() const;

	void set_subdivide_depth(const int p_divisions);
	int get_subdivide_depth() const;

	BoxMesh();
};

/**
	A cylinder
*/

class CylinderMesh : public PrimitiveMesh {
	GDCLASS(CylinderMesh, PrimitiveMesh);

private:
	float top_radius = 0.5;
	float bottom_radius = 0.5;
	float height = 2.0;
	int radial_segments = 64;
	int rings = 4;
	bool cap_top = true;
	bool cap_bottom = true;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	static void create_mesh_array(Array &p_arr, float top_radius, float bottom_radius, float height, int radial_segments = 64, int rings = 4, bool cap_top = true, bool cap_bottom = true, bool p_add_uv2 = false, const float p_uv2_padding = 1.0);

	void set_top_radius(const float p_radius);
	float get_top_radius() const;

	void set_bottom_radius(const float p_radius);
	float get_bottom_radius() const;

	void set_height(const float p_height);
	float get_height() const;

	void set_radial_segments(const int p_segments);
	int get_radial_segments() const;

	void set_rings(const int p_rings);
	int get_rings() const;

	void set_cap_top(bool p_cap_top);
	bool is_cap_top() const;

	void set_cap_bottom(bool p_cap_bottom);
	bool is_cap_bottom() const;

	CylinderMesh();
};

/*
	A flat rectangle, can be used as quad or heightmap.
*/
class PlaneMesh : public PrimitiveMesh {
	GDCLASS(PlaneMesh, PrimitiveMesh);

public:
	enum Orientation {
		FACE_X,
		FACE_Y,
		FACE_Z,
	};

private:
	Size2 size = Size2(2.0, 2.0);
	int subdivide_w = 0;
	int subdivide_d = 0;
	Vector3 center_offset;
	Orientation orientation = FACE_Y;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	void set_size(const Size2 &p_size);
	Size2 get_size() const;

	void set_subdivide_width(const int p_divisions);
	int get_subdivide_width() const;

	void set_subdivide_depth(const int p_divisions);
	int get_subdivide_depth() const;

	void set_center_offset(const Vector3 p_offset);
	Vector3 get_center_offset() const;

	void set_orientation(const Orientation p_orientation);
	Orientation get_orientation() const;

	PlaneMesh();
};

VARIANT_ENUM_CAST(PlaneMesh::Orientation)

/*
	A flat rectangle, inherits from PlaneMesh but defaults to facing the Z-plane.
*/
class QuadMesh : public PlaneMesh {
	GDCLASS(QuadMesh, PlaneMesh);

public:
	QuadMesh() {
		set_orientation(FACE_Z);
		set_size(Size2(1, 1));
	}
};

/**
	A prism shapen, handy for ramps, triangles, etc.
*/
class PrismMesh : public PrimitiveMesh {
	GDCLASS(PrismMesh, PrimitiveMesh);

private:
	float left_to_right = 0.5;
	Vector3 size = Vector3(1.0, 1.0, 1.0);
	int subdivide_w = 0;
	int subdivide_h = 0;
	int subdivide_d = 0;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	void set_left_to_right(const float p_left_to_right);
	float get_left_to_right() const;

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_subdivide_width(const int p_divisions);
	int get_subdivide_width() const;

	void set_subdivide_height(const int p_divisions);
	int get_subdivide_height() const;

	void set_subdivide_depth(const int p_divisions);
	int get_subdivide_depth() const;

	PrismMesh();
};

/**
	A sphere..
*/
class SphereMesh : public PrimitiveMesh {
	GDCLASS(SphereMesh, PrimitiveMesh);

private:
	float radius = 0.5;
	float height = 1.0;
	int radial_segments = 64;
	int rings = 32;
	bool is_hemisphere = false;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	static void create_mesh_array(Array &p_arr, float radius, float height, int radial_segments = 64, int rings = 32, bool is_hemisphere = false, bool p_add_uv2 = false, const float p_uv2_padding = 1.0);

	void set_radius(const float p_radius);
	float get_radius() const;

	void set_height(const float p_height);
	float get_height() const;

	void set_radial_segments(const int p_radial_segments);
	int get_radial_segments() const;

	void set_rings(const int p_rings);
	int get_rings() const;

	void set_is_hemisphere(const bool p_is_hemisphere);
	bool get_is_hemisphere() const;

	SphereMesh();
};

/**
	Big donut
*/
class TorusMesh : public PrimitiveMesh {
	GDCLASS(TorusMesh, PrimitiveMesh);

private:
	float inner_radius = 0.5;
	float outer_radius = 1.0;
	int rings = 64;
	int ring_segments = 32;

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

	virtual void _update_lightmap_size() override;

public:
	void set_inner_radius(const float p_inner_radius);
	float get_inner_radius() const;

	void set_outer_radius(const float p_outer_radius);
	float get_outer_radius() const;

	void set_rings(const int p_rings);
	int get_rings() const;

	void set_ring_segments(const int p_ring_segments);
	int get_ring_segments() const;

	TorusMesh();
};

/**
	A single point for use in particle systems
*/

class PointMesh : public PrimitiveMesh {
	GDCLASS(PointMesh, PrimitiveMesh)

protected:
	virtual void _create_mesh_array(Array &p_arr) const override;

public:
	PointMesh();
};

class TubeTrailMesh : public PrimitiveMesh {
	GDCLASS(TubeTrailMesh, PrimitiveMesh);

private:
	float radius = 0.5;
	int radial_steps = 8;
	int sections = 5;
	float section_length = 0.2;
	int section_rings = 3;
	bool cap_top = true;
	bool cap_bottom = true;

	Ref<Curve> curve;

	void _curve_changed();

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

public:
	void set_radius(const float p_radius);
	float get_radius() const;

	void set_radial_steps(const int p_radial_steps);
	int get_radial_steps() const;

	void set_sections(const int p_sections);
	int get_sections() const;

	void set_section_length(float p_sectionlength);
	float get_section_length() const;

	void set_section_rings(const int p_section_rings);
	int get_section_rings() const;

	void set_cap_top(bool p_cap_top);
	bool is_cap_top() const;

	void set_cap_bottom(bool p_cap_bottom);
	bool is_cap_bottom() const;

	void set_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_curve() const;

	virtual int get_builtin_bind_pose_count() const override;
	virtual Transform3D get_builtin_bind_pose(int p_index) const override;

	TubeTrailMesh();
};

class RibbonTrailMesh : public PrimitiveMesh {
	GDCLASS(RibbonTrailMesh, PrimitiveMesh);

public:
	enum Shape {
		SHAPE_FLAT,
		SHAPE_CROSS
	};

private:
	float size = 1.0;
	int sections = 5;
	float section_length = 0.2;
	int section_segments = 3;

	Shape shape = SHAPE_CROSS;

	Ref<Curve> curve;

	void _curve_changed();

protected:
	static void _bind_methods();
	virtual void _create_mesh_array(Array &p_arr) const override;

public:
	void set_shape(Shape p_shape);
	Shape get_shape() const;

	void set_size(const float p_size);
	float get_size() const;

	void set_sections(const int p_sections);
	int get_sections() const;

	void set_section_length(float p_sectionlength);
	float get_section_length() const;

	void set_section_segments(const int p_section_segments);
	int get_section_segments() const;

	void set_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_curve() const;

	virtual int get_builtin_bind_pose_count() const override;
	virtual Transform3D get_builtin_bind_pose(int p_index) const override;

	RibbonTrailMesh();
};

/**
	Text...
*/

class TextMesh : public PrimitiveMesh {
	GDCLASS(TextMesh, PrimitiveMesh);

private:
	struct ContourPoint {
		Vector2 point;
		bool sharp = false;

		ContourPoint() {}
		ContourPoint(const Vector2 &p_pt, bool p_sharp) {
			point = p_pt;
			sharp = p_sharp;
		}
	};

	struct ContourInfo {
		real_t length = 0.0;
		bool ccw = true;
		ContourInfo() {}
		ContourInfo(real_t p_len, bool p_ccw) {
			length = p_len;
			ccw = p_ccw;
		}
	};

	struct GlyphMeshKey {
		uint64_t font_id;
		uint32_t gl_id;

		bool operator==(const GlyphMeshKey &p_b) const {
			return (font_id == p_b.font_id) && (gl_id == p_b.gl_id);
		}

		GlyphMeshKey(uint64_t p_font_id, uint32_t p_gl_id) {
			font_id = p_font_id;
			gl_id = p_gl_id;
		}
	};

	struct GlyphMeshKeyHasher {
		_FORCE_INLINE_ static uint32_t hash(const GlyphMeshKey &p_a) {
			return hash_murmur3_buffer(&p_a, sizeof(GlyphMeshKey));
		}
	};

	struct GlyphMeshData {
		Vector<Vector2> triangles;
		Vector<Vector<ContourPoint>> contours;
		Vector<ContourInfo> contours_info;
		Vector2 min_p = Vector2(INFINITY, INFINITY);
		Vector2 max_p = Vector2(-INFINITY, -INFINITY);
	};
	mutable HashMap<GlyphMeshKey, GlyphMeshData, GlyphMeshKeyHasher> cache;

	RID text_rid;
	mutable Vector<RID> lines_rid;

	String text;
	String xl_text;

	int font_size = 16;
	Ref<Font> font_override;

	TextServer::AutowrapMode autowrap_mode = TextServer::AUTOWRAP_OFF;
	BitField<TextServer::JustificationFlag> jst_flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE;
	float width = 500.0;
	float line_spacing = 0.f;
	Point2 lbl_offset;

	HorizontalAlignment horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER;
	VerticalAlignment vertical_alignment = VERTICAL_ALIGNMENT_CENTER;
	bool uppercase = false;
	String language;
	TextServer::Direction text_direction = TextServer::DIRECTION_AUTO;
	TextServer::StructuredTextParser st_parser = TextServer::STRUCTURED_TEXT_DEFAULT;
	Array st_args;

	real_t depth = 0.05;
	real_t pixel_size = 0.01;
	real_t curve_step = 0.5;

	mutable bool dirty_lines = true;
	mutable bool dirty_text = true;
	mutable bool dirty_font = true;
	mutable bool dirty_cache = true;

	void _generate_glyph_mesh_data(const GlyphMeshKey &p_key, const Glyph &p_glyph) const;
	void _font_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

	virtual void _create_mesh_array(Array &p_arr) const override;

public:
	GDVIRTUAL2RC(TypedArray<Vector3i>, _structured_text_parser, Array, String)

	TextMesh();
	~TextMesh();

	void set_horizontal_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_horizontal_alignment() const;

	void set_vertical_alignment(VerticalAlignment p_alignment);
	VerticalAlignment get_vertical_alignment() const;

	void set_text(const String &p_string);
	String get_text() const;

	void set_font(const Ref<Font> &p_font);
	Ref<Font> get_font() const;
	Ref<Font> _get_font_or_default() const;

	void set_font_size(int p_size);
	int get_font_size() const;

	void set_line_spacing(float p_size);
	float get_line_spacing() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	TextServer::AutowrapMode get_autowrap_mode() const;

	void set_justification_flags(BitField<TextServer::JustificationFlag> p_flags);
	BitField<TextServer::JustificationFlag> get_justification_flags() const;

	void set_text_direction(TextServer::Direction p_text_direction);
	TextServer::Direction get_text_direction() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser);
	TextServer::StructuredTextParser get_structured_text_bidi_override() const;

	void set_structured_text_bidi_override_options(Array p_args);
	Array get_structured_text_bidi_override_options() const;

	void set_uppercase(bool p_uppercase);
	bool is_uppercase() const;

	void set_width(real_t p_width);
	real_t get_width() const;

	void set_depth(real_t p_depth);
	real_t get_depth() const;

	void set_curve_step(real_t p_step);
	real_t get_curve_step() const;

	void set_pixel_size(real_t p_amount);
	real_t get_pixel_size() const;

	void set_offset(const Point2 &p_offset);
	Point2 get_offset() const;
};

VARIANT_ENUM_CAST(RibbonTrailMesh::Shape)
