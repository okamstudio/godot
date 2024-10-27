/**************************************************************************/
/*  rendering_server.cpp                                                  */
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

#include "rendering_server.h"
#include "rendering_server.compat.inc"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "core/variant/typed_array.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_warnings.h"

RenderingServer *RenderingServer::singleton = nullptr;
RenderingServer *(*RenderingServer::create_func)() = nullptr;

RenderingServer *RenderingServer::get_singleton() {
	return singleton;
}

RenderingServer *RenderingServer::create() {
	ERR_FAIL_COND_V(singleton, nullptr);

	if (create_func) {
		return create_func();
	}

	return nullptr;
}

Array RenderingServer::_texture_debug_usage_bind() {
	List<TextureInfo> list;
	texture_debug_usage(&list);
	Array arr;
	for (const TextureInfo &E : list) {
		Dictionary dict;
		dict["texture"] = E.texture;
		dict["width"] = E.width;
		dict["height"] = E.height;
		dict["depth"] = E.depth;
		dict["format"] = E.format;
		dict["bytes"] = E.bytes;
		dict["path"] = E.path;
		arr.push_back(dict);
	}
	return arr;
}

static PackedInt64Array to_int_array(const Vector<ObjectID> &ids) {
	PackedInt64Array a;
	a.resize(ids.size());
	for (int i = 0; i < ids.size(); ++i) {
		a.write[i] = ids[i];
	}
	return a;
}

PackedInt64Array RenderingServer::_instances_cull_aabb_bind(const AABB &p_aabb, RID p_scenario) const {
	Vector<ObjectID> ids = instances_cull_aabb(p_aabb, p_scenario);
	return to_int_array(ids);
}

PackedInt64Array RenderingServer::_instances_cull_ray_bind(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario) const {
	Vector<ObjectID> ids = instances_cull_ray(p_from, p_to, p_scenario);
	return to_int_array(ids);
}

PackedInt64Array RenderingServer::_instances_cull_convex_bind(const TypedArray<Plane> &p_convex, RID p_scenario) const {
	Vector<Plane> planes;
	for (int i = 0; i < p_convex.size(); ++i) {
		const Variant &v = p_convex[i];
		ERR_FAIL_COND_V(v.get_type() != Variant::PLANE, PackedInt64Array());
		planes.push_back(v);
	}

	Vector<ObjectID> ids = instances_cull_convex(planes, p_scenario);
	return to_int_array(ids);
}

RID RenderingServer::get_test_texture() {
	if (test_texture.is_valid()) {
		return test_texture;
	};

#define TEST_TEXTURE_SIZE 256

	Vector<uint8_t> test_data;
	test_data.resize(TEST_TEXTURE_SIZE * TEST_TEXTURE_SIZE * 3);

	{
		uint8_t *w = test_data.ptrw();

		for (int x = 0; x < TEST_TEXTURE_SIZE; x++) {
			for (int y = 0; y < TEST_TEXTURE_SIZE; y++) {
				Color c;
				int r = 255 - (x + y) / 2;

				if ((x % (TEST_TEXTURE_SIZE / 8)) < 2 || (y % (TEST_TEXTURE_SIZE / 8)) < 2) {
					c.r = y;
					c.g = r;
					c.b = x;

				} else {
					c.r = r;
					c.g = x;
					c.b = y;
				}

				w[(y * TEST_TEXTURE_SIZE + x) * 3 + 0] = uint8_t(CLAMP(c.r, 0, 255));
				w[(y * TEST_TEXTURE_SIZE + x) * 3 + 1] = uint8_t(CLAMP(c.g, 0, 255));
				w[(y * TEST_TEXTURE_SIZE + x) * 3 + 2] = uint8_t(CLAMP(c.b, 0, 255));
			}
		}
	}

	Ref<Image> data = memnew(Image(TEST_TEXTURE_SIZE, TEST_TEXTURE_SIZE, false, Image::FORMAT_RGB8, test_data));

	test_texture = texture_2d_create(data);

	return test_texture;
}

void RenderingServer::_free_internal_rids() {
	if (test_texture.is_valid()) {
		free(test_texture);
	}
	if (white_texture.is_valid()) {
		free(white_texture);
	}
	if (test_material.is_valid()) {
		free(test_material);
	}
}

RID RenderingServer::_make_test_cube() {
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector3> uvs;

#define ADD_VTX(m_idx)                           \
	vertices.push_back(face_points[m_idx]);      \
	normals.push_back(normal_points[m_idx]);     \
	tangents.push_back(normal_points[m_idx][1]); \
	tangents.push_back(normal_points[m_idx][2]); \
	tangents.push_back(normal_points[m_idx][0]); \
	tangents.push_back(1.0);                     \
	uvs.push_back(Vector3(uv_points[m_idx * 2 + 0], uv_points[m_idx * 2 + 1], 0));

	for (int i = 0; i < 6; i++) {
		Vector3 face_points[4];
		Vector3 normal_points[4];
		float uv_points[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };

		for (int j = 0; j < 4; j++) {
			float v[3];
			v[0] = 1.0;
			v[1] = 1 - 2 * ((j >> 1) & 1);
			v[2] = v[1] * (1 - 2 * (j & 1));

			for (int k = 0; k < 3; k++) {
				if (i < 3) {
					face_points[j][(i + k) % 3] = v[k];
				} else {
					face_points[3 - j][(i + k) % 3] = -v[k];
				}
			}
			normal_points[j] = Vector3();
			normal_points[j][i % 3] = (i >= 3 ? -1 : 1);
		}

		// Tri 1
		ADD_VTX(0);
		ADD_VTX(1);
		ADD_VTX(2);
		// Tri 2
		ADD_VTX(2);
		ADD_VTX(3);
		ADD_VTX(0);
	}

	RID test_cube = mesh_create();

	Array d;
	d.resize(RS::ARRAY_MAX);
	d[RenderingServer::ARRAY_NORMAL] = normals;
	d[RenderingServer::ARRAY_TANGENT] = tangents;
	d[RenderingServer::ARRAY_TEX_UV] = uvs;
	d[RenderingServer::ARRAY_VERTEX] = vertices;

	Vector<int> indices;
	indices.resize(vertices.size());
	for (int i = 0; i < vertices.size(); i++) {
		indices.set(i, i);
	}
	d[RenderingServer::ARRAY_INDEX] = indices;

	mesh_add_surface_from_arrays(test_cube, PRIMITIVE_TRIANGLES, d);

	/*
	test_material = fixed_material_create();
	//material_set_flag(material, MATERIAL_FLAG_BILLBOARD_TOGGLE,true);
	fixed_material_set_texture( test_material, FIXED_MATERIAL_PARAM_DIFFUSE, get_test_texture() );
	fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_SPECULAR_EXP, 70 );
	fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_EMISSION, Color(0.2,0.2,0.2) );

	fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_DIFFUSE, Color(1, 1, 1) );
	fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_SPECULAR, Color(1,1,1) );
*/
	mesh_surface_set_material(test_cube, 0, test_material);

	return test_cube;
}

RID RenderingServer::make_sphere_mesh(int p_lats, int p_lons, real_t p_radius) {
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	const double lat_step = Math_TAU / p_lats;
	const double lon_step = Math_TAU / p_lons;

	for (int i = 1; i <= p_lats; i++) {
		double lat0 = lat_step * (i - 1) - Math_TAU / 4;
		double z0 = Math::sin(lat0);
		double zr0 = Math::cos(lat0);

		double lat1 = lat_step * i - Math_TAU / 4;
		double z1 = Math::sin(lat1);
		double zr1 = Math::cos(lat1);

		for (int j = p_lons; j >= 1; j--) {
			double lng0 = lon_step * (j - 1);
			double x0 = Math::cos(lng0);
			double y0 = Math::sin(lng0);

			double lng1 = lon_step * j;
			double x1 = Math::cos(lng1);
			double y1 = Math::sin(lng1);

			Vector3 v[4] = {
				Vector3(x1 * zr0, z0, y1 * zr0),
				Vector3(x1 * zr1, z1, y1 * zr1),
				Vector3(x0 * zr1, z1, y0 * zr1),
				Vector3(x0 * zr0, z0, y0 * zr0)
			};

#define ADD_POINT(m_idx)         \
	normals.push_back(v[m_idx]); \
	vertices.push_back(v[m_idx] * p_radius);

			ADD_POINT(0);
			ADD_POINT(1);
			ADD_POINT(2);

			ADD_POINT(2);
			ADD_POINT(3);
			ADD_POINT(0);
		}
	}

	RID mesh = mesh_create();
	Array d;
	d.resize(RS::ARRAY_MAX);

	d[ARRAY_VERTEX] = vertices;
	d[ARRAY_NORMAL] = normals;

	mesh_add_surface_from_arrays(mesh, PRIMITIVE_TRIANGLES, d);

	return mesh;
}

RID RenderingServer::get_white_texture() {
	if (white_texture.is_valid()) {
		return white_texture;
	}

	Vector<uint8_t> wt;
	wt.resize(16 * 3);
	{
		uint8_t *w = wt.ptrw();
		for (int i = 0; i < 16 * 3; i++) {
			w[i] = 255;
		}
	}
	Ref<Image> white = memnew(Image(4, 4, 0, Image::FORMAT_RGB8, wt));
	white_texture = texture_2d_create(white);
	return white_texture;
}

void _get_axis_angle(const Vector3 &p_normal, const Vector4 &p_tangent, float &r_angle, Vector3 &r_axis) {
	Vector3 normal = p_normal.normalized();
	Vector3 tangent = Vector3(p_tangent.x, p_tangent.y, p_tangent.z).normalized();
	float d = p_tangent.w;
	Vector3 binormal = normal.cross(tangent).normalized();
	real_t angle;

	Basis tbn = Basis();
	tbn.rows[0] = tangent;
	tbn.rows[1] = binormal;
	tbn.rows[2] = normal;
	tbn.get_axis_angle(r_axis, angle);
	r_angle = float(angle);

	if (d < 0.0) {
		r_angle = CLAMP((1.0 - r_angle / Math_PI) * 0.5, 0.0, 0.49999);
	} else {
		r_angle = CLAMP((r_angle / Math_PI) * 0.5 + 0.5, 0.500008, 1.0);
	}
}

// The inputs to this function should match the outputs of _get_axis_angle. I.e. p_axis is a normalized vector
// and p_angle includes the binormal direction.
void _get_tbn_from_axis_angle(const Vector3 &p_axis, float p_angle, Vector3 &r_normal, Vector4 &r_tangent) {
	float binormal_sign = p_angle > 0.5 ? 1.0 : -1.0;
	float angle = Math::abs(p_angle * 2.0 - 1.0) * Math_PI;

	Basis tbn = Basis(p_axis, angle);
	Vector3 tan = tbn.rows[0];
	r_tangent = Vector4(tan.x, tan.y, tan.z, binormal_sign);
	r_normal = tbn.rows[2];
}

AABB _compute_aabb_from_points(const Vector3 *p_data, int p_length) {
	if (p_length == 0) {
		return AABB();
	}

	Vector3 min = p_data[0];
	Vector3 max = p_data[0];

	for (int i = 1; i < p_length; ++i) {
		min = min.min(p_data[i]);
		max = max.max(p_data[i]);
	}

	return AABB(min, max - min);
}

Error RenderingServer::_surface_set_data(Array p_arrays, uint64_t p_format, uint32_t *p_offsets, uint32_t p_vertex_stride, uint32_t p_normal_stride, uint32_t p_attrib_stride, uint32_t p_skin_stride, Vector<uint8_t> &r_vertex_array, Vector<uint8_t> &r_attrib_array, Vector<uint8_t> &r_skin_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array, int p_index_array_len, AABB &r_aabb, Vector<AABB> &r_bone_aabb, Vector4 &r_uv_scale) {
	uint8_t *vw = r_vertex_array.ptrw();
	uint8_t *aw = r_attrib_array.ptrw();
	uint8_t *sw = r_skin_array.ptrw();

	uint8_t *iw = nullptr;
	if (r_index_array.size()) {
		iw = r_index_array.ptrw();
	}

	int max_bone = 0;

	// Preprocess UVs if compression is enabled
	if (p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES && ((p_format & RS::ARRAY_FORMAT_TEX_UV) || (p_format & RS::ARRAY_FORMAT_TEX_UV2))) {
		const Vector2 *uv_src = nullptr;
		if (p_format & RS::ARRAY_FORMAT_TEX_UV) {
			Vector<Vector2> array = p_arrays[RS::ARRAY_TEX_UV];
			uv_src = array.ptr();
		}

		const Vector2 *uv2_src = nullptr;
		if (p_format & RS::ARRAY_FORMAT_TEX_UV2) {
			Vector<Vector2> array = p_arrays[RS::ARRAY_TEX_UV2];
			uv2_src = array.ptr();
		}

		Vector2 max_val = Vector2(0.0, 0.0);
		Vector2 min_val = Vector2(0.0, 0.0);
		Vector2 max_val2 = Vector2(0.0, 0.0);
		Vector2 min_val2 = Vector2(0.0, 0.0);

		for (int i = 0; i < p_vertex_array_len; i++) {
			if (p_format & RS::ARRAY_FORMAT_TEX_UV) {
				max_val = max_val.max(uv_src[i]);
				min_val = min_val.min(uv_src[i]);
			}
			if (p_format & RS::ARRAY_FORMAT_TEX_UV2) {
				max_val2 = max_val2.max(uv2_src[i]);
				min_val2 = min_val2.min(uv2_src[i]);
			}
		}

		max_val = max_val.abs().max(min_val.abs());
		max_val2 = max_val2.abs().max(min_val2.abs());

		if (min_val.x >= 0.0 && min_val2.x >= 0.0 && max_val.x <= 1.0 && max_val2.x <= 1.0 &&
				min_val.y >= 0.0 && min_val2.y >= 0.0 && max_val.y <= 1.0 && max_val2.y <= 1.0) {
			// When all channels are in the 0-1 range, we will compress to 16-bit without scaling to
			// preserve the bits as best as possible.
			r_uv_scale = Vector4(0.0, 0.0, 0.0, 0.0);
		} else {
			r_uv_scale = Vector4(max_val.x, max_val.y, max_val2.x, max_val2.y) * Vector4(2.0, 2.0, 2.0, 2.0);
		}
	}

	for (int ai = 0; ai < RS::ARRAY_MAX; ai++) {
		if (!(p_format & (1ULL << ai))) { // No array
			continue;
		}

		switch (ai) {
			case RS::ARRAY_VERTEX: {
				if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
					Vector<Vector2> array = p_arrays[ai];
					ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

					const Vector2 *src = array.ptr();

					// Setting vertices means regenerating the AABB.
					Rect2 aabb;

					{
						for (int i = 0; i < p_vertex_array_len; i++) {
							float vector[2] = { (float)src[i].x, (float)src[i].y };

							memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(float) * 2);

							if (i == 0) {
								aabb = Rect2(src[i], SMALL_VEC2); // Must have a bit of size.
							} else {
								aabb.expand_to(src[i]);
							}
						}
					}

					r_aabb = AABB(Vector3(aabb.position.x, aabb.position.y, 0), Vector3(aabb.size.x, aabb.size.y, 0));

				} else {
					Vector<Vector3> array = p_arrays[ai];
					ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

					const Vector3 *src = array.ptr();

					r_aabb = _compute_aabb_from_points(src, p_vertex_array_len);
					r_aabb.size = r_aabb.size.max(SMALL_VEC3);

					if (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
						if (!(p_format & RS::ARRAY_FORMAT_NORMAL)) {
							// Early out if we are only setting vertex positions.
							for (int i = 0; i < p_vertex_array_len; i++) {
								Vector3 pos = (src[i] - r_aabb.position) / r_aabb.size;
								uint16_t vector[4] = {
									(uint16_t)CLAMP(pos.x * 65535, 0, 65535),
									(uint16_t)CLAMP(pos.y * 65535, 0, 65535),
									(uint16_t)CLAMP(pos.z * 65535, 0, 65535),
									(uint16_t)0
								};

								memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(uint16_t) * 4);
							}
							continue;
						}

						// Validate normal and tangent arrays.
						ERR_FAIL_COND_V(p_arrays[RS::ARRAY_NORMAL].get_type() != Variant::PACKED_VECTOR3_ARRAY, ERR_INVALID_PARAMETER);

						Vector<Vector3> normal_array = p_arrays[RS::ARRAY_NORMAL];
						ERR_FAIL_COND_V(normal_array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);
						const Vector3 *normal_src = normal_array.ptr();

						Variant::Type tangent_type = p_arrays[RS::ARRAY_TANGENT].get_type();
						ERR_FAIL_COND_V(tangent_type != Variant::PACKED_FLOAT32_ARRAY && tangent_type != Variant::PACKED_FLOAT64_ARRAY && tangent_type != Variant::NIL, ERR_INVALID_PARAMETER);

						// We need a different version if using double precision tangents.
						if (tangent_type == Variant::PACKED_FLOAT32_ARRAY) {
							Vector<float> tangent_array = p_arrays[RS::ARRAY_TANGENT];
							ERR_FAIL_COND_V(tangent_array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);
							const float *tangent_src = tangent_array.ptr();

							// Set data for vertex, normal, and tangent.
							for (int i = 0; i < p_vertex_array_len; i++) {
								float angle = 0.0;
								Vector3 axis;
								Vector4 tangent = Vector4(tangent_src[i * 4 + 0], tangent_src[i * 4 + 1], tangent_src[i * 4 + 2], tangent_src[i * 4 + 3]);
								_get_axis_angle(normal_src[i], tangent, angle, axis);

								// Store axis.
								{
									Vector2 res = axis.octahedron_encode();
									uint16_t vector[2] = {
										(uint16_t)CLAMP(res.x * 65535, 0, 65535),
										(uint16_t)CLAMP(res.y * 65535, 0, 65535),
									};

									memcpy(&vw[p_offsets[RS::ARRAY_NORMAL] + i * p_normal_stride], vector, 4);
								}

								// Store vertex position + angle.
								{
									Vector3 pos = (src[i] - r_aabb.position) / r_aabb.size;
									uint16_t vector[4] = {
										(uint16_t)CLAMP(pos.x * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.y * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.z * 65535, 0, 65535),
										(uint16_t)CLAMP(angle * 65535, 0, 65535)
									};

									memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(uint16_t) * 4);
								}
							}
						} else if (tangent_type == Variant::PACKED_FLOAT64_ARRAY) {
							Vector<double> tangent_array = p_arrays[RS::ARRAY_TANGENT];
							ERR_FAIL_COND_V(tangent_array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);
							const double *tangent_src = tangent_array.ptr();

							// Set data for vertex, normal, and tangent.
							for (int i = 0; i < p_vertex_array_len; i++) {
								float angle;
								Vector3 axis;
								Vector4 tangent = Vector4(tangent_src[i * 4 + 0], tangent_src[i * 4 + 1], tangent_src[i * 4 + 2], tangent_src[i * 4 + 3]);
								_get_axis_angle(normal_src[i], tangent, angle, axis);

								// Store axis.
								{
									Vector2 res = axis.octahedron_encode();
									uint16_t vector[2] = {
										(uint16_t)CLAMP(res.x * 65535, 0, 65535),
										(uint16_t)CLAMP(res.y * 65535, 0, 65535),
									};

									memcpy(&vw[p_offsets[RS::ARRAY_NORMAL] + i * p_normal_stride], vector, 4);
								}

								// Store vertex position + angle.
								{
									Vector3 pos = (src[i] - r_aabb.position) / r_aabb.size;
									uint16_t vector[4] = {
										(uint16_t)CLAMP(pos.x * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.y * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.z * 65535, 0, 65535),
										(uint16_t)CLAMP(angle * 65535, 0, 65535)
									};

									memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(uint16_t) * 4);
								}
							}
						} else { // No tangent array.
							// Set data for vertex, normal, and tangent.
							for (int i = 0; i < p_vertex_array_len; i++) {
								float angle;
								Vector3 axis;
								// Generate an arbitrary vector that is tangential to normal.
								// This assumes that the normal is never (0,0,0).
								Vector3 tan = Vector3(normal_src[i].z, -normal_src[i].x, normal_src[i].y).cross(normal_src[i].normalized()).normalized();
								Vector4 tangent = Vector4(tan.x, tan.y, tan.z, 1.0);
								_get_axis_angle(normal_src[i], tangent, angle, axis);

								// Store axis.
								{
									Vector2 res = axis.octahedron_encode();
									uint16_t vector[2] = {
										(uint16_t)CLAMP(res.x * 65535, 0, 65535),
										(uint16_t)CLAMP(res.y * 65535, 0, 65535),
									};

									memcpy(&vw[p_offsets[RS::ARRAY_NORMAL] + i * p_normal_stride], vector, 4);
								}

								// Store vertex position + angle.
								{
									Vector3 pos = (src[i] - r_aabb.position) / r_aabb.size;
									uint16_t vector[4] = {
										(uint16_t)CLAMP(pos.x * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.y * 65535, 0, 65535),
										(uint16_t)CLAMP(pos.z * 65535, 0, 65535),
										(uint16_t)CLAMP(angle * 65535, 0, 65535)
									};

									memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(uint16_t) * 4);
								}
							}
						}
					} else {
						for (int i = 0; i < p_vertex_array_len; i++) {
							float vector[3] = { (float)src[i].x, (float)src[i].y, (float)src[i].z };

							memcpy(&vw[p_offsets[ai] + i * p_vertex_stride], vector, sizeof(float) * 3);
						}
					}
				}

			} break;
			case RS::ARRAY_NORMAL: {
				// If using compression we store normal while storing vertices.
				if (!(p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
					ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_VECTOR3_ARRAY, ERR_INVALID_PARAMETER);

					Vector<Vector3> array = p_arrays[ai];
					ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

					const Vector3 *src = array.ptr();
					for (int i = 0; i < p_vertex_array_len; i++) {
						Vector2 res = src[i].octahedron_encode();
						uint16_t vector[2] = {
							(uint16_t)CLAMP(res.x * 65535, 0, 65535),
							(uint16_t)CLAMP(res.y * 65535, 0, 65535),
						};

						memcpy(&vw[p_offsets[ai] + i * p_normal_stride], vector, 4);
					}
				}
			} break;

			case RS::ARRAY_TANGENT: {
				// If using compression we store tangent while storing vertices.
				if (!(p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
					Variant::Type type = p_arrays[ai].get_type();
					ERR_FAIL_COND_V(type != Variant::PACKED_FLOAT32_ARRAY && type != Variant::PACKED_FLOAT64_ARRAY && type != Variant::NIL, ERR_INVALID_PARAMETER);

					if (type == Variant::PACKED_FLOAT32_ARRAY) {
						Vector<float> array = p_arrays[ai];
						ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);
						const float *src_ptr = array.ptr();

						for (int i = 0; i < p_vertex_array_len; i++) {
							const Vector3 src(src_ptr[i * 4 + 0], src_ptr[i * 4 + 1], src_ptr[i * 4 + 2]);
							Vector2 res = src.octahedron_tangent_encode(src_ptr[i * 4 + 3]);
							uint16_t vector[2] = {
								(uint16_t)CLAMP(res.x * 65535, 0, 65535),
								(uint16_t)CLAMP(res.y * 65535, 0, 65535),
							};

							if (vector[0] == 0 && vector[1] == 65535) {
								// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
								// So we sanitize here.
								vector[0] = 65535;
							}

							memcpy(&vw[p_offsets[ai] + i * p_normal_stride], vector, 4);
						}
					} else if (type == Variant::PACKED_FLOAT64_ARRAY) {
						Vector<double> array = p_arrays[ai];
						ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);
						const double *src_ptr = array.ptr();

						for (int i = 0; i < p_vertex_array_len; i++) {
							const Vector3 src(src_ptr[i * 4 + 0], src_ptr[i * 4 + 1], src_ptr[i * 4 + 2]);
							Vector2 res = src.octahedron_tangent_encode(src_ptr[i * 4 + 3]);
							uint16_t vector[2] = {
								(uint16_t)CLAMP(res.x * 65535, 0, 65535),
								(uint16_t)CLAMP(res.y * 65535, 0, 65535),
							};

							if (vector[0] == 0 && vector[1] == 65535) {
								// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
								// So we sanitize here.
								vector[0] = 65535;
							}

							memcpy(&vw[p_offsets[ai] + i * p_normal_stride], vector, 4);
						}
					} else { // No tangent array.
						ERR_FAIL_COND_V(p_arrays[RS::ARRAY_NORMAL].get_type() != Variant::PACKED_VECTOR3_ARRAY, ERR_INVALID_PARAMETER);

						Vector<Vector3> normal_array = p_arrays[RS::ARRAY_NORMAL];
						ERR_FAIL_COND_V(normal_array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);
						const Vector3 *normal_src = normal_array.ptr();
						// Set data for tangent.
						for (int i = 0; i < p_vertex_array_len; i++) {
							// Generate an arbitrary vector that is tangential to normal.
							// This assumes that the normal is never (0,0,0).
							Vector3 tan = Vector3(normal_src[i].z, -normal_src[i].x, normal_src[i].y).cross(normal_src[i].normalized()).normalized();
							Vector2 res = tan.octahedron_tangent_encode(1.0);
							uint16_t vector[2] = {
								(uint16_t)CLAMP(res.x * 65535, 0, 65535),
								(uint16_t)CLAMP(res.y * 65535, 0, 65535),
							};

							if (vector[0] == 0 && vector[1] == 65535) {
								// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
								// So we sanitize here.
								vector[0] = 65535;
							}

							memcpy(&vw[p_offsets[ai] + i * p_normal_stride], vector, 4);
						}
					}
				}
			} break;
			case RS::ARRAY_COLOR: {
				ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_COLOR_ARRAY, ERR_INVALID_PARAMETER);

				Vector<Color> array = p_arrays[ai];

				ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

				const Color *src = array.ptr();
				for (int i = 0; i < p_vertex_array_len; i++) {
					uint8_t color8[4] = {
						uint8_t(CLAMP(src[i].r * 255.0, 0.0, 255.0)),
						uint8_t(CLAMP(src[i].g * 255.0, 0.0, 255.0)),
						uint8_t(CLAMP(src[i].b * 255.0, 0.0, 255.0)),
						uint8_t(CLAMP(src[i].a * 255.0, 0.0, 255.0))
					};
					memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], color8, 4);
				}
			} break;
			case RS::ARRAY_TEX_UV: {
				ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_VECTOR3_ARRAY && p_arrays[ai].get_type() != Variant::PACKED_VECTOR2_ARRAY, ERR_INVALID_PARAMETER);

				Vector<Vector2> array = p_arrays[ai];

				ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

				const Vector2 *src = array.ptr();
				if (p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					for (int i = 0; i < p_vertex_array_len; i++) {
						Vector2 vec = src[i];
						if (!r_uv_scale.is_zero_approx()) {
							// Normalize into 0-1 from possible range -uv_scale - uv_scale.
							vec = vec / (Vector2(r_uv_scale.x, r_uv_scale.y)) + Vector2(0.5, 0.5);
						}

						uint16_t uv[2] = { (uint16_t)CLAMP(vec.x * 65535, 0, 65535), (uint16_t)CLAMP(vec.y * 65535, 0, 65535) };
						memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], uv, 4);
					}
				} else {
					for (int i = 0; i < p_vertex_array_len; i++) {
						float uv[2] = { (float)src[i].x, (float)src[i].y };
						memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], uv, 2 * 4);
					}
				}
			} break;

			case RS::ARRAY_TEX_UV2: {
				ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_VECTOR3_ARRAY && p_arrays[ai].get_type() != Variant::PACKED_VECTOR2_ARRAY, ERR_INVALID_PARAMETER);

				Vector<Vector2> array = p_arrays[ai];

				ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

				const Vector2 *src = array.ptr();

				if (p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					for (int i = 0; i < p_vertex_array_len; i++) {
						Vector2 vec = src[i];
						if (!r_uv_scale.is_zero_approx()) {
							// Normalize into 0-1 from possible range -uv_scale - uv_scale.
							vec = vec / (Vector2(r_uv_scale.z, r_uv_scale.w)) + Vector2(0.5, 0.5);
						}
						uint16_t uv[2] = { (uint16_t)CLAMP(vec.x * 65535, 0, 65535), (uint16_t)CLAMP(vec.y * 65535, 0, 65535) };
						memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], uv, 4);
					}
				} else {
					for (int i = 0; i < p_vertex_array_len; i++) {
						float uv[2] = { (float)src[i].x, (float)src[i].y };
						memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], uv, 2 * 4);
					}
				}
			} break;
			case RS::ARRAY_CUSTOM0:
			case RS::ARRAY_CUSTOM1:
			case RS::ARRAY_CUSTOM2:
			case RS::ARRAY_CUSTOM3: {
				uint32_t type = (p_format >> (ARRAY_FORMAT_CUSTOM_BASE + ARRAY_FORMAT_CUSTOM_BITS * (ai - RS::ARRAY_CUSTOM0))) & ARRAY_FORMAT_CUSTOM_MASK;
				switch (type) {
					case ARRAY_CUSTOM_RGBA8_UNORM:
					case ARRAY_CUSTOM_RGBA8_SNORM:
					case ARRAY_CUSTOM_RG_HALF: {
						// Size 4
						ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_BYTE_ARRAY, ERR_INVALID_PARAMETER);

						Vector<uint8_t> array = p_arrays[ai];

						ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);

						const uint8_t *src = array.ptr();

						for (int i = 0; i < p_vertex_array_len; i++) {
							memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], &src[i * 4], 4);
						}

					} break;
					case ARRAY_CUSTOM_RGBA_HALF: {
						// Size 8
						ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_BYTE_ARRAY, ERR_INVALID_PARAMETER);

						Vector<uint8_t> array = p_arrays[ai];

						ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 8, ERR_INVALID_PARAMETER);

						const uint8_t *src = array.ptr();

						for (int i = 0; i < p_vertex_array_len; i++) {
							memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], &src[i * 8], 8);
						}
					} break;
					case ARRAY_CUSTOM_R_FLOAT:
					case ARRAY_CUSTOM_RG_FLOAT:
					case ARRAY_CUSTOM_RGB_FLOAT:
					case ARRAY_CUSTOM_RGBA_FLOAT: {
						// RF
						ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_FLOAT32_ARRAY, ERR_INVALID_PARAMETER);

						Vector<float> array = p_arrays[ai];
						int32_t s = type - ARRAY_CUSTOM_R_FLOAT + 1;

						ERR_FAIL_COND_V(array.size() != p_vertex_array_len * s, ERR_INVALID_PARAMETER);

						const float *src = array.ptr();

						for (int i = 0; i < p_vertex_array_len; i++) {
							memcpy(&aw[p_offsets[ai] + i * p_attrib_stride], &src[i * s], sizeof(float) * s);
						}
					} break;
					default: {
					}
				}

			} break;
			case RS::ARRAY_WEIGHTS: {
				Variant::Type type = p_arrays[ai].get_type();
				ERR_FAIL_COND_V(type != Variant::PACKED_FLOAT32_ARRAY && type != Variant::PACKED_FLOAT64_ARRAY, ERR_INVALID_PARAMETER);
				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;
				if (type == Variant::PACKED_FLOAT32_ARRAY) {
					Vector<float> array = p_arrays[ai];
					ERR_FAIL_COND_V(array.size() != (int32_t)(p_vertex_array_len * bone_count), ERR_INVALID_PARAMETER);
					const float *src = array.ptr();
					{
						uint16_t data[8];
						for (int i = 0; i < p_vertex_array_len; i++) {
							for (uint32_t j = 0; j < bone_count; j++) {
								data[j] = CLAMP(src[i * bone_count + j] * 65535, 0, 65535);
							}

							memcpy(&sw[p_offsets[ai] + i * p_skin_stride], data, 2 * bone_count);
						}
					}
				} else { // PACKED_FLOAT64_ARRAY
					Vector<double> array = p_arrays[ai];
					ERR_FAIL_COND_V(array.size() != (int32_t)(p_vertex_array_len * bone_count), ERR_INVALID_PARAMETER);
					const double *src = array.ptr();
					{
						uint16_t data[8];
						for (int i = 0; i < p_vertex_array_len; i++) {
							for (uint32_t j = 0; j < bone_count; j++) {
								data[j] = CLAMP(src[i * bone_count + j] * 65535, 0, 65535);
							}

							memcpy(&sw[p_offsets[ai] + i * p_skin_stride], data, 2 * bone_count);
						}
					}
				}
			} break;
			case RS::ARRAY_BONES: {
				ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_INT32_ARRAY && p_arrays[ai].get_type() != Variant::PACKED_FLOAT32_ARRAY, ERR_INVALID_PARAMETER);

				Vector<int> array = p_arrays[ai];

				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;

				ERR_FAIL_COND_V(array.size() != (int32_t)(p_vertex_array_len * bone_count), ERR_INVALID_PARAMETER);

				const int *src = array.ptr();

				uint16_t data[8];

				for (int i = 0; i < p_vertex_array_len; i++) {
					for (uint32_t j = 0; j < bone_count; j++) {
						data[j] = src[i * bone_count + j];
						max_bone = MAX(data[j], max_bone);
					}

					memcpy(&sw[p_offsets[ai] + i * p_skin_stride], data, 2 * bone_count);
				}

			} break;

			case RS::ARRAY_INDEX: {
				ERR_FAIL_NULL_V(iw, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(p_index_array_len <= 0, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(p_arrays[ai].get_type() != Variant::PACKED_INT32_ARRAY, ERR_INVALID_PARAMETER);

				Vector<int> indices = p_arrays[ai];
				ERR_FAIL_COND_V(indices.is_empty(), ERR_INVALID_PARAMETER);
				ERR_FAIL_COND_V(indices.size() != p_index_array_len, ERR_INVALID_PARAMETER);

				/* determine whether using 16 or 32 bits indices */

				const int *src = indices.ptr();

				for (int i = 0; i < p_index_array_len; i++) {
					if (p_vertex_array_len <= (1 << 16) && p_vertex_array_len > 0) {
						uint16_t v = src[i];

						memcpy(&iw[i * 2], &v, 2);
					} else {
						uint32_t v = src[i];

						memcpy(&iw[i * 4], &v, 4);
					}
				}
			} break;
			default: {
				ERR_FAIL_V(ERR_INVALID_DATA);
			}
		}
	}

	if (p_format & RS::ARRAY_FORMAT_BONES) {
		// Create AABBs for each detected bone.
		int total_bones = max_bone + 1;

		bool first = r_bone_aabb.size() == 0;

		r_bone_aabb.resize(total_bones);

		int weight_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;

		if (first) {
			for (int i = 0; i < total_bones; i++) {
				r_bone_aabb.write[i].size = Vector3(-1, -1, -1); // Negative means unused.
			}
		}

		Vector<Vector3> vertices = p_arrays[RS::ARRAY_VERTEX];
		Vector<int> bones = p_arrays[RS::ARRAY_BONES];
		Vector<float> weights = p_arrays[RS::ARRAY_WEIGHTS];

		bool any_valid = false;

		if (vertices.size() && bones.size() == vertices.size() * weight_count && weights.size() == bones.size()) {
			int vs = vertices.size();
			const Vector3 *rv = vertices.ptr();
			const int *rb = bones.ptr();
			const float *rw = weights.ptr();

			AABB *bptr = r_bone_aabb.ptrw();

			for (int i = 0; i < vs; i++) {
				Vector3 v = rv[i];
				for (int j = 0; j < weight_count; j++) {
					int idx = rb[i * weight_count + j];
					float w = rw[i * weight_count + j];
					if (w == 0) {
						continue; //break;
					}
					ERR_FAIL_INDEX_V(idx, total_bones, ERR_INVALID_DATA);

					if (bptr[idx].size.x < 0) {
						// First
						bptr[idx] = AABB(v, SMALL_VEC3);
						any_valid = true;
					} else {
						bptr[idx].expand_to(v);
					}
				}
			}
		}

		if (!any_valid && first) {
			r_bone_aabb.clear();
		}
	}
	return OK;
}

