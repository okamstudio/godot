/**************************************************************************/
/*  lightmapper_rd.cpp                                                    */
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

#include "lightmapper_rd.h"

#include "lm_blendseams.glsl.gen.h"
#include "lm_compute.glsl.gen.h"
#include "lm_raster.glsl.gen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/math/geometry_2d.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "servers/rendering/rendering_device_binds.h"

#if defined(VULKAN_ENABLED)
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif
#if defined(METAL_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

//uncomment this if you want to see textures from all the process saved
//#define DEBUG_TEXTURES

void LightmapperRD::add_mesh(const MeshData &p_mesh) {
	ERR_FAIL_COND(p_mesh.albedo_on_uv2.is_null() || p_mesh.albedo_on_uv2->is_empty());
	ERR_FAIL_COND(p_mesh.emission_on_uv2.is_null() || p_mesh.emission_on_uv2->is_empty());
	ERR_FAIL_COND(p_mesh.albedo_on_uv2->get_width() != p_mesh.emission_on_uv2->get_width());
	ERR_FAIL_COND(p_mesh.albedo_on_uv2->get_height() != p_mesh.emission_on_uv2->get_height());
	ERR_FAIL_COND(p_mesh.points.is_empty());
	MeshInstance mi;
	mi.data = p_mesh;
	mesh_instances.push_back(mi);
}

void LightmapperRD::add_directional_light(const String &p_name, bool p_static, const Vector3 &p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_angular_distance, float p_shadow_blur) {
	Light l;
	l.type = LIGHT_TYPE_DIRECTIONAL;
	l.direction[0] = p_direction.x;
	l.direction[1] = p_direction.y;
	l.direction[2] = p_direction.z;
	l.color[0] = p_color.r;
	l.color[1] = p_color.g;
	l.color[2] = p_color.b;
	l.energy = p_energy;
	l.indirect_energy = p_indirect_energy;
	l.static_bake = p_static;
	l.size = Math::tan(Math::deg_to_rad(p_angular_distance));
	l.shadow_blur = p_shadow_blur;
	lights.push_back(l);
	light_names.push_back(p_name);
}

void LightmapperRD::add_omni_light(const String &p_name, bool p_static, const Vector3 &p_position, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_size, float p_shadow_blur) {
	Light l;
	l.type = LIGHT_TYPE_OMNI;
	l.position[0] = p_position.x;
	l.position[1] = p_position.y;
	l.position[2] = p_position.z;
	l.range = p_range;
	l.attenuation = p_attenuation;
	l.color[0] = p_color.r;
	l.color[1] = p_color.g;
	l.color[2] = p_color.b;
	l.energy = p_energy;
	l.indirect_energy = p_indirect_energy;
	l.static_bake = p_static;
	l.size = p_size;
	l.shadow_blur = p_shadow_blur;
	lights.push_back(l);
	light_names.push_back(p_name);
}

void LightmapperRD::add_spot_light(const String &p_name, bool p_static, const Vector3 &p_position, const Vector3 p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_spot_angle, float p_spot_attenuation, float p_size, float p_shadow_blur) {
	Light l;
	l.type = LIGHT_TYPE_SPOT;
	l.position[0] = p_position.x;
	l.position[1] = p_position.y;
	l.position[2] = p_position.z;
	l.direction[0] = p_direction.x;
	l.direction[1] = p_direction.y;
	l.direction[2] = p_direction.z;
	l.range = p_range;
	l.attenuation = p_attenuation;
	l.cos_spot_angle = Math::cos(Math::deg_to_rad(p_spot_angle));
	l.inv_spot_attenuation = 1.0f / p_spot_attenuation;
	l.color[0] = p_color.r;
	l.color[1] = p_color.g;
	l.color[2] = p_color.b;
	l.energy = p_energy;
	l.indirect_energy = p_indirect_energy;
	l.static_bake = p_static;
	l.size = p_size;
	l.shadow_blur = p_shadow_blur;
	lights.push_back(l);
	light_names.push_back(p_name);
}

void LightmapperRD::add_probe(const Vector3 &p_position) {
	Probe probe;
	probe.position[0] = p_position.x;
	probe.position[1] = p_position.y;
	probe.position[2] = p_position.z;
	probe.position[3] = 0;
	probe_positions.push_back(probe);
}

void LightmapperRD::_plot_triangle_into_triangle_index_list(int p_size, const Vector3i &p_ofs, const AABB &p_bounds, const Vector3 p_points[3], uint32_t p_triangle_index, LocalVector<TriangleSort> &p_triangles_sort, uint32_t p_grid_size) {
	int half_size = p_size / 2;

	for (int i = 0; i < 8; i++) {
		AABB aabb = p_bounds;
		aabb.size *= 0.5;
		Vector3i n = p_ofs;

		if (i & 1) {
			aabb.position.x += aabb.size.x;
			n.x += half_size;
		}
		if (i & 2) {
			aabb.position.y += aabb.size.y;
			n.y += half_size;
		}
		if (i & 4) {
			aabb.position.z += aabb.size.z;
			n.z += half_size;
		}

		{
			Vector3 qsize = aabb.size * 0.5; //quarter size, for fast aabb test

			if (!Geometry3D::triangle_box_overlap(aabb.position + qsize, qsize, p_points)) {
				//does not fit in child, go on
				continue;
			}
		}

		if (half_size == 1) {
			//got to the end
			TriangleSort ts;
			ts.cell_index = n.x + (n.y * p_grid_size) + (n.z * p_grid_size * p_grid_size);
			ts.triangle_index = p_triangle_index;
			ts.triangle_aabb.position = p_points[0];
			ts.triangle_aabb.size = Vector3();
			ts.triangle_aabb.expand_to(p_points[1]);
			ts.triangle_aabb.expand_to(p_points[2]);
			p_triangles_sort.push_back(ts);
		} else {
			_plot_triangle_into_triangle_index_list(half_size, n, aabb, p_points, p_triangle_index, p_triangles_sort, p_grid_size);
		}
	}
}

void LightmapperRD::_sort_triangle_clusters(uint32_t p_cluster_size, uint32_t p_cluster_index, uint32_t p_index_start, uint32_t p_count, LocalVector<TriangleSort> &p_triangle_sort, LocalVector<ClusterAABB> &p_cluster_aabb) {
	if (p_count == 0) {
		return;
	}

	// Compute AABB for all triangles in the range.
	SortArray<TriangleSort, TriangleSortAxis<0>> triangle_sorter_x;
	SortArray<TriangleSort, TriangleSortAxis<1>> triangle_sorter_y;
	SortArray<TriangleSort, TriangleSortAxis<2>> triangle_sorter_z;
	AABB cluster_aabb = p_triangle_sort[p_index_start].triangle_aabb;
	for (uint32_t i = 1; i < p_count; i++) {
		cluster_aabb.merge_with(p_triangle_sort[p_index_start + i].triangle_aabb);
	}

	if (p_count > p_cluster_size) {
		int longest_axis_index = cluster_aabb.get_longest_axis_index();
		switch (longest_axis_index) {
			case 0:
				triangle_sorter_x.sort(&p_triangle_sort[p_index_start], p_count);
				break;
			case 1:
				triangle_sorter_y.sort(&p_triangle_sort[p_index_start], p_count);
				break;
			case 2:
				triangle_sorter_z.sort(&p_triangle_sort[p_index_start], p_count);
				break;
			default:
				DEV_ASSERT(false && "Invalid axis returned by AABB.");
				break;
		}

		uint32_t left_cluster_count = next_power_of_2(p_count / 2);
		left_cluster_count = MAX(left_cluster_count, p_cluster_size);
		left_cluster_count = MIN(left_cluster_count, p_count);
		_sort_triangle_clusters(p_cluster_size, p_cluster_index, p_index_start, left_cluster_count, p_triangle_sort, p_cluster_aabb);

		if (left_cluster_count < p_count) {
			uint32_t cluster_index_right = p_cluster_index + (left_cluster_count / p_cluster_size);
			_sort_triangle_clusters(p_cluster_size, cluster_index_right, p_index_start + left_cluster_count, p_count - left_cluster_count, p_triangle_sort, p_cluster_aabb);
		}
	} else {
		ClusterAABB &aabb = p_cluster_aabb[p_cluster_index];
		Vector3 aabb_end = cluster_aabb.get_end();
		aabb.min_bounds[0] = cluster_aabb.position.x;
		aabb.min_bounds[1] = cluster_aabb.position.y;
		aabb.min_bounds[2] = cluster_aabb.position.z;
		aabb.max_bounds[0] = aabb_end.x;
		aabb.max_bounds[1] = aabb_end.y;
		aabb.max_bounds[2] = aabb_end.z;
	}
}

