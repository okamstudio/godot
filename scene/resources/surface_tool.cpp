/**************************************************************************/
/*  surface_tool.cpp                                                      */
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

#include "surface_tool.h"

#include "core/templates/a_hash_map.h"

#define EQ_VERTEX_DIST 0.00001

SurfaceTool::OptimizeVertexCacheFunc SurfaceTool::optimize_vertex_cache_func = nullptr;
SurfaceTool::OptimizeVertexFetchRemapFunc SurfaceTool::optimize_vertex_fetch_remap_func = nullptr;
SurfaceTool::SimplifyFunc SurfaceTool::simplify_func = nullptr;
SurfaceTool::SimplifyWithAttribFunc SurfaceTool::simplify_with_attrib_func = nullptr;
SurfaceTool::SimplifyScaleFunc SurfaceTool::simplify_scale_func = nullptr;
SurfaceTool::GenerateRemapFunc SurfaceTool::generate_remap_func = nullptr;
SurfaceTool::RemapVertexFunc SurfaceTool::remap_vertex_func = nullptr;
SurfaceTool::RemapIndexFunc SurfaceTool::remap_index_func = nullptr;

void SurfaceTool::strip_mesh_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	ERR_FAIL_COND_MSG(!generate_remap_func || !remap_vertex_func || !remap_index_func, "Meshoptimizer library is not initialized.");

	Vector<uint32_t> remap;
	remap.resize(r_vertices.size());
	uint32_t new_vertex_count = generate_remap_func(remap.ptrw(), (unsigned int *)r_indices.ptr(), r_indices.size(), r_vertices.ptr(), r_vertices.size(), sizeof(Vector3));
	remap_vertex_func(r_vertices.ptrw(), r_vertices.ptr(), r_vertices.size(), sizeof(Vector3), remap.ptr());
	r_vertices.resize(new_vertex_count);
	remap_index_func((unsigned int *)r_indices.ptrw(), (unsigned int *)r_indices.ptr(), r_indices.size(), remap.ptr());

	AHashMap<const int *, bool, TriangleHasher, TriangleHasher> found_triangles;
	int *idx_ptr = r_indices.ptrw();

	int filtered_indices_count = 0;
	for (int i = 0; i < r_indices.size() / 3; i++) {
		const int *tri = idx_ptr + (i * 3);

		if (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0]) {
			continue;
		}

		if (found_triangles.has(tri)) {
			continue;
		}

		if (i != filtered_indices_count) {
			memcpy(idx_ptr + (filtered_indices_count * 3), tri, sizeof(int) * 3);
		}

		found_triangles.insert_new(tri, true);
		filtered_indices_count++;
	}
	r_indices.resize(filtered_indices_count * 3);
}

bool SurfaceTool::Vertex::operator==(const Vertex &p_vertex) const {
	if (vertex != p_vertex.vertex) {
		return false;
	}

	if (uv != p_vertex.uv) {
		return false;
	}

	if (uv2 != p_vertex.uv2) {
		return false;
	}

	if (normal != p_vertex.normal) {
		return false;
	}

	if (binormal != p_vertex.binormal) {
		return false;
	}

	if (tangent != p_vertex.tangent) {
		return false;
	}

	if (color != p_vertex.color) {
		return false;
	}

	if (bones.size() != p_vertex.bones.size()) {
		return false;
	}

	for (int i = 0; i < bones.size(); i++) {
		if (bones[i] != p_vertex.bones[i]) {
			return false;
		}
	}

	for (int i = 0; i < weights.size(); i++) {
		if (weights[i] != p_vertex.weights[i]) {
			return false;
		}
	}

	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		if (custom[i] != p_vertex.custom[i]) {
			return false;
		}
	}

	if (smooth_group != p_vertex.smooth_group) {
		return false;
	}

	return true;
}

uint32_t SurfaceTool::VertexHasher::hash(const Vertex &p_vtx) {
	uint32_t h = hash_djb2_buffer((const uint8_t *)p_vtx.bones.ptr(), p_vtx.bones.size() * sizeof(int));
	h = hash_djb2_buffer((const uint8_t *)p_vtx.weights.ptr(), p_vtx.weights.size() * sizeof(float), h);

	const int64_t length = (int64_t)&p_vtx.vertex - (int64_t)&p_vtx.smooth_group + sizeof(p_vtx.vertex);
	const void *key = &p_vtx.smooth_group;
	h = hash_murmur3_buffer(key, length, h);
	return h;
}

bool SurfaceTool::SmoothGroupVertex::operator==(const SmoothGroupVertex &p_vertex) const {
	if (vertex != p_vertex.vertex) {
		return false;
	}

	if (smooth_group != p_vertex.smooth_group) {
		return false;
	}

	return true;
}

uint32_t SurfaceTool::SmoothGroupVertexHasher::hash(const SmoothGroupVertex &p_vtx) {
	uint32_t h = HashMapHasherDefault::hash(p_vtx.vertex);
	h = hash_murmur3_one_32(p_vtx.smooth_group, h);
	h = hash_fmix32(h);
	return h;
}

uint32_t SurfaceTool::TriangleHasher::hash(const int *p_triangle) {
	int t0 = p_triangle[0];
	int t1 = p_triangle[1];
	int t2 = p_triangle[2];

	if (t0 > t1) {
		SWAP(t0, t1);
	}
	if (t1 > t2) {
		SWAP(t1, t2);
	}
	if (t0 > t1) {
		SWAP(t0, t1);
	}

	return (t0 * 73856093) ^ (t1 * 19349663) ^ (t2 * 83492791);
}

bool SurfaceTool::TriangleHasher::compare(const int *p_lhs, const int *p_rhs) {
	int r0 = p_rhs[0];
	int r1 = p_rhs[1];
	int r2 = p_rhs[2];

	if (r0 > r1) {
		SWAP(r0, r1);
	}
	if (r1 > r2) {
		SWAP(r1, r2);
	}
	if (r0 > r1) {
		SWAP(r0, r1);
	}

	int l0 = p_lhs[0];
	int l1 = p_lhs[1];
	int l2 = p_lhs[2];

	if (l0 > l1) {
		SWAP(l0, l1);
	}
	if (l1 > l2) {
		SWAP(l1, l2);
	}
	if (l0 > l1) {
		SWAP(l0, l1);
	}

	return l0 == r0 && l1 == r1 && l2 == r2;
}