uint32_t RenderingServer::mesh_surface_get_format_offset(BitField<ArrayFormat> p_format, int p_vertex_len, int p_array_index) const {
	ERR_FAIL_INDEX_V(p_array_index, ARRAY_MAX, 0);
	p_format = uint64_t(p_format) & ~ARRAY_FORMAT_INDEX;
	uint32_t offsets[ARRAY_MAX];
	uint32_t vstr;
	uint32_t ntstr;
	uint32_t astr;
	uint32_t sstr;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, 0, offsets, vstr, ntstr, astr, sstr);
	return offsets[p_array_index];
}

uint32_t RenderingServer::mesh_surface_get_format_vertex_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const {
	p_format = uint64_t(p_format) & ~ARRAY_FORMAT_INDEX;
	uint32_t offsets[ARRAY_MAX];
	uint32_t vstr;
	uint32_t ntstr;
	uint32_t astr;
	uint32_t sstr;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, 0, offsets, vstr, ntstr, astr, sstr);
	return vstr;
}

uint32_t RenderingServer::mesh_surface_get_format_normal_tangent_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const {
	p_format = uint64_t(p_format) & ~ARRAY_FORMAT_INDEX;
	uint32_t offsets[ARRAY_MAX];
	uint32_t vstr;
	uint32_t ntstr;
	uint32_t astr;
	uint32_t sstr;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, 0, offsets, vstr, ntstr, astr, sstr);
	return ntstr;
}

uint32_t RenderingServer::mesh_surface_get_format_attribute_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const {
	p_format = uint64_t(p_format) & ~ARRAY_FORMAT_INDEX;
	uint32_t offsets[ARRAY_MAX];
	uint32_t vstr;
	uint32_t ntstr;
	uint32_t astr;
	uint32_t sstr;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, 0, offsets, vstr, ntstr, astr, sstr);
	return astr;
}
uint32_t RenderingServer::mesh_surface_get_format_skin_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const {
	p_format = uint64_t(p_format) & ~ARRAY_FORMAT_INDEX;
	uint32_t offsets[ARRAY_MAX];
	uint32_t vstr;
	uint32_t ntstr;
	uint32_t astr;
	uint32_t sstr;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, 0, offsets, vstr, ntstr, astr, sstr);
	return sstr;
}

void RenderingServer::mesh_surface_make_offsets_from_format(uint64_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets, uint32_t &r_vertex_element_size, uint32_t &r_normal_element_size, uint32_t &r_attrib_element_size, uint32_t &r_skin_element_size) const {
	r_vertex_element_size = 0;
	r_normal_element_size = 0;
	r_attrib_element_size = 0;
	r_skin_element_size = 0;

	uint32_t *size_accum = nullptr;

	for (int i = 0; i < RS::ARRAY_MAX; i++) {
		r_offsets[i] = 0; // Reset

		if (i == RS::ARRAY_VERTEX) {
			size_accum = &r_vertex_element_size;
		} else if (i == RS::ARRAY_NORMAL) {
			size_accum = &r_normal_element_size;
		} else if (i == RS::ARRAY_COLOR) {
			size_accum = &r_attrib_element_size;
		} else if (i == RS::ARRAY_BONES) {
			size_accum = &r_skin_element_size;
		}

		if (!(p_format & (1ULL << i))) { // No array
			continue;
		}

		int elem_size = 0;

		switch (i) {
			case RS::ARRAY_VERTEX: {
				if (p_format & ARRAY_FLAG_USE_2D_VERTICES) {
					elem_size = 2;
				} else {
					elem_size = (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) ? 2 : 3;
				}

				elem_size *= sizeof(float);
			} break;
			case RS::ARRAY_NORMAL: {
				elem_size = 4;
			} break;
			case RS::ARRAY_TANGENT: {
				elem_size = (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) ? 0 : 4;
			} break;
			case RS::ARRAY_COLOR: {
				elem_size = 4;
			} break;
			case RS::ARRAY_TEX_UV: {
				elem_size = (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) ? 4 : 8;
			} break;
			case RS::ARRAY_TEX_UV2: {
				elem_size = (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) ? 4 : 8;
			} break;
			case RS::ARRAY_CUSTOM0:
			case RS::ARRAY_CUSTOM1:
			case RS::ARRAY_CUSTOM2:
			case RS::ARRAY_CUSTOM3: {
				uint64_t format = (p_format >> (ARRAY_FORMAT_CUSTOM_BASE + (ARRAY_FORMAT_CUSTOM_BITS * (i - ARRAY_CUSTOM0)))) & ARRAY_FORMAT_CUSTOM_MASK;
				switch (format) {
					case ARRAY_CUSTOM_RGBA8_UNORM: {
						elem_size = 4;
					} break;
					case ARRAY_CUSTOM_RGBA8_SNORM: {
						elem_size = 4;
					} break;
					case ARRAY_CUSTOM_RG_HALF: {
						elem_size = 4;
					} break;
					case ARRAY_CUSTOM_RGBA_HALF: {
						elem_size = 8;
					} break;
					case ARRAY_CUSTOM_R_FLOAT: {
						elem_size = 4;
					} break;
					case ARRAY_CUSTOM_RG_FLOAT: {
						elem_size = 8;
					} break;
					case ARRAY_CUSTOM_RGB_FLOAT: {
						elem_size = 12;
					} break;
					case ARRAY_CUSTOM_RGBA_FLOAT: {
						elem_size = 16;
					} break;
				}
			} break;
			case RS::ARRAY_WEIGHTS: {
				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;
				elem_size = sizeof(uint16_t) * bone_count;

			} break;
			case RS::ARRAY_BONES: {
				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;
				elem_size = sizeof(uint16_t) * bone_count;
			} break;
			case RS::ARRAY_INDEX: {
				if (p_index_len <= 0) {
					ERR_PRINT("index_array_len==NO_INDEX_ARRAY");
					break;
				}
				/* determine whether using 16 or 32 bits indices */
				if (p_vertex_len <= (1 << 16) && p_vertex_len > 0) {
					elem_size = 2;
				} else {
					elem_size = 4;
				}
				r_offsets[i] = elem_size;
				continue;
			}
			default: {
				ERR_FAIL();
			}
		}

		if (size_accum != nullptr) {
			r_offsets[i] = (*size_accum);
			if (i == RS::ARRAY_NORMAL || i == RS::ARRAY_TANGENT) {
				r_offsets[i] += r_vertex_element_size * p_vertex_len;
			}
			(*size_accum) += elem_size;
		} else {
			r_offsets[i] = 0;
		}
	}
}

