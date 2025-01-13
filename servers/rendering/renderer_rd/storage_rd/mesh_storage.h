/**************************************************************************/
/*  mesh_storage.h                                                        */
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

#ifndef MESH_STORAGE_RD_H
#define MESH_STORAGE_RD_H

#include "../../rendering_server_globals.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/renderer_rd/shaders/skeleton.glsl.gen.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "servers/rendering/storage/utilities.h"

namespace RendererRD {

class MeshStorage : public RendererMeshStorage {
public:
	enum DefaultRDBuffer {
		DEFAULT_RD_BUFFER_VERTEX,
		DEFAULT_RD_BUFFER_NORMAL,
		DEFAULT_RD_BUFFER_TANGENT,
		DEFAULT_RD_BUFFER_COLOR,
		DEFAULT_RD_BUFFER_TEX_UV,
		DEFAULT_RD_BUFFER_TEX_UV2,
		DEFAULT_RD_BUFFER_CUSTOM0,
		DEFAULT_RD_BUFFER_CUSTOM1,
		DEFAULT_RD_BUFFER_CUSTOM2,
		DEFAULT_RD_BUFFER_CUSTOM3,
		DEFAULT_RD_BUFFER_BONES,
		DEFAULT_RD_BUFFER_WEIGHTS,
		DEFAULT_RD_BUFFER_MAX,
	};

private:
	static MeshStorage *singleton;

	RID default_rd_storage_buffer;

	/* Mesh */

	RID mesh_default_rd_buffers[DEFAULT_RD_BUFFER_MAX];

	struct MeshInstance;

	struct Mesh {
		struct Surface {
			RS::PrimitiveType primitive = RS::PRIMITIVE_POINTS;
			uint64_t format = 0;

			RID vertex_buffer;
			RID attribute_buffer;
			RID skin_buffer;
			uint32_t vertex_count = 0;
			uint32_t vertex_buffer_size = 0;
			uint32_t skin_buffer_size = 0;

			// A different pipeline needs to be allocated
			// depending on the inputs available in the
			// material.
			// There are never that many geometry/material
			// combinations, so a simple array is the most
			// cache-efficient structure.

			struct Version {
				uint64_t input_mask = 0;
				uint32_t current_buffer = 0;
				uint32_t previous_buffer = 0;
				bool input_motion_vectors = false;
				RD::VertexFormatID vertex_format = 0;
				RID vertex_array;
			};

			SpinLock version_lock; //needed to access versions
			Version *versions = nullptr; //allocated on demand
			uint32_t version_count = 0;

			RID index_buffer;
			RID index_array;
			uint32_t index_count = 0;

			struct LOD {
				float edge_length = 0.0;
				uint32_t index_count = 0;
				RID index_buffer;
				RID index_array;
			};

			LOD *lods = nullptr;
			uint32_t lod_count = 0;

			AABB aabb;

			Vector<AABB> bone_aabbs;

			// Transform used in runtime bone AABBs compute.
			// As bone AABBs are saved in Mesh space, but bones animation is in Skeleton space.
			Transform3D mesh_to_skeleton_xform;

			Vector4 uv_scale;

			RID blend_shape_buffer;

			RID material;

			uint32_t render_index = 0;
			uint64_t render_pass = 0;

			uint32_t multimesh_render_index = 0;
			uint64_t multimesh_render_pass = 0;

			uint32_t particles_render_index = 0;
			uint64_t particles_render_pass = 0;

			RID uniform_set;
		};

		uint32_t blend_shape_count = 0;
		RS::BlendShapeMode blend_shape_mode = RS::BLEND_SHAPE_MODE_NORMALIZED;

		Surface **surfaces = nullptr;
		uint32_t surface_count = 0;

		bool has_bone_weights = false;

		AABB aabb;
		AABB custom_aabb;
		uint64_t skeleton_aabb_version = 0;
		RID skeleton_aabb_rid;

		Vector<RID> material_cache;

		List<MeshInstance *> instances;

