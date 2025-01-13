/**************************************************************************/
/*  mesh.h                                                                */
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

#ifndef MESH_H
#define MESH_H

#include "core/io/resource.h"
#include "core/math/face3.h"
#include "core/math/triangle_mesh.h"
#ifndef _3D_DISABLED
#include "scene/resources/3d/shape_3d.h"
#endif // _3D_DISABLED
#include "scene/resources/material.h"
#include "servers/rendering_server.h"

class ConcavePolygonShape3D;
class ConvexPolygonShape3D;
class MeshConvexDecompositionSettings;
class Shape3D;

class Mesh : public Resource {
	GDCLASS(Mesh, Resource);

	mutable Ref<TriangleMesh> triangle_mesh; //cached
	mutable Vector<Ref<TriangleMesh>> surface_triangle_meshes; //cached
	mutable Vector<Vector3> debug_lines;
	Size2i lightmap_size_hint;

	Vector<Vector3> _get_faces() const;

public:
	enum PrimitiveType {
		PRIMITIVE_POINTS = RenderingServer::PRIMITIVE_POINTS,
		PRIMITIVE_LINES = RenderingServer::PRIMITIVE_LINES,
		PRIMITIVE_LINE_STRIP = RenderingServer::PRIMITIVE_LINE_STRIP,
		PRIMITIVE_TRIANGLES = RenderingServer::PRIMITIVE_TRIANGLES,
		PRIMITIVE_TRIANGLE_STRIP = RenderingServer::PRIMITIVE_TRIANGLE_STRIP,
		PRIMITIVE_MAX = RenderingServer::PRIMITIVE_MAX,
	};

protected:
	static void _bind_methods();

	GDVIRTUAL0RC_REQUIRED(int, _get_surface_count)
	GDVIRTUAL1RC_REQUIRED(int, _surface_get_array_len, int)
	GDVIRTUAL1RC_REQUIRED(int, _surface_get_array_index_len, int)
	GDVIRTUAL1RC_REQUIRED(Array, _surface_get_arrays, int)
	GDVIRTUAL1RC_REQUIRED(TypedArray<Array>, _surface_get_blend_shape_arrays, int)
	GDVIRTUAL1RC_REQUIRED(Dictionary, _surface_get_lods, int)
	GDVIRTUAL1RC_REQUIRED(uint32_t, _surface_get_format, int)
	GDVIRTUAL1RC_REQUIRED(uint32_t, _surface_get_primitive_type, int)
	GDVIRTUAL2_REQUIRED(_surface_set_material, int, Ref<Material>)
	GDVIRTUAL1RC_REQUIRED(Ref<Material>, _surface_get_material, int)
	GDVIRTUAL0RC_REQUIRED(int, _get_blend_shape_count)
	GDVIRTUAL1RC_REQUIRED(StringName, _get_blend_shape_name, int)
	GDVIRTUAL2_REQUIRED(_set_blend_shape_name, int, StringName)
	GDVIRTUAL0RC_REQUIRED(AABB, _get_aabb)

public:
	enum {
		NO_INDEX_ARRAY = RenderingServer::NO_INDEX_ARRAY,
		ARRAY_WEIGHTS_SIZE = RenderingServer::ARRAY_WEIGHTS_SIZE
	};
	enum BlendShapeMode {
		BLEND_SHAPE_MODE_NORMALIZED = RS::BLEND_SHAPE_MODE_NORMALIZED,
		BLEND_SHAPE_MODE_RELATIVE = RS::BLEND_SHAPE_MODE_RELATIVE,
	};
	enum ArrayType {
		ARRAY_VERTEX = RenderingServer::ARRAY_VERTEX,
		ARRAY_NORMAL = RenderingServer::ARRAY_NORMAL,
		ARRAY_TANGENT = RenderingServer::ARRAY_TANGENT,
		ARRAY_COLOR = RenderingServer::ARRAY_COLOR,
		ARRAY_TEX_UV = RenderingServer::ARRAY_TEX_UV,
		ARRAY_TEX_UV2 = RenderingServer::ARRAY_TEX_UV2,
		ARRAY_CUSTOM0 = RenderingServer::ARRAY_CUSTOM0,
		ARRAY_CUSTOM1 = RenderingServer::ARRAY_CUSTOM1,
		ARRAY_CUSTOM2 = RenderingServer::ARRAY_CUSTOM2,
		ARRAY_CUSTOM3 = RenderingServer::ARRAY_CUSTOM3,
		ARRAY_BONES = RenderingServer::ARRAY_BONES,
		ARRAY_WEIGHTS = RenderingServer::ARRAY_WEIGHTS,
		ARRAY_INDEX = RenderingServer::ARRAY_INDEX,
		ARRAY_MAX = RenderingServer::ARRAY_MAX

	};