Error RenderingServer::mesh_create_surface_data_from_arrays(SurfaceData *r_surface_data, PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes, const Dictionary &p_lods, uint64_t p_compress_format) {
	ERR_FAIL_INDEX_V(p_primitive, RS::PRIMITIVE_MAX, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_arrays.size() != RS::ARRAY_MAX, ERR_INVALID_PARAMETER);

	uint64_t format = 0;

	// Validation
	int index_array_len = 0;
	int array_len = 0;

	for (int i = 0; i < p_arrays.size(); i++) {
		if (p_arrays[i].get_type() == Variant::NIL) {
			continue;
		}

		format |= (1ULL << i);

		if (i == RS::ARRAY_VERTEX) {
			switch (p_arrays[i].get_type()) {
				case Variant::PACKED_VECTOR2_ARRAY: {
					Vector<Vector2> v2 = p_arrays[i];
					array_len = v2.size();
					format |= ARRAY_FLAG_USE_2D_VERTICES;
				} break;
				case Variant::PACKED_VECTOR3_ARRAY: {
					ERR_FAIL_COND_V(p_compress_format & ARRAY_FLAG_USE_2D_VERTICES, ERR_INVALID_PARAMETER);
					Vector<Vector3> v3 = p_arrays[i];
					array_len = v3.size();
				} break;
				default: {
					ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Vertex array must be a PackedVector2Array or PackedVector3Array.");
				} break;
			}
			ERR_FAIL_COND_V(array_len == 0, ERR_INVALID_DATA);
		} else if (i == RS::ARRAY_NORMAL) {
			if (p_arrays[RS::ARRAY_TANGENT].get_type() == Variant::NIL) {
				// We must use tangents if using normals.
				format |= (1ULL << RS::ARRAY_TANGENT);
			}
		} else if (i == RS::ARRAY_BONES) {
			switch (p_arrays[i].get_type()) {
				case Variant::PACKED_INT32_ARRAY: {
					Vector<Vector3> vertices = p_arrays[RS::ARRAY_VERTEX];
					Vector<int32_t> bones = p_arrays[i];
					int32_t bone_8_group_count = bones.size() / (ARRAY_WEIGHTS_SIZE * 2);
					int32_t vertex_count = vertices.size();
					if (vertex_count == bone_8_group_count) {
						format |= RS::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
					}
				} break;
				default: {
					ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Bones array must be a PackedInt32Array.");
				} break;
			}
		} else if (i == RS::ARRAY_INDEX) {
			index_array_len = PackedInt32Array(p_arrays[i]).size();
		}
	}

	if (p_blend_shapes.size()) {
		// Validate format for morphs.
		for (int i = 0; i < p_blend_shapes.size(); i++) {
			uint32_t bsformat = 0;
			Array arr = p_blend_shapes[i];
			for (int j = 0; j < arr.size(); j++) {
				if (arr[j].get_type() != Variant::NIL) {
					bsformat |= (1 << j);
				}
			}
			if (bsformat & RS::ARRAY_FORMAT_NORMAL) {
				// We must use tangents if using normals.
				bsformat |= RS::ARRAY_FORMAT_TANGENT;
			}

			ERR_FAIL_COND_V_MSG(bsformat != (format & RS::ARRAY_FORMAT_BLEND_SHAPE_MASK), ERR_INVALID_PARAMETER, "Blend shape format must match the main array format for Vertex, Normal and Tangent arrays.");
		}
	}

	for (uint32_t i = 0; i < RS::ARRAY_CUSTOM_COUNT; ++i) {
		// Include custom array format type.
		if (format & (1ULL << (ARRAY_CUSTOM0 + i))) {
			format |= (RS::ARRAY_FORMAT_CUSTOM_MASK << (RS::ARRAY_FORMAT_CUSTOM_BASE + i * RS::ARRAY_FORMAT_CUSTOM_BITS)) & p_compress_format;
		}
	}

	uint32_t offsets[RS::ARRAY_MAX];

	uint32_t vertex_element_size;
	uint32_t normal_element_size;
	uint32_t attrib_element_size;
	uint32_t skin_element_size;

	uint64_t mask = (1ULL << ARRAY_MAX) - 1ULL;
	format |= (~mask) & p_compress_format; // Make the full format.

	// Force version to the current version as this function will always return a surface with the current version.
	format &= ~(ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	format |= ARRAY_FLAG_FORMAT_CURRENT_VERSION & (ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);

	mesh_surface_make_offsets_from_format(format, array_len, index_array_len, offsets, vertex_element_size, normal_element_size, attrib_element_size, skin_element_size);

	if ((format & RS::ARRAY_FORMAT_VERTEX) == 0 && !(format & RS::ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY)) {
		ERR_PRINT("Mesh created without vertex array. This mesh will not be visible with the default shader. If using an empty vertex array is intentional, create the mesh with the ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY flag to silence this error.");
		// Set the flag here after warning to suppress errors down the pipeline.
		format |= RS::ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY;
	}

	if ((format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && ((format & RS::ARRAY_FORMAT_NORMAL) || (format & RS::ARRAY_FORMAT_TANGENT))) {
		// If using normals or tangents, then we need all three.
		ERR_FAIL_COND_V_MSG(!(format & RS::ARRAY_FORMAT_VERTEX), ERR_INVALID_PARAMETER, "Can't use compression flag 'ARRAY_FLAG_COMPRESS_ATTRIBUTES' while using normals or tangents without vertex array.");
		ERR_FAIL_COND_V_MSG(!(format & RS::ARRAY_FORMAT_NORMAL), ERR_INVALID_PARAMETER, "Can't use compression flag 'ARRAY_FLAG_COMPRESS_ATTRIBUTES' while using tangents without normal array.");
	}

	int vertex_array_size = (vertex_element_size + normal_element_size) * array_len;
	int attrib_array_size = attrib_element_size * array_len;
	int skin_array_size = skin_element_size * array_len;
	int index_array_size = offsets[RS::ARRAY_INDEX] * index_array_len;

	Vector<uint8_t> vertex_array;
	vertex_array.resize(vertex_array_size);

	Vector<uint8_t> attrib_array;
	attrib_array.resize(attrib_array_size);

	Vector<uint8_t> skin_array;
	skin_array.resize(skin_array_size);

	Vector<uint8_t> index_array;
	index_array.resize(index_array_size);

	AABB aabb;
	Vector<AABB> bone_aabb;

	Vector4 uv_scale = Vector4(0.0, 0.0, 0.0, 0.0);

	Error err = _surface_set_data(p_arrays, format, offsets, vertex_element_size, normal_element_size, attrib_element_size, skin_element_size, vertex_array, attrib_array, skin_array, array_len, index_array, index_array_len, aabb, bone_aabb, uv_scale);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_INVALID_DATA, "Invalid array format for surface.");

	Vector<uint8_t> blend_shape_data;

	if (p_blend_shapes.size()) {
		uint32_t bs_format = format & RS::ARRAY_FORMAT_BLEND_SHAPE_MASK;
		for (int i = 0; i < p_blend_shapes.size(); i++) {
			Vector<uint8_t> vertex_array_shape;
			vertex_array_shape.resize(vertex_array_size);
			Vector<uint8_t> noindex;
			Vector<uint8_t> noattrib;
			Vector<uint8_t> noskin;

			AABB laabb;
			Vector4 bone_uv_scale; // Not used.
			Error err2 = _surface_set_data(p_blend_shapes[i], bs_format, offsets, vertex_element_size, normal_element_size, 0, 0, vertex_array_shape, noattrib, noskin, array_len, noindex, 0, laabb, bone_aabb, bone_uv_scale);
			aabb.merge_with(laabb);
			ERR_FAIL_COND_V_MSG(err2 != OK, ERR_INVALID_DATA, "Invalid blend shape array format for surface.");

			blend_shape_data.append_array(vertex_array_shape);
		}
	}
	Vector<SurfaceData::LOD> lods;
	if (index_array_len) {
		List<Variant> keys;
		p_lods.get_key_list(&keys);
		keys.sort(); // otherwise lod levels may get skipped
		for (const Variant &E : keys) {
			float distance = E;
			ERR_CONTINUE(distance <= 0.0);
			Vector<int> indices = p_lods[E];
			ERR_CONTINUE(indices.is_empty());
			uint32_t index_count = indices.size();
			ERR_CONTINUE(index_count >= (uint32_t)index_array_len); // Should be smaller..

			const int *r = indices.ptr();

			Vector<uint8_t> data;
			if (array_len <= 65536) {
				// 16 bits indices
				data.resize(indices.size() * 2);
				uint8_t *w = data.ptrw();
				uint16_t *index_ptr = (uint16_t *)w;
				for (uint32_t i = 0; i < index_count; i++) {
					index_ptr[i] = r[i];
				}
			} else {
				// 32 bits indices
				data.resize(indices.size() * 4);
				uint8_t *w = data.ptrw();
				uint32_t *index_ptr = (uint32_t *)w;
				for (uint32_t i = 0; i < index_count; i++) {
					index_ptr[i] = r[i];
				}
			}

			SurfaceData::LOD lod;
			lod.edge_length = distance;
			lod.index_data = data;
			lods.push_back(lod);
		}
	}

	SurfaceData &surface_data = *r_surface_data;
	surface_data.format = format;
	surface_data.primitive = p_primitive;
	surface_data.aabb = aabb;
	surface_data.vertex_data = vertex_array;
	surface_data.attribute_data = attrib_array;
	surface_data.skin_data = skin_array;
	surface_data.vertex_count = array_len;
	surface_data.index_data = index_array;
	surface_data.index_count = index_array_len;
	surface_data.blend_shape_data = blend_shape_data;
	surface_data.bone_aabbs = bone_aabb;
	surface_data.lods = lods;
	surface_data.uv_scale = uv_scale;

	return OK;
}

void RenderingServer::mesh_add_surface_from_arrays(RID p_mesh, PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes, const Dictionary &p_lods, BitField<ArrayFormat> p_compress_format) {
	SurfaceData sd;
	Error err = mesh_create_surface_data_from_arrays(&sd, p_primitive, p_arrays, p_blend_shapes, p_lods, p_compress_format);
	if (err != OK) {
		return;
	}
	mesh_add_surface(p_mesh, sd);
}

Array RenderingServer::_get_array_from_surface(uint64_t p_format, Vector<uint8_t> p_vertex_data, Vector<uint8_t> p_attrib_data, Vector<uint8_t> p_skin_data, int p_vertex_len, Vector<uint8_t> p_index_data, int p_index_len, const AABB &p_aabb, const Vector4 &p_uv_scale) const {
	uint32_t offsets[RS::ARRAY_MAX];

	uint32_t vertex_elem_size;
	uint32_t normal_elem_size;
	uint32_t attrib_elem_size;
	uint32_t skin_elem_size;
	mesh_surface_make_offsets_from_format(p_format, p_vertex_len, p_index_len, offsets, vertex_elem_size, normal_elem_size, attrib_elem_size, skin_elem_size);

	Array ret;
	ret.resize(RS::ARRAY_MAX);

	const uint8_t *r = p_vertex_data.ptr();
	const uint8_t *ar = p_attrib_data.ptr();
	const uint8_t *sr = p_skin_data.ptr();

	for (int i = 0; i < RS::ARRAY_MAX; i++) {
		if (!(p_format & (1ULL << i))) {
			continue;
		}

		switch (i) {
			case RS::ARRAY_VERTEX: {
				if (p_format & ARRAY_FLAG_USE_2D_VERTICES) {
					Vector<Vector2> arr_2d;
					arr_2d.resize(p_vertex_len);

					{
						Vector2 *w = arr_2d.ptrw();

						for (int j = 0; j < p_vertex_len; j++) {
							const float *v = reinterpret_cast<const float *>(&r[j * vertex_elem_size + offsets[i]]);
							w[j] = Vector2(v[0], v[1]);
						}
					}

					ret[i] = arr_2d;
				} else {
					Vector<Vector3> arr_3d;
					arr_3d.resize(p_vertex_len);

					{
						Vector3 *w = arr_3d.ptrw();

						if (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
							// We only have vertices to read, so just read them and skip everything else.
							if (!(p_format & RS::ARRAY_FORMAT_NORMAL)) {
								for (int j = 0; j < p_vertex_len; j++) {
									const uint16_t *v = reinterpret_cast<const uint16_t *>(&r[j * vertex_elem_size + offsets[i]]);
									Vector3 vec = Vector3(float(v[0]) / 65535.0, float(v[1]) / 65535.0, float(v[2]) / 65535.0);
									w[j] = (vec * p_aabb.size) + p_aabb.position;
								}
								continue;
							}

							Vector<Vector3> normals;
							normals.resize(p_vertex_len);
							Vector3 *normalsw = normals.ptrw();

							Vector<float> tangents;
							tangents.resize(p_vertex_len * 4);
							float *tangentsw = tangents.ptrw();

							for (int j = 0; j < p_vertex_len; j++) {
								const uint32_t n = *(const uint32_t *)&r[j * normal_elem_size + offsets[RS::ARRAY_NORMAL]];
								Vector3 axis = Vector3::octahedron_decode(Vector2((n & 0xFFFF) / 65535.0, ((n >> 16) & 0xFFFF) / 65535.0));

								const uint16_t *v = reinterpret_cast<const uint16_t *>(&r[j * vertex_elem_size + offsets[i]]);
								Vector3 vec = Vector3(float(v[0]) / 65535.0, float(v[1]) / 65535.0, float(v[2]) / 65535.0);
								float angle = float(v[3]) / 65535.0;
								w[j] = (vec * p_aabb.size) + p_aabb.position;

								Vector3 normal;
								Vector4 tan;
								_get_tbn_from_axis_angle(axis, angle, normal, tan);

								normalsw[j] = normal;
								tangentsw[j * 4 + 0] = tan.x;
								tangentsw[j * 4 + 1] = tan.y;
								tangentsw[j * 4 + 2] = tan.z;
								tangentsw[j * 4 + 3] = tan.w;
							}
							ret[RS::ARRAY_NORMAL] = normals;
							ret[RS::ARRAY_TANGENT] = tangents;

						} else {
							for (int j = 0; j < p_vertex_len; j++) {
								const float *v = reinterpret_cast<const float *>(&r[j * vertex_elem_size + offsets[i]]);
								w[j] = Vector3(v[0], v[1], v[2]);
							}
						}
					}

					ret[i] = arr_3d;
				}

			} break;
			case RS::ARRAY_NORMAL: {
				if (!(p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
					Vector<Vector3> arr;
					arr.resize(p_vertex_len);

					Vector3 *w = arr.ptrw();

					for (int j = 0; j < p_vertex_len; j++) {
						const uint32_t v = *(const uint32_t *)&r[j * normal_elem_size + offsets[i]];

						w[j] = Vector3::octahedron_decode(Vector2((v & 0xFFFF) / 65535.0, ((v >> 16) & 0xFFFF) / 65535.0));
					}

					ret[i] = arr;
				}
			} break;

			case RS::ARRAY_TANGENT: {
				if (!(p_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
					Vector<float> arr;
					arr.resize(p_vertex_len * 4);

					float *w = arr.ptrw();

					for (int j = 0; j < p_vertex_len; j++) {
						const uint32_t v = *(const uint32_t *)&r[j * normal_elem_size + offsets[i]];
						float tangent_sign;
						Vector3 res = Vector3::octahedron_tangent_decode(Vector2((v & 0xFFFF) / 65535.0, ((v >> 16) & 0xFFFF) / 65535.0), &tangent_sign);
						w[j * 4 + 0] = res.x;
						w[j * 4 + 1] = res.y;
						w[j * 4 + 2] = res.z;
						w[j * 4 + 3] = tangent_sign;
					}

					ret[i] = arr;
				}
			} break;
			case RS::ARRAY_COLOR: {
				Vector<Color> arr;
				arr.resize(p_vertex_len);

				Color *w = arr.ptrw();

				for (int32_t j = 0; j < p_vertex_len; j++) {
					const uint8_t *v = reinterpret_cast<const uint8_t *>(&ar[j * attrib_elem_size + offsets[i]]);

					w[j] = Color(v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, v[3] / 255.0);
				}

				ret[i] = arr;
			} break;
			case RS::ARRAY_TEX_UV: {
				Vector<Vector2> arr;
				arr.resize(p_vertex_len);

				Vector2 *w = arr.ptrw();
				if (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					for (int j = 0; j < p_vertex_len; j++) {
						const uint16_t *v = reinterpret_cast<const uint16_t *>(&ar[j * attrib_elem_size + offsets[i]]);
						Vector2 vec = Vector2(float(v[0]) / 65535.0, float(v[1]) / 65535.0);
						if (!p_uv_scale.is_zero_approx()) {
							vec = (vec - Vector2(0.5, 0.5)) * Vector2(p_uv_scale.x, p_uv_scale.y);
						}

						w[j] = vec;
					}
				} else {
					for (int j = 0; j < p_vertex_len; j++) {
						const float *v = reinterpret_cast<const float *>(&ar[j * attrib_elem_size + offsets[i]]);
						w[j] = Vector2(v[0], v[1]);
					}
				}
				ret[i] = arr;
			} break;

			case RS::ARRAY_TEX_UV2: {
				Vector<Vector2> arr;
				arr.resize(p_vertex_len);

				Vector2 *w = arr.ptrw();

				if (p_format & ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					for (int j = 0; j < p_vertex_len; j++) {
						const uint16_t *v = reinterpret_cast<const uint16_t *>(&ar[j * attrib_elem_size + offsets[i]]);
						Vector2 vec = Vector2(float(v[0]) / 65535.0, float(v[1]) / 65535.0);
						if (!p_uv_scale.is_zero_approx()) {
							vec = (vec - Vector2(0.5, 0.5)) * Vector2(p_uv_scale.z, p_uv_scale.w);
						}
						w[j] = vec;
					}
				} else {
					for (int j = 0; j < p_vertex_len; j++) {
						const float *v = reinterpret_cast<const float *>(&ar[j * attrib_elem_size + offsets[i]]);
						w[j] = Vector2(v[0], v[1]);
					}
				}

				ret[i] = arr;

			} break;
			case RS::ARRAY_CUSTOM0:
			case RS::ARRAY_CUSTOM1:
			case RS::ARRAY_CUSTOM2:
			case RS::ARRAY_CUSTOM3: {
				uint32_t type = (p_format >> (ARRAY_FORMAT_CUSTOM_BASE + ARRAY_FORMAT_CUSTOM_BITS * (i - RS::ARRAY_CUSTOM0))) & ARRAY_FORMAT_CUSTOM_MASK;
				switch (type) {
					case ARRAY_CUSTOM_RGBA8_UNORM:
					case ARRAY_CUSTOM_RGBA8_SNORM:
					case ARRAY_CUSTOM_RG_HALF:
					case ARRAY_CUSTOM_RGBA_HALF: {
						// Size 4
						int s = type == ARRAY_CUSTOM_RGBA_HALF ? 8 : 4;
						Vector<uint8_t> arr;
						arr.resize(p_vertex_len * s);

						uint8_t *w = arr.ptrw();

						for (int j = 0; j < p_vertex_len; j++) {
							const uint8_t *v = reinterpret_cast<const uint8_t *>(&ar[j * attrib_elem_size + offsets[i]]);
							memcpy(&w[j * s], v, s);
						}

						ret[i] = arr;

					} break;
					case ARRAY_CUSTOM_R_FLOAT:
					case ARRAY_CUSTOM_RG_FLOAT:
					case ARRAY_CUSTOM_RGB_FLOAT:
					case ARRAY_CUSTOM_RGBA_FLOAT: {
						uint32_t s = type - ARRAY_CUSTOM_R_FLOAT + 1;

						Vector<float> arr;
						arr.resize(s * p_vertex_len);

						float *w = arr.ptrw();

						for (int j = 0; j < p_vertex_len; j++) {
							const float *v = reinterpret_cast<const float *>(&ar[j * attrib_elem_size + offsets[i]]);
							memcpy(&w[j * s], v, s * sizeof(float));
						}
						ret[i] = arr;

					} break;
					default: {
					}
				}

			} break;
			case RS::ARRAY_WEIGHTS: {
				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;

				Vector<float> arr;
				arr.resize(p_vertex_len * bone_count);
				{
					float *w = arr.ptrw();

					for (int j = 0; j < p_vertex_len; j++) {
						const uint16_t *v = (const uint16_t *)&sr[j * skin_elem_size + offsets[i]];
						for (uint32_t k = 0; k < bone_count; k++) {
							w[j * bone_count + k] = float(v[k] / 65535.0);
						}
					}
				}

				ret[i] = arr;

			} break;
			case RS::ARRAY_BONES: {
				uint32_t bone_count = (p_format & ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;

				Vector<int> arr;
				arr.resize(p_vertex_len * bone_count);

				int *w = arr.ptrw();

				for (int j = 0; j < p_vertex_len; j++) {
					const uint16_t *v = (const uint16_t *)&sr[j * skin_elem_size + offsets[i]];
					for (uint32_t k = 0; k < bone_count; k++) {
						w[j * bone_count + k] = v[k];
					}
				}

				ret[i] = arr;

			} break;
			case RS::ARRAY_INDEX: {
				/* determine whether using 16 or 32 bits indices */

				const uint8_t *ir = p_index_data.ptr();

				Vector<int> arr;
				arr.resize(p_index_len);
				if (p_vertex_len <= (1 << 16)) {
					int *w = arr.ptrw();

					for (int j = 0; j < p_index_len; j++) {
						const uint16_t *v = (const uint16_t *)&ir[j * 2];
						w[j] = *v;
					}
				} else {
					int *w = arr.ptrw();

					for (int j = 0; j < p_index_len; j++) {
						const int *v = (const int *)&ir[j * 4];
						w[j] = *v;
					}
				}
				ret[i] = arr;
			} break;
			default: {
				ERR_FAIL_V(ret);
			}
		}
	}

	return ret;
}

Array RenderingServer::mesh_surface_get_arrays(RID p_mesh, int p_surface) const {
	SurfaceData sd = mesh_get_surface(p_mesh, p_surface);
	return mesh_create_arrays_from_surface_data(sd);
}

Dictionary RenderingServer::mesh_surface_get_lods(RID p_mesh, int p_surface) const {
	SurfaceData sd = mesh_get_surface(p_mesh, p_surface);
	ERR_FAIL_COND_V(sd.vertex_count == 0, Dictionary());

	Dictionary ret;

	for (int i = 0; i < sd.lods.size(); i++) {
		Vector<int> lods;
		if (sd.vertex_count <= 65536) {
			uint32_t lc = sd.lods[i].index_data.size() / 2;
			lods.resize(lc);
			const uint8_t *r = sd.lods[i].index_data.ptr();
			const uint16_t *rptr = (const uint16_t *)r;
			int *w = lods.ptrw();
			for (uint32_t j = 0; j < lc; j++) {
				w[j] = rptr[i];
			}
		} else {
			uint32_t lc = sd.lods[i].index_data.size() / 4;
			lods.resize(lc);
			const uint8_t *r = sd.lods[i].index_data.ptr();
			const uint32_t *rptr = (const uint32_t *)r;
			int *w = lods.ptrw();
			for (uint32_t j = 0; j < lc; j++) {
				w[j] = rptr[i];
			}
		}

		ret[sd.lods[i].edge_length] = lods;
	}

	return ret;
}

TypedArray<Array> RenderingServer::mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const {
	SurfaceData sd = mesh_get_surface(p_mesh, p_surface);
	ERR_FAIL_COND_V(sd.vertex_count == 0, Array());

	Vector<uint8_t> blend_shape_data = sd.blend_shape_data;

	if (blend_shape_data.size() > 0) {
		uint32_t bs_offsets[RS::ARRAY_MAX];
		uint32_t bs_format = (sd.format & RS::ARRAY_FORMAT_BLEND_SHAPE_MASK);
		uint32_t vertex_elem_size;
		uint32_t normal_elem_size;
		uint32_t attrib_elem_size;
		uint32_t skin_elem_size;

		mesh_surface_make_offsets_from_format(bs_format, sd.vertex_count, 0, bs_offsets, vertex_elem_size, normal_elem_size, attrib_elem_size, skin_elem_size);

		int divisor = (vertex_elem_size + normal_elem_size) * sd.vertex_count;
		ERR_FAIL_COND_V((blend_shape_data.size() % divisor) != 0, Array());

		uint32_t blend_shape_count = blend_shape_data.size() / divisor;

		ERR_FAIL_COND_V(blend_shape_count != (uint32_t)mesh_get_blend_shape_count(p_mesh), Array());

		TypedArray<Array> blend_shape_array;
		blend_shape_array.resize(mesh_get_blend_shape_count(p_mesh));
		for (uint32_t i = 0; i < blend_shape_count; i++) {
			Vector<uint8_t> bs_data = blend_shape_data.slice(i * divisor, (i + 1) * divisor);
			Vector<uint8_t> unused;
			blend_shape_array.set(i, _get_array_from_surface(bs_format, bs_data, unused, unused, sd.vertex_count, unused, 0, sd.aabb, sd.uv_scale));
		}

		return blend_shape_array;
	} else {
		return TypedArray<Array>();
	}
}

Array RenderingServer::mesh_create_arrays_from_surface_data(const SurfaceData &p_data) const {
	Vector<uint8_t> vertex_data = p_data.vertex_data;
	Vector<uint8_t> attrib_data = p_data.attribute_data;
	Vector<uint8_t> skin_data = p_data.skin_data;

	ERR_FAIL_COND_V(vertex_data.is_empty() && (p_data.format & RS::ARRAY_FORMAT_VERTEX), Array());
	int vertex_len = p_data.vertex_count;

	Vector<uint8_t> index_data = p_data.index_data;
	int index_len = p_data.index_count;

	uint64_t format = p_data.format;

	return _get_array_from_surface(format, vertex_data, attrib_data, skin_data, vertex_len, index_data, index_len, p_data.aabb, p_data.uv_scale);
}
#if 0
Array RenderingServer::_mesh_surface_get_skeleton_aabb_bind(RID p_mesh, int p_surface) const {
	Vector<AABB> vec = RS::get_singleton()->mesh_surface_get_skeleton_aabb(p_mesh, p_surface);
	Array arr;
	for (int i = 0; i < vec.size(); i++) {
		arr[i] = vec[i];
	}
	return arr;
}
#endif

Rect2 RenderingServer::debug_canvas_item_get_rect(RID p_item) {
#ifdef TOOLS_ENABLED
	return _debug_canvas_item_get_rect(p_item);
#else
	return Rect2();
#endif
}

int RenderingServer::global_shader_uniform_type_get_shader_datatype(GlobalShaderParameterType p_type) {
	switch (p_type) {
		case RS::GLOBAL_VAR_TYPE_BOOL:
			return ShaderLanguage::TYPE_BOOL;
		case RS::GLOBAL_VAR_TYPE_BVEC2:
			return ShaderLanguage::TYPE_BVEC2;
		case RS::GLOBAL_VAR_TYPE_BVEC3:
			return ShaderLanguage::TYPE_BVEC3;
		case RS::GLOBAL_VAR_TYPE_BVEC4:
			return ShaderLanguage::TYPE_BVEC4;
		case RS::GLOBAL_VAR_TYPE_INT:
			return ShaderLanguage::TYPE_INT;
		case RS::GLOBAL_VAR_TYPE_IVEC2:
			return ShaderLanguage::TYPE_IVEC2;
		case RS::GLOBAL_VAR_TYPE_IVEC3:
			return ShaderLanguage::TYPE_IVEC3;
		case RS::GLOBAL_VAR_TYPE_IVEC4:
			return ShaderLanguage::TYPE_IVEC4;
		case RS::GLOBAL_VAR_TYPE_RECT2I:
			return ShaderLanguage::TYPE_IVEC4;
		case RS::GLOBAL_VAR_TYPE_UINT:
			return ShaderLanguage::TYPE_UINT;
		case RS::GLOBAL_VAR_TYPE_UVEC2:
			return ShaderLanguage::TYPE_UVEC2;
		case RS::GLOBAL_VAR_TYPE_UVEC3:
			return ShaderLanguage::TYPE_UVEC3;
		case RS::GLOBAL_VAR_TYPE_UVEC4:
			return ShaderLanguage::TYPE_UVEC4;
		case RS::GLOBAL_VAR_TYPE_FLOAT:
			return ShaderLanguage::TYPE_FLOAT;
		case RS::GLOBAL_VAR_TYPE_VEC2:
			return ShaderLanguage::TYPE_VEC2;
		case RS::GLOBAL_VAR_TYPE_VEC3:
			return ShaderLanguage::TYPE_VEC3;
		case RS::GLOBAL_VAR_TYPE_VEC4:
			return ShaderLanguage::TYPE_VEC4;
		case RS::GLOBAL_VAR_TYPE_COLOR:
			return ShaderLanguage::TYPE_VEC4;
		case RS::GLOBAL_VAR_TYPE_RECT2:
			return ShaderLanguage::TYPE_VEC4;
		case RS::GLOBAL_VAR_TYPE_MAT2:
			return ShaderLanguage::TYPE_MAT2;
		case RS::GLOBAL_VAR_TYPE_MAT3:
			return ShaderLanguage::TYPE_MAT3;
		case RS::GLOBAL_VAR_TYPE_MAT4:
			return ShaderLanguage::TYPE_MAT4;
		case RS::GLOBAL_VAR_TYPE_TRANSFORM_2D:
			return ShaderLanguage::TYPE_MAT3;
		case RS::GLOBAL_VAR_TYPE_TRANSFORM:
			return ShaderLanguage::TYPE_MAT4;
		case RS::GLOBAL_VAR_TYPE_SAMPLER2D:
			return ShaderLanguage::TYPE_SAMPLER2D;
		case RS::GLOBAL_VAR_TYPE_SAMPLER2DARRAY:
			return ShaderLanguage::TYPE_SAMPLER2DARRAY;
		case RS::GLOBAL_VAR_TYPE_SAMPLER3D:
			return ShaderLanguage::TYPE_SAMPLER3D;
		case RS::GLOBAL_VAR_TYPE_SAMPLERCUBE:
			return ShaderLanguage::TYPE_SAMPLERCUBE;
		default:
			return ShaderLanguage::TYPE_MAX; // Invalid or not found.
	}
}

RenderingDevice *RenderingServer::get_rendering_device() const {
	// Return the rendering device we're using globally.
	return RenderingDevice::get_singleton();
}

RenderingDevice *RenderingServer::create_local_rendering_device() const {
	RenderingDevice *device = RenderingDevice::get_singleton();
	if (!device) {
		return nullptr;
	}
	return device->create_local_device();
}

static Vector<Ref<Image>> _get_imgvec(const TypedArray<Image> &p_layers) {
	Vector<Ref<Image>> images;
	images.resize(p_layers.size());
	for (int i = 0; i < p_layers.size(); i++) {
		images.write[i] = p_layers[i];
	}
	return images;
}
RID RenderingServer::_texture_2d_layered_create(const TypedArray<Image> &p_layers, TextureLayeredType p_layered_type) {
	return texture_2d_layered_create(_get_imgvec(p_layers), p_layered_type);
}
RID RenderingServer::_texture_3d_create(Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const TypedArray<Image> &p_data) {
	return texture_3d_create(p_format, p_width, p_height, p_depth, p_mipmaps, _get_imgvec(p_data));
}

void RenderingServer::_texture_3d_update(RID p_texture, const TypedArray<Image> &p_data) {
	texture_3d_update(p_texture, _get_imgvec(p_data));
}

TypedArray<Image> RenderingServer::_texture_3d_get(RID p_texture) const {
	Vector<Ref<Image>> images = texture_3d_get(p_texture);
	TypedArray<Image> ret;
	ret.resize(images.size());
	for (int i = 0; i < images.size(); i++) {
		ret[i] = images[i];
	}
	return ret;
}

TypedArray<Dictionary> RenderingServer::_shader_get_shader_parameter_list(RID p_shader) const {
	List<PropertyInfo> l;
	get_shader_parameter_list(p_shader, &l);
	return convert_property_list(&l);
}

static RS::SurfaceData _dict_to_surf(const Dictionary &p_dictionary) {
	ERR_FAIL_COND_V(!p_dictionary.has("primitive"), RS::SurfaceData());
	ERR_FAIL_COND_V(!p_dictionary.has("format"), RS::SurfaceData());
	ERR_FAIL_COND_V(!p_dictionary.has("vertex_data"), RS::SurfaceData());
	ERR_FAIL_COND_V(!p_dictionary.has("vertex_count"), RS::SurfaceData());
	ERR_FAIL_COND_V(!p_dictionary.has("aabb"), RS::SurfaceData());

	RS::SurfaceData sd;

	sd.primitive = RS::PrimitiveType(int(p_dictionary["primitive"]));
	sd.format = p_dictionary["format"];
	sd.vertex_data = p_dictionary["vertex_data"];
	if (p_dictionary.has("attribute_data")) {
		sd.attribute_data = p_dictionary["attribute_data"];
	}
	if (p_dictionary.has("skin_data")) {
		sd.skin_data = p_dictionary["skin_data"];
	}

	sd.vertex_count = p_dictionary["vertex_count"];

	if (p_dictionary.has("index_data")) {
		sd.index_data = p_dictionary["index_data"];
		ERR_FAIL_COND_V(!p_dictionary.has("index_count"), RS::SurfaceData());
		sd.index_count = p_dictionary["index_count"];
	}

	sd.aabb = p_dictionary["aabb"];
	if (p_dictionary.has("uv_scale")) {
		sd.uv_scale = p_dictionary["uv_scale"];
	}

	if (p_dictionary.has("lods")) {
		Array lods = p_dictionary["lods"];
		for (int i = 0; i < lods.size(); i++) {
			Dictionary lod = lods[i];
			ERR_CONTINUE(!lod.has("edge_length"));
			ERR_CONTINUE(!lod.has("index_data"));
			RS::SurfaceData::LOD l;
			l.edge_length = lod["edge_length"];
			l.index_data = lod["index_data"];
			sd.lods.push_back(l);
		}
	}

	if (p_dictionary.has("bone_aabbs")) {
		Array aabbs = p_dictionary["bone_aabbs"];
		for (int i = 0; i < aabbs.size(); i++) {
			AABB aabb = aabbs[i];
			sd.bone_aabbs.push_back(aabb);
		}
	}

	if (p_dictionary.has("blend_shape_data")) {
		sd.blend_shape_data = p_dictionary["blend_shape_data"];
	}

	if (p_dictionary.has("material")) {
		sd.material = p_dictionary["material"];
	}

	return sd;
}
RID RenderingServer::_mesh_create_from_surfaces(const TypedArray<Dictionary> &p_surfaces, int p_blend_shape_count) {
	Vector<RS::SurfaceData> surfaces;
	for (int i = 0; i < p_surfaces.size(); i++) {
		surfaces.push_back(_dict_to_surf(p_surfaces[i]));
	}
	return mesh_create_from_surfaces(surfaces);
}
void RenderingServer::_mesh_add_surface(RID p_mesh, const Dictionary &p_surface) {
	mesh_add_surface(p_mesh, _dict_to_surf(p_surface));
}
Dictionary RenderingServer::_mesh_get_surface(RID p_mesh, int p_idx) {
	RS::SurfaceData sd = mesh_get_surface(p_mesh, p_idx);

	Dictionary d;
	d["primitive"] = sd.primitive;
	d["format"] = sd.format;
	d["vertex_data"] = sd.vertex_data;
	if (sd.attribute_data.size()) {
		d["attribute_data"] = sd.attribute_data;
	}
	if (sd.skin_data.size()) {
		d["skin_data"] = sd.skin_data;
	}
	d["vertex_count"] = sd.vertex_count;
	if (sd.index_count) {
		d["index_data"] = sd.index_data;
		d["index_count"] = sd.index_count;
	}
	d["aabb"] = sd.aabb;
	d["uv_scale"] = sd.uv_scale;

	if (sd.lods.size()) {
		Array lods;
		for (int i = 0; i < sd.lods.size(); i++) {
			Dictionary ld;
			ld["edge_length"] = sd.lods[i].edge_length;
			ld["index_data"] = sd.lods[i].index_data;
			lods.push_back(lods);
		}
		d["lods"] = lods;
	}

	if (sd.bone_aabbs.size()) {
		Array aabbs;
		for (int i = 0; i < sd.bone_aabbs.size(); i++) {
			aabbs.push_back(sd.bone_aabbs[i]);
		}
		d["bone_aabbs"] = aabbs;
	}

	if (sd.blend_shape_data.size()) {
		d["blend_shape_data"] = sd.blend_shape_data;
	}

	if (sd.material.is_valid()) {
		d["material"] = sd.material;
	}
	return d;
}

TypedArray<Dictionary> RenderingServer::_instance_geometry_get_shader_parameter_list(RID p_instance) const {
	List<PropertyInfo> params;
	instance_geometry_get_shader_parameter_list(p_instance, &params);
	return convert_property_list(&params);
}

TypedArray<Image> RenderingServer::_bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) {
	TypedArray<RID> mat_overrides;
	for (int i = 0; i < p_material_overrides.size(); i++) {
		mat_overrides.push_back(p_material_overrides[i]);
	}
	return bake_render_uv2(p_base, mat_overrides, p_image_size);
}

void RenderingServer::_particles_set_trail_bind_poses(RID p_particles, const TypedArray<Transform3D> &p_bind_poses) {
	Vector<Transform3D> tbposes;
	tbposes.resize(p_bind_poses.size());
	for (int i = 0; i < p_bind_poses.size(); i++) {
		tbposes.write[i] = p_bind_poses[i];
	}
	particles_set_trail_bind_poses(p_particles, tbposes);
}

Vector<uint8_t> _convert_surface_version_1_to_surface_version_2(uint64_t p_format, Vector<uint8_t> p_vertex_data, uint32_t p_vertex_count, uint32_t p_old_stride, uint32_t p_vertex_size, uint32_t p_normal_size, uint32_t p_position_stride, uint32_t p_normal_tangent_stride) {
	Vector<uint8_t> new_vertex_data;
	new_vertex_data.resize(p_vertex_data.size());
	uint8_t *dst_vertex_ptr = new_vertex_data.ptrw();

	const uint8_t *src_vertex_ptr = p_vertex_data.ptr();

	uint32_t position_size = p_position_stride * p_vertex_count;

	for (uint32_t j = 0; j < RS::ARRAY_COLOR; j++) {
		if (!(p_format & (1ULL << j))) {
			continue;
		}
		switch (j) {
			case RS::ARRAY_VERTEX: {
				if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
					for (uint32_t i = 0; i < p_vertex_count; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * p_old_stride];
						float *dst = (float *)&dst_vertex_ptr[i * p_position_stride];
						dst[0] = src[0];
						dst[1] = src[1];
					}
				} else {
					for (uint32_t i = 0; i < p_vertex_count; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * p_old_stride];
						float *dst = (float *)&dst_vertex_ptr[i * p_position_stride];
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
					}
				}
			} break;
			case RS::ARRAY_NORMAL: {
				for (uint32_t i = 0; i < p_vertex_count; i++) {
					const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * p_old_stride + p_vertex_size];
					uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * p_normal_tangent_stride + position_size];

					dst[0] = src[0];
					dst[1] = src[1];
				}
			} break;
			case RS::ARRAY_TANGENT: {
				for (uint32_t i = 0; i < p_vertex_count; i++) {
					const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * p_old_stride + p_vertex_size + p_normal_size];
					uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * p_normal_tangent_stride + position_size + p_normal_size];

					dst[0] = src[0];
					dst[1] = src[1];
				}
			} break;
		}
	}
	return new_vertex_data;
}

