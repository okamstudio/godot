/**************************************************************************/
/*  lightmapper_rd.h                                                      */
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

#ifndef LIGHTMAPPER_RD_H
#define LIGHTMAPPER_RD_H

#include "core/templates/local_vector.h"
#include "scene/3d/lightmapper.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_device.h"

class RDShaderFile;
class LightmapperRD : public Lightmapper {
	GDCLASS(LightmapperRD, Lightmapper)

	struct BakeParameters {
		float world_size[3] = {};
		float bias = 0.0;

		float to_cell_offset[3] = {};
		int32_t grid_size = 0;

		float to_cell_size[3] = {};
		uint32_t light_count = 0;

		float env_transform[12] = {};

		int32_t atlas_size[2] = {};
		float exposure_normalization = 0.0f;
		uint32_t bounces = 0;

		float bounce_indirect_energy = 0.0f;
		uint32_t shadowmask_light_idx = 0;
		uint32_t transparency_rays = 0;
		float supersampling_factor = 0.0f;
	};

	struct MeshInstance {
		MeshData data;
		int slice = 0;
		Vector2i offset;
	};

	struct Light {
		float position[3] = {};
		uint32_t type = LIGHT_TYPE_DIRECTIONAL;
		float direction[3] = {};
		float energy = 0.0;
		float color[3] = {};
		float size = 0.0;
		float range = 0.0;
		float attenuation = 0.0;
		float cos_spot_angle = 0.0;
		float inv_spot_attenuation = 0.0;
		float indirect_energy = 0.0;
		float shadow_blur = 0.0;
		uint32_t static_bake = 0;
		uint32_t pad = 0;

		bool operator<(const Light &p_light) const {
			return type < p_light.type;
		}
	};

	struct Vertex {
		float position[3] = {};
		float normal_z = 0.0;
		float uv[2] = {};
		float normal_xy[2] = {};

		bool operator==(const Vertex &p_vtx) const {
			return (position[0] == p_vtx.position[0]) &&
					(position[1] == p_vtx.position[1]) &&
					(position[2] == p_vtx.position[2]) &&
					(uv[0] == p_vtx.uv[0]) &&
					(uv[1] == p_vtx.uv[1]) &&
					(normal_xy[0] == p_vtx.normal_xy[0]) &&
					(normal_xy[1] == p_vtx.normal_xy[1]) &&
					(normal_z == p_vtx.normal_z);
		}
	};

	struct Edge {
		Vector3 a;
		Vector3 b;
		Vector3 na;
		Vector3 nb;
		bool operator==(const Edge &p_seam) const {
			return a == p_seam.a && b == p_seam.b && na == p_seam.na && nb == p_seam.nb;
		}
		Edge() {
		}

		Edge(const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_na, const Vector3 &p_nb) {
			a = p_a;
			b = p_b;
			na = p_na;
			nb = p_nb;
		}
	};

	struct Probe {
		float position[4] = {};
	};

	Vector<Probe> probe_positions;

	struct EdgeHash {
		_FORCE_INLINE_ static uint32_t hash(const Edge &p_edge) {
			uint32_t h = hash_murmur3_one_float(p_edge.a.x);
			h = hash_murmur3_one_float(p_edge.a.y, h);
			h = hash_murmur3_one_float(p_edge.a.z, h);
			h = hash_murmur3_one_float(p_edge.b.x, h);
			h = hash_murmur3_one_float(p_edge.b.y, h);
			h = hash_murmur3_one_float(p_edge.b.z, h);
			return h;
		}
	};
	struct EdgeUV2 {
		Vector2 a;
		Vector2 b;
		Vector2i indices;
		bool operator==(const EdgeUV2 &p_uv2) const {
			return a == p_uv2.a && b == p_uv2.b;
		}
		bool seam_found = false;
		EdgeUV2(Vector2 p_a, Vector2 p_b, Vector2i p_indices) {
			a = p_a;
			b = p_b;
			indices = p_indices;
		}
		EdgeUV2() {}
	};

	struct Seam {
		Vector2i a;
		Vector2i b;
		uint32_t slice;
		bool operator<(const Seam &p_seam) const {
			return slice < p_seam.slice;
		}
	};