	enum ArrayCustomFormat {
		ARRAY_CUSTOM_RGBA8_UNORM,
		ARRAY_CUSTOM_RGBA8_SNORM,
		ARRAY_CUSTOM_RG_HALF,
		ARRAY_CUSTOM_RGBA_HALF,
		ARRAY_CUSTOM_R_FLOAT,
		ARRAY_CUSTOM_RG_FLOAT,
		ARRAY_CUSTOM_RGB_FLOAT,
		ARRAY_CUSTOM_RGBA_FLOAT,
		ARRAY_CUSTOM_MAX
	};

	enum ArrayFormat : uint64_t {
		ARRAY_FORMAT_VERTEX = RS::ARRAY_FORMAT_VERTEX,
		ARRAY_FORMAT_NORMAL = RS::ARRAY_FORMAT_NORMAL,
		ARRAY_FORMAT_TANGENT = RS::ARRAY_FORMAT_TANGENT,
		ARRAY_FORMAT_COLOR = RS::ARRAY_FORMAT_COLOR,
		ARRAY_FORMAT_TEX_UV = RS::ARRAY_FORMAT_TEX_UV,
		ARRAY_FORMAT_TEX_UV2 = RS::ARRAY_FORMAT_TEX_UV2,
		ARRAY_FORMAT_CUSTOM0 = RS::ARRAY_FORMAT_CUSTOM0,
		ARRAY_FORMAT_CUSTOM1 = RS::ARRAY_FORMAT_CUSTOM1,
		ARRAY_FORMAT_CUSTOM2 = RS::ARRAY_FORMAT_CUSTOM2,
		ARRAY_FORMAT_CUSTOM3 = RS::ARRAY_FORMAT_CUSTOM3,
		ARRAY_FORMAT_BONES = RS::ARRAY_FORMAT_BONES,
		ARRAY_FORMAT_WEIGHTS = RS::ARRAY_FORMAT_WEIGHTS,
		ARRAY_FORMAT_INDEX = RS::ARRAY_FORMAT_INDEX,

		ARRAY_FORMAT_BLEND_SHAPE_MASK = RS::ARRAY_FORMAT_BLEND_SHAPE_MASK,

		ARRAY_FORMAT_CUSTOM_BASE = RS::ARRAY_FORMAT_CUSTOM_BASE,
		ARRAY_FORMAT_CUSTOM_BITS = RS::ARRAY_FORMAT_CUSTOM_BITS,
		ARRAY_FORMAT_CUSTOM0_SHIFT = RS::ARRAY_FORMAT_CUSTOM0_SHIFT,
		ARRAY_FORMAT_CUSTOM1_SHIFT = RS::ARRAY_FORMAT_CUSTOM1_SHIFT,
		ARRAY_FORMAT_CUSTOM2_SHIFT = RS::ARRAY_FORMAT_CUSTOM2_SHIFT,
		ARRAY_FORMAT_CUSTOM3_SHIFT = RS::ARRAY_FORMAT_CUSTOM3_SHIFT,

		ARRAY_FORMAT_CUSTOM_MASK = RS::ARRAY_FORMAT_CUSTOM_MASK,
		ARRAY_COMPRESS_FLAGS_BASE = RS::ARRAY_COMPRESS_FLAGS_BASE,

		ARRAY_FLAG_USE_2D_VERTICES = RS::ARRAY_FLAG_USE_2D_VERTICES,
		ARRAY_FLAG_USE_DYNAMIC_UPDATE = RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE,
		ARRAY_FLAG_USE_8_BONE_WEIGHTS = RS::ARRAY_FLAG_USE_8_BONE_WEIGHTS,

		ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY = RS::ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY,
		ARRAY_FLAG_COMPRESS_ATTRIBUTES = RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES,