#ifdef TOOLS_ENABLED
void RenderingServer::set_surface_upgrade_callback(SurfaceUpgradeCallback p_callback) {
	surface_upgrade_callback = p_callback;
}

void RenderingServer::set_warn_on_surface_upgrade(bool p_warn) {
	warn_on_surface_upgrade = p_warn;
}
#endif

#ifndef DISABLE_DEPRECATED
void RenderingServer::fix_surface_compatibility(SurfaceData &p_surface, const String &p_path) {
	uint64_t surface_version = p_surface.format & (ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	ERR_FAIL_COND_MSG(surface_version > ARRAY_FLAG_FORMAT_CURRENT_VERSION, "Cannot convert surface with version provided (" + itos((surface_version >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK) + ") to current version (" + itos((RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK) + ")");

#ifdef TOOLS_ENABLED
	// Editor callback to ask user about re-saving all meshes.
	if (surface_upgrade_callback && warn_on_surface_upgrade) {
		surface_upgrade_callback();
	}

	if (warn_on_surface_upgrade) {
		WARN_PRINT_ONCE_ED("At least one surface uses an old surface format and needs to be upgraded. The upgrade happens automatically at load time every time until the mesh is saved again or re-imported. Once saved (or re-imported), this mesh will be incompatible with earlier versions of Godot.");

		if (!p_path.is_empty()) {
			WARN_PRINT("A surface of " + p_path + " uses an old surface format and needs to be upgraded.");
		}
	}
#endif

	if (surface_version == ARRAY_FLAG_FORMAT_VERSION_1) {
		// The only difference for now is that Version 1 uses interleaved vertex positions while version 2 does not.
		// I.e. PNTPNTPNT -> PPPNTNTNT.

		if (p_surface.vertex_data.size() > 0 && p_surface.vertex_count > 0) {
			int vertex_size = 0;
			int normal_size = 0;
			int tangent_size = 0;
			if (p_surface.format & ARRAY_FORMAT_VERTEX) {
				if (p_surface.format & ARRAY_FLAG_USE_2D_VERTICES) {
					vertex_size = sizeof(float) * 2;
				} else {
					vertex_size = sizeof(float) * 3;
				}
			}
			if (p_surface.format & ARRAY_FORMAT_NORMAL) {
				normal_size += sizeof(uint16_t) * 2;
			}
			if (p_surface.format & ARRAY_FORMAT_TANGENT) {
				tangent_size = sizeof(uint16_t) * 2;
			}
			int stride = p_surface.vertex_data.size() / p_surface.vertex_count;
			int position_stride = vertex_size;
			int normal_tangent_stride = normal_size + tangent_size;

			p_surface.vertex_data = _convert_surface_version_1_to_surface_version_2(p_surface.format, p_surface.vertex_data, p_surface.vertex_count, stride, vertex_size, normal_size, position_stride, normal_tangent_stride);

			if (p_surface.blend_shape_data.size() > 0) {
				// The size of one blend shape.
				int divisor = (vertex_size + normal_size + tangent_size) * p_surface.vertex_count;
				ERR_FAIL_COND((p_surface.blend_shape_data.size() % divisor) != 0);

				uint32_t blend_shape_count = p_surface.blend_shape_data.size() / divisor;

				Vector<uint8_t> new_blend_shape_data;
				for (uint32_t i = 0; i < blend_shape_count; i++) {
					Vector<uint8_t> bs_data = p_surface.blend_shape_data.slice(i * divisor, (i + 1) * divisor);
					Vector<uint8_t> blend_shape = _convert_surface_version_1_to_surface_version_2(p_surface.format, bs_data, p_surface.vertex_count, stride, vertex_size, normal_size, position_stride, normal_tangent_stride);
					new_blend_shape_data.append_array(blend_shape);
				}

				ERR_FAIL_COND(p_surface.blend_shape_data.size() != new_blend_shape_data.size());

				p_surface.blend_shape_data = new_blend_shape_data;
			}
		}
	}
	p_surface.format &= ~(ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	p_surface.format |= ARRAY_FLAG_FORMAT_CURRENT_VERSION & (ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);
}
#endif

#ifdef TOOLS_ENABLED
void RenderingServer::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		if (pf == "global_shader_parameter_set" || pf == "global_shader_parameter_set_override" ||
				pf == "global_shader_parameter_get" || pf == "global_shader_parameter_get_type" || pf == "global_shader_parameter_remove") {
			for (const StringName &E : global_shader_parameter_get_list()) {
				r_options->push_back(E.operator String().quote());
			}
		} else if (pf == "has_os_feature") {
			for (const String E : { "\"rgtc\"", "\"s3tc\"", "\"bptc\"", "\"etc\"", "\"etc2\"", "\"astc\"" }) {
				r_options->push_back(E);
			}
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void RenderingServer::_bind_methods() {
	BIND_CONSTANT(NO_INDEX_ARRAY);
	BIND_CONSTANT(ARRAY_WEIGHTS_SIZE);
	BIND_CONSTANT(CANVAS_ITEM_Z_MIN);
	BIND_CONSTANT(CANVAS_ITEM_Z_MAX);
	BIND_CONSTANT(MAX_GLOW_LEVELS);
	BIND_CONSTANT(MAX_CURSORS);
	BIND_CONSTANT(MAX_2D_DIRECTIONAL_LIGHTS);
	BIND_CONSTANT(MAX_MESH_SURFACES);

	/* TEXTURE */

	ClassDB::bind_method(D_METHOD("texture_2d_create", "image"), &RenderingServer::texture_2d_create);
	ClassDB::bind_method(D_METHOD("texture_2d_layered_create", "layers", "layered_type"), &RenderingServer::_texture_2d_layered_create);
	ClassDB::bind_method(D_METHOD("texture_3d_create", "format", "width", "height", "depth", "mipmaps", "data"), &RenderingServer::_texture_3d_create);
	ClassDB::bind_method(D_METHOD("texture_proxy_create", "base"), &RenderingServer::texture_proxy_create);
	ClassDB::bind_method(D_METHOD("texture_create_from_native_handle", "type", "format", "native_handle", "width", "height", "depth", "layers", "layered_type"), &RenderingServer::texture_create_from_native_handle, DEFVAL(1), DEFVAL(TEXTURE_LAYERED_2D_ARRAY));

	ClassDB::bind_method(D_METHOD("texture_2d_update", "texture", "image", "layer"), &RenderingServer::texture_2d_update);
	ClassDB::bind_method(D_METHOD("texture_3d_update", "texture", "data"), &RenderingServer::_texture_3d_update);
	ClassDB::bind_method(D_METHOD("texture_proxy_update", "texture", "proxy_to"), &RenderingServer::texture_proxy_update);

	ClassDB::bind_method(D_METHOD("texture_2d_placeholder_create"), &RenderingServer::texture_2d_placeholder_create);
	ClassDB::bind_method(D_METHOD("texture_2d_layered_placeholder_create", "layered_type"), &RenderingServer::texture_2d_layered_placeholder_create);
	ClassDB::bind_method(D_METHOD("texture_3d_placeholder_create"), &RenderingServer::texture_3d_placeholder_create);

	ClassDB::bind_method(D_METHOD("texture_2d_get", "texture"), &RenderingServer::texture_2d_get);
	ClassDB::bind_method(D_METHOD("texture_2d_layer_get", "texture", "layer"), &RenderingServer::texture_2d_layer_get);
	ClassDB::bind_method(D_METHOD("texture_3d_get", "texture"), &RenderingServer::_texture_3d_get);

	ClassDB::bind_method(D_METHOD("texture_replace", "texture", "by_texture"), &RenderingServer::texture_replace);
	ClassDB::bind_method(D_METHOD("texture_set_size_override", "texture", "width", "height"), &RenderingServer::texture_set_size_override);

	ClassDB::bind_method(D_METHOD("texture_set_path", "texture", "path"), &RenderingServer::texture_set_path);
	ClassDB::bind_method(D_METHOD("texture_get_path", "texture"), &RenderingServer::texture_get_path);

	ClassDB::bind_method(D_METHOD("texture_get_format", "texture"), &RenderingServer::texture_get_format);

	ClassDB::bind_method(D_METHOD("texture_set_force_redraw_if_visible", "texture", "enable"), &RenderingServer::texture_set_force_redraw_if_visible);
	ClassDB::bind_method(D_METHOD("texture_rd_create", "rd_texture", "layer_type"), &RenderingServer::texture_rd_create, DEFVAL(RenderingServer::TEXTURE_LAYERED_2D_ARRAY));
	ClassDB::bind_method(D_METHOD("texture_get_rd_texture", "texture", "srgb"), &RenderingServer::texture_get_rd_texture, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("texture_get_native_handle", "texture", "srgb"), &RenderingServer::texture_get_native_handle, DEFVAL(false));

	BIND_ENUM_CONSTANT(TEXTURE_TYPE_2D);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_LAYERED);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_3D);

	BIND_ENUM_CONSTANT(TEXTURE_LAYERED_2D_ARRAY);
	BIND_ENUM_CONSTANT(TEXTURE_LAYERED_CUBEMAP);
	BIND_ENUM_CONSTANT(TEXTURE_LAYERED_CUBEMAP_ARRAY);

	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_LEFT);
	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_RIGHT);
	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_BOTTOM);
	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_TOP);
	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_FRONT);
	BIND_ENUM_CONSTANT(CUBEMAP_LAYER_BACK);

	/* SHADER */

	ClassDB::bind_method(D_METHOD("shader_create"), &RenderingServer::shader_create);
	ClassDB::bind_method(D_METHOD("shader_set_code", "shader", "code"), &RenderingServer::shader_set_code);
	ClassDB::bind_method(D_METHOD("shader_set_path_hint", "shader", "path"), &RenderingServer::shader_set_path_hint);
	ClassDB::bind_method(D_METHOD("shader_get_code", "shader"), &RenderingServer::shader_get_code);
	ClassDB::bind_method(D_METHOD("get_shader_parameter_list", "shader"), &RenderingServer::_shader_get_shader_parameter_list);
	ClassDB::bind_method(D_METHOD("shader_get_parameter_default", "shader", "name"), &RenderingServer::shader_get_parameter_default);

	ClassDB::bind_method(D_METHOD("shader_set_default_texture_parameter", "shader", "name", "texture", "index"), &RenderingServer::shader_set_default_texture_parameter, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("shader_get_default_texture_parameter", "shader", "name", "index"), &RenderingServer::shader_get_default_texture_parameter, DEFVAL(0));

	BIND_ENUM_CONSTANT(SHADER_SPATIAL);
	BIND_ENUM_CONSTANT(SHADER_CANVAS_ITEM);
	BIND_ENUM_CONSTANT(SHADER_PARTICLES);
	BIND_ENUM_CONSTANT(SHADER_SKY);
	BIND_ENUM_CONSTANT(SHADER_FOG);
	BIND_ENUM_CONSTANT(SHADER_MAX);

	/* MATERIAL */

	ClassDB::bind_method(D_METHOD("material_create"), &RenderingServer::material_create);
	ClassDB::bind_method(D_METHOD("material_set_shader", "shader_material", "shader"), &RenderingServer::material_set_shader);
	ClassDB::bind_method(D_METHOD("material_set_param", "material", "parameter", "value"), &RenderingServer::material_set_param);
	ClassDB::bind_method(D_METHOD("material_get_param", "material", "parameter"), &RenderingServer::material_get_param);
	ClassDB::bind_method(D_METHOD("material_set_render_priority", "material", "priority"), &RenderingServer::material_set_render_priority);

	ClassDB::bind_method(D_METHOD("material_set_next_pass", "material", "next_material"), &RenderingServer::material_set_next_pass);

	BIND_CONSTANT(MATERIAL_RENDER_PRIORITY_MIN);
	BIND_CONSTANT(MATERIAL_RENDER_PRIORITY_MAX);

	/* MESH API */

	ClassDB::bind_method(D_METHOD("mesh_create_from_surfaces", "surfaces", "blend_shape_count"), &RenderingServer::_mesh_create_from_surfaces, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("mesh_create"), &RenderingServer::mesh_create);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_format_offset", "format", "vertex_count", "array_index"), &RenderingServer::mesh_surface_get_format_offset);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_format_vertex_stride", "format", "vertex_count"), &RenderingServer::mesh_surface_get_format_vertex_stride);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_format_normal_tangent_stride", "format", "vertex_count"), &RenderingServer::mesh_surface_get_format_normal_tangent_stride);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_format_attribute_stride", "format", "vertex_count"), &RenderingServer::mesh_surface_get_format_attribute_stride);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_format_skin_stride", "format", "vertex_count"), &RenderingServer::mesh_surface_get_format_skin_stride);
	ClassDB::bind_method(D_METHOD("mesh_add_surface", "mesh", "surface"), &RenderingServer::_mesh_add_surface);
	ClassDB::bind_method(D_METHOD("mesh_add_surface_from_arrays", "mesh", "primitive", "arrays", "blend_shapes", "lods", "compress_format"), &RenderingServer::mesh_add_surface_from_arrays, DEFVAL(Array()), DEFVAL(Dictionary()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("mesh_get_blend_shape_count", "mesh"), &RenderingServer::mesh_get_blend_shape_count);
	ClassDB::bind_method(D_METHOD("mesh_set_blend_shape_mode", "mesh", "mode"), &RenderingServer::mesh_set_blend_shape_mode);
	ClassDB::bind_method(D_METHOD("mesh_get_blend_shape_mode", "mesh"), &RenderingServer::mesh_get_blend_shape_mode);

	ClassDB::bind_method(D_METHOD("mesh_surface_set_material", "mesh", "surface", "material"), &RenderingServer::mesh_surface_set_material);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_material", "mesh", "surface"), &RenderingServer::mesh_surface_get_material);
	ClassDB::bind_method(D_METHOD("mesh_get_surface", "mesh", "surface"), &RenderingServer::_mesh_get_surface);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_arrays", "mesh", "surface"), &RenderingServer::mesh_surface_get_arrays);
	ClassDB::bind_method(D_METHOD("mesh_surface_get_blend_shape_arrays", "mesh", "surface"), &RenderingServer::mesh_surface_get_blend_shape_arrays);
	ClassDB::bind_method(D_METHOD("mesh_get_surface_count", "mesh"), &RenderingServer::mesh_get_surface_count);
	ClassDB::bind_method(D_METHOD("mesh_set_custom_aabb", "mesh", "aabb"), &RenderingServer::mesh_set_custom_aabb);
	ClassDB::bind_method(D_METHOD("mesh_get_custom_aabb", "mesh"), &RenderingServer::mesh_get_custom_aabb);
	ClassDB::bind_method(D_METHOD("mesh_clear", "mesh"), &RenderingServer::mesh_clear);

	ClassDB::bind_method(D_METHOD("mesh_surface_update_vertex_region", "mesh", "surface", "offset", "data"), &RenderingServer::mesh_surface_update_vertex_region);
	ClassDB::bind_method(D_METHOD("mesh_surface_update_attribute_region", "mesh", "surface", "offset", "data"), &RenderingServer::mesh_surface_update_attribute_region);
	ClassDB::bind_method(D_METHOD("mesh_surface_update_skin_region", "mesh", "surface", "offset", "data"), &RenderingServer::mesh_surface_update_skin_region);

	ClassDB::bind_method(D_METHOD("mesh_set_shadow_mesh", "mesh", "shadow_mesh"), &RenderingServer::mesh_set_shadow_mesh);

	BIND_ENUM_CONSTANT(ARRAY_VERTEX);
	BIND_ENUM_CONSTANT(ARRAY_NORMAL);
	BIND_ENUM_CONSTANT(ARRAY_TANGENT);
	BIND_ENUM_CONSTANT(ARRAY_COLOR);
	BIND_ENUM_CONSTANT(ARRAY_TEX_UV);
	BIND_ENUM_CONSTANT(ARRAY_TEX_UV2);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM0);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM1);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM2);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM3);
	BIND_ENUM_CONSTANT(ARRAY_BONES);
	BIND_ENUM_CONSTANT(ARRAY_WEIGHTS);
	BIND_ENUM_CONSTANT(ARRAY_INDEX);
	BIND_ENUM_CONSTANT(ARRAY_MAX);

	BIND_CONSTANT(ARRAY_CUSTOM_COUNT);

	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA8_UNORM);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA8_SNORM);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RG_HALF);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA_HALF);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_R_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RG_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGB_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_MAX);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_VERTEX);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_NORMAL);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TANGENT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_COLOR);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TEX_UV);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TEX_UV2);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM0);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM1);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM2);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM3);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_BONES);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_WEIGHTS);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_INDEX);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_BLEND_SHAPE_MASK);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_BASE);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_BITS);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM0_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM1_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM2_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM3_SHIFT);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_MASK);
	BIND_BITFIELD_FLAG(ARRAY_COMPRESS_FLAGS_BASE);

	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_2D_VERTICES);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_DYNAMIC_UPDATE);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_8_BONE_WEIGHTS);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY);

	BIND_BITFIELD_FLAG(ARRAY_FLAG_COMPRESS_ATTRIBUTES);

	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_VERSION_BASE);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_VERSION_1);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_VERSION_2);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_CURRENT_VERSION);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_FORMAT_VERSION_MASK);

	BIND_ENUM_CONSTANT(PRIMITIVE_POINTS);
	BIND_ENUM_CONSTANT(PRIMITIVE_LINES);
	BIND_ENUM_CONSTANT(PRIMITIVE_LINE_STRIP);
	BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLES);
	BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_STRIP);
	BIND_ENUM_CONSTANT(PRIMITIVE_MAX);

	BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_NORMALIZED);
	BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_RELATIVE);

	/* MULTIMESH API */

	ClassDB::bind_method(D_METHOD("multimesh_create"), &RenderingServer::multimesh_create);
	ClassDB::bind_method(D_METHOD("multimesh_allocate_data", "multimesh", "instances", "transform_format", "color_format", "custom_data_format"), &RenderingServer::multimesh_allocate_data, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("multimesh_get_instance_count", "multimesh"), &RenderingServer::multimesh_get_instance_count);
	ClassDB::bind_method(D_METHOD("multimesh_set_mesh", "multimesh", "mesh"), &RenderingServer::multimesh_set_mesh);
	ClassDB::bind_method(D_METHOD("multimesh_instance_set_transform", "multimesh", "index", "transform"), &RenderingServer::multimesh_instance_set_transform);
	ClassDB::bind_method(D_METHOD("multimesh_instance_set_transform_2d", "multimesh", "index", "transform"), &RenderingServer::multimesh_instance_set_transform_2d);
	ClassDB::bind_method(D_METHOD("multimesh_instance_set_color", "multimesh", "index", "color"), &RenderingServer::multimesh_instance_set_color);
	ClassDB::bind_method(D_METHOD("multimesh_instance_set_custom_data", "multimesh", "index", "custom_data"), &RenderingServer::multimesh_instance_set_custom_data);
	ClassDB::bind_method(D_METHOD("multimesh_get_mesh", "multimesh"), &RenderingServer::multimesh_get_mesh);
	ClassDB::bind_method(D_METHOD("multimesh_get_aabb", "multimesh"), &RenderingServer::multimesh_get_aabb);
	ClassDB::bind_method(D_METHOD("multimesh_set_custom_aabb", "multimesh", "aabb"), &RenderingServer::multimesh_set_custom_aabb);
	ClassDB::bind_method(D_METHOD("multimesh_get_custom_aabb", "multimesh"), &RenderingServer::multimesh_get_custom_aabb);
	ClassDB::bind_method(D_METHOD("multimesh_instance_get_transform", "multimesh", "index"), &RenderingServer::multimesh_instance_get_transform);
	ClassDB::bind_method(D_METHOD("multimesh_instance_get_transform_2d", "multimesh", "index"), &RenderingServer::multimesh_instance_get_transform_2d);
	ClassDB::bind_method(D_METHOD("multimesh_instance_get_color", "multimesh", "index"), &RenderingServer::multimesh_instance_get_color);
	ClassDB::bind_method(D_METHOD("multimesh_instance_get_custom_data", "multimesh", "index"), &RenderingServer::multimesh_instance_get_custom_data);
	ClassDB::bind_method(D_METHOD("multimesh_set_visible_instances", "multimesh", "visible"), &RenderingServer::multimesh_set_visible_instances);
	ClassDB::bind_method(D_METHOD("multimesh_get_visible_instances", "multimesh"), &RenderingServer::multimesh_get_visible_instances);
	ClassDB::bind_method(D_METHOD("multimesh_set_buffer", "multimesh", "buffer"), &RenderingServer::multimesh_set_buffer);
	ClassDB::bind_method(D_METHOD("multimesh_get_buffer", "multimesh"), &RenderingServer::multimesh_get_buffer);

	ClassDB::bind_method(D_METHOD("multimesh_set_buffer_interpolated", "multimesh", "buffer", "buffer_previous"), &RenderingServer::multimesh_set_buffer_interpolated);
	ClassDB::bind_method(D_METHOD("multimesh_set_physics_interpolated", "multimesh", "interpolated"), &RenderingServer::multimesh_set_physics_interpolated);
	ClassDB::bind_method(D_METHOD("multimesh_set_physics_interpolation_quality", "multimesh", "quality"), &RenderingServer::multimesh_set_physics_interpolation_quality);
	ClassDB::bind_method(D_METHOD("multimesh_instance_reset_physics_interpolation", "multimesh", "index"), &RenderingServer::multimesh_instance_reset_physics_interpolation);

	BIND_ENUM_CONSTANT(MULTIMESH_TRANSFORM_2D);
	BIND_ENUM_CONSTANT(MULTIMESH_TRANSFORM_3D);
	BIND_ENUM_CONSTANT(MULTIMESH_INTERP_QUALITY_FAST);
	BIND_ENUM_CONSTANT(MULTIMESH_INTERP_QUALITY_HIGH);

	/* SKELETON API */

	ClassDB::bind_method(D_METHOD("skeleton_create"), &RenderingServer::skeleton_create);
	ClassDB::bind_method(D_METHOD("skeleton_allocate_data", "skeleton", "bones", "is_2d_skeleton"), &RenderingServer::skeleton_allocate_data, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("skeleton_get_bone_count", "skeleton"), &RenderingServer::skeleton_get_bone_count);
	ClassDB::bind_method(D_METHOD("skeleton_bone_set_transform", "skeleton", "bone", "transform"), &RenderingServer::skeleton_bone_set_transform);
	ClassDB::bind_method(D_METHOD("skeleton_bone_get_transform", "skeleton", "bone"), &RenderingServer::skeleton_bone_get_transform);
	ClassDB::bind_method(D_METHOD("skeleton_bone_set_transform_2d", "skeleton", "bone", "transform"), &RenderingServer::skeleton_bone_set_transform_2d);
	ClassDB::bind_method(D_METHOD("skeleton_bone_get_transform_2d", "skeleton", "bone"), &RenderingServer::skeleton_bone_get_transform_2d);
	ClassDB::bind_method(D_METHOD("skeleton_set_base_transform_2d", "skeleton", "base_transform"), &RenderingServer::skeleton_set_base_transform_2d);

	/* Light API */

	ClassDB::bind_method(D_METHOD("directional_light_create"), &RenderingServer::directional_light_create);
	ClassDB::bind_method(D_METHOD("omni_light_create"), &RenderingServer::omni_light_create);
	ClassDB::bind_method(D_METHOD("spot_light_create"), &RenderingServer::spot_light_create);

	ClassDB::bind_method(D_METHOD("light_set_color", "light", "color"), &RenderingServer::light_set_color);
	ClassDB::bind_method(D_METHOD("light_set_param", "light", "param", "value"), &RenderingServer::light_set_param);
	ClassDB::bind_method(D_METHOD("light_set_shadow", "light", "enabled"), &RenderingServer::light_set_shadow);
	ClassDB::bind_method(D_METHOD("light_set_projector", "light", "texture"), &RenderingServer::light_set_projector);
	ClassDB::bind_method(D_METHOD("light_set_negative", "light", "enable"), &RenderingServer::light_set_negative);
	ClassDB::bind_method(D_METHOD("light_set_cull_mask", "light", "mask"), &RenderingServer::light_set_cull_mask);
	ClassDB::bind_method(D_METHOD("light_set_distance_fade", "decal", "enabled", "begin", "shadow", "length"), &RenderingServer::light_set_distance_fade);
	ClassDB::bind_method(D_METHOD("light_set_reverse_cull_face_mode", "light", "enabled"), &RenderingServer::light_set_reverse_cull_face_mode);
	ClassDB::bind_method(D_METHOD("light_set_shadow_caster_mask", "light", "mask"), &RenderingServer::light_set_shadow_caster_mask);
	ClassDB::bind_method(D_METHOD("light_set_bake_mode", "light", "bake_mode"), &RenderingServer::light_set_bake_mode);
	ClassDB::bind_method(D_METHOD("light_set_max_sdfgi_cascade", "light", "cascade"), &RenderingServer::light_set_max_sdfgi_cascade);

	ClassDB::bind_method(D_METHOD("light_omni_set_shadow_mode", "light", "mode"), &RenderingServer::light_omni_set_shadow_mode);

	ClassDB::bind_method(D_METHOD("light_directional_set_shadow_mode", "light", "mode"), &RenderingServer::light_directional_set_shadow_mode);
	ClassDB::bind_method(D_METHOD("light_directional_set_blend_splits", "light", "enable"), &RenderingServer::light_directional_set_blend_splits);
	ClassDB::bind_method(D_METHOD("light_directional_set_sky_mode", "light", "mode"), &RenderingServer::light_directional_set_sky_mode);

	ClassDB::bind_method(D_METHOD("light_projectors_set_filter", "filter"), &RenderingServer::light_projectors_set_filter);
	ClassDB::bind_method(D_METHOD("lightmaps_set_bicubic_filter", "enable"), &RenderingServer::lightmaps_set_bicubic_filter);

	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS);
	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS);
	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC);

	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL);
	BIND_ENUM_CONSTANT(LIGHT_OMNI);
	BIND_ENUM_CONSTANT(LIGHT_SPOT);

	BIND_ENUM_CONSTANT(LIGHT_PARAM_ENERGY);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_INDIRECT_ENERGY);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SPECULAR);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_RANGE);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SIZE);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_ATTENUATION);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SPOT_ANGLE);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SPOT_ATTENUATION);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_MAX_DISTANCE);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_FADE_START);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_NORMAL_BIAS);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_BIAS);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_PANCAKE_SIZE);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_OPACITY);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_SHADOW_BLUR);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_TRANSMITTANCE_BIAS);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_INTENSITY);
	BIND_ENUM_CONSTANT(LIGHT_PARAM_MAX);

	BIND_ENUM_CONSTANT(LIGHT_BAKE_DISABLED);
	BIND_ENUM_CONSTANT(LIGHT_BAKE_STATIC);
	BIND_ENUM_CONSTANT(LIGHT_BAKE_DYNAMIC);

	BIND_ENUM_CONSTANT(LIGHT_OMNI_SHADOW_DUAL_PARABOLOID);
	BIND_ENUM_CONSTANT(LIGHT_OMNI_SHADOW_CUBE);

	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);
	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS);
	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);

	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY);
	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_ONLY);
	BIND_ENUM_CONSTANT(LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY);

	ClassDB::bind_method(D_METHOD("positional_soft_shadow_filter_set_quality", "quality"), &RenderingServer::positional_soft_shadow_filter_set_quality);
	ClassDB::bind_method(D_METHOD("directional_soft_shadow_filter_set_quality", "quality"), &RenderingServer::directional_soft_shadow_filter_set_quality);
	ClassDB::bind_method(D_METHOD("directional_shadow_atlas_set_size", "size", "is_16bits"), &RenderingServer::directional_shadow_atlas_set_size);

	BIND_ENUM_CONSTANT(SHADOW_QUALITY_HARD);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_SOFT_VERY_LOW);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_SOFT_LOW);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_SOFT_MEDIUM);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_SOFT_HIGH);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_SOFT_ULTRA);
	BIND_ENUM_CONSTANT(SHADOW_QUALITY_MAX);

	/* REFLECTION PROBE */

	ClassDB::bind_method(D_METHOD("reflection_probe_create"), &RenderingServer::reflection_probe_create);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_update_mode", "probe", "mode"), &RenderingServer::reflection_probe_set_update_mode);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_intensity", "probe", "intensity"), &RenderingServer::reflection_probe_set_intensity);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_ambient_mode", "probe", "mode"), &RenderingServer::reflection_probe_set_ambient_mode);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_ambient_color", "probe", "color"), &RenderingServer::reflection_probe_set_ambient_color);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_ambient_energy", "probe", "energy"), &RenderingServer::reflection_probe_set_ambient_energy);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_max_distance", "probe", "distance"), &RenderingServer::reflection_probe_set_max_distance);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_size", "probe", "size"), &RenderingServer::reflection_probe_set_size);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_origin_offset", "probe", "offset"), &RenderingServer::reflection_probe_set_origin_offset);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_as_interior", "probe", "enable"), &RenderingServer::reflection_probe_set_as_interior);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_enable_box_projection", "probe", "enable"), &RenderingServer::reflection_probe_set_enable_box_projection);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_enable_shadows", "probe", "enable"), &RenderingServer::reflection_probe_set_enable_shadows);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_cull_mask", "probe", "layers"), &RenderingServer::reflection_probe_set_cull_mask);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_reflection_mask", "probe", "layers"), &RenderingServer::reflection_probe_set_reflection_mask);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_resolution", "probe", "resolution"), &RenderingServer::reflection_probe_set_resolution);
	ClassDB::bind_method(D_METHOD("reflection_probe_set_mesh_lod_threshold", "probe", "pixels"), &RenderingServer::reflection_probe_set_mesh_lod_threshold);

	BIND_ENUM_CONSTANT(REFLECTION_PROBE_UPDATE_ONCE);
	BIND_ENUM_CONSTANT(REFLECTION_PROBE_UPDATE_ALWAYS);

	BIND_ENUM_CONSTANT(REFLECTION_PROBE_AMBIENT_DISABLED);
	BIND_ENUM_CONSTANT(REFLECTION_PROBE_AMBIENT_ENVIRONMENT);
	BIND_ENUM_CONSTANT(REFLECTION_PROBE_AMBIENT_COLOR);

	/* DECAL */

	ClassDB::bind_method(D_METHOD("decal_create"), &RenderingServer::decal_create);
	ClassDB::bind_method(D_METHOD("decal_set_size", "decal", "size"), &RenderingServer::decal_set_size);
	ClassDB::bind_method(D_METHOD("decal_set_texture", "decal", "type", "texture"), &RenderingServer::decal_set_texture);
	ClassDB::bind_method(D_METHOD("decal_set_emission_energy", "decal", "energy"), &RenderingServer::decal_set_emission_energy);
	ClassDB::bind_method(D_METHOD("decal_set_albedo_mix", "decal", "albedo_mix"), &RenderingServer::decal_set_albedo_mix);
	ClassDB::bind_method(D_METHOD("decal_set_modulate", "decal", "color"), &RenderingServer::decal_set_modulate);
	ClassDB::bind_method(D_METHOD("decal_set_cull_mask", "decal", "mask"), &RenderingServer::decal_set_cull_mask);
	ClassDB::bind_method(D_METHOD("decal_set_distance_fade", "decal", "enabled", "begin", "length"), &RenderingServer::decal_set_distance_fade);
	ClassDB::bind_method(D_METHOD("decal_set_fade", "decal", "above", "below"), &RenderingServer::decal_set_fade);
	ClassDB::bind_method(D_METHOD("decal_set_normal_fade", "decal", "fade"), &RenderingServer::decal_set_normal_fade);

	ClassDB::bind_method(D_METHOD("decals_set_filter", "filter"), &RenderingServer::decals_set_filter);

	BIND_ENUM_CONSTANT(DECAL_TEXTURE_ALBEDO);
	BIND_ENUM_CONSTANT(DECAL_TEXTURE_NORMAL);
	BIND_ENUM_CONSTANT(DECAL_TEXTURE_ORM);
	BIND_ENUM_CONSTANT(DECAL_TEXTURE_EMISSION);
	BIND_ENUM_CONSTANT(DECAL_TEXTURE_MAX);

	BIND_ENUM_CONSTANT(DECAL_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(DECAL_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(DECAL_FILTER_NEAREST_MIPMAPS);
	BIND_ENUM_CONSTANT(DECAL_FILTER_LINEAR_MIPMAPS);
	BIND_ENUM_CONSTANT(DECAL_FILTER_NEAREST_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(DECAL_FILTER_LINEAR_MIPMAPS_ANISOTROPIC);

	/* GI API (affects VoxelGI and SDFGI) */

	ClassDB::bind_method(D_METHOD("gi_set_use_half_resolution", "half_resolution"), &RenderingServer::gi_set_use_half_resolution);

	/* VOXEL GI API */

	ClassDB::bind_method(D_METHOD("voxel_gi_create"), &RenderingServer::voxel_gi_create);
	ClassDB::bind_method(D_METHOD("voxel_gi_allocate_data", "voxel_gi", "to_cell_xform", "aabb", "octree_size", "octree_cells", "data_cells", "distance_field", "level_counts"), &RenderingServer::voxel_gi_allocate_data);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_octree_size", "voxel_gi"), &RenderingServer::voxel_gi_get_octree_size);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_octree_cells", "voxel_gi"), &RenderingServer::voxel_gi_get_octree_cells);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_data_cells", "voxel_gi"), &RenderingServer::voxel_gi_get_data_cells);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_distance_field", "voxel_gi"), &RenderingServer::voxel_gi_get_distance_field);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_level_counts", "voxel_gi"), &RenderingServer::voxel_gi_get_level_counts);
	ClassDB::bind_method(D_METHOD("voxel_gi_get_to_cell_xform", "voxel_gi"), &RenderingServer::voxel_gi_get_to_cell_xform);

	ClassDB::bind_method(D_METHOD("voxel_gi_set_dynamic_range", "voxel_gi", "range"), &RenderingServer::voxel_gi_set_dynamic_range);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_propagation", "voxel_gi", "amount"), &RenderingServer::voxel_gi_set_propagation);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_energy", "voxel_gi", "energy"), &RenderingServer::voxel_gi_set_energy);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_baked_exposure_normalization", "voxel_gi", "baked_exposure"), &RenderingServer::voxel_gi_set_baked_exposure_normalization);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_bias", "voxel_gi", "bias"), &RenderingServer::voxel_gi_set_bias);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_normal_bias", "voxel_gi", "bias"), &RenderingServer::voxel_gi_set_normal_bias);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_interior", "voxel_gi", "enable"), &RenderingServer::voxel_gi_set_interior);
	ClassDB::bind_method(D_METHOD("voxel_gi_set_use_two_bounces", "voxel_gi", "enable"), &RenderingServer::voxel_gi_set_use_two_bounces);

	ClassDB::bind_method(D_METHOD("voxel_gi_set_quality", "quality"), &RenderingServer::voxel_gi_set_quality);

	BIND_ENUM_CONSTANT(VOXEL_GI_QUALITY_LOW);
	BIND_ENUM_CONSTANT(VOXEL_GI_QUALITY_HIGH);

	/* LIGHTMAP */

	ClassDB::bind_method(D_METHOD("lightmap_create"), &RenderingServer::lightmap_create);
	ClassDB::bind_method(D_METHOD("lightmap_set_textures", "lightmap", "light", "uses_sh"), &RenderingServer::lightmap_set_textures);
	ClassDB::bind_method(D_METHOD("lightmap_set_probe_bounds", "lightmap", "bounds"), &RenderingServer::lightmap_set_probe_bounds);
	ClassDB::bind_method(D_METHOD("lightmap_set_probe_interior", "lightmap", "interior"), &RenderingServer::lightmap_set_probe_interior);
	ClassDB::bind_method(D_METHOD("lightmap_set_probe_capture_data", "lightmap", "points", "point_sh", "tetrahedra", "bsp_tree"), &RenderingServer::lightmap_set_probe_capture_data);
	ClassDB::bind_method(D_METHOD("lightmap_get_probe_capture_points", "lightmap"), &RenderingServer::lightmap_get_probe_capture_points);
	ClassDB::bind_method(D_METHOD("lightmap_get_probe_capture_sh", "lightmap"), &RenderingServer::lightmap_get_probe_capture_sh);
	ClassDB::bind_method(D_METHOD("lightmap_get_probe_capture_tetrahedra", "lightmap"), &RenderingServer::lightmap_get_probe_capture_tetrahedra);
	ClassDB::bind_method(D_METHOD("lightmap_get_probe_capture_bsp_tree", "lightmap"), &RenderingServer::lightmap_get_probe_capture_bsp_tree);
	ClassDB::bind_method(D_METHOD("lightmap_set_baked_exposure_normalization", "lightmap", "baked_exposure"), &RenderingServer::lightmap_set_baked_exposure_normalization);

	ClassDB::bind_method(D_METHOD("lightmap_set_probe_capture_update_speed", "speed"), &RenderingServer::lightmap_set_probe_capture_update_speed);

	/* PARTICLES API */

	ClassDB::bind_method(D_METHOD("particles_create"), &RenderingServer::particles_create);
	ClassDB::bind_method(D_METHOD("particles_set_mode", "particles", "mode"), &RenderingServer::particles_set_mode);
	ClassDB::bind_method(D_METHOD("particles_set_emitting", "particles", "emitting"), &RenderingServer::particles_set_emitting);
	ClassDB::bind_method(D_METHOD("particles_get_emitting", "particles"), &RenderingServer::particles_get_emitting);
	ClassDB::bind_method(D_METHOD("particles_set_amount", "particles", "amount"), &RenderingServer::particles_set_amount);
	ClassDB::bind_method(D_METHOD("particles_set_amount_ratio", "particles", "ratio"), &RenderingServer::particles_set_amount_ratio);
	ClassDB::bind_method(D_METHOD("particles_set_lifetime", "particles", "lifetime"), &RenderingServer::particles_set_lifetime);
	ClassDB::bind_method(D_METHOD("particles_set_one_shot", "particles", "one_shot"), &RenderingServer::particles_set_one_shot);
	ClassDB::bind_method(D_METHOD("particles_set_pre_process_time", "particles", "time"), &RenderingServer::particles_set_pre_process_time);
	ClassDB::bind_method(D_METHOD("particles_set_explosiveness_ratio", "particles", "ratio"), &RenderingServer::particles_set_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("particles_set_randomness_ratio", "particles", "ratio"), &RenderingServer::particles_set_randomness_ratio);
	ClassDB::bind_method(D_METHOD("particles_set_interp_to_end", "particles", "factor"), &RenderingServer::particles_set_interp_to_end);
	ClassDB::bind_method(D_METHOD("particles_set_emitter_velocity", "particles", "velocity"), &RenderingServer::particles_set_emitter_velocity);
	ClassDB::bind_method(D_METHOD("particles_set_custom_aabb", "particles", "aabb"), &RenderingServer::particles_set_custom_aabb);
	ClassDB::bind_method(D_METHOD("particles_set_speed_scale", "particles", "scale"), &RenderingServer::particles_set_speed_scale);
	ClassDB::bind_method(D_METHOD("particles_set_use_local_coordinates", "particles", "enable"), &RenderingServer::particles_set_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("particles_set_process_material", "particles", "material"), &RenderingServer::particles_set_process_material);
	ClassDB::bind_method(D_METHOD("particles_set_fixed_fps", "particles", "fps"), &RenderingServer::particles_set_fixed_fps);
	ClassDB::bind_method(D_METHOD("particles_set_interpolate", "particles", "enable"), &RenderingServer::particles_set_interpolate);
	ClassDB::bind_method(D_METHOD("particles_set_fractional_delta", "particles", "enable"), &RenderingServer::particles_set_fractional_delta);
	ClassDB::bind_method(D_METHOD("particles_set_collision_base_size", "particles", "size"), &RenderingServer::particles_set_collision_base_size);
	ClassDB::bind_method(D_METHOD("particles_set_transform_align", "particles", "align"), &RenderingServer::particles_set_transform_align);
	ClassDB::bind_method(D_METHOD("particles_set_trails", "particles", "enable", "length_sec"), &RenderingServer::particles_set_trails);
	ClassDB::bind_method(D_METHOD("particles_set_trail_bind_poses", "particles", "bind_poses"), &RenderingServer::_particles_set_trail_bind_poses);

	ClassDB::bind_method(D_METHOD("particles_is_inactive", "particles"), &RenderingServer::particles_is_inactive);
	ClassDB::bind_method(D_METHOD("particles_request_process", "particles"), &RenderingServer::particles_request_process);
	ClassDB::bind_method(D_METHOD("particles_restart", "particles"), &RenderingServer::particles_restart);

	ClassDB::bind_method(D_METHOD("particles_set_subemitter", "particles", "subemitter_particles"), &RenderingServer::particles_set_subemitter);
	ClassDB::bind_method(D_METHOD("particles_emit", "particles", "transform", "velocity", "color", "custom", "emit_flags"), &RenderingServer::particles_emit);

	ClassDB::bind_method(D_METHOD("particles_set_draw_order", "particles", "order"), &RenderingServer::particles_set_draw_order);
	ClassDB::bind_method(D_METHOD("particles_set_draw_passes", "particles", "count"), &RenderingServer::particles_set_draw_passes);
	ClassDB::bind_method(D_METHOD("particles_set_draw_pass_mesh", "particles", "pass", "mesh"), &RenderingServer::particles_set_draw_pass_mesh);
	ClassDB::bind_method(D_METHOD("particles_get_current_aabb", "particles"), &RenderingServer::particles_get_current_aabb);
	ClassDB::bind_method(D_METHOD("particles_set_emission_transform", "particles", "transform"), &RenderingServer::particles_set_emission_transform);

	BIND_ENUM_CONSTANT(PARTICLES_MODE_2D);
	BIND_ENUM_CONSTANT(PARTICLES_MODE_3D);

	BIND_ENUM_CONSTANT(PARTICLES_TRANSFORM_ALIGN_DISABLED);
	BIND_ENUM_CONSTANT(PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD);
	BIND_ENUM_CONSTANT(PARTICLES_TRANSFORM_ALIGN_Y_TO_VELOCITY);
	BIND_ENUM_CONSTANT(PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY);

	BIND_CONSTANT(PARTICLES_EMIT_FLAG_POSITION);
	BIND_CONSTANT(PARTICLES_EMIT_FLAG_ROTATION_SCALE);
	BIND_CONSTANT(PARTICLES_EMIT_FLAG_VELOCITY);
	BIND_CONSTANT(PARTICLES_EMIT_FLAG_COLOR);
	BIND_CONSTANT(PARTICLES_EMIT_FLAG_CUSTOM);

	BIND_ENUM_CONSTANT(PARTICLES_DRAW_ORDER_INDEX);
	BIND_ENUM_CONSTANT(PARTICLES_DRAW_ORDER_LIFETIME);
	BIND_ENUM_CONSTANT(PARTICLES_DRAW_ORDER_REVERSE_LIFETIME);
	BIND_ENUM_CONSTANT(PARTICLES_DRAW_ORDER_VIEW_DEPTH);

	/* PARTICLES COLLISION */

	ClassDB::bind_method(D_METHOD("particles_collision_create"), &RenderingServer::particles_collision_create);
	ClassDB::bind_method(D_METHOD("particles_collision_set_collision_type", "particles_collision", "type"), &RenderingServer::particles_collision_set_collision_type);
	ClassDB::bind_method(D_METHOD("particles_collision_set_cull_mask", "particles_collision", "mask"), &RenderingServer::particles_collision_set_cull_mask);
	ClassDB::bind_method(D_METHOD("particles_collision_set_sphere_radius", "particles_collision", "radius"), &RenderingServer::particles_collision_set_sphere_radius);
	ClassDB::bind_method(D_METHOD("particles_collision_set_box_extents", "particles_collision", "extents"), &RenderingServer::particles_collision_set_box_extents);
	ClassDB::bind_method(D_METHOD("particles_collision_set_attractor_strength", "particles_collision", "strength"), &RenderingServer::particles_collision_set_attractor_strength);
	ClassDB::bind_method(D_METHOD("particles_collision_set_attractor_directionality", "particles_collision", "amount"), &RenderingServer::particles_collision_set_attractor_directionality);
	ClassDB::bind_method(D_METHOD("particles_collision_set_attractor_attenuation", "particles_collision", "curve"), &RenderingServer::particles_collision_set_attractor_attenuation);
	ClassDB::bind_method(D_METHOD("particles_collision_set_field_texture", "particles_collision", "texture"), &RenderingServer::particles_collision_set_field_texture);

	ClassDB::bind_method(D_METHOD("particles_collision_height_field_update", "particles_collision"), &RenderingServer::particles_collision_height_field_update);
	ClassDB::bind_method(D_METHOD("particles_collision_set_height_field_resolution", "particles_collision", "resolution"), &RenderingServer::particles_collision_set_height_field_resolution);

	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_BOX_ATTRACT);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_BOX_COLLIDE);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_SDF_COLLIDE);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE);

	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_512);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_2048);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_4096);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_8192);
	BIND_ENUM_CONSTANT(PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX);

	/* FOG VOLUMES */

	ClassDB::bind_method(D_METHOD("fog_volume_create"), &RenderingServer::fog_volume_create);
	ClassDB::bind_method(D_METHOD("fog_volume_set_shape", "fog_volume", "shape"), &RenderingServer::fog_volume_set_shape);
	ClassDB::bind_method(D_METHOD("fog_volume_set_size", "fog_volume", "size"), &RenderingServer::fog_volume_set_size);
	ClassDB::bind_method(D_METHOD("fog_volume_set_material", "fog_volume", "material"), &RenderingServer::fog_volume_set_material);

	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_ELLIPSOID);
	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_CONE);
	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_CYLINDER);
	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_BOX);
	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_WORLD);
	BIND_ENUM_CONSTANT(FOG_VOLUME_SHAPE_MAX);

	/* VISIBILITY NOTIFIER */

	ClassDB::bind_method(D_METHOD("visibility_notifier_create"), &RenderingServer::visibility_notifier_create);
	ClassDB::bind_method(D_METHOD("visibility_notifier_set_aabb", "notifier", "aabb"), &RenderingServer::visibility_notifier_set_aabb);
	ClassDB::bind_method(D_METHOD("visibility_notifier_set_callbacks", "notifier", "enter_callable", "exit_callable"), &RenderingServer::visibility_notifier_set_callbacks);

	/* OCCLUDER */

	ClassDB::bind_method(D_METHOD("occluder_create"), &RenderingServer::occluder_create);
	ClassDB::bind_method(D_METHOD("occluder_set_mesh", "occluder", "vertices", "indices"), &RenderingServer::occluder_set_mesh);

	/* CAMERA */

	ClassDB::bind_method(D_METHOD("camera_create"), &RenderingServer::camera_create);
	ClassDB::bind_method(D_METHOD("camera_set_perspective", "camera", "fovy_degrees", "z_near", "z_far"), &RenderingServer::camera_set_perspective);
	ClassDB::bind_method(D_METHOD("camera_set_orthogonal", "camera", "size", "z_near", "z_far"), &RenderingServer::camera_set_orthogonal);
	ClassDB::bind_method(D_METHOD("camera_set_frustum", "camera", "size", "offset", "z_near", "z_far"), &RenderingServer::camera_set_frustum);
	ClassDB::bind_method(D_METHOD("camera_set_transform", "camera", "transform"), &RenderingServer::camera_set_transform);
	ClassDB::bind_method(D_METHOD("camera_set_cull_mask", "camera", "layers"), &RenderingServer::camera_set_cull_mask);
	ClassDB::bind_method(D_METHOD("camera_set_environment", "camera", "env"), &RenderingServer::camera_set_environment);
	ClassDB::bind_method(D_METHOD("camera_set_camera_attributes", "camera", "effects"), &RenderingServer::camera_set_camera_attributes);
	ClassDB::bind_method(D_METHOD("camera_set_compositor", "camera", "compositor"), &RenderingServer::camera_set_compositor);
	ClassDB::bind_method(D_METHOD("camera_set_use_vertical_aspect", "camera", "enable"), &RenderingServer::camera_set_use_vertical_aspect);

	/* VIEWPORT */

	ClassDB::bind_method(D_METHOD("viewport_create"), &RenderingServer::viewport_create);
	ClassDB::bind_method(D_METHOD("viewport_set_use_xr", "viewport", "use_xr"), &RenderingServer::viewport_set_use_xr);
	ClassDB::bind_method(D_METHOD("viewport_set_size", "viewport", "width", "height"), &RenderingServer::viewport_set_size);
	ClassDB::bind_method(D_METHOD("viewport_set_active", "viewport", "active"), &RenderingServer::viewport_set_active);
	ClassDB::bind_method(D_METHOD("viewport_set_parent_viewport", "viewport", "parent_viewport"), &RenderingServer::viewport_set_parent_viewport);
	ClassDB::bind_method(D_METHOD("viewport_attach_to_screen", "viewport", "rect", "screen"), &RenderingServer::viewport_attach_to_screen, DEFVAL(Rect2()), DEFVAL(DisplayServer::MAIN_WINDOW_ID));
	ClassDB::bind_method(D_METHOD("viewport_set_render_direct_to_screen", "viewport", "enabled"), &RenderingServer::viewport_set_render_direct_to_screen);
	ClassDB::bind_method(D_METHOD("viewport_set_canvas_cull_mask", "viewport", "canvas_cull_mask"), &RenderingServer::viewport_set_canvas_cull_mask);

	ClassDB::bind_method(D_METHOD("viewport_set_scaling_3d_mode", "viewport", "scaling_3d_mode"), &RenderingServer::viewport_set_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("viewport_set_scaling_3d_scale", "viewport", "scale"), &RenderingServer::viewport_set_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("viewport_set_fsr_sharpness", "viewport", "sharpness"), &RenderingServer::viewport_set_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("viewport_set_texture_mipmap_bias", "viewport", "mipmap_bias"), &RenderingServer::viewport_set_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("viewport_set_update_mode", "viewport", "update_mode"), &RenderingServer::viewport_set_update_mode);
	ClassDB::bind_method(D_METHOD("viewport_get_update_mode", "viewport"), &RenderingServer::viewport_get_update_mode);
	ClassDB::bind_method(D_METHOD("viewport_set_clear_mode", "viewport", "clear_mode"), &RenderingServer::viewport_set_clear_mode);
	ClassDB::bind_method(D_METHOD("viewport_get_render_target", "viewport"), &RenderingServer::viewport_get_render_target);
	ClassDB::bind_method(D_METHOD("viewport_get_texture", "viewport"), &RenderingServer::viewport_get_texture);
	ClassDB::bind_method(D_METHOD("viewport_set_disable_3d", "viewport", "disable"), &RenderingServer::viewport_set_disable_3d);
	ClassDB::bind_method(D_METHOD("viewport_set_disable_2d", "viewport", "disable"), &RenderingServer::viewport_set_disable_2d);
	ClassDB::bind_method(D_METHOD("viewport_set_environment_mode", "viewport", "mode"), &RenderingServer::viewport_set_environment_mode);
	ClassDB::bind_method(D_METHOD("viewport_attach_camera", "viewport", "camera"), &RenderingServer::viewport_attach_camera);
	ClassDB::bind_method(D_METHOD("viewport_set_scenario", "viewport", "scenario"), &RenderingServer::viewport_set_scenario);
	ClassDB::bind_method(D_METHOD("viewport_attach_canvas", "viewport", "canvas"), &RenderingServer::viewport_attach_canvas);
	ClassDB::bind_method(D_METHOD("viewport_remove_canvas", "viewport", "canvas"), &RenderingServer::viewport_remove_canvas);
	ClassDB::bind_method(D_METHOD("viewport_set_snap_2d_transforms_to_pixel", "viewport", "enabled"), &RenderingServer::viewport_set_snap_2d_transforms_to_pixel);
	ClassDB::bind_method(D_METHOD("viewport_set_snap_2d_vertices_to_pixel", "viewport", "enabled"), &RenderingServer::viewport_set_snap_2d_vertices_to_pixel);

	ClassDB::bind_method(D_METHOD("viewport_set_default_canvas_item_texture_filter", "viewport", "filter"), &RenderingServer::viewport_set_default_canvas_item_texture_filter);
	ClassDB::bind_method(D_METHOD("viewport_set_default_canvas_item_texture_repeat", "viewport", "repeat"), &RenderingServer::viewport_set_default_canvas_item_texture_repeat);

	ClassDB::bind_method(D_METHOD("viewport_set_canvas_transform", "viewport", "canvas", "offset"), &RenderingServer::viewport_set_canvas_transform);
	ClassDB::bind_method(D_METHOD("viewport_set_canvas_stacking", "viewport", "canvas", "layer", "sublayer"), &RenderingServer::viewport_set_canvas_stacking);

	ClassDB::bind_method(D_METHOD("viewport_set_transparent_background", "viewport", "enabled"), &RenderingServer::viewport_set_transparent_background);
	ClassDB::bind_method(D_METHOD("viewport_set_global_canvas_transform", "viewport", "transform"), &RenderingServer::viewport_set_global_canvas_transform);

	ClassDB::bind_method(D_METHOD("viewport_set_sdf_oversize_and_scale", "viewport", "oversize", "scale"), &RenderingServer::viewport_set_sdf_oversize_and_scale);

	ClassDB::bind_method(D_METHOD("viewport_set_positional_shadow_atlas_size", "viewport", "size", "use_16_bits"), &RenderingServer::viewport_set_positional_shadow_atlas_size, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("viewport_set_positional_shadow_atlas_quadrant_subdivision", "viewport", "quadrant", "subdivision"), &RenderingServer::viewport_set_positional_shadow_atlas_quadrant_subdivision);
	ClassDB::bind_method(D_METHOD("viewport_set_msaa_3d", "viewport", "msaa"), &RenderingServer::viewport_set_msaa_3d);
	ClassDB::bind_method(D_METHOD("viewport_set_msaa_2d", "viewport", "msaa"), &RenderingServer::viewport_set_msaa_2d);
	ClassDB::bind_method(D_METHOD("viewport_set_use_hdr_2d", "viewport", "enabled"), &RenderingServer::viewport_set_use_hdr_2d);
	ClassDB::bind_method(D_METHOD("viewport_set_screen_space_aa", "viewport", "mode"), &RenderingServer::viewport_set_screen_space_aa);
	ClassDB::bind_method(D_METHOD("viewport_set_use_taa", "viewport", "enable"), &RenderingServer::viewport_set_use_taa);
	ClassDB::bind_method(D_METHOD("viewport_set_use_debanding", "viewport", "enable"), &RenderingServer::viewport_set_use_debanding);
	ClassDB::bind_method(D_METHOD("viewport_set_use_occlusion_culling", "viewport", "enable"), &RenderingServer::viewport_set_use_occlusion_culling);
	ClassDB::bind_method(D_METHOD("viewport_set_occlusion_rays_per_thread", "rays_per_thread"), &RenderingServer::viewport_set_occlusion_rays_per_thread);
	ClassDB::bind_method(D_METHOD("viewport_set_occlusion_culling_build_quality", "quality"), &RenderingServer::viewport_set_occlusion_culling_build_quality);

	ClassDB::bind_method(D_METHOD("viewport_get_render_info", "viewport", "type", "info"), &RenderingServer::viewport_get_render_info);
	ClassDB::bind_method(D_METHOD("viewport_set_debug_draw", "viewport", "draw"), &RenderingServer::viewport_set_debug_draw);

	ClassDB::bind_method(D_METHOD("viewport_set_measure_render_time", "viewport", "enable"), &RenderingServer::viewport_set_measure_render_time);
	ClassDB::bind_method(D_METHOD("viewport_get_measured_render_time_cpu", "viewport"), &RenderingServer::viewport_get_measured_render_time_cpu);

	ClassDB::bind_method(D_METHOD("viewport_get_measured_render_time_gpu", "viewport"), &RenderingServer::viewport_get_measured_render_time_gpu);

	ClassDB::bind_method(D_METHOD("viewport_set_vrs_mode", "viewport", "mode"), &RenderingServer::viewport_set_vrs_mode);
	ClassDB::bind_method(D_METHOD("viewport_set_vrs_update_mode", "viewport", "mode"), &RenderingServer::viewport_set_vrs_update_mode);
	ClassDB::bind_method(D_METHOD("viewport_set_vrs_texture", "viewport", "texture"), &RenderingServer::viewport_set_vrs_texture);

	BIND_ENUM_CONSTANT(VIEWPORT_SCALING_3D_MODE_BILINEAR);
	BIND_ENUM_CONSTANT(VIEWPORT_SCALING_3D_MODE_FSR);
	BIND_ENUM_CONSTANT(VIEWPORT_SCALING_3D_MODE_FSR2);
	BIND_ENUM_CONSTANT(VIEWPORT_SCALING_3D_MODE_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_UPDATE_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_UPDATE_ONCE); // Then goes to disabled, must be manually updated.
	BIND_ENUM_CONSTANT(VIEWPORT_UPDATE_WHEN_VISIBLE); // Default
	BIND_ENUM_CONSTANT(VIEWPORT_UPDATE_WHEN_PARENT_VISIBLE);
	BIND_ENUM_CONSTANT(VIEWPORT_UPDATE_ALWAYS);

	BIND_ENUM_CONSTANT(VIEWPORT_CLEAR_ALWAYS);
	BIND_ENUM_CONSTANT(VIEWPORT_CLEAR_NEVER);
	BIND_ENUM_CONSTANT(VIEWPORT_CLEAR_ONLY_NEXT_FRAME);

	BIND_ENUM_CONSTANT(VIEWPORT_ENVIRONMENT_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_ENVIRONMENT_ENABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_ENVIRONMENT_INHERIT);
	BIND_ENUM_CONSTANT(VIEWPORT_ENVIRONMENT_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_SDF_OVERSIZE_100_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_OVERSIZE_120_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_OVERSIZE_150_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_OVERSIZE_200_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_OVERSIZE_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_SDF_SCALE_100_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_SCALE_50_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_SCALE_25_PERCENT);
	BIND_ENUM_CONSTANT(VIEWPORT_SDF_SCALE_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_MSAA_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_MSAA_2X);
	BIND_ENUM_CONSTANT(VIEWPORT_MSAA_4X);
	BIND_ENUM_CONSTANT(VIEWPORT_MSAA_8X);
	BIND_ENUM_CONSTANT(VIEWPORT_MSAA_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_SCREEN_SPACE_AA_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_SCREEN_SPACE_AA_FXAA);
	BIND_ENUM_CONSTANT(VIEWPORT_SCREEN_SPACE_AA_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_OCCLUSION_BUILD_QUALITY_LOW);
	BIND_ENUM_CONSTANT(VIEWPORT_OCCLUSION_BUILD_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(VIEWPORT_OCCLUSION_BUILD_QUALITY_HIGH);

	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_TYPE_VISIBLE);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_TYPE_SHADOW);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_TYPE_CANVAS);
	BIND_ENUM_CONSTANT(VIEWPORT_RENDER_INFO_TYPE_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_UNSHADED);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_LIGHTING);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_OVERDRAW);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_WIREFRAME);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_NORMAL_BUFFER);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_VOXEL_GI_ALBEDO);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_VOXEL_GI_LIGHTING);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_VOXEL_GI_EMISSION);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SHADOW_ATLAS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SCENE_LUMINANCE);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SSAO);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SSIL);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_PSSM_SPLITS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_DECAL_ATLAS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SDFGI);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_SDFGI_PROBES);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_GI_BUFFER);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_DISABLE_LOD);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_CLUSTER_OMNI_LIGHTS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_CLUSTER_SPOT_LIGHTS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_CLUSTER_DECALS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_CLUSTER_REFLECTION_PROBES);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_OCCLUDERS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_MOTION_VECTORS);
	BIND_ENUM_CONSTANT(VIEWPORT_DEBUG_DRAW_INTERNAL_BUFFER);

	BIND_ENUM_CONSTANT(VIEWPORT_VRS_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_TEXTURE);
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_XR);
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_MAX);

	BIND_ENUM_CONSTANT(VIEWPORT_VRS_UPDATE_DISABLED);
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_UPDATE_ONCE); // Then goes to disabled, must be manually updated.
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_UPDATE_ALWAYS);
	BIND_ENUM_CONSTANT(VIEWPORT_VRS_UPDATE_MAX);

	/* SKY API */

	ClassDB::bind_method(D_METHOD("sky_create"), &RenderingServer::sky_create);
	ClassDB::bind_method(D_METHOD("sky_set_radiance_size", "sky", "radiance_size"), &RenderingServer::sky_set_radiance_size);
	ClassDB::bind_method(D_METHOD("sky_set_mode", "sky", "mode"), &RenderingServer::sky_set_mode);
	ClassDB::bind_method(D_METHOD("sky_set_material", "sky", "material"), &RenderingServer::sky_set_material);
	ClassDB::bind_method(D_METHOD("sky_bake_panorama", "sky", "energy", "bake_irradiance", "size"), &RenderingServer::sky_bake_panorama);

	BIND_ENUM_CONSTANT(SKY_MODE_AUTOMATIC);
	BIND_ENUM_CONSTANT(SKY_MODE_QUALITY);
	BIND_ENUM_CONSTANT(SKY_MODE_INCREMENTAL);
	BIND_ENUM_CONSTANT(SKY_MODE_REALTIME);

	/* COMPOSITOR EFFECT API */

	ClassDB::bind_method(D_METHOD("compositor_effect_create"), &RenderingServer::compositor_effect_create);
	ClassDB::bind_method(D_METHOD("compositor_effect_set_enabled", "effect", "enabled"), &RenderingServer::compositor_effect_set_enabled);
	ClassDB::bind_method(D_METHOD("compositor_effect_set_callback", "effect", "callback_type", "callback"), &RenderingServer::compositor_effect_set_callback);
	ClassDB::bind_method(D_METHOD("compositor_effect_set_flag", "effect", "flag", "set"), &RenderingServer::compositor_effect_set_flag);

	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_FLAG_ACCESS_RESOLVED_COLOR);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_FLAG_ACCESS_RESOLVED_DEPTH);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_FLAG_NEEDS_MOTION_VECTORS);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_FLAG_NEEDS_ROUGHNESS);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_FLAG_NEEDS_SEPARATE_SPECULAR);

	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_OPAQUE);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_SKY);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
	BIND_ENUM_CONSTANT(COMPOSITOR_EFFECT_CALLBACK_TYPE_ANY);

	/* COMPOSITOR */

	ClassDB::bind_method(D_METHOD("compositor_create"), &RenderingServer::compositor_create);

	ClassDB::bind_method(D_METHOD("compositor_set_compositor_effects", "compositor", "effects"), &RenderingServer::compositor_set_compositor_effects);

	/* ENVIRONMENT */

	ClassDB::bind_method(D_METHOD("environment_create"), &RenderingServer::environment_create);
	ClassDB::bind_method(D_METHOD("environment_set_background", "env", "bg"), &RenderingServer::environment_set_background);
	ClassDB::bind_method(D_METHOD("environment_set_sky", "env", "sky"), &RenderingServer::environment_set_sky);
	ClassDB::bind_method(D_METHOD("environment_set_sky_custom_fov", "env", "scale"), &RenderingServer::environment_set_sky_custom_fov);
	ClassDB::bind_method(D_METHOD("environment_set_sky_orientation", "env", "orientation"), &RenderingServer::environment_set_sky_orientation);
	ClassDB::bind_method(D_METHOD("environment_set_bg_color", "env", "color"), &RenderingServer::environment_set_bg_color);
	ClassDB::bind_method(D_METHOD("environment_set_bg_energy", "env", "multiplier", "exposure_value"), &RenderingServer::environment_set_bg_energy);
	ClassDB::bind_method(D_METHOD("environment_set_canvas_max_layer", "env", "max_layer"), &RenderingServer::environment_set_canvas_max_layer);
	ClassDB::bind_method(D_METHOD("environment_set_ambient_light", "env", "color", "ambient", "energy", "sky_contibution", "reflection_source"), &RenderingServer::environment_set_ambient_light, DEFVAL(RS::ENV_AMBIENT_SOURCE_BG), DEFVAL(1.0), DEFVAL(0.0), DEFVAL(RS::ENV_REFLECTION_SOURCE_BG));
	ClassDB::bind_method(D_METHOD("environment_set_glow", "env", "enable", "levels", "intensity", "strength", "mix", "bloom_threshold", "blend_mode", "hdr_bleed_threshold", "hdr_bleed_scale", "hdr_luminance_cap", "glow_map_strength", "glow_map"), &RenderingServer::environment_set_glow);
	ClassDB::bind_method(D_METHOD("environment_set_tonemap", "env", "tone_mapper", "exposure", "white"), &RenderingServer::environment_set_tonemap);
	ClassDB::bind_method(D_METHOD("environment_set_adjustment", "env", "enable", "brightness", "contrast", "saturation", "use_1d_color_correction", "color_correction"), &RenderingServer::environment_set_adjustment);
	ClassDB::bind_method(D_METHOD("environment_set_ssr", "env", "enable", "max_steps", "fade_in", "fade_out", "depth_tolerance"), &RenderingServer::environment_set_ssr);
	ClassDB::bind_method(D_METHOD("environment_set_ssao", "env", "enable", "radius", "intensity", "power", "detail", "horizon", "sharpness", "light_affect", "ao_channel_affect"), &RenderingServer::environment_set_ssao);
	ClassDB::bind_method(D_METHOD("environment_set_fog", "env", "enable", "light_color", "light_energy", "sun_scatter", "density", "height", "height_density", "aerial_perspective", "sky_affect", "fog_mode"), &RenderingServer::environment_set_fog, DEFVAL(RS::ENV_FOG_MODE_EXPONENTIAL));
	ClassDB::bind_method(D_METHOD("environment_set_sdfgi", "env", "enable", "cascades", "min_cell_size", "y_scale", "use_occlusion", "bounce_feedback", "read_sky", "energy", "normal_bias", "probe_bias"), &RenderingServer::environment_set_sdfgi);
	ClassDB::bind_method(D_METHOD("environment_set_volumetric_fog", "env", "enable", "density", "albedo", "emission", "emission_energy", "anisotropy", "length", "p_detail_spread", "gi_inject", "temporal_reprojection", "temporal_reprojection_amount", "ambient_inject", "sky_affect"), &RenderingServer::environment_set_volumetric_fog);

	ClassDB::bind_method(D_METHOD("environment_glow_set_use_bicubic_upscale", "enable"), &RenderingServer::environment_glow_set_use_bicubic_upscale);
	ClassDB::bind_method(D_METHOD("environment_set_ssr_roughness_quality", "quality"), &RenderingServer::environment_set_ssr_roughness_quality);
	ClassDB::bind_method(D_METHOD("environment_set_ssao_quality", "quality", "half_size", "adaptive_target", "blur_passes", "fadeout_from", "fadeout_to"), &RenderingServer::environment_set_ssao_quality);
	ClassDB::bind_method(D_METHOD("environment_set_ssil_quality", "quality", "half_size", "adaptive_target", "blur_passes", "fadeout_from", "fadeout_to"), &RenderingServer::environment_set_ssil_quality);
	ClassDB::bind_method(D_METHOD("environment_set_sdfgi_ray_count", "ray_count"), &RenderingServer::environment_set_sdfgi_ray_count);
	ClassDB::bind_method(D_METHOD("environment_set_sdfgi_frames_to_converge", "frames"), &RenderingServer::environment_set_sdfgi_frames_to_converge);
	ClassDB::bind_method(D_METHOD("environment_set_sdfgi_frames_to_update_light", "frames"), &RenderingServer::environment_set_sdfgi_frames_to_update_light);
	ClassDB::bind_method(D_METHOD("environment_set_volumetric_fog_volume_size", "size", "depth"), &RenderingServer::environment_set_volumetric_fog_volume_size);
	ClassDB::bind_method(D_METHOD("environment_set_volumetric_fog_filter_active", "active"), &RenderingServer::environment_set_volumetric_fog_filter_active);

	ClassDB::bind_method(D_METHOD("environment_bake_panorama", "environment", "bake_irradiance", "size"), &RenderingServer::environment_bake_panorama);

	ClassDB::bind_method(D_METHOD("screen_space_roughness_limiter_set_active", "enable", "amount", "limit"), &RenderingServer::screen_space_roughness_limiter_set_active);
	ClassDB::bind_method(D_METHOD("sub_surface_scattering_set_quality", "quality"), &RenderingServer::sub_surface_scattering_set_quality);
	ClassDB::bind_method(D_METHOD("sub_surface_scattering_set_scale", "scale", "depth_scale"), &RenderingServer::sub_surface_scattering_set_scale);

	BIND_ENUM_CONSTANT(ENV_BG_CLEAR_COLOR);
	BIND_ENUM_CONSTANT(ENV_BG_COLOR);
	BIND_ENUM_CONSTANT(ENV_BG_SKY);
	BIND_ENUM_CONSTANT(ENV_BG_CANVAS);
	BIND_ENUM_CONSTANT(ENV_BG_KEEP);
	BIND_ENUM_CONSTANT(ENV_BG_CAMERA_FEED);
	BIND_ENUM_CONSTANT(ENV_BG_MAX);

	BIND_ENUM_CONSTANT(ENV_AMBIENT_SOURCE_BG);
	BIND_ENUM_CONSTANT(ENV_AMBIENT_SOURCE_DISABLED);
	BIND_ENUM_CONSTANT(ENV_AMBIENT_SOURCE_COLOR);
	BIND_ENUM_CONSTANT(ENV_AMBIENT_SOURCE_SKY);

	BIND_ENUM_CONSTANT(ENV_REFLECTION_SOURCE_BG);
	BIND_ENUM_CONSTANT(ENV_REFLECTION_SOURCE_DISABLED);
	BIND_ENUM_CONSTANT(ENV_REFLECTION_SOURCE_SKY);

	BIND_ENUM_CONSTANT(ENV_GLOW_BLEND_MODE_ADDITIVE);
	BIND_ENUM_CONSTANT(ENV_GLOW_BLEND_MODE_SCREEN);
	BIND_ENUM_CONSTANT(ENV_GLOW_BLEND_MODE_SOFTLIGHT);
	BIND_ENUM_CONSTANT(ENV_GLOW_BLEND_MODE_REPLACE);
	BIND_ENUM_CONSTANT(ENV_GLOW_BLEND_MODE_MIX);

	BIND_ENUM_CONSTANT(ENV_FOG_MODE_EXPONENTIAL);
	BIND_ENUM_CONSTANT(ENV_FOG_MODE_DEPTH);

	BIND_ENUM_CONSTANT(ENV_TONE_MAPPER_LINEAR);
	BIND_ENUM_CONSTANT(ENV_TONE_MAPPER_REINHARD);
	BIND_ENUM_CONSTANT(ENV_TONE_MAPPER_FILMIC);
	BIND_ENUM_CONSTANT(ENV_TONE_MAPPER_ACES);

	BIND_ENUM_CONSTANT(ENV_SSR_ROUGHNESS_QUALITY_DISABLED);
	BIND_ENUM_CONSTANT(ENV_SSR_ROUGHNESS_QUALITY_LOW);
	BIND_ENUM_CONSTANT(ENV_SSR_ROUGHNESS_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(ENV_SSR_ROUGHNESS_QUALITY_HIGH);

	BIND_ENUM_CONSTANT(ENV_SSAO_QUALITY_VERY_LOW);
	BIND_ENUM_CONSTANT(ENV_SSAO_QUALITY_LOW);
	BIND_ENUM_CONSTANT(ENV_SSAO_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(ENV_SSAO_QUALITY_HIGH);
	BIND_ENUM_CONSTANT(ENV_SSAO_QUALITY_ULTRA);

	BIND_ENUM_CONSTANT(ENV_SSIL_QUALITY_VERY_LOW);
	BIND_ENUM_CONSTANT(ENV_SSIL_QUALITY_LOW);
	BIND_ENUM_CONSTANT(ENV_SSIL_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(ENV_SSIL_QUALITY_HIGH);
	BIND_ENUM_CONSTANT(ENV_SSIL_QUALITY_ULTRA);

	BIND_ENUM_CONSTANT(ENV_SDFGI_Y_SCALE_50_PERCENT);
	BIND_ENUM_CONSTANT(ENV_SDFGI_Y_SCALE_75_PERCENT);
	BIND_ENUM_CONSTANT(ENV_SDFGI_Y_SCALE_100_PERCENT);

	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_4);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_8);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_16);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_32);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_64);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_96);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_128);
	BIND_ENUM_CONSTANT(ENV_SDFGI_RAY_COUNT_MAX);

	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_5_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_10_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_15_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_20_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_25_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_IN_30_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_CONVERGE_MAX);

	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_IN_1_FRAME);
	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_IN_2_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_IN_4_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_IN_8_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_IN_16_FRAMES);
	BIND_ENUM_CONSTANT(ENV_SDFGI_UPDATE_LIGHT_MAX);

	BIND_ENUM_CONSTANT(SUB_SURFACE_SCATTERING_QUALITY_DISABLED);
	BIND_ENUM_CONSTANT(SUB_SURFACE_SCATTERING_QUALITY_LOW);
	BIND_ENUM_CONSTANT(SUB_SURFACE_SCATTERING_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(SUB_SURFACE_SCATTERING_QUALITY_HIGH);

	/* CAMERA EFFECTS */

	ClassDB::bind_method(D_METHOD("camera_attributes_create"), &RenderingServer::camera_attributes_create);

	ClassDB::bind_method(D_METHOD("camera_attributes_set_dof_blur_quality", "quality", "use_jitter"), &RenderingServer::camera_attributes_set_dof_blur_quality);
	ClassDB::bind_method(D_METHOD("camera_attributes_set_dof_blur_bokeh_shape", "shape"), &RenderingServer::camera_attributes_set_dof_blur_bokeh_shape);

	ClassDB::bind_method(D_METHOD("camera_attributes_set_dof_blur", "camera_attributes", "far_enable", "far_distance", "far_transition", "near_enable", "near_distance", "near_transition", "amount"), &RenderingServer::camera_attributes_set_dof_blur);
	ClassDB::bind_method(D_METHOD("camera_attributes_set_exposure", "camera_attributes", "multiplier", "normalization"), &RenderingServer::camera_attributes_set_exposure);
	ClassDB::bind_method(D_METHOD("camera_attributes_set_auto_exposure", "camera_attributes", "enable", "min_sensitivity", "max_sensitivity", "speed", "scale"), &RenderingServer::camera_attributes_set_auto_exposure);

	BIND_ENUM_CONSTANT(DOF_BOKEH_BOX);
	BIND_ENUM_CONSTANT(DOF_BOKEH_HEXAGON);
	BIND_ENUM_CONSTANT(DOF_BOKEH_CIRCLE);

	BIND_ENUM_CONSTANT(DOF_BLUR_QUALITY_VERY_LOW);
	BIND_ENUM_CONSTANT(DOF_BLUR_QUALITY_LOW);
	BIND_ENUM_CONSTANT(DOF_BLUR_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(DOF_BLUR_QUALITY_HIGH);

	/* SCENARIO */

	ClassDB::bind_method(D_METHOD("scenario_create"), &RenderingServer::scenario_create);
	ClassDB::bind_method(D_METHOD("scenario_set_environment", "scenario", "environment"), &RenderingServer::scenario_set_environment);
	ClassDB::bind_method(D_METHOD("scenario_set_fallback_environment", "scenario", "environment"), &RenderingServer::scenario_set_fallback_environment);
	ClassDB::bind_method(D_METHOD("scenario_set_camera_attributes", "scenario", "effects"), &RenderingServer::scenario_set_camera_attributes);
	ClassDB::bind_method(D_METHOD("scenario_set_compositor", "scenario", "compositor"), &RenderingServer::scenario_set_compositor);

	/* INSTANCE */

	ClassDB::bind_method(D_METHOD("instance_create2", "base", "scenario"), &RenderingServer::instance_create2);
	ClassDB::bind_method(D_METHOD("instance_create"), &RenderingServer::instance_create);
	ClassDB::bind_method(D_METHOD("instance_set_base", "instance", "base"), &RenderingServer::instance_set_base);
	ClassDB::bind_method(D_METHOD("instance_set_scenario", "instance", "scenario"), &RenderingServer::instance_set_scenario);
	ClassDB::bind_method(D_METHOD("instance_set_layer_mask", "instance", "mask"), &RenderingServer::instance_set_layer_mask);
	ClassDB::bind_method(D_METHOD("instance_set_pivot_data", "instance", "sorting_offset", "use_aabb_center"), &RenderingServer::instance_set_pivot_data);
	ClassDB::bind_method(D_METHOD("instance_set_transform", "instance", "transform"), &RenderingServer::instance_set_transform);
	ClassDB::bind_method(D_METHOD("instance_set_interpolated", "instance", "interpolated"), &RenderingServer::instance_set_interpolated);
	ClassDB::bind_method(D_METHOD("instance_reset_physics_interpolation", "instance"), &RenderingServer::instance_reset_physics_interpolation);
	ClassDB::bind_method(D_METHOD("instance_attach_object_instance_id", "instance", "id"), &RenderingServer::instance_attach_object_instance_id);
	ClassDB::bind_method(D_METHOD("instance_set_blend_shape_weight", "instance", "shape", "weight"), &RenderingServer::instance_set_blend_shape_weight);
	ClassDB::bind_method(D_METHOD("instance_set_surface_override_material", "instance", "surface", "material"), &RenderingServer::instance_set_surface_override_material);
	ClassDB::bind_method(D_METHOD("instance_set_visible", "instance", "visible"), &RenderingServer::instance_set_visible);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_transparency", "instance", "transparency"), &RenderingServer::instance_geometry_set_transparency);

	ClassDB::bind_method(D_METHOD("instance_set_custom_aabb", "instance", "aabb"), &RenderingServer::instance_set_custom_aabb);

	ClassDB::bind_method(D_METHOD("instance_attach_skeleton", "instance", "skeleton"), &RenderingServer::instance_attach_skeleton);
	ClassDB::bind_method(D_METHOD("instance_set_extra_visibility_margin", "instance", "margin"), &RenderingServer::instance_set_extra_visibility_margin);
	ClassDB::bind_method(D_METHOD("instance_set_visibility_parent", "instance", "parent"), &RenderingServer::instance_set_visibility_parent);
	ClassDB::bind_method(D_METHOD("instance_set_ignore_culling", "instance", "enabled"), &RenderingServer::instance_set_ignore_culling);

	ClassDB::bind_method(D_METHOD("instance_geometry_set_flag", "instance", "flag", "enabled"), &RenderingServer::instance_geometry_set_flag);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_cast_shadows_setting", "instance", "shadow_casting_setting"), &RenderingServer::instance_geometry_set_cast_shadows_setting);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_material_override", "instance", "material"), &RenderingServer::instance_geometry_set_material_override);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_material_overlay", "instance", "material"), &RenderingServer::instance_geometry_set_material_overlay);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_visibility_range", "instance", "min", "max", "min_margin", "max_margin", "fade_mode"), &RenderingServer::instance_geometry_set_visibility_range);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_lightmap", "instance", "lightmap", "lightmap_uv_scale", "lightmap_slice"), &RenderingServer::instance_geometry_set_lightmap);
	ClassDB::bind_method(D_METHOD("instance_geometry_set_lod_bias", "instance", "lod_bias"), &RenderingServer::instance_geometry_set_lod_bias);

	ClassDB::bind_method(D_METHOD("instance_geometry_set_shader_parameter", "instance", "parameter", "value"), &RenderingServer::instance_geometry_set_shader_parameter);
	ClassDB::bind_method(D_METHOD("instance_geometry_get_shader_parameter", "instance", "parameter"), &RenderingServer::instance_geometry_get_shader_parameter);
	ClassDB::bind_method(D_METHOD("instance_geometry_get_shader_parameter_default_value", "instance", "parameter"), &RenderingServer::instance_geometry_get_shader_parameter_default_value);
	ClassDB::bind_method(D_METHOD("instance_geometry_get_shader_parameter_list", "instance"), &RenderingServer::_instance_geometry_get_shader_parameter_list);

	ClassDB::bind_method(D_METHOD("instances_cull_aabb", "aabb", "scenario"), &RenderingServer::_instances_cull_aabb_bind, DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("instances_cull_ray", "from", "to", "scenario"), &RenderingServer::_instances_cull_ray_bind, DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("instances_cull_convex", "convex", "scenario"), &RenderingServer::_instances_cull_convex_bind, DEFVAL(RID()));

	BIND_ENUM_CONSTANT(INSTANCE_NONE);
	BIND_ENUM_CONSTANT(INSTANCE_MESH);
	BIND_ENUM_CONSTANT(INSTANCE_MULTIMESH);
	BIND_ENUM_CONSTANT(INSTANCE_PARTICLES);
	BIND_ENUM_CONSTANT(INSTANCE_PARTICLES_COLLISION);
	BIND_ENUM_CONSTANT(INSTANCE_LIGHT);
	BIND_ENUM_CONSTANT(INSTANCE_REFLECTION_PROBE);
	BIND_ENUM_CONSTANT(INSTANCE_DECAL);
	BIND_ENUM_CONSTANT(INSTANCE_VOXEL_GI);
	BIND_ENUM_CONSTANT(INSTANCE_LIGHTMAP);
	BIND_ENUM_CONSTANT(INSTANCE_OCCLUDER);
	BIND_ENUM_CONSTANT(INSTANCE_VISIBLITY_NOTIFIER);
	BIND_ENUM_CONSTANT(INSTANCE_FOG_VOLUME);
	BIND_ENUM_CONSTANT(INSTANCE_MAX);

	BIND_ENUM_CONSTANT(INSTANCE_GEOMETRY_MASK);

	BIND_ENUM_CONSTANT(INSTANCE_FLAG_USE_BAKED_LIGHT);
	BIND_ENUM_CONSTANT(INSTANCE_FLAG_USE_DYNAMIC_GI);
	BIND_ENUM_CONSTANT(INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE);
	BIND_ENUM_CONSTANT(INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING);
	BIND_ENUM_CONSTANT(INSTANCE_FLAG_MAX);

	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_OFF);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_ON);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_SHADOWS_ONLY);

	BIND_ENUM_CONSTANT(VISIBILITY_RANGE_FADE_DISABLED);
	BIND_ENUM_CONSTANT(VISIBILITY_RANGE_FADE_SELF);
	BIND_ENUM_CONSTANT(VISIBILITY_RANGE_FADE_DEPENDENCIES);

	/* Bake 3D Object */

	ClassDB::bind_method(D_METHOD("bake_render_uv2", "base", "material_overrides", "image_size"), &RenderingServer::bake_render_uv2);

	BIND_ENUM_CONSTANT(BAKE_CHANNEL_ALBEDO_ALPHA);
	BIND_ENUM_CONSTANT(BAKE_CHANNEL_NORMAL);
	BIND_ENUM_CONSTANT(BAKE_CHANNEL_ORM);
	BIND_ENUM_CONSTANT(BAKE_CHANNEL_EMISSION);

	/* CANVAS (2D) */

	ClassDB::bind_method(D_METHOD("canvas_create"), &RenderingServer::canvas_create);
	ClassDB::bind_method(D_METHOD("canvas_set_item_mirroring", "canvas", "item", "mirroring"), &RenderingServer::canvas_set_item_mirroring);
	ClassDB::bind_method(D_METHOD("canvas_set_item_repeat", "item", "repeat_size", "repeat_times"), &RenderingServer::canvas_set_item_repeat);
	ClassDB::bind_method(D_METHOD("canvas_set_modulate", "canvas", "color"), &RenderingServer::canvas_set_modulate);
	ClassDB::bind_method(D_METHOD("canvas_set_disable_scale", "disable"), &RenderingServer::canvas_set_disable_scale);

	/* CANVAS TEXTURE */

	ClassDB::bind_method(D_METHOD("canvas_texture_create"), &RenderingServer::canvas_texture_create);
	ClassDB::bind_method(D_METHOD("canvas_texture_set_channel", "canvas_texture", "channel", "texture"), &RenderingServer::canvas_texture_set_channel);
	ClassDB::bind_method(D_METHOD("canvas_texture_set_shading_parameters", "canvas_texture", "base_color", "shininess"), &RenderingServer::canvas_texture_set_shading_parameters);

	ClassDB::bind_method(D_METHOD("canvas_texture_set_texture_filter", "canvas_texture", "filter"), &RenderingServer::canvas_texture_set_texture_filter);
	ClassDB::bind_method(D_METHOD("canvas_texture_set_texture_repeat", "canvas_texture", "repeat"), &RenderingServer::canvas_texture_set_texture_repeat);

	BIND_ENUM_CONSTANT(CANVAS_TEXTURE_CHANNEL_DIFFUSE);
	BIND_ENUM_CONSTANT(CANVAS_TEXTURE_CHANNEL_NORMAL);
	BIND_ENUM_CONSTANT(CANVAS_TEXTURE_CHANNEL_SPECULAR);

	/* CANVAS ITEM */

	ClassDB::bind_method(D_METHOD("canvas_item_create"), &RenderingServer::canvas_item_create);
	ClassDB::bind_method(D_METHOD("canvas_item_set_parent", "item", "parent"), &RenderingServer::canvas_item_set_parent);
	ClassDB::bind_method(D_METHOD("canvas_item_set_default_texture_filter", "item", "filter"), &RenderingServer::canvas_item_set_default_texture_filter);
	ClassDB::bind_method(D_METHOD("canvas_item_set_default_texture_repeat", "item", "repeat"), &RenderingServer::canvas_item_set_default_texture_repeat);
	ClassDB::bind_method(D_METHOD("canvas_item_set_visible", "item", "visible"), &RenderingServer::canvas_item_set_visible);
	ClassDB::bind_method(D_METHOD("canvas_item_set_light_mask", "item", "mask"), &RenderingServer::canvas_item_set_light_mask);
	ClassDB::bind_method(D_METHOD("canvas_item_set_visibility_layer", "item", "visibility_layer"), &RenderingServer::canvas_item_set_visibility_layer);
	ClassDB::bind_method(D_METHOD("canvas_item_set_transform", "item", "transform"), &RenderingServer::canvas_item_set_transform);
	ClassDB::bind_method(D_METHOD("canvas_item_set_clip", "item", "clip"), &RenderingServer::canvas_item_set_clip);
	ClassDB::bind_method(D_METHOD("canvas_item_set_distance_field_mode", "item", "enabled"), &RenderingServer::canvas_item_set_distance_field_mode);
	ClassDB::bind_method(D_METHOD("canvas_item_set_custom_rect", "item", "use_custom_rect", "rect"), &RenderingServer::canvas_item_set_custom_rect, DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("canvas_item_set_modulate", "item", "color"), &RenderingServer::canvas_item_set_modulate);
	ClassDB::bind_method(D_METHOD("canvas_item_set_self_modulate", "item", "color"), &RenderingServer::canvas_item_set_self_modulate);
	ClassDB::bind_method(D_METHOD("canvas_item_set_draw_behind_parent", "item", "enabled"), &RenderingServer::canvas_item_set_draw_behind_parent);
	ClassDB::bind_method(D_METHOD("canvas_item_set_interpolated", "item", "interpolated"), &RenderingServer::canvas_item_set_interpolated);
	ClassDB::bind_method(D_METHOD("canvas_item_reset_physics_interpolation", "item"), &RenderingServer::canvas_item_reset_physics_interpolation);
	ClassDB::bind_method(D_METHOD("canvas_item_transform_physics_interpolation", "item", "transform"), &RenderingServer::canvas_item_transform_physics_interpolation);

	/* Primitives */

	ClassDB::bind_method(D_METHOD("canvas_item_add_line", "item", "from", "to", "color", "width", "antialiased"), &RenderingServer::canvas_item_add_line, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_polyline", "item", "points", "colors", "width", "antialiased"), &RenderingServer::canvas_item_add_polyline, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_multiline", "item", "points", "colors", "width", "antialiased"), &RenderingServer::canvas_item_add_multiline, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_rect", "item", "rect", "color", "antialiased"), &RenderingServer::canvas_item_add_rect, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_circle", "item", "pos", "radius", "color", "antialiased"), &RenderingServer::canvas_item_add_circle, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_texture_rect", "item", "rect", "texture", "tile", "modulate", "transpose"), &RenderingServer::canvas_item_add_texture_rect, DEFVAL(false), DEFVAL(Color(1, 1, 1)), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("canvas_item_add_msdf_texture_rect_region", "item", "rect", "texture", "src_rect", "modulate", "outline_size", "px_range", "scale"), &RenderingServer::canvas_item_add_msdf_texture_rect_region, DEFVAL(Color(1, 1, 1)), DEFVAL(0), DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("canvas_item_add_lcd_texture_rect_region", "item", "rect", "texture", "src_rect", "modulate"), &RenderingServer::canvas_item_add_lcd_texture_rect_region);
	ClassDB::bind_method(D_METHOD("canvas_item_add_texture_rect_region", "item", "rect", "texture", "src_rect", "modulate", "transpose", "clip_uv"), &RenderingServer::canvas_item_add_texture_rect_region, DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("canvas_item_add_nine_patch", "item", "rect", "source", "texture", "topleft", "bottomright", "x_axis_mode", "y_axis_mode", "draw_center", "modulate"), &RenderingServer::canvas_item_add_nine_patch, DEFVAL(NINE_PATCH_STRETCH), DEFVAL(NINE_PATCH_STRETCH), DEFVAL(true), DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("canvas_item_add_primitive", "item", "points", "colors", "uvs", "texture"), &RenderingServer::canvas_item_add_primitive);
	ClassDB::bind_method(D_METHOD("canvas_item_add_polygon", "item", "points", "colors", "uvs", "texture"), &RenderingServer::canvas_item_add_polygon, DEFVAL(Vector<Point2>()), DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("canvas_item_add_triangle_array", "item", "indices", "points", "colors", "uvs", "bones", "weights", "texture", "count"), &RenderingServer::canvas_item_add_triangle_array, DEFVAL(Vector<Point2>()), DEFVAL(Vector<int>()), DEFVAL(Vector<float>()), DEFVAL(RID()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("canvas_item_add_mesh", "item", "mesh", "transform", "modulate", "texture"), &RenderingServer::canvas_item_add_mesh, DEFVAL(Transform2D()), DEFVAL(Color(1, 1, 1)), DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("canvas_item_add_multimesh", "item", "mesh", "texture"), &RenderingServer::canvas_item_add_multimesh, DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("canvas_item_add_particles", "item", "particles", "texture"), &RenderingServer::canvas_item_add_particles);
	ClassDB::bind_method(D_METHOD("canvas_item_add_set_transform", "item", "transform"), &RenderingServer::canvas_item_add_set_transform);
	ClassDB::bind_method(D_METHOD("canvas_item_add_clip_ignore", "item", "ignore"), &RenderingServer::canvas_item_add_clip_ignore);
	ClassDB::bind_method(D_METHOD("canvas_item_add_animation_slice", "item", "animation_length", "slice_begin", "slice_end", "offset"), &RenderingServer::canvas_item_add_animation_slice, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("canvas_item_set_sort_children_by_y", "item", "enabled"), &RenderingServer::canvas_item_set_sort_children_by_y);
	ClassDB::bind_method(D_METHOD("canvas_item_set_z_index", "item", "z_index"), &RenderingServer::canvas_item_set_z_index);
	ClassDB::bind_method(D_METHOD("canvas_item_set_z_as_relative_to_parent", "item", "enabled"), &RenderingServer::canvas_item_set_z_as_relative_to_parent);
	ClassDB::bind_method(D_METHOD("canvas_item_set_copy_to_backbuffer", "item", "enabled", "rect"), &RenderingServer::canvas_item_set_copy_to_backbuffer);
	ClassDB::bind_method(D_METHOD("canvas_item_attach_skeleton", "item", "skeleton"), &RenderingServer::canvas_item_attach_skeleton);

	ClassDB::bind_method(D_METHOD("canvas_item_clear", "item"), &RenderingServer::canvas_item_clear);
	ClassDB::bind_method(D_METHOD("canvas_item_set_draw_index", "item", "index"), &RenderingServer::canvas_item_set_draw_index);
	ClassDB::bind_method(D_METHOD("canvas_item_set_material", "item", "material"), &RenderingServer::canvas_item_set_material);
	ClassDB::bind_method(D_METHOD("canvas_item_set_use_parent_material", "item", "enabled"), &RenderingServer::canvas_item_set_use_parent_material);

	ClassDB::bind_method(D_METHOD("canvas_item_set_visibility_notifier", "item", "enable", "area", "enter_callable", "exit_callable"), &RenderingServer::canvas_item_set_visibility_notifier);
	ClassDB::bind_method(D_METHOD("canvas_item_set_canvas_group_mode", "item", "mode", "clear_margin", "fit_empty", "fit_margin", "blur_mipmaps"), &RenderingServer::canvas_item_set_canvas_group_mode, DEFVAL(5.0), DEFVAL(false), DEFVAL(0.0), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("debug_canvas_item_get_rect", "item"), &RenderingServer::debug_canvas_item_get_rect);

	BIND_ENUM_CONSTANT(NINE_PATCH_STRETCH);
	BIND_ENUM_CONSTANT(NINE_PATCH_TILE);
	BIND_ENUM_CONSTANT(NINE_PATCH_TILE_FIT);

	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_DEFAULT);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_FILTER_MAX);

	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	BIND_ENUM_CONSTANT(CANVAS_ITEM_TEXTURE_REPEAT_MAX);

	BIND_ENUM_CONSTANT(CANVAS_GROUP_MODE_DISABLED);
	BIND_ENUM_CONSTANT(CANVAS_GROUP_MODE_CLIP_ONLY);
	BIND_ENUM_CONSTANT(CANVAS_GROUP_MODE_CLIP_AND_DRAW);
	BIND_ENUM_CONSTANT(CANVAS_GROUP_MODE_TRANSPARENT);

	/* CANVAS LIGHT */

	ClassDB::bind_method(D_METHOD("canvas_light_create"), &RenderingServer::canvas_light_create);
	ClassDB::bind_method(D_METHOD("canvas_light_attach_to_canvas", "light", "canvas"), &RenderingServer::canvas_light_attach_to_canvas);
	ClassDB::bind_method(D_METHOD("canvas_light_set_enabled", "light", "enabled"), &RenderingServer::canvas_light_set_enabled);
	ClassDB::bind_method(D_METHOD("canvas_light_set_texture_scale", "light", "scale"), &RenderingServer::canvas_light_set_texture_scale);
	ClassDB::bind_method(D_METHOD("canvas_light_set_transform", "light", "transform"), &RenderingServer::canvas_light_set_transform);
	ClassDB::bind_method(D_METHOD("canvas_light_set_texture", "light", "texture"), &RenderingServer::canvas_light_set_texture);
	ClassDB::bind_method(D_METHOD("canvas_light_set_texture_offset", "light", "offset"), &RenderingServer::canvas_light_set_texture_offset);
	ClassDB::bind_method(D_METHOD("canvas_light_set_color", "light", "color"), &RenderingServer::canvas_light_set_color);
	ClassDB::bind_method(D_METHOD("canvas_light_set_height", "light", "height"), &RenderingServer::canvas_light_set_height);
	ClassDB::bind_method(D_METHOD("canvas_light_set_energy", "light", "energy"), &RenderingServer::canvas_light_set_energy);
	ClassDB::bind_method(D_METHOD("canvas_light_set_z_range", "light", "min_z", "max_z"), &RenderingServer::canvas_light_set_z_range);
	ClassDB::bind_method(D_METHOD("canvas_light_set_layer_range", "light", "min_layer", "max_layer"), &RenderingServer::canvas_light_set_layer_range);
	ClassDB::bind_method(D_METHOD("canvas_light_set_item_cull_mask", "light", "mask"), &RenderingServer::canvas_light_set_item_cull_mask);
	ClassDB::bind_method(D_METHOD("canvas_light_set_item_shadow_cull_mask", "light", "mask"), &RenderingServer::canvas_light_set_item_shadow_cull_mask);
	ClassDB::bind_method(D_METHOD("canvas_light_set_mode", "light", "mode"), &RenderingServer::canvas_light_set_mode);
	ClassDB::bind_method(D_METHOD("canvas_light_set_shadow_enabled", "light", "enabled"), &RenderingServer::canvas_light_set_shadow_enabled);
	ClassDB::bind_method(D_METHOD("canvas_light_set_shadow_filter", "light", "filter"), &RenderingServer::canvas_light_set_shadow_filter);
	ClassDB::bind_method(D_METHOD("canvas_light_set_shadow_color", "light", "color"), &RenderingServer::canvas_light_set_shadow_color);
	ClassDB::bind_method(D_METHOD("canvas_light_set_shadow_smooth", "light", "smooth"), &RenderingServer::canvas_light_set_shadow_smooth);
	ClassDB::bind_method(D_METHOD("canvas_light_set_blend_mode", "light", "mode"), &RenderingServer::canvas_light_set_blend_mode);
	ClassDB::bind_method(D_METHOD("canvas_light_set_interpolated", "light", "interpolated"), &RenderingServer::canvas_light_set_interpolated);
	ClassDB::bind_method(D_METHOD("canvas_light_reset_physics_interpolation", "light"), &RenderingServer::canvas_light_reset_physics_interpolation);
	ClassDB::bind_method(D_METHOD("canvas_light_transform_physics_interpolation", "light", "transform"), &RenderingServer::canvas_light_transform_physics_interpolation);

	BIND_ENUM_CONSTANT(CANVAS_LIGHT_MODE_POINT);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_MODE_DIRECTIONAL);

	BIND_ENUM_CONSTANT(CANVAS_LIGHT_BLEND_MODE_ADD);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_BLEND_MODE_SUB);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_BLEND_MODE_MIX);

	BIND_ENUM_CONSTANT(CANVAS_LIGHT_FILTER_NONE);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_FILTER_PCF5);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_FILTER_PCF13);
	BIND_ENUM_CONSTANT(CANVAS_LIGHT_FILTER_MAX);

	/* CANVAS OCCLUDER */

	ClassDB::bind_method(D_METHOD("canvas_light_occluder_create"), &RenderingServer::canvas_light_occluder_create);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_attach_to_canvas", "occluder", "canvas"), &RenderingServer::canvas_light_occluder_attach_to_canvas);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_enabled", "occluder", "enabled"), &RenderingServer::canvas_light_occluder_set_enabled);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_polygon", "occluder", "polygon"), &RenderingServer::canvas_light_occluder_set_polygon);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_as_sdf_collision", "occluder", "enable"), &RenderingServer::canvas_light_occluder_set_as_sdf_collision);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_transform", "occluder", "transform"), &RenderingServer::canvas_light_occluder_set_transform);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_light_mask", "occluder", "mask"), &RenderingServer::canvas_light_occluder_set_light_mask);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_set_interpolated", "occluder", "interpolated"), &RenderingServer::canvas_light_occluder_set_interpolated);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_reset_physics_interpolation", "occluder"), &RenderingServer::canvas_light_occluder_reset_physics_interpolation);
	ClassDB::bind_method(D_METHOD("canvas_light_occluder_transform_physics_interpolation", "occluder", "transform"), &RenderingServer::canvas_light_occluder_transform_physics_interpolation);

	/* CANVAS LIGHT OCCLUDER POLYGON */

	ClassDB::bind_method(D_METHOD("canvas_occluder_polygon_create"), &RenderingServer::canvas_occluder_polygon_create);
	ClassDB::bind_method(D_METHOD("canvas_occluder_polygon_set_shape", "occluder_polygon", "shape", "closed"), &RenderingServer::canvas_occluder_polygon_set_shape);
	ClassDB::bind_method(D_METHOD("canvas_occluder_polygon_set_cull_mode", "occluder_polygon", "mode"), &RenderingServer::canvas_occluder_polygon_set_cull_mode);

	ClassDB::bind_method(D_METHOD("canvas_set_shadow_texture_size", "size"), &RenderingServer::canvas_set_shadow_texture_size);

	BIND_ENUM_CONSTANT(CANVAS_OCCLUDER_POLYGON_CULL_DISABLED);
	BIND_ENUM_CONSTANT(CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE);
	BIND_ENUM_CONSTANT(CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE);

	/* GLOBAL SHADER UNIFORMS */

	ClassDB::bind_method(D_METHOD("global_shader_parameter_add", "name", "type", "default_value"), &RenderingServer::global_shader_parameter_add);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_remove", "name"), &RenderingServer::global_shader_parameter_remove);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_get_list"), &RenderingServer::_global_shader_parameter_get_list);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_set", "name", "value"), &RenderingServer::global_shader_parameter_set);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_set_override", "name", "value"), &RenderingServer::global_shader_parameter_set_override);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_get", "name"), &RenderingServer::global_shader_parameter_get);
	ClassDB::bind_method(D_METHOD("global_shader_parameter_get_type", "name"), &RenderingServer::global_shader_parameter_get_type);

	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_BOOL);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_BVEC2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_BVEC3);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_BVEC4);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_INT);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_IVEC2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_IVEC3);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_IVEC4);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_RECT2I);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_UINT);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_UVEC2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_UVEC3);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_UVEC4);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_FLOAT);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_VEC2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_VEC3);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_VEC4);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_COLOR);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_RECT2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_MAT2);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_MAT3);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_MAT4);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_TRANSFORM_2D);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_TRANSFORM);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_SAMPLER2D);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_SAMPLER2DARRAY);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_SAMPLER3D);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_SAMPLERCUBE);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_SAMPLEREXT);
	BIND_ENUM_CONSTANT(GLOBAL_VAR_TYPE_MAX);

	/* Free */
	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &RenderingServer::free); // Shouldn't conflict with Object::free().

	/* Misc */

	ClassDB::bind_method(D_METHOD("request_frame_drawn_callback", "callable"), &RenderingServer::request_frame_drawn_callback);
	ClassDB::bind_method(D_METHOD("has_changed"), &RenderingServer::has_changed);
	ClassDB::bind_method(D_METHOD("get_rendering_info", "info"), &RenderingServer::get_rendering_info);
	ClassDB::bind_method(D_METHOD("get_video_adapter_name"), &RenderingServer::get_video_adapter_name);
	ClassDB::bind_method(D_METHOD("get_video_adapter_vendor"), &RenderingServer::get_video_adapter_vendor);
	ClassDB::bind_method(D_METHOD("get_video_adapter_type"), &RenderingServer::get_video_adapter_type);
	ClassDB::bind_method(D_METHOD("get_video_adapter_api_version"), &RenderingServer::get_video_adapter_api_version);

	ClassDB::bind_method(D_METHOD("make_sphere_mesh", "latitudes", "longitudes", "radius"), &RenderingServer::make_sphere_mesh);
	ClassDB::bind_method(D_METHOD("get_test_cube"), &RenderingServer::get_test_cube);

	ClassDB::bind_method(D_METHOD("get_test_texture"), &RenderingServer::get_test_texture);
	ClassDB::bind_method(D_METHOD("get_white_texture"), &RenderingServer::get_white_texture);

	ClassDB::bind_method(D_METHOD("set_boot_image", "image", "color", "scale", "use_filter"), &RenderingServer::set_boot_image, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_default_clear_color"), &RenderingServer::get_default_clear_color);
	ClassDB::bind_method(D_METHOD("set_default_clear_color", "color"), &RenderingServer::set_default_clear_color);

	ClassDB::bind_method(D_METHOD("has_os_feature", "feature"), &RenderingServer::has_os_feature);
	ClassDB::bind_method(D_METHOD("set_debug_generate_wireframes", "generate"), &RenderingServer::set_debug_generate_wireframes);

	ClassDB::bind_method(D_METHOD("is_render_loop_enabled"), &RenderingServer::is_render_loop_enabled);
	ClassDB::bind_method(D_METHOD("set_render_loop_enabled", "enabled"), &RenderingServer::set_render_loop_enabled);

	ClassDB::bind_method(D_METHOD("get_frame_setup_time_cpu"), &RenderingServer::get_frame_setup_time_cpu);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "render_loop_enabled"), "set_render_loop_enabled", "is_render_loop_enabled");

	BIND_ENUM_CONSTANT(RENDERING_INFO_TOTAL_OBJECTS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDERING_INFO_TOTAL_PRIMITIVES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDERING_INFO_TEXTURE_MEM_USED);
	BIND_ENUM_CONSTANT(RENDERING_INFO_BUFFER_MEM_USED);
	BIND_ENUM_CONSTANT(RENDERING_INFO_VIDEO_MEM_USED);
	BIND_ENUM_CONSTANT(RENDERING_INFO_PIPELINE_COMPILATIONS_CANVAS);
	BIND_ENUM_CONSTANT(RENDERING_INFO_PIPELINE_COMPILATIONS_MESH);
	BIND_ENUM_CONSTANT(RENDERING_INFO_PIPELINE_COMPILATIONS_SURFACE);
	BIND_ENUM_CONSTANT(RENDERING_INFO_PIPELINE_COMPILATIONS_DRAW);
	BIND_ENUM_CONSTANT(RENDERING_INFO_PIPELINE_COMPILATIONS_SPECIALIZATION);

	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_CANVAS);
	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_MESH);
	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_SURFACE);
	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_DRAW);
	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_SPECIALIZATION);
	BIND_ENUM_CONSTANT(PIPELINE_SOURCE_MAX);

	ADD_SIGNAL(MethodInfo("frame_pre_draw"));
	ADD_SIGNAL(MethodInfo("frame_post_draw"));

	ClassDB::bind_method(D_METHOD("force_sync"), &RenderingServer::sync);
	ClassDB::bind_method(D_METHOD("force_draw", "swap_buffers", "frame_step"), &RenderingServer::draw, DEFVAL(true), DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("get_rendering_device"), &RenderingServer::get_rendering_device);
	ClassDB::bind_method(D_METHOD("create_local_rendering_device"), &RenderingServer::create_local_rendering_device);

	ClassDB::bind_method(D_METHOD("is_on_render_thread"), &RenderingServer::is_on_render_thread);
	ClassDB::bind_method(D_METHOD("call_on_render_thread", "callable"), &RenderingServer::call_on_render_thread);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("has_feature", "feature"), &RenderingServer::has_feature);

	BIND_ENUM_CONSTANT(FEATURE_SHADERS);
	BIND_ENUM_CONSTANT(FEATURE_MULTITHREADED);