void SurfaceTool::begin(Mesh::PrimitiveType p_primitive) {
	clear();

	primitive = p_primitive;
	begun = true;
	first = true;
}

void SurfaceTool::add_vertex(const Vector3 &p_vertex) {
	ERR_FAIL_COND(!begun);

	Vertex vtx;
	vtx.vertex = p_vertex;
	vtx.color = last_color;
	vtx.normal = last_normal;
	vtx.uv = last_uv;
	vtx.uv2 = last_uv2;
	vtx.weights = last_weights;
	vtx.bones = last_bones;
	vtx.tangent = last_tangent.normal;
	vtx.binormal = last_normal.cross(last_tangent.normal).normalized() * last_tangent.d;
	vtx.smooth_group = last_smooth_group;

	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		vtx.custom[i] = last_custom[i];
	}

	const int expected_vertices = skin_weights == SKIN_8_WEIGHTS ? 8 : 4;

	if ((format & Mesh::ARRAY_FORMAT_WEIGHTS || format & Mesh::ARRAY_FORMAT_BONES) && (vtx.weights.size() != expected_vertices || vtx.bones.size() != expected_vertices)) {
		//ensure vertices are the expected amount
		ERR_FAIL_COND(vtx.weights.size() != vtx.bones.size());
		if (vtx.weights.size() < expected_vertices) {
			//less than required, fill
			for (int i = vtx.weights.size(); i < expected_vertices; i++) {
				vtx.weights.push_back(0);
				vtx.bones.push_back(0);
			}
		} else if (vtx.weights.size() > expected_vertices) {
			//more than required, sort, cap and normalize.
			Vector<WeightSort> weights;
			for (int i = 0; i < vtx.weights.size(); i++) {
				WeightSort ws;
				ws.index = vtx.bones[i];
				ws.weight = vtx.weights[i];
				weights.push_back(ws);
			}

			//sort
			weights.sort();
			//cap
			weights.resize(expected_vertices);
			//renormalize
			float total = 0.0;
			for (int i = 0; i < expected_vertices; i++) {
				total += weights[i].weight;
			}

			vtx.weights.resize(expected_vertices);
			vtx.bones.resize(expected_vertices);

			for (int i = 0; i < expected_vertices; i++) {
				if (total > 0) {
					vtx.weights.write[i] = weights[i].weight / total;
				} else {
					vtx.weights.write[i] = 0;
				}
				vtx.bones.write[i] = weights[i].index;
			}
		}
	}

	vertex_array.push_back(vtx);
	first = false;

	format |= Mesh::ARRAY_FORMAT_VERTEX;
}

void SurfaceTool::set_color(Color p_color) {
	ERR_FAIL_COND(!begun);

	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_COLOR));

	format |= Mesh::ARRAY_FORMAT_COLOR;
	last_color = p_color;
}

void SurfaceTool::set_normal(const Vector3 &p_normal) {
	ERR_FAIL_COND(!begun);

	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_NORMAL));

	format |= Mesh::ARRAY_FORMAT_NORMAL;
	last_normal = p_normal;
}

void SurfaceTool::set_tangent(const Plane &p_tangent) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_TANGENT));

	format |= Mesh::ARRAY_FORMAT_TANGENT;
	last_tangent = p_tangent;
}

void SurfaceTool::set_uv(const Vector2 &p_uv) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_TEX_UV));

	format |= Mesh::ARRAY_FORMAT_TEX_UV;
	last_uv = p_uv;
}

void SurfaceTool::set_uv2(const Vector2 &p_uv2) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_TEX_UV2));

	format |= Mesh::ARRAY_FORMAT_TEX_UV2;
	last_uv2 = p_uv2;
}

void SurfaceTool::set_custom(int p_channel_index, const Color &p_custom) {
	ERR_FAIL_INDEX(p_channel_index, RS::ARRAY_CUSTOM_COUNT);
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(last_custom_format[p_channel_index] == CUSTOM_MAX);
	static const uint32_t mask[RS::ARRAY_CUSTOM_COUNT] = { Mesh::ARRAY_FORMAT_CUSTOM0, Mesh::ARRAY_FORMAT_CUSTOM1, Mesh::ARRAY_FORMAT_CUSTOM2, Mesh::ARRAY_FORMAT_CUSTOM3 };
	ERR_FAIL_COND(!first && !(format & mask[p_channel_index]));

	if (first) {
		format |= mask[p_channel_index];
	}
	last_custom[p_channel_index] = p_custom;
}

void SurfaceTool::set_bones(const Vector<int> &p_bones) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_BONES));

	format |= Mesh::ARRAY_FORMAT_BONES;
	if (skin_weights == SKIN_8_WEIGHTS) {
		format |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
	}
	last_bones = p_bones;
}

void SurfaceTool::set_weights(const Vector<float> &p_weights) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(!first && !(format & Mesh::ARRAY_FORMAT_WEIGHTS));

	format |= Mesh::ARRAY_FORMAT_WEIGHTS;
	if (skin_weights == SKIN_8_WEIGHTS) {
		format |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
	}
	last_weights = p_weights;
}

void SurfaceTool::set_smooth_group(uint32_t p_group) {
	last_smooth_group = p_group;
}

void SurfaceTool::_add_triangle_fan(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uvs, const Vector<Color> &p_colors, const Vector<Vector2> &p_uv2s, const Vector<Vector3> &p_normals, const TypedArray<Plane> &p_tangents) {
	add_triangle_fan(p_vertices, p_uvs, p_colors, p_uv2s, p_normals, Variant(p_tangents));
}

void SurfaceTool::add_triangle_fan(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uvs, const Vector<Color> &p_colors, const Vector<Vector2> &p_uv2s, const Vector<Vector3> &p_normals, const Vector<Plane> &p_tangents) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(primitive != Mesh::PRIMITIVE_TRIANGLES);
	ERR_FAIL_COND(p_vertices.size() < 3);

