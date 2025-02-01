/**************************************************************************/
/*  surface_tool.h                                                        */
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

#include "core/templates/local_vector.h"
#include "scene/resources/mesh.h"
#include "thirdparty/misc/mikktspace.h"

class SurfaceTool : public RefCounted {
	GDCLASS(SurfaceTool, RefCounted);

	static const uint32_t custom_mask[RS::ARRAY_CUSTOM_COUNT];
	static const uint32_t custom_shift[RS::ARRAY_CUSTOM_COUNT];

public:
	struct Vertex {
		// Trivial data for which the hash is computed using hash_buffer.
		// ----------------------------------------------------------------
		uint32_t smooth_group = 0; // Must be first.

		Color color;
		Vector3 normal; // normal, binormal, tangent.
		Vector3 binormal;
		Vector3 tangent;
		Vector2 uv;
		Vector2 uv2;
		Color custom[RS::ARRAY_CUSTOM_COUNT];

		Vector3 vertex; // Must be last.
		// ----------------------------------------------------------------

		Vector<int> bones;
		Vector<float> weights;

		bool operator==(const Vertex &p_vertex) const;

		Vertex() {}
	};

	enum CustomFormat {
		CUSTOM_RGBA8_UNORM = RS::ARRAY_CUSTOM_RGBA8_UNORM,
		CUSTOM_RGBA8_SNORM = RS::ARRAY_CUSTOM_RGBA8_SNORM,
		CUSTOM_RG_HALF = RS::ARRAY_CUSTOM_RG_HALF,
		CUSTOM_RGBA_HALF = RS::ARRAY_CUSTOM_RGBA_HALF,
		CUSTOM_R_FLOAT = RS::ARRAY_CUSTOM_R_FLOAT,
		CUSTOM_RG_FLOAT = RS::ARRAY_CUSTOM_RG_FLOAT,
		CUSTOM_RGB_FLOAT = RS::ARRAY_CUSTOM_RGB_FLOAT,
		CUSTOM_RGBA_FLOAT = RS::ARRAY_CUSTOM_RGBA_FLOAT,
		CUSTOM_MAX = RS::ARRAY_CUSTOM_MAX
	};

	enum SkinWeightCount {
		SKIN_4_WEIGHTS,
		SKIN_8_WEIGHTS
	};

	enum {
		/* Do not move vertices that are located on the topological border (vertices on triangle edges that don't have a paired triangle). Useful for simplifying portions of the larger mesh. */
		SIMPLIFY_LOCK_BORDER = 1 << 0, // From meshopt_SimplifyLockBorder
		/* Improve simplification performance assuming input indices are a sparse subset of the mesh. Note that error becomes relative to subset extents. */
		SIMPLIFY_SPARSE = 1 << 1, // From meshopt_SimplifySparse
		/* Treat error limit and resulting error as absolute instead of relative to mesh extents. */
		SIMPLIFY_ERROR_ABSOLUTE = 1 << 2, // From meshopt_SimplifyErrorAbsolute
		/* Remove disconnected parts of the mesh during simplification incrementally, regardless of the topological restrictions inside components. */
		SIMPLIFY_PRUNE = 1 << 3, // From meshopt_SimplifyPrune
	};

	typedef void (*OptimizeVertexCacheFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, size_t vertex_count);
	static OptimizeVertexCacheFunc optimize_vertex_cache_func;
	typedef size_t (*OptimizeVertexFetchRemapFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, size_t vertex_count);
	static OptimizeVertexFetchRemapFunc optimize_vertex_fetch_remap_func;
	typedef size_t (*SimplifyFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, const float *vertex_positions, size_t vertex_count, size_t vertex_positions_stride, size_t target_index_count, float target_error, unsigned int options, float *r_error);
	static SimplifyFunc simplify_func;
	typedef size_t (*SimplifyWithAttribFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, const float *vertex_data, size_t vertex_count, size_t vertex_stride, const float *attributes, size_t attribute_stride, const float *attribute_weights, size_t attribute_count, const unsigned char *vertex_lock, size_t target_index_count, float target_error, unsigned int options, float *result_error);
	static SimplifyWithAttribFunc simplify_with_attrib_func;
	typedef float (*SimplifyScaleFunc)(const float *vertex_positions, size_t vertex_count, size_t vertex_positions_stride);
	static SimplifyScaleFunc simplify_scale_func;
	typedef size_t (*GenerateRemapFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, const void *vertices, size_t vertex_count, size_t vertex_size);
	static GenerateRemapFunc generate_remap_func;
	typedef void (*RemapVertexFunc)(void *destination, const void *vertices, size_t vertex_count, size_t vertex_size, const unsigned int *remap);
	static RemapVertexFunc remap_vertex_func;
	typedef void (*RemapIndexFunc)(unsigned int *destination, const unsigned int *indices, size_t index_count, const unsigned int *remap);
	static RemapIndexFunc remap_index_func;
	static void strip_mesh_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices);

