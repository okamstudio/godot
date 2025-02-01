/**************************************************************************/
/*  occluder_instance_3d.h                                                */
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

#include "scene/3d/visual_instance_3d.h"

class Occluder3D : public Resource {
	GDCLASS(Occluder3D, Resource);
	RES_BASE_EXTENSION("occ");

	RID occluder;
	PackedVector3Array vertices;
	PackedInt32Array indices;
	AABB aabb;

	mutable Ref<ArrayMesh> debug_mesh;
	mutable Vector<Vector3> debug_lines;

protected:
	void _update();
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) = 0;

	void _notification(int p_what);

public:
	PackedVector3Array get_vertices() const;
	PackedInt32Array get_indices() const;

	Vector<Vector3> get_debug_lines() const;
	Ref<ArrayMesh> get_debug_mesh() const;
	AABB get_aabb() const;

	virtual RID get_rid() const override;
	Occluder3D();
	virtual ~Occluder3D();
};

class ArrayOccluder3D : public Occluder3D {
	GDCLASS(ArrayOccluder3D, Occluder3D);

	PackedVector3Array vertices;
	PackedInt32Array indices;

protected:
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) override;
	static void _bind_methods();

public:
	void set_arrays(PackedVector3Array p_vertices, PackedInt32Array p_indices);
	void set_vertices(PackedVector3Array p_vertices);
	void set_indices(PackedInt32Array p_indices);

	ArrayOccluder3D();
	~ArrayOccluder3D();
};

class QuadOccluder3D : public Occluder3D {
	GDCLASS(QuadOccluder3D, Occluder3D);

private:
	Size2 size = Vector2(1.0f, 1.0f);

protected:
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) override;
	static void _bind_methods();

public:
	Size2 get_size() const;
	void set_size(const Size2 &p_size);

	QuadOccluder3D();
	~QuadOccluder3D();
};

class BoxOccluder3D : public Occluder3D {
	GDCLASS(BoxOccluder3D, Occluder3D);

private:
	Vector3 size = Vector3(1.0f, 1.0f, 1.0f);

protected:
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) override;
	static void _bind_methods();

public:
	Vector3 get_size() const;
	void set_size(const Vector3 &p_size);

	BoxOccluder3D();
	~BoxOccluder3D();
};

class SphereOccluder3D : public Occluder3D {
	GDCLASS(SphereOccluder3D, Occluder3D);

private:
	static constexpr int RINGS = 7;
	static constexpr int RADIAL_SEGMENTS = 7;
	float radius = 1.0f;

protected:
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) override;
	static void _bind_methods();

public:
	float get_radius() const;
	void set_radius(float p_radius);

	SphereOccluder3D();
	~SphereOccluder3D();
};

class PolygonOccluder3D : public Occluder3D {
	GDCLASS(PolygonOccluder3D, Occluder3D);

private:
	Vector<Vector2> polygon;

	bool _has_editable_3d_polygon_no_depth() const;

protected:
	virtual void _update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) override;
	static void _bind_methods();

public:
	void set_polygon(const Vector<Vector2> &p_polygon);
	Vector<Vector2> get_polygon() const;

	PolygonOccluder3D();
	~PolygonOccluder3D();
};

class OccluderInstance3D : public VisualInstance3D {
	GDCLASS(OccluderInstance3D, VisualInstance3D);

private:
	Ref<Occluder3D> occluder;
	uint32_t bake_mask = 0xFFFFFFFF;
	float bake_simplification_dist = 0.1f;

	void _occluder_changed();

	static bool _bake_material_check(Ref<Material> p_material);
	static void _bake_surface(const Transform3D &p_transform, Array p_surface_arrays, Ref<Material> p_material, float p_simplification_dist, PackedVector3Array &r_vertices, PackedInt32Array &r_indices);
	void _bake_node(Node *p_node, PackedVector3Array &r_vertices, PackedInt32Array &r_indices);

	bool _is_editable_3d_polygon() const;
	Ref<Resource> _get_editable_3d_polygon_resource() const;

protected:
	static void _bind_methods();

public:
	virtual PackedStringArray get_configuration_warnings() const override;

	enum BakeError {
		BAKE_ERROR_OK,
		BAKE_ERROR_NO_SAVE_PATH,
		BAKE_ERROR_NO_MESHES,
		BAKE_ERROR_CANT_SAVE,
	};

	void set_occluder(const Ref<Occluder3D> &p_occluder);
	Ref<Occluder3D> get_occluder() const;

	virtual AABB get_aabb() const override;

	void set_bake_mask(uint32_t p_mask);
	uint32_t get_bake_mask() const;

	void set_bake_simplification_distance(float p_dist);
	float get_bake_simplification_distance() const;

	void set_bake_mask_value(int p_layer_number, bool p_enable);
	bool get_bake_mask_value(int p_layer_number) const;

	BakeError bake_scene(Node *p_from_node, String p_occluder_path = "");
	static void bake_single_node(const Node3D *p_node, float p_simplification_distance, PackedVector3Array &r_vertices, PackedInt32Array &r_indices);

	OccluderInstance3D();
	~OccluderInstance3D();
};