#endif
}

void RenderingServer::mesh_add_surface_from_mesh_data(RID p_mesh, const Geometry3D::MeshData &p_mesh_data) {
	Vector<Vector3> vertices;
	Vector<Vector3> normals;

	for (const Geometry3D::MeshData::Face &f : p_mesh_data.faces) {
		for (uint32_t j = 2; j < f.indices.size(); j++) {
			vertices.push_back(p_mesh_data.vertices[f.indices[0]]);
			normals.push_back(f.plane.normal);

			vertices.push_back(p_mesh_data.vertices[f.indices[j - 1]]);
			normals.push_back(f.plane.normal);

			vertices.push_back(p_mesh_data.vertices[f.indices[j]]);
			normals.push_back(f.plane.normal);
		}
	}

	Array d;
	d.resize(RS::ARRAY_MAX);
	d[ARRAY_VERTEX] = vertices;
	d[ARRAY_NORMAL] = normals;
	mesh_add_surface_from_arrays(p_mesh, PRIMITIVE_TRIANGLES, d);
}

void RenderingServer::mesh_add_surface_from_planes(RID p_mesh, const Vector<Plane> &p_planes) {
	Geometry3D::MeshData mdata = Geometry3D::build_convex_mesh(p_planes);
	mesh_add_surface_from_mesh_data(p_mesh, mdata);
}