Lightmapper::BakeError LightmapperRD::_blit_meshes_into_atlas(int p_max_texture_size, int p_denoiser_range, Vector<Ref<Image>> &albedo_images, Vector<Ref<Image>> &emission_images, AABB &bounds, Size2i &atlas_size, int &atlas_slices, BakeStepFunc p_step_function, void *p_bake_userdata) {
	Vector<Size2i> sizes;

	for (int m_i = 0; m_i < mesh_instances.size(); m_i++) {
		MeshInstance &mi = mesh_instances.write[m_i];
		Size2i s = Size2i(mi.data.albedo_on_uv2->get_width(), mi.data.albedo_on_uv2->get_height());
		sizes.push_back(s);
		atlas_size = atlas_size.max(s + Size2i(2, 2).maxi(p_denoiser_range));
	}

	int max = nearest_power_of_2_templated(atlas_size.width);
	max = MAX(max, nearest_power_of_2_templated(atlas_size.height));

	if (max > p_max_texture_size) {
		return BAKE_ERROR_TEXTURE_EXCEEDS_MAX_SIZE;
	}

	if (p_step_function) {
		if (p_step_function(0.1, RTR("Determining optimal atlas size"), p_bake_userdata, true)) {
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	atlas_size = Size2i(max, max);

	Size2i best_atlas_size;
	int best_atlas_slices = 0;
	int best_atlas_memory = 0x7FFFFFFF;
	Vector<Vector3i> best_atlas_offsets;

	// Determine best texture array atlas size by bruteforce fitting.
	while (atlas_size.x <= p_max_texture_size && atlas_size.y <= p_max_texture_size) {
		Vector<Vector2i> source_sizes;
		Vector<int> source_indices;
		source_sizes.resize(sizes.size());
		source_indices.resize(sizes.size());
		for (int i = 0; i < source_indices.size(); i++) {
			source_sizes.write[i] = sizes[i] + Vector2i(2, 2).maxi(p_denoiser_range); // Add padding between lightmaps.
			source_indices.write[i] = i;
		}
		Vector<Vector3i> atlas_offsets;
		atlas_offsets.resize(source_sizes.size());

		// Ensure the sizes can all fit into a single atlas layer.
		// This should always happen, and this check is only in place to prevent an infinite loop.
		for (int i = 0; i < source_sizes.size(); i++) {
			if (source_sizes[i] > atlas_size) {
				return BAKE_ERROR_ATLAS_TOO_SMALL;
			}
		}

		int slices = 0;

		while (source_sizes.size() > 0) {
			Vector<Vector3i> offsets = Geometry2D::partial_pack_rects(source_sizes, atlas_size);
			Vector<int> new_indices;
			Vector<Vector2i> new_sources;
			for (int i = 0; i < offsets.size(); i++) {
				Vector3i ofs = offsets[i];
				int sidx = source_indices[i];
				if (ofs.z > 0) {
					//valid
					ofs.z = slices;
					atlas_offsets.write[sidx] = ofs + Vector3i(1, 1, 0); // Center lightmap in the reserved oversized region
				} else {
					new_indices.push_back(sidx);
					new_sources.push_back(source_sizes[i]);
				}
			}

			source_sizes = new_sources;
			source_indices = new_indices;
			slices++;
		}

		int mem_used = atlas_size.x * atlas_size.y * slices;
		if (mem_used < best_atlas_memory) {
			best_atlas_size = atlas_size;
			best_atlas_offsets = atlas_offsets;
			best_atlas_slices = slices;
			best_atlas_memory = mem_used;
		}

		if (atlas_size.width == atlas_size.height) {
			atlas_size.width *= 2;
		} else {
			atlas_size.height *= 2;
		}
	}
	atlas_size = best_atlas_size;
	atlas_slices = best_atlas_slices;

	// apply the offsets and slice to all images, and also blit albedo and emission
	albedo_images.resize(atlas_slices);
	emission_images.resize(atlas_slices);

	if (p_step_function) {
		if (p_step_function(0.2, RTR("Blitting albedo and emission"), p_bake_userdata, true)) {
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	for (int i = 0; i < atlas_slices; i++) {
		Ref<Image> albedo = Image::create_empty(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBA8);
		albedo->set_as_black();
		albedo_images.write[i] = albedo;

		Ref<Image> emission = Image::create_empty(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH);
		emission->set_as_black();
		emission_images.write[i] = emission;
	}

	//assign uv positions

	for (int m_i = 0; m_i < mesh_instances.size(); m_i++) {
		MeshInstance &mi = mesh_instances.write[m_i];
		mi.offset.x = best_atlas_offsets[m_i].x;
		mi.offset.y = best_atlas_offsets[m_i].y;
		mi.slice = best_atlas_offsets[m_i].z;
		albedo_images.write[mi.slice]->blit_rect(mi.data.albedo_on_uv2, Rect2i(Vector2i(), mi.data.albedo_on_uv2->get_size()), mi.offset);
		emission_images.write[mi.slice]->blit_rect(mi.data.emission_on_uv2, Rect2(Vector2i(), mi.data.emission_on_uv2->get_size()), mi.offset);
	}

	return BAKE_OK;
}

void LightmapperRD::_create_acceleration_structures(RenderingDevice *rd, Size2i atlas_size, int atlas_slices, AABB &bounds, int grid_size, uint32_t p_cluster_size, Vector<Probe> &p_probe_positions, GenerateProbes p_generate_probes, Vector<int> &slice_triangle_count, Vector<int> &slice_seam_count, RID &vertex_buffer, RID &triangle_buffer, RID &lights_buffer, RID &r_triangle_indices_buffer, RID &r_cluster_indices_buffer, RID &r_cluster_aabbs_buffer, RID &probe_positions_buffer, RID &grid_texture, RID &seams_buffer, BakeStepFunc p_step_function, void *p_bake_userdata) {
	HashMap<Vertex, uint32_t, VertexHash> vertex_map;

	//fill triangles array and vertex array
	LocalVector<Triangle> triangles;
	LocalVector<Vertex> vertex_array;
	LocalVector<Seam> seams;

	slice_triangle_count.resize(atlas_slices);
	slice_seam_count.resize(atlas_slices);

	for (int i = 0; i < atlas_slices; i++) {
		slice_triangle_count.write[i] = 0;
		slice_seam_count.write[i] = 0;
	}

	bounds = AABB();

	for (int m_i = 0; m_i < mesh_instances.size(); m_i++) {
		if (p_step_function) {
			float p = float(m_i + 1) / MAX(1, mesh_instances.size()) * 0.1;
			p_step_function(0.3 + p, vformat(RTR("Plotting mesh into acceleration structure %d/%d"), m_i + 1, mesh_instances.size()), p_bake_userdata, false);
		}

		HashMap<Edge, EdgeUV2, EdgeHash> edges;

		MeshInstance &mi = mesh_instances.write[m_i];

		Vector2 uv_scale = Vector2(mi.data.albedo_on_uv2->get_width(), mi.data.albedo_on_uv2->get_height()) / Vector2(atlas_size);
		Vector2 uv_offset = Vector2(mi.offset) / Vector2(atlas_size);
		if (m_i == 0) {
			bounds.position = mi.data.points[0];
		}

		for (int i = 0; i < mi.data.points.size(); i += 3) {
			Vector3 vtxs[3] = { mi.data.points[i + 0], mi.data.points[i + 1], mi.data.points[i + 2] };
			Vector2 uvs[3] = { mi.data.uv2[i + 0] * uv_scale + uv_offset, mi.data.uv2[i + 1] * uv_scale + uv_offset, mi.data.uv2[i + 2] * uv_scale + uv_offset };
			Vector3 normal[3] = { mi.data.normal[i + 0], mi.data.normal[i + 1], mi.data.normal[i + 2] };

			AABB taabb;
			Triangle t;
			t.slice = mi.slice;
			for (int k = 0; k < 3; k++) {
				bounds.expand_to(vtxs[k]);

				Vertex v;
				v.position[0] = vtxs[k].x;
				v.position[1] = vtxs[k].y;
				v.position[2] = vtxs[k].z;
				v.uv[0] = uvs[k].x;
				v.uv[1] = uvs[k].y;
				v.normal_xy[0] = normal[k].x;
				v.normal_xy[1] = normal[k].y;
				v.normal_z = normal[k].z;

				uint32_t *indexptr = vertex_map.getptr(v);

				if (indexptr) {
					t.indices[k] = *indexptr;
				} else {
					uint32_t new_index = vertex_map.size();
					t.indices[k] = new_index;
					vertex_map[v] = new_index;
					vertex_array.push_back(v);
				}

				if (k == 0) {
					taabb.position = vtxs[k];
				} else {
					taabb.expand_to(vtxs[k]);
				}
			}

			//compute seams that will need to be blended later
			for (int k = 0; k < 3; k++) {
				int n = (k + 1) % 3;

				Edge edge(vtxs[k], vtxs[n], normal[k], normal[n]);
				Vector2i edge_indices(t.indices[k], t.indices[n]);
				EdgeUV2 uv2(uvs[k], uvs[n], edge_indices);

				if (edge.b == edge.a) {
					continue; //degenerate, somehow
				}
				if (edge.b < edge.a) {
					SWAP(edge.a, edge.b);
					SWAP(edge.na, edge.nb);
					SWAP(uv2.a, uv2.b);
					SWAP(uv2.indices.x, uv2.indices.y);
					SWAP(edge_indices.x, edge_indices.y);
				}

				EdgeUV2 *euv2 = edges.getptr(edge);
				if (!euv2) {
					edges[edge] = uv2;
				} else {
					if (*euv2 == uv2) {
						continue; // seam shared UV space, no need to blend
					}
					if (euv2->seam_found) {
						continue; //bad geometry
					}

					Seam seam;
					seam.a = edge_indices;
					seam.b = euv2->indices;
					seam.slice = mi.slice;
					seams.push_back(seam);
					slice_seam_count.write[mi.slice]++;
					euv2->seam_found = true;
				}
			}

			t.min_bounds[0] = taabb.position.x;
			t.min_bounds[1] = taabb.position.y;
			t.min_bounds[2] = taabb.position.z;
			t.max_bounds[0] = taabb.position.x + MAX(taabb.size.x, 0.0001);
			t.max_bounds[1] = taabb.position.y + MAX(taabb.size.y, 0.0001);
			t.max_bounds[2] = taabb.position.z + MAX(taabb.size.z, 0.0001);
			t.pad0 = t.pad1 = 0; //make valgrind not complain
			triangles.push_back(t);
			slice_triangle_count.write[t.slice]++;
		}
	}

	//also consider probe positions for bounds
	for (int i = 0; i < p_probe_positions.size(); i++) {
		Vector3 pp(p_probe_positions[i].position[0], p_probe_positions[i].position[1], p_probe_positions[i].position[2]);
		bounds.expand_to(pp);
	}
	bounds.grow_by(0.1); //grow a bit to avoid numerical error

	triangles.sort(); //sort by slice
	seams.sort();

	if (p_step_function) {
		p_step_function(0.4, RTR("Optimizing acceleration structure"), p_bake_userdata, true);
	}

	//fill list of triangles in grid
	LocalVector<TriangleSort> triangle_sort;
	for (uint32_t i = 0; i < triangles.size(); i++) {
		const Triangle &t = triangles[i];
		Vector3 face[3] = {
			Vector3(vertex_array[t.indices[0]].position[0], vertex_array[t.indices[0]].position[1], vertex_array[t.indices[0]].position[2]),
			Vector3(vertex_array[t.indices[1]].position[0], vertex_array[t.indices[1]].position[1], vertex_array[t.indices[1]].position[2]),
			Vector3(vertex_array[t.indices[2]].position[0], vertex_array[t.indices[2]].position[1], vertex_array[t.indices[2]].position[2])
		};
		_plot_triangle_into_triangle_index_list(grid_size, Vector3i(), bounds, face, i, triangle_sort, grid_size);
	}
	//sort it
	triangle_sort.sort();

	LocalVector<uint32_t> cluster_indices;
	LocalVector<ClusterAABB> cluster_aabbs;
	Vector<uint32_t> triangle_indices;
	triangle_indices.resize(triangle_sort.size());
	Vector<uint32_t> grid_indices;
	grid_indices.resize(grid_size * grid_size * grid_size * 2);
	memset(grid_indices.ptrw(), 0, grid_indices.size() * sizeof(uint32_t));

	{
		// Fill grid with cell indices.
		uint32_t last_cell = 0xFFFFFFFF;
		uint32_t *giw = grid_indices.ptrw();
		uint32_t cluster_count = 0;
		uint32_t solid_cell_count = 0;
		for (uint32_t i = 0; i < triangle_sort.size(); i++) {
			uint32_t cell = triangle_sort[i].cell_index;
			if (cell != last_cell) {
				giw[cell * 2 + 1] = solid_cell_count;
				solid_cell_count++;
			}

			if ((giw[cell * 2] % p_cluster_size) == 0) {
				// Add an extra cluster every time the triangle counter reaches a multiple of the cluster size.
				cluster_count++;
			}

			giw[cell * 2]++;
			last_cell = cell;
		}

		// Build fixed-size triangle clusters for all the cells to speed up the traversal. A cell can hold multiple clusters that each contain a fixed
		// amount of triangles and an AABB. The tracer will check against the AABBs first to know whether it needs to visit the cell's triangles.
		//
		// The building algorithm will divide the triangles recursively contained inside each cell, sorting by the longest axis of the AABB on each step.
		//
		// - If the amount of triangles is less or equal to the cluster size, the AABB will be stored and the algorithm stops.
		//
		// - The division by two is increased to the next power of two of half the amount of triangles (with cluster size as the minimum value) to
		//   ensure the first half always fills the cluster.

		cluster_indices.resize(solid_cell_count * 2);
		cluster_aabbs.resize(cluster_count);

		uint32_t i = 0;
		uint32_t cluster_index = 0;
		uint32_t solid_cell_index = 0;
		uint32_t *tiw = triangle_indices.ptrw();
		while (i < triangle_sort.size()) {
			cluster_indices[solid_cell_index * 2] = cluster_index;
			cluster_indices[solid_cell_index * 2 + 1] = i;

			uint32_t cell = triangle_sort[i].cell_index;
			uint32_t triangle_count = giw[cell * 2];
			uint32_t cell_cluster_count = (triangle_count + p_cluster_size - 1) / p_cluster_size;
			_sort_triangle_clusters(p_cluster_size, cluster_index, i, triangle_count, triangle_sort, cluster_aabbs);

			for (uint32_t j = 0; j < triangle_count; j++) {
				tiw[i + j] = triangle_sort[i + j].triangle_index;
			}

			i += triangle_count;
			cluster_index += cell_cluster_count;
			solid_cell_index++;
		}
	}
#if 0
	for (int i = 0; i < grid_size; i++) {
		for (int j = 0; j < grid_size; j++) {
			for (int k = 0; k < grid_size; k++) {
				uint32_t index = i * (grid_size * grid_size) + j * grid_size + k;
				grid_indices.write[index * 2] = float(i) / grid_size * 255;
				grid_indices.write[index * 2 + 1] = float(j) / grid_size * 255;
			}
		}
	}
#endif

#if 0
	for (int i = 0; i < grid_size; i++) {
		Vector<uint8_t> grid_usage;
		grid_usage.resize(grid_size * grid_size);
		for (int j = 0; j < grid_usage.size(); j++) {
			uint32_t ofs = i * grid_size * grid_size + j;
			uint32_t count = grid_indices[ofs * 2];
			grid_usage.write[j] = count > 0 ? 255 : 0;
		}

		Ref<Image> img = Image::create_from_data(grid_size, grid_size, false, Image::FORMAT_L8, grid_usage);
		img->save_png("res://grid_layer_" + itos(1000 + i).substr(1, 3) + ".png");
	}
#endif

	/*****************************/
	/*** CREATE GPU STRUCTURES ***/
	/*****************************/

	lights.sort();

	Vector<Vector2i> seam_buffer_vec;
	seam_buffer_vec.resize(seams.size() * 2);
	for (uint32_t i = 0; i < seams.size(); i++) {
		seam_buffer_vec.write[i * 2 + 0] = seams[i].a;
		seam_buffer_vec.write[i * 2 + 1] = seams[i].b;
	}

	{ //buffers
		Vector<uint8_t> vb = vertex_array.to_byte_array();
		vertex_buffer = rd->storage_buffer_create(vb.size(), vb);

		Vector<uint8_t> tb = triangles.to_byte_array();
		triangle_buffer = rd->storage_buffer_create(tb.size(), tb);

		Vector<uint8_t> tib = triangle_indices.to_byte_array();
		r_triangle_indices_buffer = rd->storage_buffer_create(tib.size(), tib);

		Vector<uint8_t> cib = cluster_indices.to_byte_array();
		r_cluster_indices_buffer = rd->storage_buffer_create(cib.size(), cib);

		Vector<uint8_t> cab = cluster_aabbs.to_byte_array();
		r_cluster_aabbs_buffer = rd->storage_buffer_create(cab.size(), cab);

		Vector<uint8_t> lb = lights.to_byte_array();
		if (lb.size() == 0) {
			lb.resize(sizeof(Light)); //even if no lights, the buffer must exist
		}
		lights_buffer = rd->storage_buffer_create(lb.size(), lb);

		Vector<uint8_t> sb = seam_buffer_vec.to_byte_array();
		if (sb.size() == 0) {
			sb.resize(sizeof(Vector2i) * 2); //even if no seams, the buffer must exist
		}
		seams_buffer = rd->storage_buffer_create(sb.size(), sb);

		Vector<uint8_t> pb = p_probe_positions.to_byte_array();
		if (pb.size() == 0) {
			pb.resize(sizeof(Probe));
		}
		probe_positions_buffer = rd->storage_buffer_create(pb.size(), pb);
	}

	{ //grid

		RD::TextureFormat tf;
		tf.width = grid_size;
		tf.height = grid_size;
		tf.depth = grid_size;
		tf.texture_type = RD::TEXTURE_TYPE_3D;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;

		Vector<Vector<uint8_t>> texdata;
		texdata.resize(1);
		//grid and indices
		tf.format = RD::DATA_FORMAT_R32G32_UINT;
		texdata.write[0] = grid_indices.to_byte_array();
		grid_texture = rd->texture_create(tf, RD::TextureView(), texdata);
	}
}

void LightmapperRD::_raster_geometry(RenderingDevice *rd, Size2i atlas_size, int atlas_slices, int grid_size, AABB bounds, float p_bias, Vector<int> slice_triangle_count, RID position_tex, RID unocclude_tex, RID normal_tex, RID raster_depth_buffer, RID rasterize_shader, RID raster_base_uniform) {
	Vector<RID> framebuffers;

	for (int i = 0; i < atlas_slices; i++) {
		RID slice_pos_tex = rd->texture_create_shared_from_slice(RD::TextureView(), position_tex, i, 0);
		RID slice_unoc_tex = rd->texture_create_shared_from_slice(RD::TextureView(), unocclude_tex, i, 0);
		RID slice_norm_tex = rd->texture_create_shared_from_slice(RD::TextureView(), normal_tex, i, 0);
		Vector<RID> fb;
		fb.push_back(slice_pos_tex);
		fb.push_back(slice_norm_tex);
		fb.push_back(slice_unoc_tex);
		fb.push_back(raster_depth_buffer);
		framebuffers.push_back(rd->framebuffer_create(fb));
	}

	RD::PipelineDepthStencilState ds;
	ds.enable_depth_test = true;
	ds.enable_depth_write = true;
	ds.depth_compare_operator = RD::COMPARE_OP_LESS; //so it does render same pixel twice

	RID raster_pipeline = rd->render_pipeline_create(rasterize_shader, rd->framebuffer_get_format(framebuffers[0]), RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(3), 0);
	RID raster_pipeline_wire;
	{
		RD::PipelineRasterizationState rw;
		rw.wireframe = true;
		raster_pipeline_wire = rd->render_pipeline_create(rasterize_shader, rd->framebuffer_get_format(framebuffers[0]), RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, rw, RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(3), 0);
	}

	uint32_t triangle_offset = 0;
	Vector<Color> clear_colors;
	clear_colors.push_back(Color(0, 0, 0, 0));
	clear_colors.push_back(Color(0, 0, 0, 0));
	clear_colors.push_back(Color(0, 0, 0, 0));

	for (int i = 0; i < atlas_slices; i++) {
		RasterPushConstant raster_push_constant;
		raster_push_constant.atlas_size[0] = atlas_size.x;
		raster_push_constant.atlas_size[1] = atlas_size.y;
		raster_push_constant.base_triangle = triangle_offset;
		raster_push_constant.to_cell_offset[0] = bounds.position.x;
		raster_push_constant.to_cell_offset[1] = bounds.position.y;
		raster_push_constant.to_cell_offset[2] = bounds.position.z;
		raster_push_constant.bias = p_bias;
		raster_push_constant.to_cell_size[0] = (1.0 / bounds.size.x) * float(grid_size);
		raster_push_constant.to_cell_size[1] = (1.0 / bounds.size.y) * float(grid_size);
		raster_push_constant.to_cell_size[2] = (1.0 / bounds.size.z) * float(grid_size);
		raster_push_constant.grid_size[0] = grid_size;
		raster_push_constant.grid_size[1] = grid_size;
		raster_push_constant.grid_size[2] = grid_size;

		// Half pixel offset is required so the rasterizer doesn't output face edges directly aligned into pixels.
		// This fixes artifacts where the pixel would be traced from the edge of a face, causing half the rays to
		// be outside of the boundaries of the geometry. See <https://github.com/godotengine/godot/issues/69126>.
		raster_push_constant.uv_offset[0] = -0.5f / float(atlas_size.x);
		raster_push_constant.uv_offset[1] = -0.5f / float(atlas_size.y);

		RD::DrawListID draw_list = rd->draw_list_begin(framebuffers[i], RD::DRAW_CLEAR_ALL, clear_colors, 1.0f, 0, Rect2(), RDD::BreadcrumbMarker::LIGHTMAPPER_PASS);
		//draw opaque
		rd->draw_list_bind_render_pipeline(draw_list, raster_pipeline);
		rd->draw_list_bind_uniform_set(draw_list, raster_base_uniform, 0);
		rd->draw_list_set_push_constant(draw_list, &raster_push_constant, sizeof(RasterPushConstant));
		rd->draw_list_draw(draw_list, false, 1, slice_triangle_count[i] * 3);
		//draw wire
		rd->draw_list_bind_render_pipeline(draw_list, raster_pipeline_wire);
		rd->draw_list_bind_uniform_set(draw_list, raster_base_uniform, 0);
		rd->draw_list_set_push_constant(draw_list, &raster_push_constant, sizeof(RasterPushConstant));
		rd->draw_list_draw(draw_list, false, 1, slice_triangle_count[i] * 3);

		rd->draw_list_end();

		triangle_offset += slice_triangle_count[i];
	}
}

static Vector<RD::Uniform> dilate_or_denoise_common_uniforms(RID &p_source_light_tex, RID &p_dest_light_tex) {
	Vector<RD::Uniform> uniforms;
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
		u.binding = 0;
		u.append_id(p_dest_light_tex);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 1;
		u.append_id(p_source_light_tex);
		uniforms.push_back(u);
	}

	return uniforms;
}

LightmapperRD::BakeError LightmapperRD::_dilate(RenderingDevice *rd, Ref<RDShaderFile> &compute_shader, RID &compute_base_uniform_set, PushConstant &push_constant, RID &source_light_tex, RID &dest_light_tex, const Size2i &atlas_size, int atlas_slices) {
	Vector<RD::Uniform> uniforms = dilate_or_denoise_common_uniforms(source_light_tex, dest_light_tex);

	RID compute_shader_dilate = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("dilate"));
	ERR_FAIL_COND_V(compute_shader_dilate.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); //internal check, should not happen
	RID compute_shader_dilate_pipeline = rd->compute_pipeline_create(compute_shader_dilate);

	RID dilate_uniform_set = rd->uniform_set_create(uniforms, compute_shader_dilate, 1);

	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_dilate_pipeline);
	rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
	rd->compute_list_bind_uniform_set(compute_list, dilate_uniform_set, 1);
	push_constant.region_ofs[0] = 0;
	push_constant.region_ofs[1] = 0;
	Vector3i group_size(Math::division_round_up(atlas_size.x, 8), Math::division_round_up(atlas_size.y, 8), 1); //restore group size

	for (int i = 0; i < atlas_slices; i++) {
		push_constant.atlas_slice = i;
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		rd->compute_list_dispatch(compute_list, group_size.x, group_size.y, group_size.z);
		//no barrier, let them run all together
	}
	rd->compute_list_end();
	rd->free(compute_shader_dilate);

#ifdef DEBUG_TEXTURES
	for (int i = 0; i < atlas_slices; i++) {
		Vector<uint8_t> s = rd->texture_get_data(source_light_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->convert(Image::FORMAT_RGBA8);
		img->save_png("res://5_dilated_" + itos(i) + ".png");
	}
#endif
	return BAKE_OK;
}

LightmapperRD::BakeError LightmapperRD::_pack_l1(RenderingDevice *rd, Ref<RDShaderFile> &compute_shader, RID &compute_base_uniform_set, PushConstant &push_constant, RID &source_light_tex, RID &dest_light_tex, const Size2i &atlas_size, int atlas_slices) {
	Vector<RD::Uniform> uniforms = dilate_or_denoise_common_uniforms(source_light_tex, dest_light_tex);

	RID compute_shader_pack = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("pack_coeffs"));
	ERR_FAIL_COND_V(compute_shader_pack.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); //internal check, should not happen
	RID compute_shader_pack_pipeline = rd->compute_pipeline_create(compute_shader_pack);

	RID dilate_uniform_set = rd->uniform_set_create(uniforms, compute_shader_pack, 1);

	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_pack_pipeline);
	rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
	rd->compute_list_bind_uniform_set(compute_list, dilate_uniform_set, 1);
	push_constant.region_ofs[0] = 0;
	push_constant.region_ofs[1] = 0;
	Vector3i group_size(Math::division_round_up(atlas_size.x, 8), Math::division_round_up(atlas_size.y, 8), 1); //restore group size

	for (int i = 0; i < atlas_slices; i++) {
		push_constant.atlas_slice = i;
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		rd->compute_list_dispatch(compute_list, group_size.x, group_size.y, group_size.z);
		//no barrier, let them run all together
	}
	rd->compute_list_end();
	rd->free(compute_shader_pack);

	return BAKE_OK;
}