		ARRAY_FLAG_FORMAT_VERSION_BASE = RS::ARRAY_FLAG_FORMAT_VERSION_BASE,
		ARRAY_FLAG_FORMAT_VERSION_SHIFT = RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT,
		ARRAY_FLAG_FORMAT_VERSION_1 = RS::ARRAY_FLAG_FORMAT_VERSION_1,
		ARRAY_FLAG_FORMAT_VERSION_2 = (uint64_t)RS::ARRAY_FLAG_FORMAT_VERSION_2,
		ARRAY_FLAG_FORMAT_CURRENT_VERSION = (uint64_t)RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION,
		ARRAY_FLAG_FORMAT_VERSION_MASK = RS::ARRAY_FLAG_FORMAT_VERSION_MASK,
	};

	virtual int get_surface_count() const;
	virtual int surface_get_array_len(int p_idx) const;
	virtual int surface_get_array_index_len(int p_idx) const;
	virtual Array surface_get_arrays(int p_surface) const;
	virtual TypedArray<Array> surface_get_blend_shape_arrays(int p_surface) const;
	virtual Dictionary surface_get_lods(int p_surface) const;
	virtual BitField<ArrayFormat> surface_get_format(int p_idx) const;
	virtual PrimitiveType surface_get_primitive_type(int p_idx) const;
	virtual void surface_set_material(int p_idx, const Ref<Material> &p_material);
	virtual Ref<Material> surface_get_material(int p_idx) const;
	virtual int get_blend_shape_count() const;
	virtual StringName get_blend_shape_name(int p_index) const;
	virtual void set_blend_shape_name(int p_index, const StringName &p_name);
	virtual AABB get_aabb() const;

	Vector<Face3> get_faces() const;
	Vector<Face3> get_surface_faces(int p_surface) const;
	Ref<TriangleMesh> generate_triangle_mesh() const;
	Ref<TriangleMesh> generate_surface_triangle_mesh(int p_surface) const;
	void generate_debug_mesh_lines(Vector<Vector3> &r_lines);
	void generate_debug_mesh_indices(Vector<Vector3> &r_points);

	Ref<Mesh> create_outline(float p_margin) const;

	void set_lightmap_size_hint(const Size2i &p_size);
	Size2i get_lightmap_size_hint() const;
	void clear_cache() const;

	typedef Vector<Vector<Vector3>> (*ConvexDecompositionFunc)(const real_t *p_vertices, int p_vertex_count, const uint32_t *p_triangles, int p_triangle_count, const Ref<MeshConvexDecompositionSettings> &p_settings, Vector<Vector<uint32_t>> *r_convex_indices);

	static ConvexDecompositionFunc convex_decomposition_function;

#ifndef _3D_DISABLED
	Vector<Ref<Shape3D>> convex_decompose(const Ref<MeshConvexDecompositionSettings> &p_settings) const;
	Ref<ConvexPolygonShape3D> create_convex_shape(bool p_clean = true, bool p_simplify = false) const;
	Ref<ConcavePolygonShape3D> create_trimesh_shape() const;
#endif // _3D_DISABLED

	virtual int get_builtin_bind_pose_count() const;
	virtual Transform3D get_builtin_bind_pose(int p_index) const;

	virtual Ref<Resource> create_placeholder() const;

	Mesh();
};

class MeshConvexDecompositionSettings : public RefCounted {
	GDCLASS(MeshConvexDecompositionSettings, RefCounted);

public:
	enum Mode : int {
		CONVEX_DECOMPOSITION_MODE_VOXEL = 0,
		CONVEX_DECOMPOSITION_MODE_TETRAHEDRON = 1
	};

private:
	Mode mode = CONVEX_DECOMPOSITION_MODE_VOXEL;

	/// Maximum concavity. [Range: 0.0 -> 1.0]
	real_t max_concavity = 1.0;
	/// Controls the bias toward clipping along symmetry planes. [Range: 0.0 -> 1.0]
	real_t symmetry_planes_clipping_bias = 0.05;
	/// Controls the bias toward clipping along revolution axes. [Range: 0.0 -> 1.0]
	real_t revolution_axes_clipping_bias = 0.05;
	real_t min_volume_per_convex_hull = 0.0001;
	/// Maximum number of voxels generated during the voxelization stage.
	uint32_t resolution = 10'000;
	uint32_t max_num_vertices_per_convex_hull = 32;
	/// Controls the granularity of the search for the "best" clipping plane.
	/// [Range: 1 -> 16]
	uint32_t plane_downsampling = 4;
	/// Controls the precision of the convex-hull generation process during the
	/// clipping plane selection stage.
	/// [Range: 1 -> 16]
	uint32_t convex_hull_downsampling = 4;
	/// enable/disable normalizing the mesh before applying the convex decomposition.
	bool normalize_mesh = false;