RID RenderingServer::instance_create2(RID p_base, RID p_scenario) {
	RID instance = instance_create();
	instance_set_base(instance, p_base);
	instance_set_scenario(instance, p_scenario);
	return instance;
}

bool RenderingServer::is_render_loop_enabled() const {
	return render_loop_enabled;
}

void RenderingServer::set_render_loop_enabled(bool p_enabled) {
	render_loop_enabled = p_enabled;
}

RenderingServer::RenderingServer() {
	//ERR_FAIL_COND(singleton);

	singleton = this;
}

TypedArray<StringName> RenderingServer::_global_shader_parameter_get_list() const {
	TypedArray<StringName> gsp;
	Vector<StringName> gsp_sn = global_shader_parameter_get_list();
	gsp.resize(gsp_sn.size());
	for (int i = 0; i < gsp_sn.size(); i++) {
		gsp[i] = gsp_sn[i];
	}
	return gsp;
}

void RenderingServer::init() {
	// These are overrides, even if they are false Godot will still
	// import the texture formats that the host platform needs.
	// See `const bool can_s3tc_bptc` in the resource importer.
	GLOBAL_DEF_RST("rendering/textures/vram_compression/import_s3tc_bptc", false);
	GLOBAL_DEF_RST("rendering/textures/vram_compression/import_etc2_astc", false);
	GLOBAL_DEF("rendering/textures/vram_compression/compress_with_gpu", true);
	GLOBAL_DEF("rendering/textures/vram_compression/cache_gpu_compressor", true);

	GLOBAL_DEF("rendering/textures/lossless_compression/force_png", false);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/textures/webp_compression/compression_method", PROPERTY_HINT_RANGE, "0,6,1"), 2);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/textures/webp_compression/lossless_compression_factor", PROPERTY_HINT_RANGE, "0,100,1"), 25);

	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/limits/time/time_rollover_secs", PROPERTY_HINT_RANGE, "0,10000,1,or_greater"), 3600);

	GLOBAL_DEF_RST("rendering/lights_and_shadows/use_physical_light_units", false);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lights_and_shadows/directional_shadow/size", PROPERTY_HINT_RANGE, "256,16384"), 4096);
	GLOBAL_DEF("rendering/lights_and_shadows/directional_shadow/size.mobile", 2048);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality", PROPERTY_HINT_ENUM, "Hard (Fastest),Soft Very Low (Faster),Soft Low (Fast),Soft Medium (Average),Soft High (Slow),Soft Ultra (Slowest)"), 2);
	GLOBAL_DEF("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality.mobile", 0);
	GLOBAL_DEF("rendering/lights_and_shadows/directional_shadow/16_bits", true);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", PROPERTY_HINT_ENUM, "Hard (Fastest),Soft Very Low (Faster),Soft Low (Fast),Soft Medium (Average),Soft High (Slow),Soft Ultra (Slowest)"), 2);
	GLOBAL_DEF("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality.mobile", 0);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/2d/shadow_atlas/size", PROPERTY_HINT_RANGE, "128,16384"), 2048);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/2d/batching/item_buffer_size", PROPERTY_HINT_RANGE, "128,1048576,1"), 16384);

	// Number of commands that can be drawn per frame.
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/gl_compatibility/item_buffer_size", PROPERTY_HINT_RANGE, "128,1048576,1"), 16384);

	GLOBAL_DEF("rendering/shader_compiler/shader_cache/enabled", true);
	GLOBAL_DEF("rendering/shader_compiler/shader_cache/compress", true);
	GLOBAL_DEF("rendering/shader_compiler/shader_cache/use_zstd_compression", true);
	GLOBAL_DEF("rendering/shader_compiler/shader_cache/strip_debug", false);
	GLOBAL_DEF("rendering/shader_compiler/shader_cache/strip_debug.release", true);

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/reflections/sky_reflections/roughness_layers", PROPERTY_HINT_RANGE, "1,32,1"), 8); // Assumes a 256x256 cubemap
	GLOBAL_DEF_RST("rendering/reflections/sky_reflections/texture_array_reflections", true);
	GLOBAL_DEF("rendering/reflections/sky_reflections/texture_array_reflections.mobile", false);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/reflections/sky_reflections/ggx_samples", PROPERTY_HINT_RANGE, "0,256,1"), 32);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/reflections/sky_reflections/ggx_samples.mobile", PROPERTY_HINT_RANGE, "0,128,1"), 16);
	GLOBAL_DEF("rendering/reflections/sky_reflections/fast_filter_high_quality", false);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/reflections/reflection_atlas/reflection_size", PROPERTY_HINT_RANGE, "0,4096,1"), 256);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/reflections/reflection_atlas/reflection_size.mobile", PROPERTY_HINT_RANGE, "0,2048,1"), 128);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/reflections/reflection_atlas/reflection_count", PROPERTY_HINT_RANGE, "0,256,1"), 64);

	GLOBAL_DEF("rendering/global_illumination/gi/use_half_resolution", false);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/global_illumination/voxel_gi/quality", PROPERTY_HINT_ENUM, "Low (4 Cones - Fast),High (6 Cones - Slow)"), 0);

	GLOBAL_DEF_RST("rendering/shading/overrides/force_vertex_shading", false);
	GLOBAL_DEF("rendering/shading/overrides/force_lambert_over_burley", false);
	GLOBAL_DEF("rendering/shading/overrides/force_lambert_over_burley.mobile", true);

	GLOBAL_DEF_RST("rendering/driver/depth_prepass/enable", true);
	GLOBAL_DEF_RST("rendering/driver/depth_prepass/disable_for_vendors", "PowerVR,Mali,Adreno,Apple");

	GLOBAL_DEF_RST("rendering/textures/default_filters/use_nearest_mipmap_filter", false);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/textures/default_filters/anisotropic_filtering_level", PROPERTY_HINT_ENUM, String::utf8("Disabled (Fastest),2× (Faster),4× (Fast),8× (Average),16× (Slow)")), 2);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/camera/depth_of_field/depth_of_field_bokeh_shape", PROPERTY_HINT_ENUM, "Box (Fast),Hexagon (Average),Circle (Slowest)"), 1);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/camera/depth_of_field/depth_of_field_bokeh_quality", PROPERTY_HINT_ENUM, "Very Low (Fastest),Low (Fast),Medium (Average),High (Slow)"), 1);
	GLOBAL_DEF("rendering/camera/depth_of_field/depth_of_field_use_jitter", false);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/ssao/quality", PROPERTY_HINT_ENUM, "Very Low (Fast),Low (Fast),Medium (Average),High (Slow),Ultra (Custom)"), 2);
	GLOBAL_DEF("rendering/environment/ssao/half_size", true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssao/adaptive_target", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), 0.5);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/ssao/blur_passes", PROPERTY_HINT_RANGE, "0,6"), 2);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssao/fadeout_from", PROPERTY_HINT_RANGE, "0.0,512,0.1,or_greater"), 50.0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssao/fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater"), 300.0);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/ssil/quality", PROPERTY_HINT_ENUM, "Very Low (Fast),Low (Fast),Medium (Average),High (Slow),Ultra (Custom)"), 2);
	GLOBAL_DEF("rendering/environment/ssil/half_size", true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssil/adaptive_target", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), 0.5);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/ssil/blur_passes", PROPERTY_HINT_RANGE, "0,6"), 4);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssil/fadeout_from", PROPERTY_HINT_RANGE, "0.0,512,0.1,or_greater"), 50.0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/ssil/fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater"), 300.0);

	GLOBAL_DEF("rendering/anti_aliasing/screen_space_roughness_limiter/enabled", true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/anti_aliasing/screen_space_roughness_limiter/amount", PROPERTY_HINT_RANGE, "0.01,4.0,0.01"), 0.25);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/anti_aliasing/screen_space_roughness_limiter/limit", PROPERTY_HINT_RANGE, "0.01,1.0,0.01"), 0.18);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/scaling_3d/mode", PROPERTY_HINT_ENUM, "Bilinear (Fastest),FSR 1.0 (Fast),FSR 2.2 (Slow)"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/scale", PROPERTY_HINT_RANGE, "0.25,2.0,0.01"), 1.0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/fsr_sharpness", PROPERTY_HINT_RANGE, "0,2,0.1"), 0.2f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/textures/default_filters/texture_mipmap_bias", PROPERTY_HINT_RANGE, "-2,2,0.001"), 0.0f);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/textures/decals/filter", PROPERTY_HINT_ENUM, "Nearest (Fast),Linear (Fast),Nearest Mipmap (Fast),Linear Mipmap (Fast),Nearest Mipmap Anisotropic (Average),Linear Mipmap Anisotropic (Average)"), DECAL_FILTER_LINEAR_MIPMAPS);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/textures/light_projectors/filter", PROPERTY_HINT_ENUM, "Nearest (Fast),Linear (Fast),Nearest Mipmap (Fast),Linear Mipmap (Fast),Nearest Mipmap Anisotropic (Average),Linear Mipmap Anisotropic (Average)"), LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS);

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/occlusion_culling/occlusion_rays_per_thread", PROPERTY_HINT_RANGE, "1,2048,1,or_greater"), 512);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/glow/upscale_mode", PROPERTY_HINT_ENUM, "Linear (Fast),Bicubic (Slow)"), 1);
	GLOBAL_DEF("rendering/environment/glow/upscale_mode.mobile", 0);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/screen_space_reflection/roughness_quality", PROPERTY_HINT_ENUM, "Disabled (Fastest),Low (Fast),Medium (Average),High (Slow)"), 1);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/subsurface_scattering/subsurface_scattering_quality", PROPERTY_HINT_ENUM, "Disabled (Fastest),Low (Fast),Medium (Average),High (Slow)"), 1);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/subsurface_scattering/subsurface_scattering_scale", PROPERTY_HINT_RANGE, "0.001,1,0.001"), 0.05);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/environment/subsurface_scattering/subsurface_scattering_depth_scale", PROPERTY_HINT_RANGE, "0.001,1,0.001"), 0.01);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/limits/global_shader_variables/buffer_size", PROPERTY_HINT_RANGE, "16,1048576,1"), 65536);

	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/lightmapping/probe_capture/update_speed", PROPERTY_HINT_RANGE, "0.001,256,0.001"), 15);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/lightmapping/primitive_meshes/texel_size", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 0.2);
	GLOBAL_DEF("rendering/lightmapping/lightmap_gi/use_bicubic_filter", true);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/global_illumination/sdfgi/probe_ray_count", PROPERTY_HINT_ENUM, "8 (Fastest),16,32,64,96,128 (Slowest)"), 1);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/global_illumination/sdfgi/frames_to_converge", PROPERTY_HINT_ENUM, "5 (Less Latency but Lower Quality),10,15,20,25,30 (More Latency but Higher Quality)"), 5);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/global_illumination/sdfgi/frames_to_update_lights", PROPERTY_HINT_ENUM, "1 (Slower),2,4,8,16 (Faster)"), 2);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/volumetric_fog/volume_size", PROPERTY_HINT_RANGE, "16,512,1"), 64);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/volumetric_fog/volume_depth", PROPERTY_HINT_RANGE, "16,512,1"), 64);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/environment/volumetric_fog/use_filter", PROPERTY_HINT_ENUM, "No (Faster),Yes (Higher Quality)"), 1);

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/limits/spatial_indexer/update_iterations_per_frame", PROPERTY_HINT_RANGE, "0,1024,1"), 10);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/limits/spatial_indexer/threaded_cull_minimum_instances", PROPERTY_HINT_RANGE, "32,65536,1"), 1000);

	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/limits/cluster_builder/max_clustered_elements", PROPERTY_HINT_RANGE, "32,8192,1"), 512);

	// OpenGL limits
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/limits/opengl/max_renderable_elements", PROPERTY_HINT_RANGE, "1024,65536,1"), 65536);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/limits/opengl/max_renderable_lights", PROPERTY_HINT_RANGE, "2,256,1"), 32);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/limits/opengl/max_lights_per_object", PROPERTY_HINT_RANGE, "2,1024,1"), 8);

	GLOBAL_DEF_RST_BASIC("xr/shaders/enabled", false);

	GLOBAL_DEF("debug/shader_language/warnings/enable", true);
	GLOBAL_DEF("debug/shader_language/warnings/treat_warnings_as_errors", false);

#ifdef DEBUG_ENABLED
	for (int i = 0; i < (int)ShaderWarning::WARNING_MAX; i++) {
		GLOBAL_DEF("debug/shader_language/warnings/" + ShaderWarning::get_name_from_code((ShaderWarning::Code)i).to_lower(), true);
	}
#endif
}

RenderingServer::~RenderingServer() {
	singleton = nullptr;
}
