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

#ifndef MESH_STORAGE_GLES3_H
#define MESH_STORAGE_GLES3_H

#ifdef GLES3_ENABLED

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "drivers/gles3/shaders/skeleton.glsl.gen.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "servers/rendering/storage/utilities.h"

#include "platform_gl.h"

namespace GLES3 {

struct MeshInstance;

struct Mesh {
	struct Surface {
		struct Attrib {
			bool enabled;
			bool integer;
			GLint size;
			GLenum type;
			GLboolean normalized;
			GLsizei stride;
			uint32_t offset;
		};
		RS::PrimitiveType primitive = RS::PRIMITIVE_POINTS;
		uint64_t format = 0;

		GLuint vertex_buffer = 0;
		GLuint attribute_buffer = 0;
		GLuint skin_buffer = 0;
		uint32_t vertex_count = 0;
		uint32_t vertex_buffer_size = 0;
		uint32_t attribute_buffer_size = 0;
		uint32_t skin_buffer_size = 0;

		// Cache vertex arrays so they can be created
		struct Version {
			uint32_t input_mask = 0;
			GLuint vertex_array = 0;

			Attrib attribs[RS::ARRAY_MAX];
		};

		SpinLock version_lock; //needed to access versions
		Version *versions = nullptr; //allocated on demand
		uint32_t version_count = 0;

		GLuint index_buffer = 0;
		uint32_t index_count = 0;
		uint32_t index_buffer_size = 0;

		struct Wireframe {
			GLuint index_buffer = 0;
			uint32_t index_count = 0;
			uint32_t index_buffer_size = 0;
		};

		Wireframe *wireframe = nullptr;

		struct LOD {
			float edge_length = 0.0;
			uint32_t index_count = 0;
			uint32_t index_buffer_size = 0;
			GLuint index_buffer = 0;
		};

		LOD *lods = nullptr;
		uint32_t lod_count = 0;

		AABB aabb;

		Vector<AABB> bone_aabbs;

		// Transform used in runtime bone AABBs compute.
		// As bone AABBs are saved in Mesh space, but bones animation is in Skeleton space.
		Transform3D mesh_to_skeleton_xform;

		Vector4 uv_scale;

		struct BlendShape {
			GLuint vertex_buffer = 0;
			GLuint vertex_array = 0;
		};

		BlendShape *blend_shapes = nullptr;
		GLuint skeleton_vertex_array = 0;

		RID material;
	};

	uint32_t blend_shape_count = 0;
	RS::BlendShapeMode blend_shape_mode = RS::BLEND_SHAPE_MODE_NORMALIZED;

	Surface **surfaces = nullptr;
	uint32_t surface_count = 0;

	bool has_bone_weights = false;

	AABB aabb;
	AABB custom_aabb;
	uint64_t skeleton_aabb_version = 0;

	Vector<RID> material_cache;

	List<MeshInstance *> instances;

	RID shadow_mesh;
	HashSet<Mesh *> shadow_owners;

	String path;

	Dependency dependency;
};

/* Mesh Instance */

struct MeshInstance {
	Mesh *mesh = nullptr;
	RID skeleton;
	struct Surface {
		GLuint vertex_buffers[2] = { 0, 0 };
		GLuint vertex_arrays[2] = { 0, 0 };
		GLuint vertex_buffer = 0;
		int vertex_stride_cache = 0;
		int vertex_size_cache = 0;
		int vertex_normal_offset_cache = 0;
		int vertex_tangent_offset_cache = 0;
		uint64_t format_cache = 0;

		Mesh::Surface::Version *versions = nullptr; //allocated on demand
		uint32_t version_count = 0;
	};
	LocalVector<Surface> surfaces;
	LocalVector<float> blend_weights;

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
	uint32_t stride_cache = 0;
	uint32_t color_offset_cache = 0;
	uint32_t custom_data_offset_cache = 0;