#define ADD_POINT(n)                    \
	{                                   \
		if (p_colors.size() > n)        \
			set_color(p_colors[n]);     \
		if (p_uvs.size() > n)           \
			set_uv(p_uvs[n]);           \
		if (p_uv2s.size() > n)          \
			set_uv2(p_uv2s[n]);         \
		if (p_normals.size() > n)       \
			set_normal(p_normals[n]);   \
		if (p_tangents.size() > n)      \
			set_tangent(p_tangents[n]); \
		add_vertex(p_vertices[n]);      \
	}

	for (int i = 0; i < p_vertices.size() - 2; i++) {
		ADD_POINT(0);
		ADD_POINT(i + 1);
		ADD_POINT(i + 2);
	}

#undef ADD_POINT
}

void SurfaceTool::add_index(int p_index) {
	ERR_FAIL_COND(!begun);
	ERR_FAIL_COND(p_index < 0);

	format |= Mesh::ARRAY_FORMAT_INDEX;
	index_array.push_back(p_index);
}

Array SurfaceTool::commit_to_arrays() {
	int varr_len = vertex_array.size();

	Array a;
	a.resize(Mesh::ARRAY_MAX);

	for (int i = 0; i < Mesh::ARRAY_MAX; i++) {
		if (!(format & (1ULL << i))) {
			continue; //not in format
		}

		switch (i) {
			case Mesh::ARRAY_VERTEX:
			case Mesh::ARRAY_NORMAL: {
				Vector<Vector3> array;
				array.resize(varr_len);
				Vector3 *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					switch (i) {
						case Mesh::ARRAY_VERTEX: {
							w[idx] = v.vertex;
						} break;
						case Mesh::ARRAY_NORMAL: {
							w[idx] = v.normal;
						} break;
					}
				}

				a[i] = array;

			} break;

			case Mesh::ARRAY_TEX_UV:
			case Mesh::ARRAY_TEX_UV2: {
				Vector<Vector2> array;
				array.resize(varr_len);
				Vector2 *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					switch (i) {
						case Mesh::ARRAY_TEX_UV: {
							w[idx] = v.uv;
						} break;
						case Mesh::ARRAY_TEX_UV2: {
							w[idx] = v.uv2;
						} break;
					}
				}

				a[i] = array;
			} break;
			case Mesh::ARRAY_TANGENT: {
				Vector<float> array;
				array.resize(varr_len * 4);
				float *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					w[idx * 4 + 0] = v.tangent.x;
					w[idx * 4 + 1] = v.tangent.y;
					w[idx * 4 + 2] = v.tangent.z;

					//float d = v.tangent.dot(v.binormal,v.normal);
					float d = v.binormal.dot(v.normal.cross(v.tangent));
					w[idx * 4 + 3] = d < 0 ? -1 : 1;
				}

				a[i] = array;

			} break;
			case Mesh::ARRAY_COLOR: {
				Vector<Color> array;
				array.resize(varr_len);
				Color *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					w[idx] = v.color;
				}

				a[i] = array;
			} break;
			case Mesh::ARRAY_CUSTOM0:
			case Mesh::ARRAY_CUSTOM1:
			case Mesh::ARRAY_CUSTOM2:
			case Mesh::ARRAY_CUSTOM3: {
				int fmt = i - Mesh::ARRAY_CUSTOM0;
				switch (last_custom_format[fmt]) {
					case CUSTOM_RGBA8_UNORM: {
						Vector<uint8_t> array;
						array.resize(varr_len * 4);
						uint8_t *w = array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 4 + 0] = CLAMP(int32_t(c.r * 255.0), 0, 255);
							w[idx * 4 + 1] = CLAMP(int32_t(c.g * 255.0), 0, 255);
							w[idx * 4 + 2] = CLAMP(int32_t(c.b * 255.0), 0, 255);
							w[idx * 4 + 3] = CLAMP(int32_t(c.a * 255.0), 0, 255);
						}

						a[i] = array;
					} break;
					case CUSTOM_RGBA8_SNORM: {
						Vector<uint8_t> array;
						array.resize(varr_len * 4);
						uint8_t *w = array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 4 + 0] = uint8_t(int8_t(CLAMP(int32_t(c.r * 127.0), -128, 127)));
							w[idx * 4 + 1] = uint8_t(int8_t(CLAMP(int32_t(c.g * 127.0), -128, 127)));
							w[idx * 4 + 2] = uint8_t(int8_t(CLAMP(int32_t(c.b * 127.0), -128, 127)));
							w[idx * 4 + 3] = uint8_t(int8_t(CLAMP(int32_t(c.a * 127.0), -128, 127)));
						}

						a[i] = array;
					} break;
					case CUSTOM_RG_HALF: {
						Vector<uint8_t> array;
						array.resize(varr_len * 4);
						uint16_t *w = (uint16_t *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 2 + 0] = Math::make_half_float(c.r);
							w[idx * 2 + 1] = Math::make_half_float(c.g);
						}

						a[i] = array;
					} break;
					case CUSTOM_RGBA_HALF: {
						Vector<uint8_t> array;
						array.resize(varr_len * 8);
						uint16_t *w = (uint16_t *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 4 + 0] = Math::make_half_float(c.r);
							w[idx * 4 + 1] = Math::make_half_float(c.g);
							w[idx * 4 + 2] = Math::make_half_float(c.b);
							w[idx * 4 + 3] = Math::make_half_float(c.a);
						}

						a[i] = array;
					} break;
					case CUSTOM_R_FLOAT: {
						Vector<float> array;
						array.resize(varr_len);
						float *w = (float *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx] = c.r;
						}

						a[i] = array;
					} break;
					case CUSTOM_RG_FLOAT: {
						Vector<float> array;
						array.resize(varr_len * 2);
						float *w = (float *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 2 + 0] = c.r;
							w[idx * 2 + 1] = c.g;
						}

						a[i] = array;
					} break;
					case CUSTOM_RGB_FLOAT: {
						Vector<float> array;
						array.resize(varr_len * 3);
						float *w = (float *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 3 + 0] = c.r;
							w[idx * 3 + 1] = c.g;
							w[idx * 3 + 2] = c.b;
						}

						a[i] = array;
					} break;
					case CUSTOM_RGBA_FLOAT: {
						Vector<float> array;
						array.resize(varr_len * 4);
						float *w = (float *)array.ptrw();

						for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
							const Vertex &v = vertex_array[idx];

							const Color &c = v.custom[fmt];
							w[idx * 4 + 0] = c.r;
							w[idx * 4 + 1] = c.g;
							w[idx * 4 + 2] = c.b;
							w[idx * 4 + 3] = c.a;
						}

						a[i] = array;
					} break;
					default: {
					} //unreachable but compiler warning anyway
				}
			} break;
			case Mesh::ARRAY_BONES: {
				int count = skin_weights == SKIN_8_WEIGHTS ? 8 : 4;
				Vector<int> array;
				array.resize(varr_len * count);
				array.fill(0);
				int *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					if (v.bones.size() != count) {
						ERR_PRINT_ONCE(vformat("Invalid bones size %d vs count %d", v.bones.size(), count));
						continue;
					}

					for (int j = 0; j < count; j++) {
						w[idx * count + j] = v.bones[j];
					}
				}

				a[i] = array;

			} break;
			case Mesh::ARRAY_WEIGHTS: {
				Vector<float> array;
				int count = skin_weights == SKIN_8_WEIGHTS ? 8 : 4;

				array.resize(varr_len * count);
				array.fill(0.0f);
				float *w = array.ptrw();

				for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
					const Vertex &v = vertex_array[idx];

					if (v.weights.size() != count) {
						ERR_PRINT_ONCE(vformat("Invalid weight size %d vs count %d", v.weights.size(), count));
						continue;
					}

					for (int j = 0; j < count; j++) {
						w[idx * count + j] = v.weights[j];
					}
				}

				a[i] = array;

			} break;
			case Mesh::ARRAY_INDEX: {
				ERR_CONTINUE(index_array.is_empty());

				Vector<int> array;
				array.resize(index_array.size());
				int *w = array.ptrw();

				for (uint32_t idx = 0; idx < index_array.size(); idx++) {
					w[idx] = index_array[idx];
				}

				a[i] = array;
			} break;

			default: {
			}
		}
	}

	return a;
}