	bool convex_hull_approximation = true;
	/// This is the maximum number of convex hulls to produce from the merge operation.
	uint32_t max_convex_hulls = 1;
	bool project_hull_vertices = true;

protected:
	static void _bind_methods();

public:
	void set_max_concavity(real_t p_max_concavity);
	real_t get_max_concavity() const;

	void set_symmetry_planes_clipping_bias(real_t p_symmetry_planes_clipping_bias);
	real_t get_symmetry_planes_clipping_bias() const;

	void set_revolution_axes_clipping_bias(real_t p_revolution_axes_clipping_bias);
	real_t get_revolution_axes_clipping_bias() const;

	void set_min_volume_per_convex_hull(real_t p_min_volume_per_convex_hull);
	real_t get_min_volume_per_convex_hull() const;

	void set_resolution(uint32_t p_resolution);
	uint32_t get_resolution() const;

	void set_max_num_vertices_per_convex_hull(uint32_t p_max_num_vertices_per_convex_hull);
	uint32_t get_max_num_vertices_per_convex_hull() const;

	void set_plane_downsampling(uint32_t p_plane_downsampling);
	uint32_t get_plane_downsampling() const;

	void set_convex_hull_downsampling(uint32_t p_convex_hull_downsampling);
	uint32_t get_convex_hull_downsampling() const;

	void set_normalize_mesh(bool p_normalize_mesh);
	bool get_normalize_mesh() const;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	void set_convex_hull_approximation(bool p_convex_hull_approximation);
	bool get_convex_hull_approximation() const;

	void set_max_convex_hulls(uint32_t p_max_convex_hulls);
	uint32_t get_max_convex_hulls() const;

	void set_project_hull_vertices(bool p_project_hull_vertices);
	bool get_project_hull_vertices() const;
};

VARIANT_ENUM_CAST(MeshConvexDecompositionSettings::Mode);

class ArrayMesh : public Mesh {
	GDCLASS(ArrayMesh, Mesh);
	RES_BASE_EXTENSION("mesh");

	PackedStringArray _get_blend_shape_names() const;
	void _set_blend_shape_names(const PackedStringArray &p_names);

	Array _get_surfaces() const;
	void _set_surfaces(const Array &p_data);
	Ref<ArrayMesh> shadow_mesh;

private:
	struct Surface {
		uint64_t format = 0;
		int array_length = 0;
		int index_array_length = 0;
		PrimitiveType primitive = PrimitiveType::PRIMITIVE_MAX;

		String name;
		AABB aabb;
		Ref<Material> material;
		bool is_2d = false;
	};
	Vector<Surface> surfaces;
	mutable RID mesh;
	AABB aabb;
	BlendShapeMode blend_shape_mode = BLEND_SHAPE_MODE_RELATIVE;
	Vector<StringName> blend_shapes;
	AABB custom_aabb;

	_FORCE_INLINE_ void _create_if_empty() const;
	void _recompute_aabb();

protected:
	virtual bool _is_generated() const { return false; }

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool surface_index_0 = false;

	virtual void reset_state() override;

	static void _bind_methods();

public:
	void add_surface_from_arrays(PrimitiveType p_primitive, const Array &p_arrays, const TypedArray<Array> &p_blend_shapes = TypedArray<Array>(), const Dictionary &p_lods = Dictionary(), BitField<ArrayFormat> p_flags = 0);