	struct VertexHash {
		_FORCE_INLINE_ static uint32_t hash(const Vertex &p_vtx) {
			uint32_t h = hash_murmur3_one_float(p_vtx.position[0]);
			h = hash_murmur3_one_float(p_vtx.position[1], h);
			h = hash_murmur3_one_float(p_vtx.position[2], h);
			h = hash_murmur3_one_float(p_vtx.uv[0], h);
			h = hash_murmur3_one_float(p_vtx.uv[1], h);
			h = hash_murmur3_one_float(p_vtx.normal_xy[0], h);
			h = hash_murmur3_one_float(p_vtx.normal_xy[1], h);
			h = hash_murmur3_one_float(p_vtx.normal_z, h);
			return hash_fmix32(h);
		}
	};

	struct Triangle {
		uint32_t indices[3] = {};
		uint32_t slice = 0;
		float min_bounds[3] = {};
		uint32_t cull_mode = 0;
		float max_bounds[3] = {};
		float pad1 = 0.0;
		bool operator<(const Triangle &p_triangle) const {
			return slice < p_triangle.slice;
		}
	};

	struct ClusterAABB {
		float min_bounds[3];
		float pad0 = 0.0f;
		float max_bounds[3];
		float pad1 = 0.0f;
	};

	Vector<MeshInstance> mesh_instances;

	Vector<Light> lights;
	Vector<String> light_names;

	struct TriangleSort {
		uint32_t cell_index = 0;
		uint32_t triangle_index = 0;
		AABB triangle_aabb;

		bool operator<(const TriangleSort &p_triangle_sort) const {
			return cell_index < p_triangle_sort.cell_index; //sorting by triangle index in this case makes no sense
		}
	};

	template <int T>
	struct TriangleSortAxis {
		bool operator()(const TriangleSort &p_a, const TriangleSort &p_b) const {
			return p_a.triangle_aabb.get_center()[T] < p_b.triangle_aabb.get_center()[T];
		}
	};

	void _plot_triangle_into_triangle_index_list(int p_size, const Vector3i &p_ofs, const AABB &p_bounds, const Vector3 p_points[3], uint32_t p_triangle_index, LocalVector<TriangleSort> &triangles, uint32_t p_grid_size);
	void _sort_triangle_clusters(uint32_t p_cluster_size, uint32_t p_cluster_index, uint32_t p_index_start, uint32_t p_count, LocalVector<TriangleSort> &p_triangle_sort, LocalVector<ClusterAABB> &p_cluster_aabb);

	struct RasterPushConstant {
		float atlas_size[2] = {};
		float uv_offset[2] = {};
		float to_cell_size[3] = {};
		uint32_t base_triangle = 0;
		float to_cell_offset[3] = {};
		float bias = 0.0;
		int32_t grid_size[3] = {};
		uint32_t pad2 = 0;
	};

	struct RasterSeamsPushConstant {
		uint32_t base_index = 0;
		uint32_t slice = 0;
		float uv_offset[2] = {};
		uint32_t debug = 0;
		float blend = 0.0;
		uint32_t pad[2] = {};
	};

	struct PushConstant {
		uint32_t atlas_slice = 0;
		uint32_t ray_count = 0;
		uint32_t ray_from = 0;
		uint32_t ray_to = 0;
		uint32_t region_ofs[2] = {};
		uint32_t probe_count = 0;
		uint32_t pad = 0;
	};

	Vector<Ref<Image>> lightmap_textures;
	Vector<Ref<Image>> shadowmask_textures;
	Vector<Color> probe_values;

	struct DilateParams {
		uint32_t radius;
		uint32_t pad[3];
	};

	struct DenoiseParams {
		float spatial_bandwidth;
		float light_bandwidth;
		float albedo_bandwidth;
		float normal_bandwidth;

		int half_search_window;
		float filter_strength;
		float pad[2];
	};