	Vector<float> data_cache; //used if individual setting is used
	bool *data_cache_dirty_regions = nullptr;
	uint32_t data_cache_used_dirty_regions = 0;

	GLuint buffer = 0;

	bool dirty = false;
	MultiMesh *dirty_list = nullptr;

	RendererMeshStorage::MultiMeshInterpolator interpolator;

	Dependency dependency;
};

struct Skeleton {
	bool use_2d = false;
	int size = 0;
	int height = 0;
	LocalVector<float> data;

	bool dirty = false;
	Skeleton *dirty_list = nullptr;
	Transform2D base_transform_2d;

	GLuint transforms_texture = 0;

	uint64_t version = 1;

	Dependency dependency;
};

class MeshStorage : public RendererMeshStorage {
private:
	static MeshStorage *singleton;

	struct {
		SkeletonShaderGLES3 shader;
		RID shader_version;
	} skeleton_shader;

	/* Mesh */

	mutable RID_Owner<Mesh, true> mesh_owner;

	void _mesh_surface_generate_version_for_input_mask(Mesh::Surface::Version &v, Mesh::Surface *s, uint64_t p_input_mask, MeshInstance::Surface *mis = nullptr);
	void _mesh_surface_clear(Mesh *mesh, int p_surface);

	/* Mesh Instance API */

	mutable RID_Owner<MeshInstance> mesh_instance_owner;

	void _mesh_instance_clear(MeshInstance *mi);
	void _mesh_instance_add_surface(MeshInstance *mi, Mesh *mesh, uint32_t p_surface);
	void _mesh_instance_remove_surface(MeshInstance *mi, int p_surface);
	void _blend_shape_bind_mesh_instance_buffer(MeshInstance *p_mi, uint32_t p_surface);
	SelfList<MeshInstance>::List dirty_mesh_instance_weights;
	SelfList<MeshInstance>::List dirty_mesh_instance_arrays;

	/* MultiMesh */

	mutable RID_Owner<MultiMesh, true> multimesh_owner;

	MultiMesh *multimesh_dirty_list = nullptr;

	_FORCE_INLINE_ void _multimesh_make_local(MultiMesh *multimesh) const;
	_FORCE_INLINE_ void _multimesh_mark_dirty(MultiMesh *multimesh, int p_index, bool p_aabb);
	_FORCE_INLINE_ void _multimesh_mark_all_dirty(MultiMesh *multimesh, bool p_data, bool p_aabb);
	_FORCE_INLINE_ void _multimesh_re_create_aabb(MultiMesh *multimesh, const float *p_data, int p_instances);

	/* Skeleton */

	mutable RID_Owner<Skeleton, true> skeleton_owner;

	_FORCE_INLINE_ void _skeleton_make_dirty(Skeleton *skeleton);
	void _compute_skeleton(MeshInstance *p_mi, Skeleton *p_sk, uint32_t p_surface);

	Skeleton *skeleton_dirty_list = nullptr;

public:
	static MeshStorage *get_singleton();

	MeshStorage();
	virtual ~MeshStorage();

	/* MESH API */

	Mesh *get_mesh(RID p_rid) { return mesh_owner.get_or_null(p_rid); }
	bool owns_mesh(RID p_rid) { return mesh_owner.owns(p_rid); }

	virtual RID mesh_allocate() override;
	virtual void mesh_initialize(RID p_rid) override;
	virtual void mesh_free(RID p_rid) override;

	virtual void mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) override;
	virtual bool mesh_needs_instance(RID p_mesh, bool p_has_skeleton) override;

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

	virtual void mesh_set_path(RID p_mesh, const String &p_path) override;
	virtual String mesh_get_path(RID p_mesh) const override;