Ref<ArrayMesh> SurfaceTool::commit(const Ref<ArrayMesh> &p_existing, uint64_t p_compress_flags) {
	Ref<ArrayMesh> mesh;
	if (p_existing.is_valid()) {
		mesh = p_existing;
	} else {
		mesh.instantiate();
	}

	int varr_len = vertex_array.size();

	if (varr_len == 0) {
		return mesh;
	}

	int surface = mesh->get_surface_count();

	Array a = commit_to_arrays();

	uint64_t compress_flags = (p_compress_flags >> RS::ARRAY_COMPRESS_FLAGS_BASE) << RS::ARRAY_COMPRESS_FLAGS_BASE;
	static const uint64_t shift[RS::ARRAY_CUSTOM_COUNT] = { Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM1_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM2_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM3_SHIFT };
	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		if (last_custom_format[i] != CUSTOM_MAX) {
			compress_flags |= uint64_t(last_custom_format[i]) << shift[i];
		}
	}

	mesh->add_surface_from_arrays(primitive, a, Array(), Dictionary(), compress_flags);

	if (material.is_valid()) {
		mesh->surface_set_material(surface, material);
	}

	return mesh;
}

void SurfaceTool::index() {
	if (index_array.size()) {
		return; //already indexed
	}

	AHashMap<Vertex &, int, VertexHasher> indices = vertex_array.size();

	uint32_t new_size = 0;
	for (Vertex &vertex : vertex_array) {
		int *idxptr = indices.getptr(vertex);
		int idx;
		if (!idxptr) {
			idx = indices.size();
			vertex_array[new_size] = vertex;
			indices.insert_new(vertex_array[new_size], idx);
			new_size++;
		} else {
			idx = *idxptr;
		}

		index_array.push_back(idx);
	}

	vertex_array.resize(new_size);
	format |= Mesh::ARRAY_FORMAT_INDEX;
}

void SurfaceTool::deindex() {
	if (index_array.size() == 0) {
		return; //nothing to deindex
	}

	LocalVector<Vertex> old_vertex_array = vertex_array;
	vertex_array.clear();
	for (const int &index : index_array) {
		ERR_FAIL_COND(uint32_t(index) >= old_vertex_array.size());
		vertex_array.push_back(old_vertex_array[index]);
	}
	format &= ~Mesh::ARRAY_FORMAT_INDEX;
	index_array.clear();
}

void SurfaceTool::_create_list(const Ref<Mesh> &p_existing, int p_surface, LocalVector<Vertex> *r_vertex, LocalVector<int> *r_index, uint64_t &lformat) {
	ERR_FAIL_COND_MSG(p_existing.is_null(), "First argument in SurfaceTool::_create_list() must be a valid object of type Mesh");

	Array arr = p_existing->surface_get_arrays(p_surface);
	ERR_FAIL_COND(arr.size() != RS::ARRAY_MAX);
	_create_list_from_arrays(arr, r_vertex, r_index, lformat);
}

const uint32_t SurfaceTool::custom_mask[RS::ARRAY_CUSTOM_COUNT] = { Mesh::ARRAY_FORMAT_CUSTOM0, Mesh::ARRAY_FORMAT_CUSTOM1, Mesh::ARRAY_FORMAT_CUSTOM2, Mesh::ARRAY_FORMAT_CUSTOM3 };
const uint32_t SurfaceTool::custom_shift[RS::ARRAY_CUSTOM_COUNT] = { Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM1_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM2_SHIFT, Mesh::ARRAY_FORMAT_CUSTOM3_SHIFT };