	BakeError _blit_meshes_into_atlas(int p_max_texture_size, int p_denoiser_range, Vector<Ref<Image>> &albedo_images, Vector<Ref<Image>> &emission_images, AABB &bounds, Size2i &atlas_size, int &atlas_slices, float p_supersampling_factor, BakeStepFunc p_step_function, void *p_bake_userdata);
	void _create_acceleration_structures(RenderingDevice *rd, Size2i atlas_size, int atlas_slices, AABB &bounds, int grid_size, uint32_t p_cluster_size, Vector<Probe> &probe_positions, GenerateProbes p_generate_probes, Vector<int> &slice_triangle_count, Vector<int> &slice_seam_count, RID &vertex_buffer, RID &triangle_buffer, RID &lights_buffer, RID &r_triangle_indices_buffer, RID &r_cluster_indices_buffer, RID &r_cluster_aabbs_buffer, RID &probe_positions_buffer, RID &grid_texture, RID &seams_buffer, BakeStepFunc p_step_function, void *p_bake_userdata);
	void _raster_geometry(RenderingDevice *rd, Size2i atlas_size, int atlas_slices, int grid_size, AABB bounds, float p_bias, Vector<int> slice_triangle_count, RID position_tex, RID unocclude_tex, RID normal_tex, RID raster_depth_buffer, RID rasterize_shader, RID raster_base_uniform);

	BakeError _dilate(RenderingDevice *rd, Ref<RDShaderFile> &compute_shader, RID &compute_base_uniform_set, PushConstant &push_constant, RID &source_light_tex, RID &dest_light_tex, const Size2i &atlas_size, int atlas_slices);
	BakeError _denoise(RenderingDevice *p_rd, Ref<RDShaderFile> &p_compute_shader, const RID &p_compute_base_uniform_set, PushConstant &p_push_constant, RID p_source_light_tex, RID p_source_normal_tex, RID p_dest_light_tex, float p_denoiser_strength, int p_denoiser_range, const Size2i &p_atlas_size, int p_atlas_slices, bool p_bake_sh, BakeStepFunc p_step_function, void *p_bake_userdata);
	BakeError _pack_l1(RenderingDevice *rd, Ref<RDShaderFile> &compute_shader, RID &compute_base_uniform_set, PushConstant &push_constant, RID &source_light_tex, RID &dest_light_tex, const Size2i &atlas_size, int atlas_slices);

	Error _store_pfm(RenderingDevice *p_rd, RID p_atlas_tex, int p_index, const Size2i &p_atlas_size, const String &p_name, bool p_shadowmask);
	Ref<Image> _read_pfm(const String &p_name, bool p_shadowmask);
	BakeError _denoise_oidn(RenderingDevice *p_rd, RID p_source_light_tex, RID p_source_normal_tex, RID p_dest_light_tex, const Size2i &p_atlas_size, int p_atlas_slices, bool p_bake_sh, bool p_shadowmask, const String &p_exe);

public:
	virtual void add_mesh(const MeshData &p_mesh) override;
	virtual void add_directional_light(const String &p_name, bool p_static, const Vector3 &p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_angular_distance, float p_shadow_blur) override;
	virtual void add_omni_light(const String &p_name, bool p_static, const Vector3 &p_position, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_size, float p_shadow_blur) override;
	virtual void add_spot_light(const String &p_name, bool p_static, const Vector3 &p_position, const Vector3 p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_spot_angle, float p_spot_attenuation, float p_size, float p_shadow_blur) override;
	virtual void add_probe(const Vector3 &p_position) override;
	virtual BakeError bake(BakeQuality p_quality, bool p_use_denoiser, float p_denoiser_strength, int p_denoiser_range, int p_bounces, float p_bounce_indirect_energy, float p_bias, int p_max_texture_size, bool p_bake_sh, bool p_bake_shadowmask, bool p_texture_for_bounces, GenerateProbes p_generate_probes, const Ref<Image> &p_environment_panorama, const Basis &p_environment_transform, BakeStepFunc p_step_function = nullptr, void *p_bake_userdata = nullptr, float p_exposure_normalization = 1.0, float p_supersampling_factor = 1.0f) override;

	int get_bake_texture_count() const override;
	Ref<Image> get_bake_texture(int p_index) const override;
	int get_shadowmask_texture_count() const override;
	Ref<Image> get_shadowmask_texture(int p_index) const override;
	int get_bake_mesh_count() const override;
	Variant get_bake_mesh_userdata(int p_index) const override;
	Rect2 get_bake_mesh_uv_scale(int p_index) const override;
	int get_bake_mesh_texture_slice(int p_index) const override;
	int get_bake_probe_count() const override;
	Vector3 get_bake_probe_point(int p_probe) const override;
	Vector<Color> get_bake_probe_sh(int p_probe) const override;

	LightmapperRD();
};

#endif // LIGHTMAPPER_RD_H