		RID shadow_mesh;
		HashSet<Mesh *> shadow_owners;

		String path;

		Dependency dependency;
	};

	mutable RID_Owner<Mesh, true> mesh_owner;

	/* Mesh Instance API */

	struct MeshInstance {
		Mesh *mesh = nullptr;
		RID skeleton;
		struct Surface {
			RID vertex_buffer[2];
			RID uniform_set[2];
			uint32_t current_buffer = 0;
			uint32_t previous_buffer = 0;
			uint64_t last_change = 0;

			Mesh::Surface::Version *versions = nullptr; //allocated on demand
			uint32_t version_count = 0;
		};
		LocalVector<Surface> surfaces;
		LocalVector<float> blend_weights;

		RID blend_weights_buffer;
		List<MeshInstance *>::Element *I = nullptr; //used to erase itself
		uint64_t skeleton_version = 0;
		bool dirty = false;
		bool weights_dirty = false;
		SelfList<MeshInstance> weight_update_list;
		SelfList<MeshInstance> array_update_list;
		Transform2D canvas_item_transform_2d;
		MeshInstance() :
				weight_update_list(this), array_update_list(this) {}
	};

	RD::VertexFormatID _mesh_surface_generate_vertex_format(uint64_t p_surface_format, uint64_t p_input_mask, bool p_instanced_surface, bool p_input_motion_vectors, uint32_t &r_position_stride);
	void _mesh_surface_generate_version_for_input_mask(Mesh::Surface::Version &v, Mesh::Surface *s, uint64_t p_input_mask, bool p_input_motion_vectors, MeshInstance::Surface *mis = nullptr, uint32_t p_current_buffer = 0, uint32_t p_previous_buffer = 0);

	void _mesh_instance_clear(MeshInstance *mi);
	void _mesh_instance_add_surface(MeshInstance *mi, Mesh *mesh, uint32_t p_surface);
	void _mesh_instance_add_surface_buffer(MeshInstance *mi, Mesh *mesh, MeshInstance::Surface *s, uint32_t p_surface, uint32_t p_buffer_index);

	mutable RID_Owner<MeshInstance> mesh_instance_owner;

	SelfList<MeshInstance>::List dirty_mesh_instance_weights;
	SelfList<MeshInstance>::List dirty_mesh_instance_arrays;

	/* MultiMesh */

	struct MultiMesh {
		RID mesh;
		int instances = 0;
		RS::MultimeshTransformFormat xform_format = RS::MULTIMESH_TRANSFORM_3D;
		bool uses_colors = false;
		bool uses_custom_data = false;
		int visible_instances = -1;
		AABB aabb;
		AABB custom_aabb;
		bool aabb_dirty = false;
		bool buffer_set = false;
		bool motion_vectors_enabled = false;
		uint32_t motion_vectors_current_offset = 0;
		uint32_t motion_vectors_previous_offset = 0;
		uint64_t motion_vectors_last_change = -1;
		uint32_t stride_cache = 0;
		uint32_t color_offset_cache = 0;
		uint32_t custom_data_offset_cache = 0;

		Vector<float> data_cache; //used if individual setting is used
		bool *data_cache_dirty_regions = nullptr;
		uint32_t data_cache_dirty_region_count = 0;
		bool *previous_data_cache_dirty_regions = nullptr;
		uint32_t previous_data_cache_dirty_region_count = 0;

		RID buffer; //storage buffer
		RID uniform_set_3d;
		RID uniform_set_2d;

		bool dirty = false;
		MultiMesh *dirty_list = nullptr;

		RendererMeshStorage::MultiMeshInterpolator interpolator;

		Dependency dependency;
	};

	mutable RID_Owner<MultiMesh, true> multimesh_owner;

	MultiMesh *multimesh_dirty_list = nullptr;