void SurfaceTool::create_vertex_array_from_arrays(const Array &p_arrays, LocalVector<SurfaceTool::Vertex> &ret, uint64_t *r_format) {
	ERR_FAIL_INDEX(RS::ARRAY_WEIGHTS, p_arrays.size());

	ret.clear();

	Vector<Vector3> varr = p_arrays[RS::ARRAY_VERTEX];
	Vector<Vector3> narr = p_arrays[RS::ARRAY_NORMAL];
	Vector<float> tarr = p_arrays[RS::ARRAY_TANGENT];
	Vector<Color> carr = p_arrays[RS::ARRAY_COLOR];
	Vector<Vector2> uvarr = p_arrays[RS::ARRAY_TEX_UV];
	Vector<Vector2> uv2arr = p_arrays[RS::ARRAY_TEX_UV2];
	Vector<int> barr = p_arrays[RS::ARRAY_BONES];
	Vector<float> warr = p_arrays[RS::ARRAY_WEIGHTS];
	Vector<float> custom_float[RS::ARRAY_CUSTOM_COUNT];

	int vc = varr.size();
	if (vc == 0) {
		if (r_format) {
			*r_format = 0;
		}
		return;
	}

	uint64_t lformat = 0;
	if (varr.size()) {
		lformat |= RS::ARRAY_FORMAT_VERTEX;
	}
	if (narr.size()) {
		lformat |= RS::ARRAY_FORMAT_NORMAL;
	}
	if (tarr.size()) {
		lformat |= RS::ARRAY_FORMAT_TANGENT;
	}
	if (carr.size()) {
		lformat |= RS::ARRAY_FORMAT_COLOR;
	}
	if (uvarr.size()) {
		lformat |= RS::ARRAY_FORMAT_TEX_UV;
	}
	if (uv2arr.size()) {
		lformat |= RS::ARRAY_FORMAT_TEX_UV2;
	}
	int wcount = 0;
	if (barr.size() && warr.size()) {
		lformat |= RS::ARRAY_FORMAT_BONES;
		lformat |= RS::ARRAY_FORMAT_WEIGHTS;

		wcount = barr.size() / varr.size();
		if (wcount == 8) {
			lformat |= RS::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
		}
	}

	if (warr.size()) {
		lformat |= RS::ARRAY_FORMAT_WEIGHTS;
	}

	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		ERR_CONTINUE_MSG(p_arrays[RS::ARRAY_CUSTOM0 + i].get_type() == Variant::PACKED_BYTE_ARRAY, "Extracting Byte/Half formats is not supported");
		if (p_arrays[RS::ARRAY_CUSTOM0 + i].get_type() == Variant::PACKED_FLOAT32_ARRAY) {
			lformat |= custom_mask[i];
			custom_float[i] = p_arrays[RS::ARRAY_CUSTOM0 + i];
			int fmt = custom_float[i].size() / varr.size();
			if (fmt == 1) {
				lformat |= CUSTOM_R_FLOAT << custom_shift[i];
			} else if (fmt == 2) {
				lformat |= CUSTOM_RG_FLOAT << custom_shift[i];
			} else if (fmt == 3) {
				lformat |= CUSTOM_RGB_FLOAT << custom_shift[i];
			} else if (fmt == 4) {
				lformat |= CUSTOM_RGBA_FLOAT << custom_shift[i];
			}
		}
	}

	for (int i = 0; i < vc; i++) {
		Vertex v;
		if (lformat & RS::ARRAY_FORMAT_VERTEX) {
			v.vertex = varr[i];
		}
		if (lformat & RS::ARRAY_FORMAT_NORMAL) {
			v.normal = narr[i];
		}
		if (lformat & RS::ARRAY_FORMAT_TANGENT) {
			v.tangent = Vector3(tarr[i * 4 + 0], tarr[i * 4 + 1], tarr[i * 4 + 2]);
			float d = tarr[i * 4 + 3];
			v.binormal = v.normal.cross(v.tangent).normalized() * d;
		}
		if (lformat & RS::ARRAY_FORMAT_COLOR) {
			v.color = carr[i];
		}
		if (lformat & RS::ARRAY_FORMAT_TEX_UV) {
			v.uv = uvarr[i];
		}
		if (lformat & RS::ARRAY_FORMAT_TEX_UV2) {
			v.uv2 = uv2arr[i];
		}
		if (lformat & RS::ARRAY_FORMAT_BONES) {
			Vector<int> b;
			b.resize(wcount);
			for (int j = 0; j < wcount; j++) {
				b.write[j] = barr[i * wcount + j];
			}
			v.bones = b;
		}
		if (lformat & RS::ARRAY_FORMAT_WEIGHTS) {
			Vector<float> w;
			w.resize(wcount);
			for (int j = 0; j < wcount; j++) {
				w.write[j] = warr[i * wcount + j];
			}
			v.weights = w;
		}

		for (int j = 0; j < RS::ARRAY_CUSTOM_COUNT; j++) {
			if (lformat & custom_mask[j]) {
				int cc = custom_float[j].size() / varr.size();
				for (int k = 0; k < cc; k++) {
					v.custom[j][k] = custom_float[j][i * cc + k];
				}
			}
		}

		ret.push_back(v);
	}

	if (r_format) {
		*r_format = lformat;
	}
}

void SurfaceTool::_create_list_from_arrays(Array arr, LocalVector<Vertex> *r_vertex, LocalVector<int> *r_index, uint64_t &lformat) {
	create_vertex_array_from_arrays(arr, *r_vertex, &lformat);
	ERR_FAIL_COND(r_vertex->size() == 0);

	//indices
	r_index->clear();

	Vector<int> idx = arr[RS::ARRAY_INDEX];
	int is = idx.size();
	if (is) {
		lformat |= RS::ARRAY_FORMAT_INDEX;
		const int *iarr = idx.ptr();
		for (int i = 0; i < is; i++) {
			r_index->push_back(iarr[i]);
		}
	}
}

void SurfaceTool::create_from_arrays(const Array &p_arrays, Mesh::PrimitiveType p_primitive_type) {
	clear();
	primitive = p_primitive_type;
	_create_list_from_arrays(p_arrays, &vertex_array, &index_array, format);

	for (int j = 0; j < RS::ARRAY_CUSTOM_COUNT; j++) {
		if (format & custom_mask[j]) {
			last_custom_format[j] = (CustomFormat)((format >> custom_shift[j]) & RS::ARRAY_FORMAT_CUSTOM_MASK);
		}
	}
}

void SurfaceTool::create_from_triangle_arrays(const Array &p_arrays) {
	create_from_arrays(p_arrays, Mesh::PRIMITIVE_TRIANGLES);
}