	virtual void mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) override;

	virtual void mesh_clear(RID p_mesh) override;
	virtual void mesh_surface_remove(RID p_mesh, int p_surface) override;

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

	_FORCE_INLINE_ uint32_t mesh_surface_get_lod(void *p_surface, float p_model_scale, float p_distance_threshold, float p_mesh_lod_threshold, uint32_t &r_index_count) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);
		ERR_FAIL_NULL_V(s, 0);

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

	_FORCE_INLINE_ GLuint mesh_surface_get_index_buffer(void *p_surface, uint32_t p_lod) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		if (p_lod == 0) {
			return s->index_buffer;
		} else {
			return s->lods[p_lod - 1].index_buffer;
		}
	}

	_FORCE_INLINE_ GLuint mesh_surface_get_index_buffer_wireframe(void *p_surface) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		if (s->wireframe) {
			return s->wireframe->index_buffer;
		}

		return 0;
	}

	_FORCE_INLINE_ GLenum mesh_surface_get_index_type(void *p_surface) const {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		return (s->vertex_count <= 65536 && s->vertex_count > 0) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	}

	// Use this to cache Vertex Array Objects so they are only generated once
	_FORCE_INLINE_ void mesh_surface_get_vertex_arrays_and_format(void *p_surface, uint64_t p_input_mask, GLuint &r_vertex_array_gl) {
		Mesh::Surface *s = reinterpret_cast<Mesh::Surface *>(p_surface);

		s->version_lock.lock();

		// There will never be more than 3 or 4 versions, so iterating is the fastest way.

		for (uint32_t i = 0; i < s->version_count; i++) {
			if (s->versions[i].input_mask != p_input_mask) {
				continue;
			}
			// We have this version, hooray.
			r_vertex_array_gl = s->versions[i].vertex_array;
			s->version_lock.unlock();
			return;
		}

		uint32_t version = s->version_count;
		s->version_count++;
		s->versions = (Mesh::Surface::Version *)memrealloc(s->versions, sizeof(Mesh::Surface::Version) * s->version_count);

		_mesh_surface_generate_version_for_input_mask(s->versions[version], s, p_input_mask);

		r_vertex_array_gl = s->versions[version].vertex_array;

		s->version_lock.unlock();
	}

	/* MESH INSTANCE API */

	MeshInstance *get_mesh_instance(RID p_rid) { return mesh_instance_owner.get_or_null(p_rid); }
	bool owns_mesh_instance(RID p_rid) { return mesh_instance_owner.owns(p_rid); }

	virtual RID mesh_instance_create(RID p_base) override;
	virtual void mesh_instance_free(RID p_rid) override;
	virtual void mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) override;
	virtual void mesh_instance_set_blend_shape_weight(RID p_mesh_instance, int p_shape, float p_weight) override;
	virtual void mesh_instance_check_for_update(RID p_mesh_instance) override;
	virtual void mesh_instance_set_canvas_item_transform(RID p_mesh_instance, const Transform2D &p_transform) override;
	virtual void update_mesh_instances() override;

	// TODO: considering hashing versions with multimesh buffer RID.
	// Doing so would allow us to avoid specifying multimesh buffer pointers every frame and may improve performance.
	_FORCE_INLINE_ void mesh_instance_surface_get_vertex_arrays_and_format(RID p_mesh_instance, uint32_t p_surface_index, uint64_t p_input_mask, GLuint &r_vertex_array_gl) {
		MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
		ERR_FAIL_NULL(mi);
		Mesh *mesh = mi->mesh;
		ERR_FAIL_UNSIGNED_INDEX(p_surface_index, mesh->surface_count);

		MeshInstance::Surface *mis = &mi->surfaces[p_surface_index];
		Mesh::Surface *s = mesh->surfaces[p_surface_index];

		s->version_lock.lock();

		//there will never be more than, at much, 3 or 4 versions, so iterating is the fastest way

		for (uint32_t i = 0; i < mis->version_count; i++) {
			if (mis->versions[i].input_mask != p_input_mask) {
				continue;
			}
			//we have this version, hooray
			r_vertex_array_gl = mis->versions[i].vertex_array;
			s->version_lock.unlock();
			return;
		}

		uint32_t version = mis->version_count;
		mis->version_count++;
		mis->versions = (Mesh::Surface::Version *)memrealloc(mis->versions, sizeof(Mesh::Surface::Version) * mis->version_count);

		_mesh_surface_generate_version_for_input_mask(mis->versions[version], s, p_input_mask, mis);

		r_vertex_array_gl = mis->versions[version].vertex_array;

		s->version_lock.unlock();
	}

	/* MULTIMESH API */

	MultiMesh *get_multimesh(RID p_rid) { return multimesh_owner.get_or_null(p_rid); }
	bool owns_multimesh(RID p_rid) { return multimesh_owner.owns(p_rid); }

	virtual RID _multimesh_allocate() override;
	virtual void _multimesh_initialize(RID p_rid) override;
	virtual void _multimesh_free(RID p_rid) override;
	virtual void _multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors = false, bool p_use_custom_data = false, bool p_use_indirect = false) override;
	virtual int _multimesh_get_instance_count(RID p_multimesh) const override;

	virtual void _multimesh_set_mesh(RID p_multimesh, RID p_mesh) override;
	virtual void _multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) override;
	virtual void _multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override;
	virtual void _multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override;
	virtual void _multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) override;

	virtual RID _multimesh_get_mesh(RID p_multimesh) const override;
	virtual void _multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) override;
	virtual AABB _multimesh_get_custom_aabb(RID p_multimesh) const override;
	virtual AABB _multimesh_get_aabb(RID p_multimesh) override;

	virtual Transform3D _multimesh_instance_get_transform(RID p_multimesh, int p_index) const override;
	virtual Transform2D _multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override;
	virtual Color _multimesh_instance_get_color(RID p_multimesh, int p_index) const override;
	virtual Color _multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override;
	virtual void _multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) override;
	virtual RID _multimesh_get_command_buffer_rd_rid(RID p_multimesh) const override;
	virtual RID _multimesh_get_buffer_rd_rid(RID p_multimesh) const override;
	virtual Vector<float> _multimesh_get_buffer(RID p_multimesh) const override;

	virtual void _multimesh_set_visible_instances(RID p_multimesh, int p_visible) override;
	virtual int _multimesh_get_visible_instances(RID p_multimesh) const override;

	virtual MultiMeshInterpolator *_multimesh_get_interpolator(RID p_multimesh) const override;

	void _update_dirty_multimeshes();

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

	_FORCE_INLINE_ GLuint multimesh_get_gl_buffer(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->buffer;
	}

	_FORCE_INLINE_ uint32_t multimesh_get_stride(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->stride_cache;
	}

	_FORCE_INLINE_ uint32_t multimesh_get_color_offset(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->color_offset_cache;
	}

	_FORCE_INLINE_ uint32_t multimesh_get_custom_data_offset(RID p_multimesh) const {
		MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
		return multimesh->custom_data_offset_cache;
	}

	/* SKELETON API */

	Skeleton *get_skeleton(RID p_rid) { return skeleton_owner.get_or_null(p_rid); }
	bool owns_skeleton(RID p_rid) { return skeleton_owner.owns(p_rid); }

	virtual RID skeleton_allocate() override;
	virtual void skeleton_initialize(RID p_rid) override;
	virtual void skeleton_free(RID p_rid) override;

	virtual void skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) override;
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) override;
	virtual int skeleton_get_bone_count(RID p_skeleton) const override;
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) override;
	virtual Transform3D skeleton_bone_get_transform(RID p_skeleton, int p_bone) const override;
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) override;
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const override;

	virtual void skeleton_update_dependency(RID p_base, DependencyTracker *p_instance) override;

	void _update_dirty_skeletons();

	_FORCE_INLINE_ bool skeleton_is_valid(RID p_skeleton) {
		return skeleton_owner.get_or_null(p_skeleton) != nullptr;
	}
};

} // namespace GLES3

#endif // GLES3_ENABLED

#endif // MESH_STORAGE_GLES3_H