	void add_surface(BitField<ArrayFormat> p_format, PrimitiveType p_primitive, const Vector<uint8_t> &p_array, const Vector<uint8_t> &p_attribute_array, const Vector<uint8_t> &p_skin_array, int p_vertex_count, const Vector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<uint8_t> &p_blend_shape_data = Vector<uint8_t>(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>(), const Vector<RS::SurfaceData::LOD> &p_lods = Vector<RS::SurfaceData::LOD>(), const Vector4 p_uv_scale = Vector4());

	Array surface_get_arrays(int p_surface) const override;
	TypedArray<Array> surface_get_blend_shape_arrays(int p_surface) const override;
	Dictionary surface_get_lods(int p_surface) const override;

	void add_blend_shape(const StringName &p_name);
	int get_blend_shape_count() const override;
	StringName get_blend_shape_name(int p_index) const override;
	void set_blend_shape_name(int p_index, const StringName &p_name) override;
	void clear_blend_shapes();

	void set_blend_shape_mode(BlendShapeMode p_mode);
	BlendShapeMode get_blend_shape_mode() const;

	void surface_update_vertex_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data);
	void surface_update_attribute_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data);
	void surface_update_skin_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data);

	int get_surface_count() const override;

	void surface_remove(int p_surface);
	void clear_surfaces();

	void surface_set_custom_aabb(int p_idx, const AABB &p_aabb); //only recognized by driver

	int surface_get_array_len(int p_idx) const override;
	int surface_get_array_index_len(int p_idx) const override;
	BitField<ArrayFormat> surface_get_format(int p_idx) const override;
	PrimitiveType surface_get_primitive_type(int p_idx) const override;

	virtual void surface_set_material(int p_idx, const Ref<Material> &p_material) override;
	virtual Ref<Material> surface_get_material(int p_idx) const override;

	int surface_find_by_name(const String &p_name) const;
	void surface_set_name(int p_idx, const String &p_name);
	String surface_get_name(int p_idx) const;

	void set_custom_aabb(const AABB &p_custom);
	AABB get_custom_aabb() const;

	AABB get_aabb() const override;
	virtual RID get_rid() const override;

	void regen_normal_maps();

	Error lightmap_unwrap(const Transform3D &p_base_transform = Transform3D(), float p_texel_size = 0.05);
	Error lightmap_unwrap_cached(const Transform3D &p_base_transform, float p_texel_size, const Vector<uint8_t> &p_src_cache, Vector<uint8_t> &r_dst_cache, bool p_generate_cache = true);

	virtual void reload_from_file() override;

	void set_shadow_mesh(const Ref<ArrayMesh> &p_mesh);
	Ref<ArrayMesh> get_shadow_mesh() const;

	ArrayMesh();

	~ArrayMesh();
};

VARIANT_ENUM_CAST(Mesh::ArrayType);
VARIANT_BITFIELD_CAST(Mesh::ArrayFormat);
VARIANT_ENUM_CAST(Mesh::ArrayCustomFormat);
VARIANT_ENUM_CAST(Mesh::PrimitiveType);
VARIANT_ENUM_CAST(Mesh::BlendShapeMode);

class PlaceholderMesh : public Mesh {
	GDCLASS(PlaceholderMesh, Mesh);

	RID rid;
	AABB aabb;

protected:
	static void _bind_methods();

public:
	virtual int get_surface_count() const override { return 0; }
	virtual int surface_get_array_len(int p_idx) const override { return 0; }
	virtual int surface_get_array_index_len(int p_idx) const override { return 0; }
	virtual Array surface_get_arrays(int p_surface) const override { return Array(); }
	virtual TypedArray<Array> surface_get_blend_shape_arrays(int p_surface) const override { return TypedArray<Array>(); }
	virtual Dictionary surface_get_lods(int p_surface) const override { return Dictionary(); }
	virtual BitField<ArrayFormat> surface_get_format(int p_idx) const override { return 0; }
	virtual PrimitiveType surface_get_primitive_type(int p_idx) const override { return PRIMITIVE_TRIANGLES; }
	virtual void surface_set_material(int p_idx, const Ref<Material> &p_material) override {}
	virtual Ref<Material> surface_get_material(int p_idx) const override { return Ref<Material>(); }
	virtual int get_blend_shape_count() const override { return 0; }
	virtual StringName get_blend_shape_name(int p_index) const override { return StringName(); }
	virtual void set_blend_shape_name(int p_index, const StringName &p_name) override {}
	virtual RID get_rid() const override { return rid; }
	virtual AABB get_aabb() const override { return aabb; }
	void set_aabb(const AABB &p_aabb) { aabb = p_aabb; }

	virtual int get_builtin_bind_pose_count() const override { return 0; }
	virtual Transform3D get_builtin_bind_pose(int p_index) const override { return Transform3D(); }

	PlaceholderMesh();
	~PlaceholderMesh();
};

#endif // MESH_H