void SurfaceTool::create_from(const Ref<Mesh> &p_existing, int p_surface) {
	ERR_FAIL_COND_MSG(p_existing.is_null(), "First argument in SurfaceTool::create_from() must be a valid object of type Mesh");

	clear();
	primitive = p_existing->surface_get_primitive_type(p_surface);
	_create_list(p_existing, p_surface, &vertex_array, &index_array, format);
	material = p_existing->surface_get_material(p_surface);

	for (int j = 0; j < RS::ARRAY_CUSTOM_COUNT; j++) {
		if (format & custom_mask[j]) {
			last_custom_format[j] = (CustomFormat)((format >> custom_shift[j]) & RS::ARRAY_FORMAT_CUSTOM_MASK);
		}
	}
}

void SurfaceTool::create_from_blend_shape(const Ref<Mesh> &p_existing, int p_surface, const String &p_blend_shape_name) {
	ERR_FAIL_COND_MSG(p_existing.is_null(), "First argument in SurfaceTool::create_from_blend_shape() must be a valid object of type Mesh");

	clear();
	primitive = p_existing->surface_get_primitive_type(p_surface);
	Array arr = p_existing->surface_get_blend_shape_arrays(p_surface);
	Array blend_shape_names;
	int32_t shape_idx = -1;
	for (int32_t i = 0; i < p_existing->get_blend_shape_count(); i++) {
		String name = p_existing->get_blend_shape_name(i);
		if (name == p_blend_shape_name) {
			shape_idx = i;
			break;
		}
	}
	ERR_FAIL_COND(shape_idx == -1);
	ERR_FAIL_COND(shape_idx >= arr.size());
	Array blendshape_mesh_arrays = arr[shape_idx];
	ERR_FAIL_COND(blendshape_mesh_arrays.size() != RS::ARRAY_MAX);

	Array source_mesh_arrays = p_existing->surface_get_arrays(p_surface);
	ERR_FAIL_COND(source_mesh_arrays.size() != RS::ARRAY_MAX);

	// Copy BlendShape vertex data over while keeping e.g. bones, weights, index from existing mesh intact.
	source_mesh_arrays[RS::ARRAY_VERTEX] = blendshape_mesh_arrays[RS::ARRAY_VERTEX];
	source_mesh_arrays[RS::ARRAY_NORMAL] = blendshape_mesh_arrays[RS::ARRAY_NORMAL];
	source_mesh_arrays[RS::ARRAY_TANGENT] = blendshape_mesh_arrays[RS::ARRAY_TANGENT];

	_create_list_from_arrays(source_mesh_arrays, &vertex_array, &index_array, format);

	material = p_existing->surface_get_material(p_surface);
	format = p_existing->surface_get_format(p_surface);

	for (int j = 0; j < RS::ARRAY_CUSTOM_COUNT; j++) {
		if (format & custom_mask[j]) {
			last_custom_format[j] = (CustomFormat)((format >> custom_shift[j]) & RS::ARRAY_FORMAT_CUSTOM_MASK);
		}
	}
}

void SurfaceTool::append_from(const Ref<Mesh> &p_existing, int p_surface, const Transform3D &p_xform) {
	ERR_FAIL_COND_MSG(p_existing.is_null(), "First argument in SurfaceTool::append_from() must be a valid object of type Mesh");

	if (vertex_array.size() == 0) {
		primitive = p_existing->surface_get_primitive_type(p_surface);
		format = 0;
	}

	uint64_t nformat = 0;
	LocalVector<Vertex> nvertices;
	LocalVector<int> nindices;
	_create_list(p_existing, p_surface, &nvertices, &nindices, nformat);
	format |= nformat;

	for (int j = 0; j < RS::ARRAY_CUSTOM_COUNT; j++) {
		if (format & custom_mask[j]) {
			CustomFormat new_format = (CustomFormat)((format >> custom_shift[j]) & RS::ARRAY_FORMAT_CUSTOM_MASK);
			last_custom_format[j] = new_format;
		}
	}
	int vfrom = vertex_array.size();

	for (Vertex &v : nvertices) {
		v.vertex = p_xform.xform(v.vertex);
		if (nformat & RS::ARRAY_FORMAT_NORMAL) {
			v.normal = p_xform.basis.xform(v.normal);
		}
		if (nformat & RS::ARRAY_FORMAT_TANGENT) {
			v.tangent = p_xform.basis.xform(v.tangent);
			v.binormal = p_xform.basis.xform(v.binormal);
		}

		vertex_array.push_back(v);
	}

	for (const int &index : nindices) {
		int dst_index = index + vfrom;
		index_array.push_back(dst_index);
	}
	if (index_array.size() % 3) {
		WARN_PRINT("SurfaceTool: Index array not a multiple of 3.");
	}
}

//mikktspace callbacks
namespace {
struct TangentGenerationContextUserData {
	LocalVector<SurfaceTool::Vertex> *vertices;
	LocalVector<int> *indices;
};
} // namespace

int SurfaceTool::mikktGetNumFaces(const SMikkTSpaceContext *pContext) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);

	if (triangle_data.indices->size() > 0) {
		return triangle_data.indices->size() / 3;
	} else {
		return triangle_data.vertices->size() / 3;
	}
}

int SurfaceTool::mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3; //always 3
}

void SurfaceTool::mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector3 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).vertex;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).vertex;
	}

	fvPosOut[0] = v.x;
	fvPosOut[1] = v.y;
	fvPosOut[2] = v.z;
}

void SurfaceTool::mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector3 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).normal;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).normal;
	}

	fvNormOut[0] = v.x;
	fvNormOut[1] = v.y;
	fvNormOut[2] = v.z;
}

void SurfaceTool::mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector2 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).uv;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).uv;
	}

	fvTexcOut[0] = v.x;
	fvTexcOut[1] = v.y;
}

void SurfaceTool::mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
		const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vertex *vtx = nullptr;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			vtx = &triangle_data.vertices->operator[](index);
		}
	} else {
		vtx = &triangle_data.vertices->operator[](iFace * 3 + iVert);
	}

	if (vtx != nullptr) {
		vtx->tangent = Vector3(fvTangent[0], fvTangent[1], fvTangent[2]);
		vtx->binormal = Vector3(-fvBiTangent[0], -fvBiTangent[1], -fvBiTangent[2]); // for some reason these are reversed, something with the coordinate system in Godot
	}
}