Error LightmapperRD::_store_pfm(RenderingDevice *p_rd, RID p_atlas_tex, int p_index, const Size2i &p_atlas_size, const String &p_name, bool p_shadowmask) {
	Vector<uint8_t> data = p_rd->texture_get_data(p_atlas_tex, p_index);
	Ref<Image> img = Image::create_from_data(p_atlas_size.width, p_atlas_size.height, false, p_shadowmask ? Image::FORMAT_RGBA8 : Image::FORMAT_RGBAH, data);
	img->convert(Image::FORMAT_RGBF);
	Vector<uint8_t> data_float = img->get_data();

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_name, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save PFN at path: '%s'.", p_name));
	file->store_line("PF");
	file->store_line(vformat("%d %d", img->get_width(), img->get_height()));
#ifdef BIG_ENDIAN_ENABLED
	file->store_line("1.0");
#else
	file->store_line("-1.0");
#endif
	file->store_buffer(data_float);
	file->close();

	return OK;
}

Ref<Image> LightmapperRD::_read_pfm(const String &p_name, bool p_shadowmask) {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_name, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err, Ref<Image>(), vformat("Can't load PFM at path: '%s'.", p_name));
	ERR_FAIL_COND_V(file->get_line() != "PF", Ref<Image>());

	Vector<String> new_size = file->get_line().split(" ");
	ERR_FAIL_COND_V(new_size.size() != 2, Ref<Image>());
	int new_width = new_size[0].to_int();
	int new_height = new_size[1].to_int();

	float endian = file->get_line().to_float();
	Vector<uint8_t> new_data = file->get_buffer(file->get_length() - file->get_position());
	file->close();