	_FORCE_INLINE_ void _multimesh_make_local(MultiMesh *multimesh) const;
	_FORCE_INLINE_ void _multimesh_enable_motion_vectors(MultiMesh *multimesh);
	_FORCE_INLINE_ void _multimesh_update_motion_vectors_data_cache(MultiMesh *multimesh);
	_FORCE_INLINE_ bool _multimesh_uses_motion_vectors(MultiMesh *multimesh);
	_FORCE_INLINE_ void _multimesh_mark_dirty(MultiMesh *multimesh, int p_index, bool p_aabb);
	_FORCE_INLINE_ void _multimesh_mark_all_dirty(MultiMesh *multimesh, bool p_data, bool p_aabb);
	_FORCE_INLINE_ void _multimesh_re_create_aabb(MultiMesh *multimesh, const float *p_data, int p_instances);

	/* Skeleton */

	struct SkeletonShader {
		struct PushConstant {
			uint32_t has_normal;
			uint32_t has_tangent;
			uint32_t has_skeleton;
			uint32_t has_blend_shape;

			uint32_t vertex_count;
			uint32_t vertex_stride;
			uint32_t skin_stride;
			uint32_t skin_weight_offset;

			uint32_t blend_shape_count;
			uint32_t normalized_blend_shapes;
			uint32_t normal_tangent_stride;
			uint32_t pad1;
			float skeleton_transform_x[2];
			float skeleton_transform_y[2];

			float skeleton_transform_offset[2];
			float inverse_transform_x[2];

			float inverse_transform_y[2];
			float inverse_transform_offset[2];
		};

		enum {
			UNIFORM_SET_INSTANCE = 0,
			UNIFORM_SET_SURFACE = 1,
			UNIFORM_SET_SKELETON = 2,
		};
		enum {
			SHADER_MODE_2D,
			SHADER_MODE_3D,
			SHADER_MODE_MAX
		};

		SkeletonShaderRD shader;
		RID version;
		RID version_shader[SHADER_MODE_MAX];
		RID pipeline[SHADER_MODE_MAX];

		RID default_skeleton_uniform_set;
	} skeleton_shader;

	struct Skeleton {
		bool use_2d = false;
		int size = 0;
		LocalVector<float> data;
		RID buffer;

		bool dirty = false;
		Skeleton *dirty_list = nullptr;
		Transform2D base_transform_2d;

		RID uniform_set_3d;
		RID uniform_set_mi;

		uint64_t version = 1;

		Dependency dependency;
	};

	mutable RID_Owner<Skeleton, true> skeleton_owner;

	_FORCE_INLINE_ void _skeleton_make_dirty(Skeleton *skeleton);

	Skeleton *skeleton_dirty_list = nullptr;

	enum AttributeLocation {
		ATTRIBUTE_LOCATION_PREV_VERTEX = 12,
		ATTRIBUTE_LOCATION_PREV_NORMAL = 13,
		ATTRIBUTE_LOCATION_PREV_TANGENT = 14
	};

public:
	static MeshStorage *get_singleton();

	MeshStorage();
	virtual ~MeshStorage();

	bool free(RID p_rid);

	RID get_default_rd_storage_buffer() const { return default_rd_storage_buffer; }

	/* MESH API */

	bool owns_mesh(RID p_rid) { return mesh_owner.owns(p_rid); }

	virtual RID mesh_allocate() override;
	virtual void mesh_initialize(RID p_mesh) override;
	virtual void mesh_free(RID p_rid) override;

	virtual void mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) override;

	/// Return stride
	virtual void mesh_add_surface(RID p_mesh, const RS::SurfaceData &p_surface) override;

	virtual int mesh_get_blend_shape_count(RID p_mesh) const override;

	virtual void mesh_set_blend_shape_mode(RID p_mesh, RS::BlendShapeMode p_mode) override;
	virtual RS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const override;

	virtual void mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override;
	virtual void mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override;
	virtual void mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override;

	virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) override;
	virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const override;

	virtual RS::SurfaceData mesh_get_surface(RID p_mesh, int p_surface) const override;

	virtual int mesh_get_surface_count(RID p_mesh) const override;

	virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) override;
	virtual AABB mesh_get_custom_aabb(RID p_mesh) const override;

	virtual AABB mesh_get_aabb(RID p_mesh, RID p_skeleton = RID()) override;
	virtual void mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) override;

	virtual void mesh_set_path(RID p_mesh, const String &p_path) override;
	virtual String mesh_get_path(RID p_mesh) const override;

	virtual void mesh_clear(RID p_mesh) override;

	virtual bool mesh_needs_instance(RID p_mesh, bool p_has_skeleton) override;

	_FORCE_INLINE_ const RID *mesh_get_surface_count_and_materials(RID p_mesh, uint32_t &r_surface_count) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		ERR_FAIL_NULL_V(mesh, nullptr);
		r_surface_count = mesh->surface_count;
		if (r_surface_count == 0) {
			return nullptr;
		}
		if (mesh->material_cache.is_empty()) {
			mesh->material_cache.resize(mesh->surface_count);
			for (uint32_t i = 0; i < r_surface_count; i++) {
				mesh->material_cache.write[i] = mesh->surfaces[i]->material;
			}
		}

		return mesh->material_cache.ptr();
	}

	_FORCE_INLINE_ void *mesh_get_surface(RID p_mesh, uint32_t p_surface_index) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		ERR_FAIL_NULL_V(mesh, nullptr);
		ERR_FAIL_UNSIGNED_INDEX_V(p_surface_index, mesh->surface_count, nullptr);

		return mesh->surfaces[p_surface_index];
	}

	_FORCE_INLINE_ RID mesh_get_shadow_mesh(RID p_mesh) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		ERR_FAIL_NULL_V(mesh, RID());

		return mesh->shadow_mesh;
	}

	_FORCE_INLINE_ RS::PrimitiveType mesh_surface_get_primitive(void *p_surface) {
		Mesh::Surface *surface = reinterpret_cast<Mesh::Surface *>(p_surface);
		return surface->primitive;
	}

	_FORCE_INLINE_ bool mesh_surface_has_lod(void *p_surface) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		return s->lod_count > 0;
	}

	_FORCE_INLINE_ uint32_t mesh_surface_get_vertices_drawn_count(void *p_surface) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		return s->index_count ? s->index_count : s->vertex_count;
	}

	_FORCE_INLINE_ AABB mesh_surface_get_aabb(void *p_surface) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		return s->aabb;
	}

	_FORCE_INLINE_ uint64_t mesh_surface_get_format(void *p_surface) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		return s->format;
	}

	_FORCE_INLINE_ Vector4 mesh_surface_get_uv_scale(void *p_surface) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		return s->uv_scale;
	}

	_FORCE_INLINE_ uint32_t mesh_surface_get_lod(void *p_surface, float p_model_scale, float p_distance_threshold, float p_mesh_lod_threshold, uint32_t &r_index_count) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		int32_t current_lod = -1;
		r_index_count = s->index_count;
		for (uint32_t i = 0; i < s->lod_count; i++) {
			float screen_size = s->lods[i].edge_length * p_model_scale / p_distance_threshold;
			if (screen_size > p_mesh_lod_threshold) {
				break;
			}
			current_lod = i;
		}
		if (current_lod == -1) {
			return 0;
		} else {
			r_index_count = s->lods[current_lod].index_count;
			return current_lod + 1;
		}
	}

	_FORCE_INLINE_ RID mesh_surface_get_index_array(void *p_surface, uint32_t p_lod) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		if (p_lod == 0) {
			return s->index_array;
		} else {
			return s->lods[p_lod - 1].index_array;
		}
	}

	_FORCE_INLINE_ void mesh_surface_get_vertex_arrays_and_format(void *p_surface, uint64_t p_input_mask, bool p_input_motion_vectors, RID &r_vertex_array_rd, RD::VertexFormatID &r_vertex_format) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		s->version_lock.lock();

		//there will never be more than, at much, 3 or 4 versions, so iterating is the fastest way

		for (uint32_t i = 0; i < s->version_count; i++) {
			if (s->versions[i].input_mask != p_input_mask || s->versions[i].input_motion_vectors != p_input_motion_vectors) {
				// Find the version that matches the inputs required.
				continue;
			}

			//we have this version, hooray
			r_vertex_format = s->versions[i].vertex_format;
			r_vertex_array_rd = s->versions[i].vertex_array;
			s->version_lock.unlock();
			return;
		}

		uint32_t version = s->version_count;
		s->version_count++;
		s->versions = (Mesh::Surface::Version *)memrealloc(s->versions, sizeof(Mesh::Surface::Version) * s->version_count);

		_mesh_surface_generate_version_for_input_mask(s->versions[version], s, p_input_mask, p_input_motion_vectors);

		r_vertex_format = s->versions[version].vertex_format;
		r_vertex_array_rd = s->versions[version].vertex_array;

		s->version_lock.unlock();
	}

	_FORCE_INLINE_ void mesh_instance_surface_get_vertex_arrays_and_format(RID p_mesh_instance, uint64_t p_surface_index, uint64_t p_input_mask, bool p_input_motion_vectors, RID &r_vertex_array_rd, RD::VertexFormatID &r_vertex_format) {
		MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
		ERR_FAIL_NULL(mi);
		Mesh *mesh = mi->mesh;
		ERR_FAIL_UNSIGNED_INDEX(p_surface_index, mesh->surface_count);

		MeshInstance::Surface *mis = &mi->surfaces[p_surface_index];
		Mesh::Surface *s = mesh->surfaces[p_surface_index];
		uint32_t current_buffer = mis->current_buffer;

		// Using the previous buffer is only allowed if the surface was updated this frame and motion vectors are required.
		uint32_t previous_buffer = p_input_motion_vectors && (RSG::rasterizer->get_frame_number() == mis->last_change) ? mis->previous_buffer : current_buffer;

		s->version_lock.lock();

		//there will never be more than, at much, 3 or 4 versions, so iterating is the fastest way

		for (uint32_t i = 0; i < mis->version_count; i++) {
			if (mis->versions[i].input_mask != p_input_mask || mis->versions[i].input_motion_vectors != p_input_motion_vectors) {
				// Find the version that matches the inputs required.
				continue;
			}

			if (mis->versions[i].current_buffer != current_buffer || mis->versions[i].previous_buffer != previous_buffer) {
				// Find the version that corresponds to the correct buffers that should be used.
				continue;
			}

			//we have this version, hooray
			r_vertex_format = mis->versions[i].vertex_format;
			r_vertex_array_rd = mis->versions[i].vertex_array;
			s->version_lock.unlock();
			return;
		}

		uint32_t version = mis->version_count;
		mis->version_count++;
		mis->versions = (Mesh::Surface::Version *)memrealloc(mis->versions, sizeof(Mesh::Surface::Version) * mis->version_count);

		_mesh_surface_generate_version_for_input_mask(mis->versions[version], s, p_input_mask, p_input_motion_vectors, mis, current_buffer, previous_buffer);

		r_vertex_format = mis->versions[version].vertex_format;
		r_vertex_array_rd = mis->versions[version].vertex_array;

		s->version_lock.unlock();
	}

	_FORCE_INLINE_ RID mesh_get_default_rd_buffer(DefaultRDBuffer p_buffer) {
		ERR_FAIL_INDEX_V(p_buffer, DEFAULT_RD_BUFFER_MAX, RID());
		return mesh_default_rd_buffers[p_buffer];
	}

	_FORCE_INLINE_ uint32_t mesh_surface_get_render_pass_index(RID p_mesh, uint32_t p_surface_index, uint64_t p_render_pass, uint32_t *r_index) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		Mesh::Surface *s = mesh->surfaces[p_surface_index];

		if (s->render_pass != p_render_pass) {
			(*r_index)++;
			s->render_pass = p_render_pass;
			s->render_index = *r_index;
		}

		return s->render_index;
	}

	_FORCE_INLINE_ uint32_t mesh_surface_get_multimesh_render_pass_index(RID p_mesh, uint32_t p_surface_index, uint64_t p_render_pass, uint32_t *r_index) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		Mesh::Surface *s = mesh->surfaces[p_surface_index];

		if (s->multimesh_render_pass != p_render_pass) {
			(*r_index)++;
			s->multimesh_render_pass = p_render_pass;
			s->multimesh_render_index = *r_index;
		}

		return s->multimesh_render_index;
	}

	_FORCE_INLINE_ uint32_t mesh_surface_get_particles_render_pass_index(RID p_mesh, uint32_t p_surface_index, uint64_t p_render_pass, uint32_t *r_index) {
		Mesh *mesh = mesh_owner.get_or_null(p_mesh);
		Mesh::Surface *s = mesh->surfaces[p_surface_index];

		if (s->particles_render_pass != p_render_pass) {
			(*r_index)++;
			s->particles_render_pass = p_render_pass;
			s->particles_render_index = *r_index;
		}

		return s->particles_render_index;
	}

	_FORCE_INLINE_ RD::VertexFormatID mesh_surface_get_vertex_format(void *p_surface, uint64_t p_input_mask, bool p_instanced_surface, bool p_input_motion_vectors) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		uint32_t position_stride = 0;
		return _mesh_surface_generate_vertex_format(s->format, p_input_mask, p_instanced_surface, p_input_motion_vectors, position_stride);
	}

	Dependency *mesh_get_dependency(RID p_mesh) const;

	/* MESH INSTANCE API */

	bool owns_mesh_instance(RID p_rid) const { return mesh_instance_owner.owns(p_rid); }

	virtual RID mesh_instance_create(RID p_base) override;
	virtual void mesh_instance_free(RID p_rid) override;
	virtual void mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) override;
	virtual void mesh_instance_set_blend_shape_weight(RID p_mesh_instance, int p_shape, float p_weight) override;
	virtual void mesh_instance_check_for_update(RID p_mesh_instance) override;
	virtual void mesh_instance_set_canvas_item_transform(RID p_mesh_instance, const Transform2D &p_transform) override;
	virtual void update_mesh_instances() override;

	/* MULTIMESH API */

	bool owns_multimesh(RID p_rid) { return multimesh_owner.owns(p_rid); }

	virtual RID _multimesh_allocate() override;
	virtual void _multimesh_initialize(RID p_multimesh) override;
	virtual void _multimesh_free(RID p_rid) override;

	virtual void _multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors = false, bool p_use_custom_data = false) override;
	virtual int _multimesh_get_instance_count(RID p_multimesh) const override;

	virtual void _multimesh_set_mesh(RID p_multimesh, RID p_mesh) override;
	virtual void _multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) override;
	virtual void _multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override;
	virtual void _multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override;
	virtual void _multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) override;

	virtual RID _multimesh_get_mesh(RID p_multimesh) const override;

	virtual Transform3D _multimesh_instance_get_transform(RID p_multimesh, int p_index) const override;
	virtual Transform2D _multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override;
	virtual Color _multimesh_instance_get_color(RID p_multimesh, int p_index) const override;
	virtual Color _multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override;

	virtual void _multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) override;
	virtual RID _multimesh_get_buffer_rd_rid(RID p_multimesh) const override;
	virtual Vector<float> _multimesh_get_buffer(RID p_multimesh) const override;

	virtual void _multimesh_set_visible_instances(RID p_multimesh, int p_visible) override;
	virtual int _multimesh_get_visible_instances(RID p_multimesh) const override;

	virtual void _multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) override;
	virtual AABB _multimesh_get_custom_aabb(RID p_multimesh) const override;

	virtual AABB _multimesh_get_aabb(RID p_multimesh) override;

	virtual MultiMeshInterpolator *_multimesh_get_interpolator(RID p_multimesh) const override;

	void _update_dirty_multimeshes();
	void _multimesh_get_motion_vectors_offsets(RID p_multimesh, uint32_t &r_current_offset, uint32_t &r_prev_offset);
	bool _multimesh_uses_motion_vectors_offsets(RID p_multimesh);
	bool _multimesh_uses_motion_vectors(RID p_multimesh);

	_FORCE_INLINE_ RS::MultimeshTransformFormat multimesh_get_transform_format(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->xform_format;
	}

	_FORCE_INLINE_ bool multimesh_uses_colors(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->uses_colors;
	}

	_FORCE_INLINE_ bool multimesh_uses_custom_data(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->uses_custom_data;
	}

	_FORCE_INLINE_ uint32_t multimesh_get_instances_to_draw(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		if (multimesh->visible_instances >= 0) {
			return multimesh->visible_instances;
		}
		return multimesh->instances;
	}

	_FORCE_INLINE_ RID multimesh_get_3d_uniform_set(RID p_multimesh, RID p_shader, uint32_t p_set) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		if (multimesh == nullptr) {
			return RID();
		}
		if (!multimesh->uniform_set_3d.is_valid()) {
			if (!multimesh->buffer.is_valid()) {
				return RID();
			}
			Vector<RD::Uniform> uniforms;
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(multimesh->buffer);
			uniforms.push_back(u);
			multimesh->uniform_set_3d = RD::get_singleton()->uniform_set_create(uniforms, p_shader, p_set);
		}

		return multimesh->uniform_set_3d;
	}

	_FORCE_INLINE_ RID multimesh_get_2d_uniform_set(RID p_multimesh, RID p_shader, uint32_t p_set) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		if (multimesh == nullptr) {
			return RID();
		}
		if (!multimesh->uniform_set_2d.is_valid()) {
			if (!multimesh->buffer.is_valid()) {
				return RID();
			}
			Vector<RD::Uniform> uniforms;
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(multimesh->buffer);
			uniforms.push_back(u);
			multimesh->uniform_set_2d = RD::get_singleton()->uniform_set_create(uniforms, p_shader, p_set);
		}

		return multimesh->uniform_set_2d;
	}

	Dependency *multimesh_get_dependency(RID p_multimesh) const;

	/* SKELETON API */

	bool owns_skeleton(RID p_rid) const { return skeleton_owner.owns(p_rid); }

	virtual RID skeleton_allocate() override;
	virtual void skeleton_initialize(RID p_skeleton) override;
	virtual void skeleton_free(RID p_rid) override;

	virtual void skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) override;
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) override;
	virtual int skeleton_get_bone_count(RID p_skeleton) const override;
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) override;
	virtual Transform3D skeleton_bone_get_transform(RID p_skeleton, int p_bone) const override;
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) override;
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const override;

	virtual void skeleton_update_dependency(RID p_skeleton, DependencyTracker *p_instance) override;

	void _update_dirty_skeletons();

	_FORCE_INLINE_ bool skeleton_is_valid(RID p_skeleton) {
		return skeleton_owner.get_or_null(p_skeleton) != nullptr;
	}

	_FORCE_INLINE_ RID skeleton_get_3d_uniform_set(RID p_skeleton, RID p_shader, uint32_t p_set) const {
		Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
		ERR_FAIL_NULL_V(skeleton, RID());
		if (skeleton->size == 0) {
			return RID();
		}
		if (skeleton->use_2d) {
			return RID();
		}
		if (!skeleton->uniform_set_3d.is_valid()) {
			Vector<RD::Uniform> uniforms;
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(skeleton->buffer);
			uniforms.push_back(u);
			skeleton->uniform_set_3d = RD::get_singleton()->uniform_set_create(uniforms, p_shader, p_set);
		}

		return skeleton->uniform_set_3d;
	}
};

} // namespace RendererRD

#endif // MESH_STORAGE_RD_H