void SurfaceTool::generate_tangents() {
	ERR_FAIL_COND_MSG(!(format & Mesh::ARRAY_FORMAT_TEX_UV), "UVs are required to generate tangents.");
	ERR_FAIL_COND(!(format & Mesh::ARRAY_FORMAT_NORMAL));

	SMikkTSpaceInterface mkif;
	mkif.m_getNormal = mikktGetNormal;
	mkif.m_getNumFaces = mikktGetNumFaces;
	mkif.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
	mkif.m_getPosition = mikktGetPosition;
	mkif.m_getTexCoord = mikktGetTexCoord;
	mkif.m_setTSpace = mikktSetTSpaceDefault;
	mkif.m_setTSpaceBasic = nullptr;

	SMikkTSpaceContext msc;
	msc.m_pInterface = &mkif;

	TangentGenerationContextUserData triangle_data;
	triangle_data.vertices = &vertex_array;
	for (Vertex &vertex : vertex_array) {
		vertex.binormal = Vector3();
		vertex.tangent = Vector3();
	}
	triangle_data.indices = &index_array;
	msc.m_pUserData = &triangle_data;

	bool res = genTangSpaceDefault(&msc);

	ERR_FAIL_COND(!res);
	format |= Mesh::ARRAY_FORMAT_TANGENT;
}

void SurfaceTool::generate_normals(bool p_flip) {
	ERR_FAIL_COND(primitive != Mesh::PRIMITIVE_TRIANGLES);

	bool was_indexed = index_array.size();

	deindex();

	ERR_FAIL_COND((vertex_array.size() % 3) != 0);

	AHashMap<SmoothGroupVertex, Vector3, SmoothGroupVertexHasher> smooth_hash = vertex_array.size();

	for (uint32_t vi = 0; vi < vertex_array.size(); vi += 3) {
		Vertex *v = &vertex_array[vi];

		Vector3 normal;
		if (!p_flip) {
			normal = Plane(v[0].vertex, v[1].vertex, v[2].vertex).normal;
		} else {
			normal = Plane(v[2].vertex, v[1].vertex, v[0].vertex).normal;
		}

		for (int i = 0; i < 3; i++) {
			// Add face normal to smooth vertex influence if vertex is member of a smoothing group
			if (v[i].smooth_group != UINT32_MAX) {
				Vector3 *lv = smooth_hash.getptr(v[i]);
				if (!lv) {
					smooth_hash.insert_new(v[i], normal);
				} else {
					(*lv) += normal;
				}
			} else {
				v[i].normal = normal;
			}
		}
	}

	for (Vertex &vertex : vertex_array) {
		if (vertex.smooth_group != UINT32_MAX) {
			Vector3 *lv = smooth_hash.getptr(vertex);
			if (!lv) {
				vertex.normal = Vector3();
			} else {
				vertex.normal = lv->normalized();
			}
		}
	}

	format |= Mesh::ARRAY_FORMAT_NORMAL;

	if (was_indexed) {
		index();
	}
}

void SurfaceTool::set_material(const Ref<Material> &p_material) {
	material = p_material;
}

Ref<Material> SurfaceTool::get_material() const {
	return material;
}

void SurfaceTool::clear() {
	begun = false;
	primitive = Mesh::PRIMITIVE_LINES;
	format = 0;
	last_bones.clear();
	last_weights.clear();
	index_array.clear();
	vertex_array.clear();
	material.unref();
	last_smooth_group = 0;
	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		last_custom_format[i] = CUSTOM_MAX;
	}
	skin_weights = SKIN_4_WEIGHTS;
}

void SurfaceTool::set_skin_weight_count(SkinWeightCount p_weights) {
	ERR_FAIL_COND(begun);
	skin_weights = p_weights;
}
SurfaceTool::SkinWeightCount SurfaceTool::get_skin_weight_count() const {
	return skin_weights;
}

void SurfaceTool::set_custom_format(int p_channel_index, CustomFormat p_format) {
	ERR_FAIL_INDEX(p_channel_index, RS::ARRAY_CUSTOM_COUNT);
	ERR_FAIL_COND(!begun);
	ERR_FAIL_INDEX(p_format, CUSTOM_MAX + 1);
	last_custom_format[p_channel_index] = p_format;
}

Mesh::PrimitiveType SurfaceTool::get_primitive_type() const {
	return primitive;
}
SurfaceTool::CustomFormat SurfaceTool::get_custom_format(int p_channel_index) const {
	ERR_FAIL_INDEX_V(p_channel_index, RS::ARRAY_CUSTOM_COUNT, CUSTOM_MAX);
	return last_custom_format[p_channel_index];
}
void SurfaceTool::optimize_indices_for_cache() {
	ERR_FAIL_NULL(optimize_vertex_cache_func);
	ERR_FAIL_COND(index_array.is_empty());
	ERR_FAIL_COND(primitive != Mesh::PRIMITIVE_TRIANGLES);
	ERR_FAIL_COND(index_array.size() % 3 != 0);

	LocalVector old_index_array = index_array;
	memset(index_array.ptr(), 0, index_array.size() * sizeof(int));
	optimize_vertex_cache_func((unsigned int *)index_array.ptr(), (unsigned int *)old_index_array.ptr(), old_index_array.size(), vertex_array.size());
}

AABB SurfaceTool::get_aabb() const {
	ERR_FAIL_COND_V(vertex_array.is_empty(), AABB());

	AABB aabb;
	for (uint32_t i = 0; i < vertex_array.size(); i++) {
		if (i == 0) {
			aabb.position = vertex_array[i].vertex;
		} else {
			aabb.expand_to(vertex_array[i].vertex);
		}
	}

	return aabb;
}
Vector<int> SurfaceTool::generate_lod(float p_threshold, int p_target_index_count) {
	WARN_DEPRECATED_MSG(R"*(The "SurfaceTool.generate_lod()" method is deprecated. Consider using "ImporterMesh.generate_lods()" instead.)*");

	Vector<int> lod;

	ERR_FAIL_NULL_V(simplify_func, lod);
	ERR_FAIL_COND_V(p_target_index_count < 0, lod);
	ERR_FAIL_COND_V(vertex_array.is_empty(), lod);
	ERR_FAIL_COND_V(index_array.is_empty(), lod);
	ERR_FAIL_COND_V(index_array.size() % 3 != 0, lod);
	ERR_FAIL_COND_V(index_array.size() < (unsigned int)p_target_index_count, lod);

	lod.resize(index_array.size());
	LocalVector<float> vertices; //uses floats
	vertices.resize(vertex_array.size() * 3);
	for (uint32_t i = 0; i < vertex_array.size(); i++) {
		vertices[i * 3 + 0] = vertex_array[i].vertex.x;
		vertices[i * 3 + 1] = vertex_array[i].vertex.y;
		vertices[i * 3 + 2] = vertex_array[i].vertex.z;
	}

	float error;
	const int simplify_options = SIMPLIFY_LOCK_BORDER;
	uint32_t index_count = simplify_func((unsigned int *)lod.ptrw(), (unsigned int *)index_array.ptr(), index_array.size(), vertices.ptr(), vertex_array.size(), sizeof(float) * 3, p_target_index_count, p_threshold, simplify_options, &error);
	ERR_FAIL_COND_V(index_count == 0, lod);
	lod.resize(index_count);

	return lod;
}

void SurfaceTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_skin_weight_count", "count"), &SurfaceTool::set_skin_weight_count);
	ClassDB::bind_method(D_METHOD("get_skin_weight_count"), &SurfaceTool::get_skin_weight_count);

	ClassDB::bind_method(D_METHOD("set_custom_format", "channel_index", "format"), &SurfaceTool::set_custom_format);
	ClassDB::bind_method(D_METHOD("get_custom_format", "channel_index"), &SurfaceTool::get_custom_format);

	ClassDB::bind_method(D_METHOD("begin", "primitive"), &SurfaceTool::begin);

	ClassDB::bind_method(D_METHOD("add_vertex", "vertex"), &SurfaceTool::add_vertex);
	ClassDB::bind_method(D_METHOD("set_color", "color"), &SurfaceTool::set_color);
	ClassDB::bind_method(D_METHOD("set_normal", "normal"), &SurfaceTool::set_normal);
	ClassDB::bind_method(D_METHOD("set_tangent", "tangent"), &SurfaceTool::set_tangent);
	ClassDB::bind_method(D_METHOD("set_uv", "uv"), &SurfaceTool::set_uv);
	ClassDB::bind_method(D_METHOD("set_uv2", "uv2"), &SurfaceTool::set_uv2);
	ClassDB::bind_method(D_METHOD("set_bones", "bones"), &SurfaceTool::set_bones);
	ClassDB::bind_method(D_METHOD("set_weights", "weights"), &SurfaceTool::set_weights);
	ClassDB::bind_method(D_METHOD("set_custom", "channel_index", "custom_color"), &SurfaceTool::set_custom);
	ClassDB::bind_method(D_METHOD("set_smooth_group", "index"), &SurfaceTool::set_smooth_group);

	ClassDB::bind_method(D_METHOD("add_triangle_fan", "vertices", "uvs", "colors", "uv2s", "normals", "tangents"), &SurfaceTool::_add_triangle_fan, DEFVAL(Vector<Vector2>()), DEFVAL(Vector<Color>()), DEFVAL(Vector<Vector2>()), DEFVAL(Vector<Vector3>()), DEFVAL(TypedArray<Plane>()));

	ClassDB::bind_method(D_METHOD("add_index", "index"), &SurfaceTool::add_index);

	ClassDB::bind_method(D_METHOD("index"), &SurfaceTool::index);
	ClassDB::bind_method(D_METHOD("deindex"), &SurfaceTool::deindex);
	ClassDB::bind_method(D_METHOD("generate_normals", "flip"), &SurfaceTool::generate_normals, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("generate_tangents"), &SurfaceTool::generate_tangents);

	ClassDB::bind_method(D_METHOD("optimize_indices_for_cache"), &SurfaceTool::optimize_indices_for_cache);

	ClassDB::bind_method(D_METHOD("get_aabb"), &SurfaceTool::get_aabb);
	ClassDB::bind_method(D_METHOD("generate_lod", "nd_threshold", "target_index_count"), &SurfaceTool::generate_lod, DEFVAL(3));

	ClassDB::bind_method(D_METHOD("set_material", "material"), &SurfaceTool::set_material);
	ClassDB::bind_method(D_METHOD("get_primitive_type"), &SurfaceTool::get_primitive_type);

	ClassDB::bind_method(D_METHOD("clear"), &SurfaceTool::clear);

	ClassDB::bind_method(D_METHOD("create_from", "existing", "surface"), &SurfaceTool::create_from);
	ClassDB::bind_method(D_METHOD("create_from_arrays", "arrays", "primitive_type"), &SurfaceTool::create_from_arrays, DEFVAL(Mesh::PRIMITIVE_TRIANGLES));
	ClassDB::bind_method(D_METHOD("create_from_blend_shape", "existing", "surface", "blend_shape"), &SurfaceTool::create_from_blend_shape);
	ClassDB::bind_method(D_METHOD("append_from", "existing", "surface", "transform"), &SurfaceTool::append_from);
	ClassDB::bind_method(D_METHOD("commit", "existing", "flags"), &SurfaceTool::commit, DEFVAL(Variant()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("commit_to_arrays"), &SurfaceTool::commit_to_arrays);

	BIND_ENUM_CONSTANT(CUSTOM_RGBA8_UNORM);
	BIND_ENUM_CONSTANT(CUSTOM_RGBA8_SNORM);
	BIND_ENUM_CONSTANT(CUSTOM_RG_HALF);
	BIND_ENUM_CONSTANT(CUSTOM_RGBA_HALF);
	BIND_ENUM_CONSTANT(CUSTOM_R_FLOAT);
	BIND_ENUM_CONSTANT(CUSTOM_RG_FLOAT);
	BIND_ENUM_CONSTANT(CUSTOM_RGB_FLOAT);
	BIND_ENUM_CONSTANT(CUSTOM_RGBA_FLOAT);
	BIND_ENUM_CONSTANT(CUSTOM_MAX);
	BIND_ENUM_CONSTANT(SKIN_4_WEIGHTS);
	BIND_ENUM_CONSTANT(SKIN_8_WEIGHTS);
}

SurfaceTool::SurfaceTool() {
	for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
		last_custom_format[i] = CUSTOM_MAX;
	}
}