#ifdef BIG_ENDIAN_ENABLED
	if (unlikely(endian < 0.0)) {
		uint32_t count = new_data.size() / 4;
		uint16_t *dst = (uint16_t *)new_data.ptrw();
		for (uint32_t j = 0; j < count; j++) {
			dst[j * 4] = BSWAP32(dst[j * 4]);
		}
	}
#else
	if (unlikely(endian > 0.0)) {
		uint32_t count = new_data.size() / 4;
		uint16_t *dst = (uint16_t *)new_data.ptrw();
		for (uint32_t j = 0; j < count; j++) {
			dst[j * 4] = BSWAP32(dst[j * 4]);
		}
	}
#endif
	Ref<Image> img = Image::create_from_data(new_width, new_height, false, Image::FORMAT_RGBF, new_data);
	img->convert(p_shadowmask ? Image::FORMAT_RGBA8 : Image::FORMAT_RGBAH);
	return img;
}

LightmapperRD::BakeError LightmapperRD::_denoise_oidn(RenderingDevice *p_rd, RID p_source_light_tex, RID p_source_normal_tex, RID p_dest_light_tex, const Size2i &p_atlas_size, int p_atlas_slices, bool p_bake_sh, bool p_shadowmask, const String &p_exe) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	for (int i = 0; i < p_atlas_slices; i++) {
		String fname_norm_in = EditorPaths::get_singleton()->get_cache_dir().path_join(vformat("temp_norm_%d.pfm", i));
		_store_pfm(p_rd, p_source_normal_tex, i, p_atlas_size, fname_norm_in, false);

		for (int j = 0; j < (p_bake_sh ? 4 : 1); j++) {
			int index = i * (p_bake_sh ? 4 : 1) + j;
			String fname_light_in = EditorPaths::get_singleton()->get_cache_dir().path_join(vformat("temp_light_%d.pfm", index));
			String fname_out = EditorPaths::get_singleton()->get_cache_dir().path_join(vformat("temp_denoised_%d.pfm", index));

			_store_pfm(p_rd, p_source_light_tex, index, p_atlas_size, fname_light_in, p_shadowmask);

			List<String> args;
			args.push_back("--device");
			args.push_back("default");

			args.push_back("--filter");
			args.push_back("RTLightmap");

			args.push_back(p_shadowmask ? "--ldr" : "--hdr");
			args.push_back(fname_light_in);

			args.push_back("--nrm");
			args.push_back(fname_norm_in);

			args.push_back("--output");
			args.push_back(fname_out);

			String str;
			int exitcode = 0;

			Error err = OS::get_singleton()->execute(p_exe, args, &str, &exitcode, true);

			da->remove(fname_light_in);

			if (err != OK || exitcode != 0) {
				da->remove(fname_out);
				print_verbose(str);
				ERR_FAIL_V_MSG(BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES, vformat("OIDN denoiser failed, return code: %d", exitcode));
			}

			Ref<Image> img = _read_pfm(fname_out, p_shadowmask);
			da->remove(fname_out);

			ERR_FAIL_COND_V(img.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

			Vector<uint8_t> old_data = p_rd->texture_get_data(p_source_light_tex, index);
			Vector<uint8_t> new_data = img->get_data();
			img.unref(); // Avoid copy on write.

			uint32_t count = old_data.size() / 2;
			const uint16_t *src = (const uint16_t *)old_data.ptr();
			uint16_t *dst = (uint16_t *)new_data.ptrw();
			for (uint32_t k = 0; k < count; k += 4) {
				dst[k + 3] = src[k + 3];
			}

			p_rd->texture_update(p_dest_light_tex, index, new_data);
		}
		da->remove(fname_norm_in);
	}
	return BAKE_OK;
}

LightmapperRD::BakeError LightmapperRD::_denoise(RenderingDevice *p_rd, Ref<RDShaderFile> &p_compute_shader, const RID &p_compute_base_uniform_set, PushConstant &p_push_constant, RID p_source_light_tex, RID p_source_normal_tex, RID p_dest_light_tex, float p_denoiser_strength, int p_denoiser_range, const Size2i &p_atlas_size, int p_atlas_slices, bool p_bake_sh, BakeStepFunc p_step_function, void *p_bake_userdata) {
	RID denoise_params_buffer = p_rd->uniform_buffer_create(sizeof(DenoiseParams));
	DenoiseParams denoise_params;
	denoise_params.spatial_bandwidth = 5.0f;
	denoise_params.light_bandwidth = p_denoiser_strength;
	denoise_params.albedo_bandwidth = 1.0f;
	denoise_params.normal_bandwidth = 0.1f;
	denoise_params.filter_strength = 10.0f;
	denoise_params.half_search_window = p_denoiser_range;
	p_rd->buffer_update(denoise_params_buffer, 0, sizeof(DenoiseParams), &denoise_params);

	Vector<RD::Uniform> uniforms = dilate_or_denoise_common_uniforms(p_source_light_tex, p_dest_light_tex);
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 2;
		u.append_id(p_source_normal_tex);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 3;
		u.append_id(denoise_params_buffer);
		uniforms.push_back(u);
	}

	RID compute_shader_denoise = p_rd->shader_create_from_spirv(p_compute_shader->get_spirv_stages("denoise"));
	ERR_FAIL_COND_V(compute_shader_denoise.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	RID compute_shader_denoise_pipeline = p_rd->compute_pipeline_create(compute_shader_denoise);
	RID denoise_uniform_set = p_rd->uniform_set_create(uniforms, compute_shader_denoise, 1);

	// We denoise in fixed size regions and synchronize execution to avoid GPU timeouts.
	// We use a region with 1/4 the amount of pixels if we're denoising SH lightmaps, as
	// all four of them are denoised in the shader in one dispatch.
	const int max_region_size = p_bake_sh ? 512 : 1024;
	int x_regions = Math::division_round_up(p_atlas_size.width, max_region_size);
	int y_regions = Math::division_round_up(p_atlas_size.height, max_region_size);
	for (int s = 0; s < p_atlas_slices; s++) {
		p_push_constant.atlas_slice = s;

		for (int i = 0; i < x_regions; i++) {
			for (int j = 0; j < y_regions; j++) {
				int x = i * max_region_size;
				int y = j * max_region_size;
				int w = MIN((i + 1) * max_region_size, p_atlas_size.width) - x;
				int h = MIN((j + 1) * max_region_size, p_atlas_size.height) - y;
				p_push_constant.region_ofs[0] = x;
				p_push_constant.region_ofs[1] = y;

				RD::ComputeListID compute_list = p_rd->compute_list_begin();
				p_rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_denoise_pipeline);
				p_rd->compute_list_bind_uniform_set(compute_list, p_compute_base_uniform_set, 0);
				p_rd->compute_list_bind_uniform_set(compute_list, denoise_uniform_set, 1);
				p_rd->compute_list_set_push_constant(compute_list, &p_push_constant, sizeof(PushConstant));
				p_rd->compute_list_dispatch(compute_list, Math::division_round_up(w, 8), Math::division_round_up(h, 8), 1);
				p_rd->compute_list_end();

				p_rd->submit();
				p_rd->sync();
			}
		}
		if (p_step_function) {
			int percent = (s + 1) * 100 / p_atlas_slices;
			float p = float(s) / p_atlas_slices * 0.1;
			if (p_step_function(0.8 + p, vformat(RTR("Denoising %d%%"), percent), p_bake_userdata, false)) {
				return BAKE_ERROR_USER_ABORTED;
			}
		}
	}

	p_rd->free(compute_shader_denoise);
	p_rd->free(denoise_params_buffer);

	return BAKE_OK;
}