private:
	struct VertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const Vertex &p_vtx);
	};

	struct SmoothGroupVertex {
		Vector3 vertex;
		uint32_t smooth_group = 0;
		bool operator==(const SmoothGroupVertex &p_vertex) const;

		SmoothGroupVertex(const Vertex &p_vertex) {
			vertex = p_vertex.vertex;
			smooth_group = p_vertex.smooth_group;
		}
	};

	struct SmoothGroupVertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const SmoothGroupVertex &p_vtx);
	};

	struct TriangleHasher {
		static _FORCE_INLINE_ uint32_t hash(const int *p_triangle);
		static _FORCE_INLINE_ bool compare(const int *p_lhs, const int *p_rhs);
	};

	struct WeightSort {
		int index = 0;
		float weight = 0.0;
		bool operator<(const WeightSort &p_right) const {
			return weight < p_right.weight;
		}
	};

	bool begun = false;
	bool first = false;
	Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_LINES;
	uint64_t format = 0;
	Ref<Material> material;
	//arrays
	LocalVector<Vertex> vertex_array;
	LocalVector<int> index_array;

	//memory
	Color last_color;
	Vector3 last_normal;
	Vector2 last_uv;
	Vector2 last_uv2;
	Vector<int> last_bones;
	Vector<float> last_weights;
	Plane last_tangent;
	uint32_t last_smooth_group = 0;

	SkinWeightCount skin_weights = SKIN_4_WEIGHTS;

	Color last_custom[RS::ARRAY_CUSTOM_COUNT];

	CustomFormat last_custom_format[RS::ARRAY_CUSTOM_COUNT];

	void _create_list_from_arrays(Array arr, LocalVector<Vertex> *r_vertex, LocalVector<int> *r_index, uint64_t &lformat);
	void _create_list(const Ref<Mesh> &p_existing, int p_surface, LocalVector<Vertex> *r_vertex, LocalVector<int> *r_index, uint64_t &lformat);

	//mikktspace callbacks
	static int mikktGetNumFaces(const SMikkTSpaceContext *pContext);
	static int mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace);
	static void mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert);
	static void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert);
	static void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert);
	static void mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
			const tbool bIsOrientationPreserving, const int iFace, const int iVert);

	void _add_triangle_fan(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uvs = Vector<Vector2>(), const Vector<Color> &p_colors = Vector<Color>(), const Vector<Vector2> &p_uv2s = Vector<Vector2>(), const Vector<Vector3> &p_normals = Vector<Vector3>(), const TypedArray<Plane> &p_tangents = TypedArray<Plane>());

protected:
	static void _bind_methods();

public:
	void set_skin_weight_count(SkinWeightCount p_weights);
	SkinWeightCount get_skin_weight_count() const;

	void set_custom_format(int p_channel_index, CustomFormat p_format);
	CustomFormat get_custom_format(int p_channel_index) const;

	Mesh::PrimitiveType get_primitive_type() const;

	void begin(Mesh::PrimitiveType p_primitive);

	void set_color(Color p_color);
	void set_normal(const Vector3 &p_normal);
	void set_tangent(const Plane &p_tangent);
	void set_uv(const Vector2 &p_uv);
	void set_uv2(const Vector2 &p_uv2);
	void set_custom(int p_channel_index, const Color &p_custom);
	void set_bones(const Vector<int> &p_bones);
	void set_weights(const Vector<float> &p_weights);
	void set_smooth_group(uint32_t p_group);

	void add_vertex(const Vector3 &p_vertex);

	void add_triangle_fan(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uvs = Vector<Vector2>(), const Vector<Color> &p_colors = Vector<Color>(), const Vector<Vector2> &p_uv2s = Vector<Vector2>(), const Vector<Vector3> &p_normals = Vector<Vector3>(), const Vector<Plane> &p_tangents = Vector<Plane>());

	void add_index(int p_index);

	void index();
	void deindex();
	void generate_normals(bool p_flip = false);
	void generate_tangents();

	void optimize_indices_for_cache();
	AABB get_aabb() const;
	Vector<int> generate_lod(float p_threshold, int p_target_index_count = 3);

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const;

	void clear();

	LocalVector<Vertex> &get_vertex_array() {
		return vertex_array;
	}

	void create_from_triangle_arrays(const Array &p_arrays);
	void create_from_arrays(const Array &p_arrays, Mesh::PrimitiveType p_primitive_type = Mesh::PRIMITIVE_TRIANGLES);
	static void create_vertex_array_from_arrays(const Array &p_arrays, LocalVector<Vertex> &ret, uint64_t *r_format = nullptr);
	Array commit_to_arrays();
	void create_from(const Ref<Mesh> &p_existing, int p_surface);
	void create_from_blend_shape(const Ref<Mesh> &p_existing, int p_surface, const String &p_blend_shape_name);
	void append_from(const Ref<Mesh> &p_existing, int p_surface, const Transform3D &p_xform);
	Ref<ArrayMesh> commit(const Ref<ArrayMesh> &p_existing = Ref<ArrayMesh>(), uint64_t p_compress_flags = 0);

	SurfaceTool();
};

VARIANT_ENUM_CAST(SurfaceTool::CustomFormat)
VARIANT_ENUM_CAST(SurfaceTool::SkinWeightCount)