LightmapperRD::BakeError LightmapperRD::bake(BakeQuality p_quality, bool p_use_denoiser, float p_denoiser_strength, int p_denoiser_range, int p_bounces, float p_bounce_indirect_energy, float p_bias, int p_max_texture_size, bool p_bake_sh, bool p_bake_shadowmask, bool p_texture_for_bounces, GenerateProbes p_generate_probes, const Ref<Image> &p_environment_panorama, const Basis &p_environment_transform, BakeStepFunc p_step_function, void *p_bake_userdata, float p_exposure_normalization) {
	int denoiser = GLOBAL_GET("rendering/lightmapping/denoising/denoiser");
	String oidn_path = EDITOR_GET("filesystem/tools/oidn/oidn_denoise_path");

	if (p_use_denoiser && denoiser == 1) {
		// OIDN (external).
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

		if (da->dir_exists(oidn_path)) {
			if (OS::get_singleton()->get_name() == "Windows") {
				oidn_path = oidn_path.path_join("oidnDenoise.exe");
			} else {
				oidn_path = oidn_path.path_join("oidnDenoise");
			}
		}
		ERR_FAIL_COND_V_MSG(oidn_path.is_empty() || !da->file_exists(oidn_path), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES, "OIDN denoiser is selected in the project settings, but no or invalid OIDN executable path is configured in the editor settings.");
	}

	if (p_step_function) {
		p_step_function(0.0, RTR("Begin Bake"), p_bake_userdata, true);
	}
	lightmap_textures.clear();
	shadowmask_textures.clear();
	int grid_size = 128;

	/* STEP 1: Fetch material textures and compute the bounds */

	AABB bounds;
	Size2i atlas_size;
	int atlas_slices;
	Vector<Ref<Image>> albedo_images;
	Vector<Ref<Image>> emission_images;

	BakeError bake_error = _blit_meshes_into_atlas(p_max_texture_size, p_denoiser_range, albedo_images, emission_images, bounds, atlas_size, atlas_slices, p_step_function, p_bake_userdata);
	if (bake_error != BAKE_OK) {
		return bake_error;
	}

	// The index of the directional light used for shadowmasking.
	int shadowmask_light_idx = -1;

	// If there are no valid directional lights for shadowmasking, the entire
	// scene would be shadowed and this saves baking time.
	if (p_bake_shadowmask) {
		int shadowmask_lights_count = 0;

		for (int i = 0; i < lights.size(); i++) {
			if (lights[i].type == LightType::LIGHT_TYPE_DIRECTIONAL && !lights[i].static_bake) {
				if (shadowmask_light_idx < 0) {
					shadowmask_light_idx = i;
				}

				shadowmask_lights_count += 1;
			}
		}

		if (shadowmask_light_idx < 0) {
			p_bake_shadowmask = false;
			WARN_PRINT("Shadowmask disabled: no directional light with their bake mode set to dynamic exists.");

		} else if (shadowmask_lights_count > 1) {
			WARN_PRINT(
					vformat("%d directional lights detected for shadowmask baking. Only %s will be used.",
							shadowmask_lights_count, light_names[shadowmask_light_idx]));
		}
	}

#ifdef DEBUG_TEXTURES
	for (int i = 0; i < atlas_slices; i++) {
		albedo_images[i]->save_png("res://0_albedo_" + itos(i) + ".png");
		emission_images[i]->save_png("res://0_emission_" + itos(i) + ".png");
	}
#endif

	// Attempt to create a local device by requesting it from rendering server first.
	// If that fails because the current renderer is not implemented on top of RD, we fall back to creating
	// a local rendering device manually depending on the current platform.
	Error err;
	RenderingContextDriver *rcd = nullptr;
	RenderingDevice *rd = RenderingServer::get_singleton()->create_local_rendering_device();
	if (rd == nullptr) {
#if defined(RD_ENABLED)
#if defined(METAL_ENABLED)
		rcd = memnew(RenderingContextDriverMetal);
		rd = memnew(RenderingDevice);
#endif
#if defined(VULKAN_ENABLED)
		if (rcd == nullptr) {
			rcd = memnew(RenderingContextDriverVulkan);
			rd = memnew(RenderingDevice);
		}
#endif
#endif
		if (rcd != nullptr && rd != nullptr) {
			err = rcd->initialize();
			if (err == OK) {
				err = rd->initialize(rcd);
			}

			if (err != OK) {
				memdelete(rd);
				memdelete(rcd);
				rd = nullptr;
				rcd = nullptr;
			}
		}
	}

	ERR_FAIL_NULL_V(rd, BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	RID albedo_array_tex;
	RID emission_array_tex;
	RID normal_tex;
	RID position_tex;
	RID unocclude_tex;
	RID light_source_tex;
	RID light_dest_tex;
	RID light_accum_tex;
	RID light_accum_tex2;
	RID light_environment_tex;
	RID shadowmask_tex;
	RID shadowmask_tex2;

#define FREE_TEXTURES                \
	rd->free(albedo_array_tex);      \
	rd->free(emission_array_tex);    \
	rd->free(normal_tex);            \
	rd->free(position_tex);          \
	rd->free(unocclude_tex);         \
	rd->free(light_source_tex);      \
	rd->free(light_accum_tex2);      \
	rd->free(light_accum_tex);       \
	rd->free(light_environment_tex); \
	if (p_bake_shadowmask) {         \
		rd->free(shadowmask_tex);    \
		rd->free(shadowmask_tex2);   \
	}

	{ // create all textures

		Vector<Vector<uint8_t>> albedo_data;
		Vector<Vector<uint8_t>> emission_data;
		for (int i = 0; i < atlas_slices; i++) {
			albedo_data.push_back(albedo_images[i]->get_data());
			emission_data.push_back(emission_images[i]->get_data());
		}

		RD::TextureFormat tf;
		tf.width = atlas_size.width;
		tf.height = atlas_size.height;
		tf.array_layers = atlas_slices;
		tf.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;

		albedo_array_tex = rd->texture_create(tf, RD::TextureView(), albedo_data);

		tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;

		emission_array_tex = rd->texture_create(tf, RD::TextureView(), emission_data);

		//this will be rastered to
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
		normal_tex = rd->texture_create(tf, RD::TextureView());
		tf.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		position_tex = rd->texture_create(tf, RD::TextureView());
		unocclude_tex = rd->texture_create(tf, RD::TextureView());

		tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;

		// shadowmask
		if (p_bake_shadowmask) {
			tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;

			shadowmask_tex = rd->texture_create(tf, RD::TextureView());
			rd->texture_clear(shadowmask_tex, Color(0, 0, 0, 0), 0, 1, 0, atlas_slices);

			shadowmask_tex2 = rd->texture_create(tf, RD::TextureView());
			rd->texture_clear(shadowmask_tex2, Color(0, 0, 0, 0), 0, 1, 0, atlas_slices);
		}

		// lightmap
		tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;

		light_source_tex = rd->texture_create(tf, RD::TextureView());
		rd->texture_clear(light_source_tex, Color(0, 0, 0, 0), 0, 1, 0, atlas_slices);

		if (p_bake_sh) {
			tf.array_layers *= 4;
		}
		light_accum_tex = rd->texture_create(tf, RD::TextureView());
		rd->texture_clear(light_accum_tex, Color(0, 0, 0, 0), 0, 1, 0, tf.array_layers);
		light_dest_tex = rd->texture_create(tf, RD::TextureView());
		rd->texture_clear(light_dest_tex, Color(0, 0, 0, 0), 0, 1, 0, tf.array_layers);
		light_accum_tex2 = light_dest_tex;

		//env
		{
			Ref<Image> panorama_tex;
			if (p_environment_panorama.is_valid()) {
				panorama_tex = p_environment_panorama;
				panorama_tex->convert(Image::FORMAT_RGBAF);
			} else {
				panorama_tex.instantiate();
				panorama_tex->initialize_data(8, 8, false, Image::FORMAT_RGBAF);
				panorama_tex->fill(Color(0, 0, 0, 1));
			}

			RD::TextureFormat tfp;
			tfp.width = panorama_tex->get_width();
			tfp.height = panorama_tex->get_height();
			tfp.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
			tfp.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;

			Vector<Vector<uint8_t>> tdata;
			tdata.push_back(panorama_tex->get_data());
			light_environment_tex = rd->texture_create(tfp, RD::TextureView(), tdata);

#ifdef DEBUG_TEXTURES
			panorama_tex->save_exr("res://0_panorama.exr", false);
#endif
		}
	}

	/* STEP 2: create the acceleration structure for the GPU*/

	Vector<int> slice_triangle_count;
	RID bake_parameters_buffer;
	RID vertex_buffer;
	RID triangle_buffer;
	RID lights_buffer;
	RID triangle_indices_buffer;
	RID cluster_indices_buffer;
	RID cluster_aabbs_buffer;
	RID grid_texture;
	RID seams_buffer;
	RID probe_positions_buffer;

	Vector<int> slice_seam_count;

#define FREE_BUFFERS                   \
	rd->free(bake_parameters_buffer);  \
	rd->free(vertex_buffer);           \
	rd->free(triangle_buffer);         \
	rd->free(lights_buffer);           \
	rd->free(triangle_indices_buffer); \
	rd->free(cluster_indices_buffer);  \
	rd->free(cluster_aabbs_buffer);    \
	rd->free(grid_texture);            \
	rd->free(seams_buffer);            \
	rd->free(probe_positions_buffer);

	const uint32_t cluster_size = 16;
	_create_acceleration_structures(rd, atlas_size, atlas_slices, bounds, grid_size, cluster_size, probe_positions, p_generate_probes, slice_triangle_count, slice_seam_count, vertex_buffer, triangle_buffer, lights_buffer, triangle_indices_buffer, cluster_indices_buffer, cluster_aabbs_buffer, probe_positions_buffer, grid_texture, seams_buffer, p_step_function, p_bake_userdata);

	// Create global bake parameters buffer.
	BakeParameters bake_parameters;
	bake_parameters.world_size[0] = bounds.size.x;
	bake_parameters.world_size[1] = bounds.size.y;
	bake_parameters.world_size[2] = bounds.size.z;
	bake_parameters.bias = p_bias;
	bake_parameters.to_cell_offset[0] = bounds.position.x;
	bake_parameters.to_cell_offset[1] = bounds.position.y;
	bake_parameters.to_cell_offset[2] = bounds.position.z;
	bake_parameters.grid_size = grid_size;
	bake_parameters.to_cell_size[0] = (1.0 / bounds.size.x) * float(grid_size);
	bake_parameters.to_cell_size[1] = (1.0 / bounds.size.y) * float(grid_size);
	bake_parameters.to_cell_size[2] = (1.0 / bounds.size.z) * float(grid_size);
	bake_parameters.light_count = lights.size();
	bake_parameters.env_transform[0] = p_environment_transform.rows[0][0];
	bake_parameters.env_transform[1] = p_environment_transform.rows[1][0];
	bake_parameters.env_transform[2] = p_environment_transform.rows[2][0];
	bake_parameters.env_transform[3] = 0.0f;
	bake_parameters.env_transform[4] = p_environment_transform.rows[0][1];
	bake_parameters.env_transform[5] = p_environment_transform.rows[1][1];
	bake_parameters.env_transform[6] = p_environment_transform.rows[2][1];
	bake_parameters.env_transform[7] = 0.0f;
	bake_parameters.env_transform[8] = p_environment_transform.rows[0][2];
	bake_parameters.env_transform[9] = p_environment_transform.rows[1][2];
	bake_parameters.env_transform[10] = p_environment_transform.rows[2][2];
	bake_parameters.env_transform[11] = 0.0f;
	bake_parameters.atlas_size[0] = atlas_size.width;
	bake_parameters.atlas_size[1] = atlas_size.height;
	bake_parameters.exposure_normalization = p_exposure_normalization;
	bake_parameters.bounces = p_bounces;
	bake_parameters.bounce_indirect_energy = p_bounce_indirect_energy;
	bake_parameters.shadowmask_light_idx = shadowmask_light_idx;

	bake_parameters_buffer = rd->uniform_buffer_create(sizeof(BakeParameters));
	rd->buffer_update(bake_parameters_buffer, 0, sizeof(BakeParameters), &bake_parameters);

	if (p_step_function) {
		if (p_step_function(0.47, RTR("Preparing shaders"), p_bake_userdata, true)) {
			FREE_TEXTURES
			FREE_BUFFERS
			memdelete(rd);
			if (rcd != nullptr) {
				memdelete(rcd);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	//shaders
	Ref<RDShaderFile> raster_shader;
	raster_shader.instantiate();
	err = raster_shader->parse_versions_from_text(lm_raster_shader_glsl);
	if (err != OK) {
		raster_shader->print_errors("raster_shader");

		FREE_TEXTURES
		FREE_BUFFERS

		memdelete(rd);

		if (rcd != nullptr) {
			memdelete(rcd);
		}
	}
	ERR_FAIL_COND_V(err != OK, BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	RID rasterize_shader = rd->shader_create_from_spirv(raster_shader->get_spirv_stages());

	ERR_FAIL_COND_V(rasterize_shader.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); //this is a bug check, though, should not happen

	RID sampler;
	{
		RD::SamplerState s;
		s.mag_filter = RD::SAMPLER_FILTER_LINEAR;
		s.min_filter = RD::SAMPLER_FILTER_LINEAR;
		s.max_lod = 0;

		sampler = rd->sampler_create(s);
	}

	Vector<RD::Uniform> base_uniforms;
	{
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 0;
			u.append_id(bake_parameters_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(vertex_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(triangle_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			u.append_id(triangle_indices_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 4;
			u.append_id(lights_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 5;
			u.append_id(seams_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 6;
			u.append_id(probe_positions_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 7;
			u.append_id(grid_texture);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 8;
			u.append_id(albedo_array_tex);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 9;
			u.append_id(emission_array_tex);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 10;
			u.append_id(sampler);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 11;
			u.append_id(cluster_indices_buffer);
			base_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 12;
			u.append_id(cluster_aabbs_buffer);
			base_uniforms.push_back(u);
		}
	}

	RID raster_base_uniform = rd->uniform_set_create(base_uniforms, rasterize_shader, 0);
	RID raster_depth_buffer;
	{
		RD::TextureFormat tf;
		tf.width = atlas_size.width;
		tf.height = atlas_size.height;
		tf.depth = 1;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		tf.format = RD::DATA_FORMAT_D32_SFLOAT;
		tf.is_discardable = true;

		raster_depth_buffer = rd->texture_create(tf, RD::TextureView());
	}

	rd->submit();
	rd->sync();

	/* STEP 3: Raster the geometry to UV2 coords in the atlas textures GPU*/

	_raster_geometry(rd, atlas_size, atlas_slices, grid_size, bounds, p_bias, slice_triangle_count, position_tex, unocclude_tex, normal_tex, raster_depth_buffer, rasterize_shader, raster_base_uniform);

#ifdef DEBUG_TEXTURES

	for (int i = 0; i < atlas_slices; i++) {
		Vector<uint8_t> s = rd->texture_get_data(position_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAF, s);
		img->save_exr("res://1_position_" + itos(i) + ".exr", false);

		s = rd->texture_get_data(normal_tex, i);
		img->set_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->save_exr("res://1_normal_" + itos(i) + ".exr", false);
	}
#endif

#define FREE_RASTER_RESOURCES   \
	rd->free(rasterize_shader); \
	rd->free(sampler);          \
	rd->free(raster_depth_buffer);

	/* Plot direct light */

	Ref<RDShaderFile> compute_shader;
	String defines = "";
	defines += "\n#define CLUSTER_SIZE " + uitos(cluster_size) + "\n";

	if (p_bake_sh) {
		defines += "\n#define USE_SH_LIGHTMAPS\n";
	}

	if (p_texture_for_bounces) {
		defines += "\n#define USE_LIGHT_TEXTURE_FOR_BOUNCES\n";
	}

	if (p_bake_shadowmask) {
		defines += "\n#define USE_SHADOWMASK\n";
	}

	compute_shader.instantiate();
	err = compute_shader->parse_versions_from_text(lm_compute_shader_glsl, defines);
	if (err != OK) {
		FREE_TEXTURES
		FREE_BUFFERS
		FREE_RASTER_RESOURCES
		memdelete(rd);

		if (rcd != nullptr) {
			memdelete(rcd);
		}

		compute_shader->print_errors("compute_shader");
	}
	ERR_FAIL_COND_V(err != OK, BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	// Unoccluder
	RID compute_shader_unocclude = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("unocclude"));
	ERR_FAIL_COND_V(compute_shader_unocclude.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); // internal check, should not happen
	RID compute_shader_unocclude_pipeline = rd->compute_pipeline_create(compute_shader_unocclude);

	// Direct light
	RID compute_shader_primary = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("primary"));
	ERR_FAIL_COND_V(compute_shader_primary.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); // internal check, should not happen
	RID compute_shader_primary_pipeline = rd->compute_pipeline_create(compute_shader_primary);

	// Indirect light
	RID compute_shader_secondary = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("secondary"));
	ERR_FAIL_COND_V(compute_shader_secondary.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); //internal check, should not happen
	RID compute_shader_secondary_pipeline = rd->compute_pipeline_create(compute_shader_secondary);

	// Light probes
	RID compute_shader_light_probes = rd->shader_create_from_spirv(compute_shader->get_spirv_stages("light_probes"));
	ERR_FAIL_COND_V(compute_shader_light_probes.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES); //internal check, should not happen
	RID compute_shader_light_probes_pipeline = rd->compute_pipeline_create(compute_shader_light_probes);

	RID compute_base_uniform_set = rd->uniform_set_create(base_uniforms, compute_shader_primary, 0);

#define FREE_COMPUTE_RESOURCES          \
	rd->free(compute_shader_unocclude); \
	rd->free(compute_shader_primary);   \
	rd->free(compute_shader_secondary); \
	rd->free(compute_shader_light_probes);

	Vector3i group_size(Math::division_round_up(atlas_size.x, 8), Math::division_round_up(atlas_size.y, 8), 1);
	rd->submit();
	rd->sync();

	if (p_step_function) {
		if (p_step_function(0.49, RTR("Un-occluding geometry"), p_bake_userdata, true)) {
			FREE_TEXTURES
			FREE_BUFFERS
			FREE_RASTER_RESOURCES
			FREE_COMPUTE_RESOURCES
			memdelete(rd);
			if (rcd != nullptr) {
				memdelete(rcd);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	PushConstant push_constant;

	/* UNOCCLUDE */
	{
		Vector<RD::Uniform> uniforms;
		{
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 0;
				u.append_id(position_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 1;
				u.append_id(unocclude_tex); //will be unused
				uniforms.push_back(u);
			}
		}

		RID unocclude_uniform_set = rd->uniform_set_create(uniforms, compute_shader_unocclude, 1);

		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_unocclude_pipeline);
		rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
		rd->compute_list_bind_uniform_set(compute_list, unocclude_uniform_set, 1);

		for (int i = 0; i < atlas_slices; i++) {
			push_constant.atlas_slice = i;
			rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
			rd->compute_list_dispatch(compute_list, group_size.x, group_size.y, group_size.z);
			//no barrier, let them run all together
		}
		rd->compute_list_end(); //done
	}

	if (p_step_function) {
		if (p_step_function(0.5, RTR("Plot direct lighting"), p_bake_userdata, true)) {
			FREE_TEXTURES
			FREE_BUFFERS
			FREE_RASTER_RESOURCES
			FREE_COMPUTE_RESOURCES
			memdelete(rd);
			if (rcd != nullptr) {
				memdelete(rcd);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	// Set ray count to the quality used for direct light and bounces.
	switch (p_quality) {
		case BAKE_QUALITY_LOW: {
			push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/low_quality_ray_count");
		} break;
		case BAKE_QUALITY_MEDIUM: {
			push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/medium_quality_ray_count");
		} break;
		case BAKE_QUALITY_HIGH: {
			push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/high_quality_ray_count");
		} break;
		case BAKE_QUALITY_ULTRA: {
			push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/ultra_quality_ray_count");
		} break;
	}

	push_constant.ray_count = CLAMP(push_constant.ray_count, 16u, 8192u);

	/* PRIMARY (direct) LIGHT PASS */
	{
		Vector<RD::Uniform> uniforms;
		{
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 0;
				u.append_id(light_source_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				u.append_id(light_dest_tex); //will be unused
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 2;
				u.append_id(position_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 3;
				u.append_id(normal_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 4;
				u.append_id(light_accum_tex);
				uniforms.push_back(u);
			}

			if (p_bake_shadowmask) {
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 5;
				u.append_id(shadowmask_tex);
				uniforms.push_back(u);
			}
		}

		RID light_uniform_set = rd->uniform_set_create(uniforms, compute_shader_primary, 1);

		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_primary_pipeline);
		rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
		rd->compute_list_bind_uniform_set(compute_list, light_uniform_set, 1);

		for (int i = 0; i < atlas_slices; i++) {
			push_constant.atlas_slice = i;
			rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
			rd->compute_list_dispatch(compute_list, group_size.x, group_size.y, group_size.z);
			//no barrier, let them run all together
		}
		rd->compute_list_end(); //done
	}

#ifdef DEBUG_TEXTURES

	for (int i = 0; i < atlas_slices; i++) {
		Vector<uint8_t> s = rd->texture_get_data(light_source_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->save_exr("res://2_light_primary_" + itos(i) + ".exr", false);
	}

	if (p_bake_sh) {
		for (int i = 0; i < atlas_slices * 4; i++) {
			Vector<uint8_t> s = rd->texture_get_data(light_accum_tex, i);
			Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
			img->save_exr("res://2_light_primary_accum_" + itos(i) + ".exr", false);
		}
	}
#endif

	/* SECONDARY (indirect) LIGHT PASS(ES) */

	if (p_bounces > 0) {
		Vector<RD::Uniform> uniforms;
		{
			{
				// Unused.
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 0;
				u.append_id(light_dest_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				u.append_id(light_source_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 2;
				u.append_id(position_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 3;
				u.append_id(normal_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 4;
				u.append_id(light_accum_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 5;
				u.append_id(light_environment_tex);
				uniforms.push_back(u);
			}
		}

		RID secondary_uniform_set;
		secondary_uniform_set = rd->uniform_set_create(uniforms, compute_shader_secondary, 1);

		int max_region_size = nearest_power_of_2_templated(int(GLOBAL_GET("rendering/lightmapping/bake_performance/region_size")));
		int max_rays = GLOBAL_GET("rendering/lightmapping/bake_performance/max_rays_per_pass");

		int x_regions = Math::division_round_up(atlas_size.width, max_region_size);
		int y_regions = Math::division_round_up(atlas_size.height, max_region_size);

		int ray_iterations = Math::division_round_up((int32_t)push_constant.ray_count, max_rays);

		rd->submit();
		rd->sync();

		if (p_step_function) {
			if (p_step_function(0.6, RTR("Integrate indirect lighting"), p_bake_userdata, true)) {
				FREE_TEXTURES
				FREE_BUFFERS
				FREE_RASTER_RESOURCES
				FREE_COMPUTE_RESOURCES
				memdelete(rd);
				if (rcd != nullptr) {
					memdelete(rcd);
				}
				return BAKE_ERROR_USER_ABORTED;
			}
		}

		int count = 0;
		for (int s = 0; s < atlas_slices; s++) {
			push_constant.atlas_slice = s;

			for (int i = 0; i < x_regions; i++) {
				for (int j = 0; j < y_regions; j++) {
					int x = i * max_region_size;
					int y = j * max_region_size;
					int w = MIN((i + 1) * max_region_size, atlas_size.width) - x;
					int h = MIN((j + 1) * max_region_size, atlas_size.height) - y;

					push_constant.region_ofs[0] = x;
					push_constant.region_ofs[1] = y;

					group_size = Vector3i(Math::division_round_up(w, 8), Math::division_round_up(h, 8), 1);

					for (int k = 0; k < ray_iterations; k++) {
						RD::ComputeListID compute_list = rd->compute_list_begin();
						rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_secondary_pipeline);
						rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
						rd->compute_list_bind_uniform_set(compute_list, secondary_uniform_set, 1);

						push_constant.ray_from = k * max_rays;
						push_constant.ray_to = MIN((k + 1) * max_rays, int32_t(push_constant.ray_count));
						rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
						rd->compute_list_dispatch(compute_list, group_size.x, group_size.y, group_size.z);

						rd->compute_list_end();
						rd->submit();
						rd->sync();

						count++;
						if (p_step_function) {
							int total = (atlas_slices * x_regions * y_regions * ray_iterations);
							int percent = count * 100 / total;
							float p = float(count) / total * 0.1;
							if (p_step_function(0.6 + p, vformat(RTR("Integrate indirect lighting %d%%"), percent), p_bake_userdata, false)) {
								FREE_TEXTURES
								FREE_BUFFERS
								FREE_RASTER_RESOURCES
								FREE_COMPUTE_RESOURCES
								memdelete(rd);
								if (rcd != nullptr) {
									memdelete(rcd);
								}
								return BAKE_ERROR_USER_ABORTED;
							}
						}
					}
				}
			}
		}
	}

	/* LIGHTPROBES */

	RID light_probe_buffer;

	if (probe_positions.size()) {
		light_probe_buffer = rd->storage_buffer_create(sizeof(float) * 4 * 9 * probe_positions.size());

		if (p_step_function) {
			if (p_step_function(0.7, RTR("Baking light probes"), p_bake_userdata, true)) {
				FREE_TEXTURES
				FREE_BUFFERS
				FREE_RASTER_RESOURCES
				FREE_COMPUTE_RESOURCES
				if (probe_positions.size() > 0) {
					rd->free(light_probe_buffer);
				}
				memdelete(rd);
				if (rcd != nullptr) {
					memdelete(rcd);
				}
				return BAKE_ERROR_USER_ABORTED;
			}
		}

		Vector<RD::Uniform> uniforms;
		{
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(light_probe_buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				u.append_id(light_source_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 2;
				u.append_id(light_environment_tex);
				uniforms.push_back(u);
			}
		}
		RID light_probe_uniform_set = rd->uniform_set_create(uniforms, compute_shader_light_probes, 1);

		switch (p_quality) {
			case BAKE_QUALITY_LOW: {
				push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/low_quality_probe_ray_count");
			} break;
			case BAKE_QUALITY_MEDIUM: {
				push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/medium_quality_probe_ray_count");
			} break;
			case BAKE_QUALITY_HIGH: {
				push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/high_quality_probe_ray_count");
			} break;
			case BAKE_QUALITY_ULTRA: {
				push_constant.ray_count = GLOBAL_GET("rendering/lightmapping/bake_quality/ultra_quality_probe_ray_count");
			} break;
		}

		push_constant.ray_count = CLAMP(push_constant.ray_count, 16u, 8192u);
		push_constant.probe_count = probe_positions.size();

		int max_rays = GLOBAL_GET("rendering/lightmapping/bake_performance/max_rays_per_probe_pass");
		int ray_iterations = Math::division_round_up((int32_t)push_constant.ray_count, max_rays);

		for (int i = 0; i < ray_iterations; i++) {
			RD::ComputeListID compute_list = rd->compute_list_begin();
			rd->compute_list_bind_compute_pipeline(compute_list, compute_shader_light_probes_pipeline);
			rd->compute_list_bind_uniform_set(compute_list, compute_base_uniform_set, 0);
			rd->compute_list_bind_uniform_set(compute_list, light_probe_uniform_set, 1);

			push_constant.ray_from = i * max_rays;
			push_constant.ray_to = MIN((i + 1) * max_rays, int32_t(push_constant.ray_count));
			rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
			rd->compute_list_dispatch(compute_list, Math::division_round_up((int)probe_positions.size(), 64), 1, 1);

			rd->compute_list_end(); //done
			rd->submit();
			rd->sync();

			if (p_step_function) {
				int percent = i * 100 / ray_iterations;
				float p = float(i) / ray_iterations * 0.1;
				if (p_step_function(0.7 + p, vformat(RTR("Integrating light probes %d%%"), percent), p_bake_userdata, false)) {
					FREE_TEXTURES
					FREE_BUFFERS
					FREE_RASTER_RESOURCES
					FREE_COMPUTE_RESOURCES
					if (probe_positions.size() > 0) {
						rd->free(light_probe_buffer);
					}
					memdelete(rd);
					if (rcd != nullptr) {
						memdelete(rcd);
					}
					return BAKE_ERROR_USER_ABORTED;
				}
			}
		}
	}

#if 0
	for (int i = 0; i < probe_positions.size(); i++) {
		Ref<Image> img = Image::create_empty(6, 4, false, Image::FORMAT_RGB8);
		for (int j = 0; j < 6; j++) {
			Vector<uint8_t> s = rd->texture_get_data(lightprobe_tex, i * 6 + j);
			Ref<Image> img2 = Image::create_from_data(2, 2, false, Image::FORMAT_RGBAF, s);
			img2->convert(Image::FORMAT_RGB8);
			img->blit_rect(img2, Rect2i(0, 0, 2, 2), Point2i((j % 3) * 2, (j / 3) * 2));
		}
		img->save_png("res://3_light_probe_" + itos(i) + ".png");
	}
#endif

	/* DENOISE */

	if (p_use_denoiser) {
		if (p_step_function) {
			if (p_step_function(0.8, RTR("Denoising"), p_bake_userdata, true)) {
				FREE_TEXTURES
				FREE_BUFFERS
				FREE_RASTER_RESOURCES
				FREE_COMPUTE_RESOURCES
				if (probe_positions.size() > 0) {
					rd->free(light_probe_buffer);
				}
				memdelete(rd);
				if (rcd != nullptr) {
					memdelete(rcd);
				}
				return BAKE_ERROR_USER_ABORTED;
			}
		}

		{
			BakeError error;
			if (denoiser == 1) {
				// OIDN (external).
				error = _denoise_oidn(rd, light_accum_tex, normal_tex, light_accum_tex, atlas_size, atlas_slices, p_bake_sh, false, oidn_path);
			} else {
				// JNLM (built-in).
				SWAP(light_accum_tex, light_accum_tex2);
				error = _denoise(rd, compute_shader, compute_base_uniform_set, push_constant, light_accum_tex2, normal_tex, light_accum_tex, p_denoiser_strength, p_denoiser_range, atlas_size, atlas_slices, p_bake_sh, p_step_function, p_bake_userdata);
			}
			if (unlikely(error != BAKE_OK)) {
				return error;
			}
		}

		if (p_bake_shadowmask) {
			BakeError error;
			if (denoiser == 1) {
				// OIDN (external).
				error = _denoise_oidn(rd, shadowmask_tex, normal_tex, shadowmask_tex, atlas_size, atlas_slices, false, true, oidn_path);
			} else {
				// JNLM (built-in).
				SWAP(shadowmask_tex, shadowmask_tex2);
				error = _denoise(rd, compute_shader, compute_base_uniform_set, push_constant, shadowmask_tex2, normal_tex, shadowmask_tex, p_denoiser_strength, p_denoiser_range, atlas_size, atlas_slices, false, p_step_function, p_bake_userdata);
			}
			if (unlikely(error != BAKE_OK)) {
				return error;
			}
		}
	}

	/* DILATE */

	{
		SWAP(light_accum_tex, light_accum_tex2);
		BakeError error = _dilate(rd, compute_shader, compute_base_uniform_set, push_constant, light_accum_tex2, light_accum_tex, atlas_size, atlas_slices * (p_bake_sh ? 4 : 1));
		if (unlikely(error != BAKE_OK)) {
			return error;
		}

		if (p_bake_shadowmask) {
			SWAP(shadowmask_tex, shadowmask_tex2);
			error = _dilate(rd, compute_shader, compute_base_uniform_set, push_constant, shadowmask_tex2, shadowmask_tex, atlas_size, atlas_slices);
			if (unlikely(error != BAKE_OK)) {
				return error;
			}
		}
	}

#ifdef DEBUG_TEXTURES

	for (int i = 0; i < atlas_slices * (p_bake_sh ? 4 : 1); i++) {
		Vector<uint8_t> s = rd->texture_get_data(light_accum_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->save_exr("res://4_light_secondary_" + itos(i) + ".exr", false);
	}
#endif

	/* BLEND SEAMS */
	//shaders
	Ref<RDShaderFile> blendseams_shader;
	blendseams_shader.instantiate();
	err = blendseams_shader->parse_versions_from_text(lm_blendseams_shader_glsl);
	if (err != OK) {
		FREE_TEXTURES
		FREE_BUFFERS
		FREE_RASTER_RESOURCES
		FREE_COMPUTE_RESOURCES
		memdelete(rd);

		if (rcd != nullptr) {
			memdelete(rcd);
		}

		blendseams_shader->print_errors("blendseams_shader");
	}
	ERR_FAIL_COND_V(err != OK, BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	RID blendseams_line_raster_shader = rd->shader_create_from_spirv(blendseams_shader->get_spirv_stages("lines"));

	ERR_FAIL_COND_V(blendseams_line_raster_shader.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

	RID blendseams_triangle_raster_shader = rd->shader_create_from_spirv(blendseams_shader->get_spirv_stages("triangles"));

	ERR_FAIL_COND_V(blendseams_triangle_raster_shader.is_null(), BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES);

#define FREE_BLENDSEAMS_RESOURCES            \
	rd->free(blendseams_line_raster_shader); \
	rd->free(blendseams_triangle_raster_shader);

	{
		//pre copy
		for (int i = 0; i < atlas_slices * (p_bake_sh ? 4 : 1); i++) {
			rd->texture_copy(light_accum_tex, light_accum_tex2, Vector3(), Vector3(), Vector3(atlas_size.width, atlas_size.height, 1), 0, 0, i, i);
		}

		Vector<RID> framebuffers;
		for (int i = 0; i < atlas_slices * (p_bake_sh ? 4 : 1); i++) {
			RID slice_tex = rd->texture_create_shared_from_slice(RD::TextureView(), light_accum_tex, i, 0);
			Vector<RID> fb;
			fb.push_back(slice_tex);
			fb.push_back(raster_depth_buffer);
			framebuffers.push_back(rd->framebuffer_create(fb));
		}

		Vector<RD::Uniform> uniforms;
		{
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 0;
				u.append_id(light_accum_tex2);
				uniforms.push_back(u);
			}
		}

		RID blendseams_raster_uniform = rd->uniform_set_create(uniforms, blendseams_line_raster_shader, 1);

		bool debug = false;
		RD::PipelineColorBlendState bs = RD::PipelineColorBlendState::create_blend(1);
		bs.attachments.write[0].src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
		bs.attachments.write[0].dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE;

		RD::PipelineDepthStencilState ds;
		ds.enable_depth_test = true;
		ds.enable_depth_write = true;
		ds.depth_compare_operator = RD::COMPARE_OP_LESS; //so it does not render same pixel twice, this avoids wrong blending

		RID blendseams_line_raster_pipeline = rd->render_pipeline_create(blendseams_line_raster_shader, rd->framebuffer_get_format(framebuffers[0]), RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_LINES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), ds, bs, 0);
		RID blendseams_triangle_raster_pipeline = rd->render_pipeline_create(blendseams_triangle_raster_shader, rd->framebuffer_get_format(framebuffers[0]), RD::INVALID_FORMAT_ID, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), ds, bs, 0);

		uint32_t seam_offset = 0;
		uint32_t triangle_offset = 0;

		for (int i = 0; i < atlas_slices; i++) {
			int subslices = (p_bake_sh ? 4 : 1);

			if (slice_seam_count[i] == 0) {
				continue;
			}

			for (int k = 0; k < subslices; k++) {
				RasterSeamsPushConstant seams_push_constant;
				seams_push_constant.slice = uint32_t(i * subslices + k);
				seams_push_constant.debug = debug;

				// Store the current subslice in the breadcrumb.
				RD::DrawListID draw_list = rd->draw_list_begin(framebuffers[i * subslices + k], RD::DRAW_CLEAR_DEPTH, Vector<Color>(), 1.0f, 0, Rect2(), RDD::BreadcrumbMarker::LIGHTMAPPER_PASS | seams_push_constant.slice);

				rd->draw_list_bind_uniform_set(draw_list, raster_base_uniform, 0);
				rd->draw_list_bind_uniform_set(draw_list, blendseams_raster_uniform, 1);

				const int uv_offset_count = 9;
				static const Vector3 uv_offsets[uv_offset_count] = {
					Vector3(0, 0, 0.5), //using zbuffer, so go inwards-outwards
					Vector3(0, 1, 0.2),
					Vector3(0, -1, 0.2),
					Vector3(1, 0, 0.2),
					Vector3(-1, 0, 0.2),
					Vector3(-1, -1, 0.1),
					Vector3(1, -1, 0.1),
					Vector3(1, 1, 0.1),
					Vector3(-1, 1, 0.1),
				};

				/* step 1 use lines to blend the edges */
				{
					seams_push_constant.base_index = seam_offset;
					rd->draw_list_bind_render_pipeline(draw_list, blendseams_line_raster_pipeline);
					seams_push_constant.uv_offset[0] = (uv_offsets[0].x - 0.5f) / float(atlas_size.width);
					seams_push_constant.uv_offset[1] = (uv_offsets[0].y - 0.5f) / float(atlas_size.height);
					seams_push_constant.blend = uv_offsets[0].z;

					rd->draw_list_set_push_constant(draw_list, &seams_push_constant, sizeof(RasterSeamsPushConstant));
					rd->draw_list_draw(draw_list, false, 1, slice_seam_count[i] * 4);
				}

				/* step 2 use triangles to mask the interior */

				{
					seams_push_constant.base_index = triangle_offset;
					rd->draw_list_bind_render_pipeline(draw_list, blendseams_triangle_raster_pipeline);
					seams_push_constant.blend = 0; //do not draw them, just fill the z-buffer so its used as a mask

					rd->draw_list_set_push_constant(draw_list, &seams_push_constant, sizeof(RasterSeamsPushConstant));
					rd->draw_list_draw(draw_list, false, 1, slice_triangle_count[i] * 3);
				}
				/* step 3 blend around the triangle */

				rd->draw_list_bind_render_pipeline(draw_list, blendseams_line_raster_pipeline);

				for (int j = 1; j < uv_offset_count; j++) {
					seams_push_constant.base_index = seam_offset;
					seams_push_constant.uv_offset[0] = (uv_offsets[j].x - 0.5f) / float(atlas_size.width);
					seams_push_constant.uv_offset[1] = (uv_offsets[j].y - 0.5f) / float(atlas_size.height);
					seams_push_constant.blend = uv_offsets[0].z;

					rd->draw_list_set_push_constant(draw_list, &seams_push_constant, sizeof(RasterSeamsPushConstant));
					rd->draw_list_draw(draw_list, false, 1, slice_seam_count[i] * 4);
				}
				rd->draw_list_end();
			}
			seam_offset += slice_seam_count[i];
			triangle_offset += slice_triangle_count[i];
		}
	}

	if (p_bake_sh) {
		SWAP(light_accum_tex, light_accum_tex2);
		BakeError error = _pack_l1(rd, compute_shader, compute_base_uniform_set, push_constant, light_accum_tex2, light_accum_tex, atlas_size, atlas_slices);
		if (unlikely(error != BAKE_OK)) {
			return error;
		}
	}

#ifdef DEBUG_TEXTURES

	for (int i = 0; i < atlas_slices * (p_bake_sh ? 4 : 1); i++) {
		Vector<uint8_t> s = rd->texture_get_data(light_accum_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->save_exr("res://5_blendseams" + itos(i) + ".exr", false);
	}
#endif

	if (p_step_function) {
		p_step_function(0.9, RTR("Retrieving textures"), p_bake_userdata, true);
	}

	for (int i = 0; i < atlas_slices * (p_bake_sh ? 4 : 1); i++) {
		Vector<uint8_t> s = rd->texture_get_data(light_accum_tex, i);
		Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBAH, s);
		img->convert(Image::FORMAT_RGBH); //remove alpha
		lightmap_textures.push_back(img);
	}

	if (p_bake_shadowmask) {
		for (int i = 0; i < atlas_slices; i++) {
			Vector<uint8_t> s = rd->texture_get_data(shadowmask_tex, i);
			Ref<Image> img = Image::create_from_data(atlas_size.width, atlas_size.height, false, Image::FORMAT_RGBA8, s);
			img->convert(Image::FORMAT_R8);
			shadowmask_textures.push_back(img);
		}
	}

	if (probe_positions.size() > 0) {
		probe_values.resize(probe_positions.size() * 9);
		Vector<uint8_t> probe_data = rd->buffer_get_data(light_probe_buffer);
		memcpy(probe_values.ptrw(), probe_data.ptr(), probe_data.size());
		rd->free(light_probe_buffer);

#ifdef DEBUG_TEXTURES
		{
			Ref<Image> img2 = Image::create_from_data(probe_values.size(), 1, false, Image::FORMAT_RGBAF, probe_data);
			img2->save_exr("res://6_lightprobes.exr", false);
		}
#endif
	}

	FREE_TEXTURES
	FREE_BUFFERS
	FREE_RASTER_RESOURCES
	FREE_COMPUTE_RESOURCES
	FREE_BLENDSEAMS_RESOURCES

	memdelete(rd);

	if (rcd != nullptr) {
		memdelete(rcd);
	}

	return BAKE_OK;
}

int LightmapperRD::get_bake_texture_count() const {
	return lightmap_textures.size();
}

Ref<Image> LightmapperRD::get_bake_texture(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, lightmap_textures.size(), Ref<Image>());
	return lightmap_textures[p_index];
}

int LightmapperRD::get_shadowmask_texture_count() const {
	return shadowmask_textures.size();
}

Ref<Image> LightmapperRD::get_shadowmask_texture(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, shadowmask_textures.size(), Ref<Image>());
	return shadowmask_textures[p_index];
}

int LightmapperRD::get_bake_mesh_count() const {
	return mesh_instances.size();
}

Variant LightmapperRD::get_bake_mesh_userdata(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, mesh_instances.size(), Variant());
	return mesh_instances[p_index].data.userdata;
}

Rect2 LightmapperRD::get_bake_mesh_uv_scale(int p_index) const {
	ERR_FAIL_COND_V(lightmap_textures.is_empty(), Rect2());
	Rect2 uv_ofs;
	Vector2 atlas_size = Vector2(lightmap_textures[0]->get_width(), lightmap_textures[0]->get_height());
	uv_ofs.position = Vector2(mesh_instances[p_index].offset) / atlas_size;
	uv_ofs.size = Vector2(mesh_instances[p_index].data.albedo_on_uv2->get_width(), mesh_instances[p_index].data.albedo_on_uv2->get_height()) / atlas_size;
	return uv_ofs;
}

int LightmapperRD::get_bake_mesh_texture_slice(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, mesh_instances.size(), Variant());
	return mesh_instances[p_index].slice;
}

int LightmapperRD::get_bake_probe_count() const {
	return probe_positions.size();
}

Vector3 LightmapperRD::get_bake_probe_point(int p_probe) const {
	ERR_FAIL_INDEX_V(p_probe, probe_positions.size(), Variant());
	return Vector3(probe_positions[p_probe].position[0], probe_positions[p_probe].position[1], probe_positions[p_probe].position[2]);
}

Vector<Color> LightmapperRD::get_bake_probe_sh(int p_probe) const {
	ERR_FAIL_INDEX_V(p_probe, probe_positions.size(), Vector<Color>());
	Vector<Color> ret;
	ret.resize(9);
	memcpy(ret.ptrw(), &probe_values[p_probe * 9], sizeof(Color) * 9);
	return ret;
}

LightmapperRD::LightmapperRD() {
}
