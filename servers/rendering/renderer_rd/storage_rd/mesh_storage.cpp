/**************************************************************************/
/*  mesh_storage.cpp                                                      */
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

#include "mesh_storage.h"

using namespace RendererRD;

MeshStorage *MeshStorage::singleton = nullptr;

MeshStorage *MeshStorage::get_singleton() {
	return singleton;
}

MeshStorage::MeshStorage() {
	singleton = this;

	default_rd_storage_buffer = RD::get_singleton()->storage_buffer_create(sizeof(uint32_t) * 4);

	//default rd buffers
	{
		Vector<uint8_t> buffer;
		{
			buffer.resize(sizeof(float) * 3);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 0.0;
				fptr[1] = 0.0;
				fptr[2] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_VERTEX] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //normal
			buffer.resize(sizeof(float) * 3);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 1.0;
				fptr[1] = 0.0;
				fptr[2] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_NORMAL] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //tangent
			buffer.resize(sizeof(float) * 4);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 1.0;
				fptr[1] = 0.0;
				fptr[2] = 0.0;
				fptr[3] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_TANGENT] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //color
			buffer.resize(sizeof(float) * 4);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 1.0;
				fptr[1] = 1.0;
				fptr[2] = 1.0;
				fptr[3] = 1.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_COLOR] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //tex uv 1
			buffer.resize(sizeof(float) * 2);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 0.0;
				fptr[1] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_TEX_UV] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}
		{ //tex uv 2
			buffer.resize(sizeof(float) * 2);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 0.0;
				fptr[1] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_TEX_UV2] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
			buffer.resize(sizeof(float) * 4);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 0.0;
				fptr[1] = 0.0;
				fptr[2] = 0.0;
				fptr[3] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_CUSTOM0 + i] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //bones
			buffer.resize(sizeof(uint32_t) * 4);
			{
				uint8_t *w = buffer.ptrw();
				uint32_t *fptr = reinterpret_cast<uint32_t *>(w);
				fptr[0] = 0;
				fptr[1] = 0;
				fptr[2] = 0;
				fptr[3] = 0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_BONES] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}

		{ //weights
			buffer.resize(sizeof(float) * 4);
			{
				uint8_t *w = buffer.ptrw();
				float *fptr = reinterpret_cast<float *>(w);
				fptr[0] = 0.0;
				fptr[1] = 0.0;
				fptr[2] = 0.0;
				fptr[3] = 0.0;
			}
			mesh_default_rd_buffers[DEFAULT_RD_BUFFER_WEIGHTS] = RD::get_singleton()->vertex_buffer_create(buffer.size(), buffer);
		}
	}

	{
		Vector<String> skeleton_modes;
		skeleton_modes.push_back("\n#define MODE_2D\n");
		skeleton_modes.push_back("");

		skeleton_shader.shader.initialize(skeleton_modes);
		skeleton_shader.version = skeleton_shader.shader.version_create();
		for (int i = 0; i < SkeletonShader::SHADER_MODE_MAX; i++) {
			skeleton_shader.version_shader[i] = skeleton_shader.shader.version_get_shader(skeleton_shader.version, i);
			skeleton_shader.pipeline[i] = RD::get_singleton()->compute_pipeline_create(skeleton_shader.version_shader[i]);
		}

		{
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.binding = 0;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.append_id(default_rd_storage_buffer);
				uniforms.push_back(u);
			}
			skeleton_shader.default_skeleton_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, skeleton_shader.version_shader[0], SkeletonShader::UNIFORM_SET_SKELETON);
		}
	}
}

MeshStorage::~MeshStorage() {
	//def buffers
	for (int i = 0; i < DEFAULT_RD_BUFFER_MAX; i++) {
		RD::get_singleton()->free(mesh_default_rd_buffers[i]);
	}

	skeleton_shader.shader.version_free(skeleton_shader.version);

	RD::get_singleton()->free(default_rd_storage_buffer);

	singleton = nullptr;
}

bool MeshStorage::free(RID p_rid) {
	if (owns_mesh(p_rid)) {
		mesh_free(p_rid);
		return true;
	} else if (owns_mesh_instance(p_rid)) {
		mesh_instance_free(p_rid);
		return true;
	} else if (owns_multimesh(p_rid)) {
		multimesh_free(p_rid);
		return true;
	} else if (owns_skeleton(p_rid)) {
		skeleton_free(p_rid);
		return true;
	}

	return false;
}

/* MESH API */

RID MeshStorage::mesh_allocate() {
	return mesh_owner.allocate_rid();
}

void MeshStorage::mesh_initialize(RID p_rid) {
	mesh_owner.initialize_rid(p_rid, Mesh());
}

void MeshStorage::mesh_free(RID p_rid) {
	mesh_clear(p_rid);
	mesh_set_shadow_mesh(p_rid, RID());
	Mesh *mesh = mesh_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(mesh);

	mesh->dependency.deleted_notify(p_rid);
	if (mesh->instances.size()) {
		ERR_PRINT("deleting mesh with active instances");
	}
	if (mesh->shadow_owners.size()) {
		for (Mesh *E : mesh->shadow_owners) {
			Mesh *shadow_owner = E;
			shadow_owner->shadow_mesh = RID();
			shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
		}
	}
	mesh_owner.free(p_rid);
}

void MeshStorage::mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) {
	ERR_FAIL_COND(p_blend_shape_count < 0);

	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	ERR_FAIL_COND(mesh->surface_count > 0); //surfaces already exist

	mesh->blend_shape_count = p_blend_shape_count;
}

/// Returns stride
void MeshStorage::mesh_add_surface(RID p_mesh, const RS::SurfaceData &p_surface) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	ERR_FAIL_COND(mesh->surface_count == RS::MAX_MESH_SURFACES);

#ifdef DEBUG_ENABLED
	//do a validation, to catch errors first
	{
		uint32_t stride = 0;
		uint32_t attrib_stride = 0;
		uint32_t skin_stride = 0;

		for (int i = 0; i < RS::ARRAY_WEIGHTS; i++) {
			if ((p_surface.format & (1ULL << i))) {
				switch (i) {
					case RS::ARRAY_VERTEX: {
						if ((p_surface.format & RS::ARRAY_FLAG_USE_2D_VERTICES) || (p_surface.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
							stride += sizeof(float) * 2;
						} else {
							stride += sizeof(float) * 3;
						}

					} break;
					case RS::ARRAY_NORMAL: {
						stride += sizeof(uint16_t) * 2;

					} break;
					case RS::ARRAY_TANGENT: {
						if (!(p_surface.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
							stride += sizeof(uint16_t) * 2;
						}
					} break;
					case RS::ARRAY_COLOR: {
						attrib_stride += sizeof(uint32_t);
					} break;
					case RS::ARRAY_TEX_UV: {
						if (p_surface.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
							attrib_stride += sizeof(uint16_t) * 2;
						} else {
							attrib_stride += sizeof(float) * 2;
						}

					} break;
					case RS::ARRAY_TEX_UV2: {
						if (p_surface.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
							attrib_stride += sizeof(uint16_t) * 2;
						} else {
							attrib_stride += sizeof(float) * 2;
						}

					} break;
					case RS::ARRAY_CUSTOM0:
					case RS::ARRAY_CUSTOM1:
					case RS::ARRAY_CUSTOM2:
					case RS::ARRAY_CUSTOM3: {
						int idx = i - RS::ARRAY_CUSTOM0;
						const uint32_t fmt_shift[RS::ARRAY_CUSTOM_COUNT] = { RS::ARRAY_FORMAT_CUSTOM0_SHIFT, RS::ARRAY_FORMAT_CUSTOM1_SHIFT, RS::ARRAY_FORMAT_CUSTOM2_SHIFT, RS::ARRAY_FORMAT_CUSTOM3_SHIFT };
						uint32_t fmt = (p_surface.format >> fmt_shift[idx]) & RS::ARRAY_FORMAT_CUSTOM_MASK;
						const uint32_t fmtsize[RS::ARRAY_CUSTOM_MAX] = { 4, 4, 4, 8, 4, 8, 12, 16 };
						attrib_stride += fmtsize[fmt];

					} break;
					case RS::ARRAY_WEIGHTS:
					case RS::ARRAY_BONES: {
						//uses a separate array
						bool use_8 = p_surface.format & RS::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
						skin_stride += sizeof(int16_t) * (use_8 ? 16 : 8);
					} break;
				}
			}
		}

		int expected_size = stride * p_surface.vertex_count;
		ERR_FAIL_COND_MSG(expected_size != p_surface.vertex_data.size(), "Size of vertex data provided (" + itos(p_surface.vertex_data.size()) + ") does not match expected (" + itos(expected_size) + ")");

		int bs_expected_size = expected_size * mesh->blend_shape_count;

		ERR_FAIL_COND_MSG(bs_expected_size != p_surface.blend_shape_data.size(), "Size of blend shape data provided (" + itos(p_surface.blend_shape_data.size()) + ") does not match expected (" + itos(bs_expected_size) + ")");

		int expected_attrib_size = attrib_stride * p_surface.vertex_count;
		ERR_FAIL_COND_MSG(expected_attrib_size != p_surface.attribute_data.size(), "Size of attribute data provided (" + itos(p_surface.attribute_data.size()) + ") does not match expected (" + itos(expected_attrib_size) + ")");

		if ((p_surface.format & RS::ARRAY_FORMAT_WEIGHTS) && (p_surface.format & RS::ARRAY_FORMAT_BONES)) {
			expected_size = skin_stride * p_surface.vertex_count;
			ERR_FAIL_COND_MSG(expected_size != p_surface.skin_data.size(), "Size of skin data provided (" + itos(p_surface.skin_data.size()) + ") does not match expected (" + itos(expected_size) + ")");
		}
	}

#endif

	uint64_t surface_version = p_surface.format & (uint64_t(RS::ARRAY_FLAG_FORMAT_VERSION_MASK) << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	RS::SurfaceData new_surface = p_surface;
#ifdef DISABLE_DEPRECATED

	ERR_FAIL_COND_MSG(surface_version != RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION, "Surface version provided (" + itos(int(surface_version >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT)) + ") does not match current version (" + itos(RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) + ")");

#else

	if (surface_version != uint64_t(RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION)) {
		RS::get_singleton()->fix_surface_compatibility(new_surface);
		surface_version = new_surface.format & (RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
		ERR_FAIL_COND_MSG(surface_version != RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION,
				vformat("Surface version provided (%d) does not match current version (%d).",
						(surface_version >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK,
						(RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK));
	}
#endif

	Mesh::Surface *s = memnew(Mesh::Surface);

	s->format = new_surface.format;
	s->primitive = new_surface.primitive;

	bool use_as_storage = (new_surface.skin_data.size() || mesh->blend_shape_count > 0);

	if (new_surface.vertex_data.size()) {
		// If we have an uncompressed surface that contains normals, but not tangents, we need to differentiate the array
		// from a compressed array in the shader. To do so, we allow the normal to read 4 components out of the buffer
		// But only give it 2 components per normal. So essentially, each vertex reads the next normal in normal.zw.
		// This allows us to avoid adding a shader permutation, and avoid passing dummy tangents. Since the stride is kept small
		// this should still be a net win for bandwidth.
		// If we do this, then the last normal will read past the end of the array. So we need to pad the array with dummy data.
		if (!(new_surface.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && (new_surface.format & RS::ARRAY_FORMAT_NORMAL) && !(new_surface.format & RS::ARRAY_FORMAT_TANGENT)) {
			// Unfortunately, we need to copy the buffer, which is fine as doing a resize triggers a CoW anyway.
			Vector<uint8_t> new_vertex_data;
			new_vertex_data.resize_zeroed(new_surface.vertex_data.size() + sizeof(uint16_t) * 2);
			memcpy(new_vertex_data.ptrw(), new_surface.vertex_data.ptr(), new_surface.vertex_data.size());
			s->vertex_buffer = RD::get_singleton()->vertex_buffer_create(new_vertex_data.size(), new_vertex_data, use_as_storage);
			s->vertex_buffer_size = new_vertex_data.size();
		} else {
			s->vertex_buffer = RD::get_singleton()->vertex_buffer_create(new_surface.vertex_data.size(), new_surface.vertex_data, use_as_storage);
			s->vertex_buffer_size = new_surface.vertex_data.size();
		}
	}

	if (new_surface.attribute_data.size()) {
		s->attribute_buffer = RD::get_singleton()->vertex_buffer_create(new_surface.attribute_data.size(), new_surface.attribute_data);
	}
	if (new_surface.skin_data.size()) {
		s->skin_buffer = RD::get_singleton()->vertex_buffer_create(new_surface.skin_data.size(), new_surface.skin_data, use_as_storage);
		s->skin_buffer_size = new_surface.skin_data.size();
	}

	s->vertex_count = new_surface.vertex_count;

	if (new_surface.format & RS::ARRAY_FORMAT_BONES) {
		mesh->has_bone_weights = true;
	}

	if (new_surface.index_count) {
		bool is_index_16 = new_surface.vertex_count <= 65536 && new_surface.vertex_count > 0;

		s->index_buffer = RD::get_singleton()->index_buffer_create(new_surface.index_count, is_index_16 ? RD::INDEX_BUFFER_FORMAT_UINT16 : RD::INDEX_BUFFER_FORMAT_UINT32, new_surface.index_data, false);
		s->index_count = new_surface.index_count;
		s->index_array = RD::get_singleton()->index_array_create(s->index_buffer, 0, s->index_count);
		if (new_surface.lods.size()) {
			s->lods = memnew_arr(Mesh::Surface::LOD, new_surface.lods.size());
			s->lod_count = new_surface.lods.size();

			for (int i = 0; i < new_surface.lods.size(); i++) {
				uint32_t indices = new_surface.lods[i].index_data.size() / (is_index_16 ? 2 : 4);
				s->lods[i].index_buffer = RD::get_singleton()->index_buffer_create(indices, is_index_16 ? RD::INDEX_BUFFER_FORMAT_UINT16 : RD::INDEX_BUFFER_FORMAT_UINT32, new_surface.lods[i].index_data);
				s->lods[i].index_array = RD::get_singleton()->index_array_create(s->lods[i].index_buffer, 0, indices);
				s->lods[i].edge_length = new_surface.lods[i].edge_length;
				s->lods[i].index_count = indices;
			}
		}
	}

	ERR_FAIL_COND_MSG(!new_surface.index_count && !new_surface.vertex_count, "Meshes must contain a vertex array, an index array, or both");

	s->aabb = new_surface.aabb;
	s->bone_aabbs = new_surface.bone_aabbs; //only really useful for returning them.
	s->mesh_to_skeleton_xform = p_surface.mesh_to_skeleton_xform;

	s->uv_scale = new_surface.uv_scale;

	if (mesh->blend_shape_count > 0) {
		s->blend_shape_buffer = RD::get_singleton()->storage_buffer_create(new_surface.blend_shape_data.size(), new_surface.blend_shape_data);
	}

	if (use_as_storage) {
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.binding = 0;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			if (s->vertex_buffer.is_valid()) {
				u.append_id(s->vertex_buffer);
			} else {
				u.append_id(default_rd_storage_buffer);
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 1;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			if (s->skin_buffer.is_valid()) {
				u.append_id(s->skin_buffer);
			} else {
				u.append_id(default_rd_storage_buffer);
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 2;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			if (s->blend_shape_buffer.is_valid()) {
				u.append_id(s->blend_shape_buffer);
			} else {
				u.append_id(default_rd_storage_buffer);
			}
			uniforms.push_back(u);
		}

		s->uniform_set = RD::get_singleton()->uniform_set_create(uniforms, skeleton_shader.version_shader[0], SkeletonShader::UNIFORM_SET_SURFACE);
	}

	if (mesh->surface_count == 0) {
		mesh->aabb = new_surface.aabb;
	} else {
		mesh->aabb.merge_with(new_surface.aabb);
	}
	mesh->skeleton_aabb_version = 0;

	s->material = new_surface.material;

	mesh->surfaces = (Mesh::Surface **)memrealloc(mesh->surfaces, sizeof(Mesh::Surface *) * (mesh->surface_count + 1));
	mesh->surfaces[mesh->surface_count] = s;
	mesh->surface_count++;

	for (MeshInstance *mi : mesh->instances) {
		_mesh_instance_add_surface(mi, mesh, mesh->surface_count - 1);
	}

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);

	for (Mesh *E : mesh->shadow_owners) {
		Mesh *shadow_owner = E;
		shadow_owner->shadow_mesh = RID();
		shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	}

	mesh->material_cache.clear();
}

int MeshStorage::mesh_get_blend_shape_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, -1);
	return mesh->blend_shape_count;
}

void MeshStorage::mesh_set_blend_shape_mode(RID p_mesh, RS::BlendShapeMode p_mode) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_INDEX((int)p_mode, 2);

	mesh->blend_shape_mode = p_mode;
}

RS::BlendShapeMode MeshStorage::mesh_get_blend_shape_mode(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RS::BLEND_SHAPE_MODE_NORMALIZED);
	return mesh->blend_shape_mode;
}

void MeshStorage::mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
	ERR_FAIL_COND(p_data.is_empty());
	ERR_FAIL_COND(mesh->surfaces[p_surface]->vertex_buffer.is_null());
	uint64_t data_size = p_data.size();
	const uint8_t *r = p_data.ptr();

	RD::get_singleton()->buffer_update(mesh->surfaces[p_surface]->vertex_buffer, p_offset, data_size, r);
}

void MeshStorage::mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
	ERR_FAIL_COND(p_data.is_empty());
	ERR_FAIL_COND(mesh->surfaces[p_surface]->attribute_buffer.is_null());
	uint64_t data_size = p_data.size();
	const uint8_t *r = p_data.ptr();

	RD::get_singleton()->buffer_update(mesh->surfaces[p_surface]->attribute_buffer, p_offset, data_size, r);
}

void MeshStorage::mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
	ERR_FAIL_COND(p_data.is_empty());
	ERR_FAIL_COND(mesh->surfaces[p_surface]->skin_buffer.is_null());
	uint64_t data_size = p_data.size();
	const uint8_t *r = p_data.ptr();

	RD::get_singleton()->buffer_update(mesh->surfaces[p_surface]->skin_buffer, p_offset, data_size, r);
}

void MeshStorage::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
	mesh->surfaces[p_surface]->material = p_material;

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
	mesh->material_cache.clear();
}

RID MeshStorage::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RID());
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RID());

	return mesh->surfaces[p_surface]->material;
}

RS::SurfaceData MeshStorage::mesh_get_surface(RID p_mesh, int p_surface) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RS::SurfaceData());
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RS::SurfaceData());

	Mesh::Surface &s = *mesh->surfaces[p_surface];

	RS::SurfaceData sd;
	sd.format = s.format;
	if (s.vertex_buffer.is_valid()) {
		sd.vertex_data = RD::get_singleton()->buffer_get_data(s.vertex_buffer);
		// When using an uncompressed buffer with normals, but without tangents, we have to trim the padding.
		if (!(s.format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && (s.format & RS::ARRAY_FORMAT_NORMAL) && !(s.format & RS::ARRAY_FORMAT_TANGENT)) {
			sd.vertex_data.resize(sd.vertex_data.size() - sizeof(uint16_t) * 2);
		}
	}
	if (s.attribute_buffer.is_valid()) {
		sd.attribute_data = RD::get_singleton()->buffer_get_data(s.attribute_buffer);
	}
	if (s.skin_buffer.is_valid()) {
		sd.skin_data = RD::get_singleton()->buffer_get_data(s.skin_buffer);
	}
	sd.vertex_count = s.vertex_count;
	sd.index_count = s.index_count;
	sd.primitive = s.primitive;

	if (sd.index_count) {
		sd.index_data = RD::get_singleton()->buffer_get_data(s.index_buffer);
	}
	sd.aabb = s.aabb;
	sd.uv_scale = s.uv_scale;
	for (uint32_t i = 0; i < s.lod_count; i++) {
		RS::SurfaceData::LOD lod;
		lod.edge_length = s.lods[i].edge_length;
		lod.index_data = RD::get_singleton()->buffer_get_data(s.lods[i].index_buffer);
		sd.lods.push_back(lod);
	}

	sd.bone_aabbs = s.bone_aabbs;
	sd.mesh_to_skeleton_xform = s.mesh_to_skeleton_xform;

	if (s.blend_shape_buffer.is_valid()) {
		sd.blend_shape_data = RD::get_singleton()->buffer_get_data(s.blend_shape_buffer);
	}

	return sd;
}

int MeshStorage::mesh_get_surface_count(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, 0);
	return mesh->surface_count;
}

void MeshStorage::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	mesh->custom_aabb = p_aabb;

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB MeshStorage::mesh_get_custom_aabb(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, AABB());
	return mesh->custom_aabb;
}

AABB MeshStorage::mesh_get_aabb(RID p_mesh, RID p_skeleton) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, AABB());

	if (mesh->custom_aabb != AABB()) {
		return mesh->custom_aabb;
	}

	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	// A mesh can be shared by multiple skeletons and we need to avoid using the AABB from a different skeleton.
	if (!skeleton || skeleton->size == 0 || (mesh->skeleton_aabb_version == skeleton->version && mesh->skeleton_aabb_rid == p_skeleton)) {
		return mesh->aabb;
	}

	AABB aabb;

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		AABB laabb;
		const Mesh::Surface &surface = *mesh->surfaces[i];
		if ((surface.format & RS::ARRAY_FORMAT_BONES) && surface.bone_aabbs.size()) {
			int bs = surface.bone_aabbs.size();
			const AABB *skbones = surface.bone_aabbs.ptr();

			int sbs = skeleton->size;
			ERR_CONTINUE(bs > sbs);
			const float *baseptr = skeleton->data.ptr();

			bool found_bone_aabb = false;

			if (skeleton->use_2d) {
				for (int j = 0; j < bs; j++) {
					if (skbones[j].size == Vector3(-1, -1, -1)) {
						continue; //bone is unused
					}

					const float *dataptr = baseptr + j * 8;

					Transform3D mtx;

					mtx.basis.rows[0][0] = dataptr[0];
					mtx.basis.rows[0][1] = dataptr[1];
					mtx.origin.x = dataptr[3];

					mtx.basis.rows[1][0] = dataptr[4];
					mtx.basis.rows[1][1] = dataptr[5];
					mtx.origin.y = dataptr[7];

					// Transform bounds to skeleton's space before applying animation data.
					AABB baabb = surface.mesh_to_skeleton_xform.xform(skbones[j]);
					baabb = mtx.xform(baabb);

					if (!found_bone_aabb) {
						laabb = baabb;
						found_bone_aabb = true;
					} else {
						laabb.merge_with(baabb);
					}
				}
			} else {
				for (int j = 0; j < bs; j++) {
					if (skbones[j].size == Vector3(-1, -1, -1)) {
						continue; //bone is unused
					}

					const float *dataptr = baseptr + j * 12;

					Transform3D mtx;

					mtx.basis.rows[0][0] = dataptr[0];
					mtx.basis.rows[0][1] = dataptr[1];
					mtx.basis.rows[0][2] = dataptr[2];
					mtx.origin.x = dataptr[3];
					mtx.basis.rows[1][0] = dataptr[4];
					mtx.basis.rows[1][1] = dataptr[5];
					mtx.basis.rows[1][2] = dataptr[6];
					mtx.origin.y = dataptr[7];
					mtx.basis.rows[2][0] = dataptr[8];
					mtx.basis.rows[2][1] = dataptr[9];
					mtx.basis.rows[2][2] = dataptr[10];
					mtx.origin.z = dataptr[11];

					// Transform bounds to skeleton's space before applying animation data.
					AABB baabb = surface.mesh_to_skeleton_xform.xform(skbones[j]);
					baabb = mtx.xform(baabb);

					if (!found_bone_aabb) {
						laabb = baabb;
						found_bone_aabb = true;
					} else {
						laabb.merge_with(baabb);
					}
				}
			}

			if (found_bone_aabb) {
				// Transform skeleton bounds back to mesh's space if any animated AABB applied.
				laabb = surface.mesh_to_skeleton_xform.affine_inverse().xform(laabb);
			}

			if (laabb.size == Vector3()) {
				laabb = surface.aabb;
			}
		} else {
			laabb = surface.aabb;
		}

		if (i == 0) {
			aabb = laabb;
		} else {
			aabb.merge_with(laabb);
		}
	}

	mesh->aabb = aabb;

	mesh->skeleton_aabb_version = skeleton->version;
	mesh->skeleton_aabb_rid = p_skeleton;
	return aabb;
}

void MeshStorage::mesh_set_path(RID p_mesh, const String &p_path) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	mesh->path = p_path;
}

String MeshStorage::mesh_get_path(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, String());

	return mesh->path;
}

void MeshStorage::mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) {
	ERR_FAIL_COND_MSG(p_mesh == p_shadow_mesh, "Cannot set a mesh as its own shadow mesh.");
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	Mesh *shadow_mesh = mesh_owner.get_or_null(mesh->shadow_mesh);
	if (shadow_mesh) {
		shadow_mesh->shadow_owners.erase(mesh);
	}
	mesh->shadow_mesh = p_shadow_mesh;

	shadow_mesh = mesh_owner.get_or_null(mesh->shadow_mesh);

	if (shadow_mesh) {
		shadow_mesh->shadow_owners.insert(mesh);
	}

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

void MeshStorage::mesh_clear(RID p_mesh) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	// Clear instance data before mesh data.
	for (MeshInstance *mi : mesh->instances) {
		_mesh_instance_clear(mi);
	}

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		Mesh::Surface &s = *mesh->surfaces[i];
		if (s.vertex_buffer.is_valid()) {
			RD::get_singleton()->free(s.vertex_buffer); //clears arrays as dependency automatically, including all versions
		}
		if (s.attribute_buffer.is_valid()) {
			RD::get_singleton()->free(s.attribute_buffer);
		}
		if (s.skin_buffer.is_valid()) {
			RD::get_singleton()->free(s.skin_buffer);
		}
		if (s.versions) {
			memfree(s.versions); //reallocs, so free with memfree.
		}

		if (s.index_buffer.is_valid()) {
			RD::get_singleton()->free(s.index_buffer);
		}

		if (s.lod_count) {
			for (uint32_t j = 0; j < s.lod_count; j++) {
				RD::get_singleton()->free(s.lods[j].index_buffer);
			}
			memdelete_arr(s.lods);
		}

		if (s.blend_shape_buffer.is_valid()) {
			RD::get_singleton()->free(s.blend_shape_buffer);
		}

		memdelete(mesh->surfaces[i]);
	}
	if (mesh->surfaces) {
		memfree(mesh->surfaces);
	}

	mesh->surfaces = nullptr;
	mesh->surface_count = 0;
	mesh->material_cache.clear();
	mesh->has_bone_weights = false;
	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);

	for (Mesh *E : mesh->shadow_owners) {
		Mesh *shadow_owner = E;
		shadow_owner->shadow_mesh = RID();
		shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	}
}

bool MeshStorage::mesh_needs_instance(RID p_mesh, bool p_has_skeleton) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, false);

	return mesh->blend_shape_count > 0 || (mesh->has_bone_weights && p_has_skeleton);
}

Dependency *MeshStorage::mesh_get_dependency(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, nullptr);

	return &mesh->dependency;
}

/* MESH INSTANCE */

RID MeshStorage::mesh_instance_create(RID p_base) {
	Mesh *mesh = mesh_owner.get_or_null(p_base);
	ERR_FAIL_NULL_V(mesh, RID());

	RID rid = mesh_instance_owner.make_rid();
	MeshInstance *mi = mesh_instance_owner.get_or_null(rid);

	mi->mesh = mesh;

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		_mesh_instance_add_surface(mi, mesh, i);
	}

	mi->I = mesh->instances.push_back(mi);

	mi->dirty = true;

	return rid;
}

void MeshStorage::mesh_instance_free(RID p_rid) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_rid);
	_mesh_instance_clear(mi);
	mi->mesh->instances.erase(mi->I);
	mi->I = nullptr;

	mesh_instance_owner.free(p_rid);
}

void MeshStorage::mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	if (mi->skeleton == p_skeleton) {
		return;
	}
	mi->skeleton = p_skeleton;
	mi->skeleton_version = 0;
	mi->dirty = true;
}

void MeshStorage::mesh_instance_set_blend_shape_weight(RID p_mesh_instance, int p_shape, float p_weight) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	ERR_FAIL_NULL(mi);
	ERR_FAIL_INDEX(p_shape, (int)mi->blend_weights.size());
	mi->blend_weights[p_shape] = p_weight;
	mi->weights_dirty = true;
	//will be eventually updated
}

void MeshStorage::_mesh_instance_clear(MeshInstance *mi) {
	for (const RendererRD::MeshStorage::MeshInstance::Surface &surface : mi->surfaces) {
		if (surface.versions) {
			for (uint32_t j = 0; j < surface.version_count; j++) {
				RD::get_singleton()->free(surface.versions[j].vertex_array);
			}
			memfree(surface.versions);
		}

		for (uint32_t i = 0; i < 2; i++) {
			if (surface.vertex_buffer[i].is_valid()) {
				RD::get_singleton()->free(surface.vertex_buffer[i]);
			}
		}
	}
	mi->surfaces.clear();

	if (mi->blend_weights_buffer.is_valid()) {
		RD::get_singleton()->free(mi->blend_weights_buffer);
		mi->blend_weights_buffer = RID();
	}
	mi->blend_weights.clear();
	mi->weights_dirty = false;
	mi->skeleton_version = 0;
}

void MeshStorage::_mesh_instance_add_surface(MeshInstance *mi, Mesh *mesh, uint32_t p_surface) {
	if (mesh->blend_shape_count > 0 && mi->blend_weights_buffer.is_null()) {
		mi->blend_weights.resize(mesh->blend_shape_count);
		for (float &weight : mi->blend_weights) {
			weight = 0;
		}
		mi->blend_weights_buffer = RD::get_singleton()->storage_buffer_create(sizeof(float) * mi->blend_weights.size(), mi->blend_weights.to_byte_array());
		mi->weights_dirty = true;
	}

	MeshInstance::Surface s;
	if ((mesh->blend_shape_count > 0 || (mesh->surfaces[p_surface]->format & RS::ARRAY_FORMAT_BONES)) && mesh->surfaces[p_surface]->vertex_buffer_size > 0) {
		_mesh_instance_add_surface_buffer(mi, mesh, &s, p_surface, 0);
	}

	mi->surfaces.push_back(s);
	mi->dirty = true;
}

void MeshStorage::_mesh_instance_add_surface_buffer(MeshInstance *mi, Mesh *mesh, MeshInstance::Surface *s, uint32_t p_surface, uint32_t p_buffer_index) {
	s->vertex_buffer[p_buffer_index] = RD::get_singleton()->vertex_buffer_create(mesh->surfaces[p_surface]->vertex_buffer_size, Vector<uint8_t>(), true);

	Vector<RD::Uniform> uniforms;
	{
		RD::Uniform u;
		u.binding = 1;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.append_id(s->vertex_buffer[p_buffer_index]);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.binding = 2;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		if (mi->blend_weights_buffer.is_valid()) {
			u.append_id(mi->blend_weights_buffer);
		} else {
			u.append_id(default_rd_storage_buffer);
		}
		uniforms.push_back(u);
	}
	s->uniform_set[p_buffer_index] = RD::get_singleton()->uniform_set_create(uniforms, skeleton_shader.version_shader[0], SkeletonShader::UNIFORM_SET_INSTANCE);
}

void MeshStorage::mesh_instance_check_for_update(RID p_mesh_instance) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);

	bool needs_update = mi->dirty;

	if (mi->weights_dirty && !mi->weight_update_list.in_list()) {
		dirty_mesh_instance_weights.add(&mi->weight_update_list);
		needs_update = true;
	}

	if (mi->array_update_list.in_list()) {
		return;
	}

	if (!needs_update && mi->skeleton.is_valid()) {
		Skeleton *sk = skeleton_owner.get_or_null(mi->skeleton);
		if (sk && sk->version != mi->skeleton_version) {
			needs_update = true;
		}
	}

	if (needs_update) {
		dirty_mesh_instance_arrays.add(&mi->array_update_list);
	}
}

void MeshStorage::mesh_instance_set_canvas_item_transform(RID p_mesh_instance, const Transform2D &p_transform) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	mi->canvas_item_transform_2d = p_transform;
}

void MeshStorage::update_mesh_instances() {
	while (dirty_mesh_instance_weights.first()) {
		MeshInstance *mi = dirty_mesh_instance_weights.first()->self();

		if (mi->blend_weights_buffer.is_valid()) {
			RD::get_singleton()->buffer_update(mi->blend_weights_buffer, 0, mi->blend_weights.size() * sizeof(float), mi->blend_weights.ptr());
		}
		dirty_mesh_instance_weights.remove(&mi->weight_update_list);
		mi->weights_dirty = false;
	}
	if (dirty_mesh_instance_arrays.first() == nullptr) {
		return; //nothing to do
	}

	//process skeletons and blend shapes
	uint64_t frame = RSG::rasterizer->get_frame_number();
	bool uses_motion_vectors = (RSG::viewport->get_num_viewports_with_motion_vectors() > 0) || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	while (dirty_mesh_instance_arrays.first()) {
		MeshInstance *mi = dirty_mesh_instance_arrays.first()->self();

		Skeleton *sk = skeleton_owner.get_or_null(mi->skeleton);

		for (uint32_t i = 0; i < mi->surfaces.size(); i++) {
			if (mi->surfaces[i].uniform_set[0].is_null() || mi->mesh->surfaces[i]->uniform_set.is_null()) {
				// Skip over mesh instances that don't require their own uniform buffers.
				continue;
			}

			mi->surfaces[i].previous_buffer = mi->surfaces[i].current_buffer;

			if (uses_motion_vectors && mi->surfaces[i].last_change && (frame - mi->surfaces[i].last_change) <= 2) {
				// Use a 2-frame tolerance so that stepped skeletal animations have correct motion vectors
				// (stepped animation is common for distant NPCs).
				uint32_t new_buffer_index = mi->surfaces[i].current_buffer ^ 1;

				if (mi->surfaces[i].uniform_set[new_buffer_index].is_null()) {
					// Create the new vertex buffer on demand where the result for the current frame will be stored.
					_mesh_instance_add_surface_buffer(mi, mi->mesh, &mi->surfaces[i], i, new_buffer_index);
				}

				mi->surfaces[i].current_buffer = new_buffer_index;
			}

			mi->surfaces[i].last_change = frame;

			RID mi_surface_uniform_set = mi->surfaces[i].uniform_set[mi->surfaces[i].current_buffer];
			if (mi_surface_uniform_set.is_null()) {
				continue;
			}

			bool array_is_2d = mi->mesh->surfaces[i]->format & RS::ARRAY_FLAG_USE_2D_VERTICES;

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, skeleton_shader.pipeline[array_is_2d ? SkeletonShader::SHADER_MODE_2D : SkeletonShader::SHADER_MODE_3D]);

			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, mi_surface_uniform_set, SkeletonShader::UNIFORM_SET_INSTANCE);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, mi->mesh->surfaces[i]->uniform_set, SkeletonShader::UNIFORM_SET_SURFACE);
			if (sk && sk->uniform_set_mi.is_valid()) {
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, sk->uniform_set_mi, SkeletonShader::UNIFORM_SET_SKELETON);
			} else {
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, skeleton_shader.default_skeleton_uniform_set, SkeletonShader::UNIFORM_SET_SKELETON);
			}

			SkeletonShader::PushConstant push_constant;

			push_constant.has_normal = mi->mesh->surfaces[i]->format & RS::ARRAY_FORMAT_NORMAL;
			push_constant.has_tangent = mi->mesh->surfaces[i]->format & RS::ARRAY_FORMAT_TANGENT;
			push_constant.has_skeleton = sk != nullptr && sk->use_2d == array_is_2d && (mi->mesh->surfaces[i]->format & RS::ARRAY_FORMAT_BONES);
			push_constant.has_blend_shape = mi->mesh->blend_shape_count > 0;

			push_constant.normal_tangent_stride = (push_constant.has_normal ? 1 : 0) + (push_constant.has_tangent ? 1 : 0);

			push_constant.vertex_count = mi->mesh->surfaces[i]->vertex_count;
			push_constant.vertex_stride = ((mi->mesh->surfaces[i]->vertex_buffer_size / mi->mesh->surfaces[i]->vertex_count) / 4) - push_constant.normal_tangent_stride;
			push_constant.skin_stride = (mi->mesh->surfaces[i]->skin_buffer_size / mi->mesh->surfaces[i]->vertex_count) / 4;
			push_constant.skin_weight_offset = (mi->mesh->surfaces[i]->format & RS::ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 4 : 2;

			Transform2D transform = Transform2D();
			if (sk && sk->use_2d) {
				transform = mi->canvas_item_transform_2d.affine_inverse() * sk->base_transform_2d;
			}
			push_constant.skeleton_transform_x[0] = transform.columns[0][0];
			push_constant.skeleton_transform_x[1] = transform.columns[0][1];
			push_constant.skeleton_transform_y[0] = transform.columns[1][0];
			push_constant.skeleton_transform_y[1] = transform.columns[1][1];
			push_constant.skeleton_transform_offset[0] = transform.columns[2][0];
			push_constant.skeleton_transform_offset[1] = transform.columns[2][1];

			Transform2D inverse_transform = transform.affine_inverse();
			push_constant.inverse_transform_x[0] = inverse_transform.columns[0][0];
			push_constant.inverse_transform_x[1] = inverse_transform.columns[0][1];
			push_constant.inverse_transform_y[0] = inverse_transform.columns[1][0];
			push_constant.inverse_transform_y[1] = inverse_transform.columns[1][1];
			push_constant.inverse_transform_offset[0] = inverse_transform.columns[2][0];
			push_constant.inverse_transform_offset[1] = inverse_transform.columns[2][1];

			push_constant.blend_shape_count = mi->mesh->blend_shape_count;
			push_constant.normalized_blend_shapes = mi->mesh->blend_shape_mode == RS::BLEND_SHAPE_MODE_NORMALIZED;
			push_constant.pad1 = 0;

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SkeletonShader::PushConstant));

			//dispatch without barrier, so all is done at the same time
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.vertex_count, 1, 1);
		}

		mi->dirty = false;
		if (sk) {
			mi->skeleton_version = sk->version;
		}
		dirty_mesh_instance_arrays.remove(&mi->array_update_list);
	}

	RD::get_singleton()->compute_list_end();
}

RD::VertexFormatID MeshStorage::_mesh_surface_generate_vertex_format(uint64_t p_surface_format, uint64_t p_input_mask, bool p_instanced_surface, bool p_input_motion_vectors, uint32_t &r_position_stride) {
	Vector<RD::VertexAttribute> attributes;
	uint32_t normal_tangent_stride = 0;
	uint32_t attribute_stride = 0;
	uint32_t skin_stride = 0;

	r_position_stride = 0;

	for (int i = 0; i < RS::ARRAY_INDEX; i++) {
		RD::VertexAttribute vd;
		vd.location = i;

		if (!(p_surface_format & (1ULL << i))) {
			vd.stride = 0;
			switch (i) {
				case RS::ARRAY_VERTEX:
				case RS::ARRAY_NORMAL:
					vd.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
					break;
				case RS::ARRAY_TEX_UV:
				case RS::ARRAY_TEX_UV2:
					vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
					break;
				case RS::ARRAY_BONES:
					vd.format = RD::DATA_FORMAT_R32G32B32A32_UINT;
					break;
				case RS::ARRAY_TANGENT:
				case RS::ARRAY_COLOR:
				case RS::ARRAY_CUSTOM0:
				case RS::ARRAY_CUSTOM1:
				case RS::ARRAY_CUSTOM2:
				case RS::ARRAY_CUSTOM3:
				case RS::ARRAY_WEIGHTS:
					vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
					break;
				default:
					DEV_ASSERT(false && "Unknown vertex format element.");
					break;
			}
		} else {
			// Mark that it needs a stride set (default uses 0).
			vd.stride = 1;

			switch (i) {
				case RS::ARRAY_VERTEX: {
					vd.offset = r_position_stride;

					if (p_surface_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
						vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
						r_position_stride = sizeof(float) * 2;
					} else {
						if (!p_instanced_surface && (p_surface_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
							vd.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
							r_position_stride = sizeof(uint16_t) * 4;
						} else {
							vd.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
							r_position_stride = sizeof(float) * 3;
						}
					}

				} break;
				case RS::ARRAY_NORMAL: {
					vd.offset = 0;

					if (!p_instanced_surface && (p_surface_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
						vd.format = RD::DATA_FORMAT_R16G16_UNORM;
						normal_tangent_stride += sizeof(uint16_t) * 2;
					} else {
						vd.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
						// A small trick here: if we are uncompressed and we have normals, but no tangents. We need
						// the shader to think there are 4 components to "axis_tangent_attrib". So we give a size of 4,
						// but a stride based on only having 2 elements.
						if (!(p_surface_format & RS::ARRAY_FORMAT_TANGENT)) {
							normal_tangent_stride += sizeof(uint16_t) * 2;
						} else {
							normal_tangent_stride += sizeof(uint16_t) * 4;
						}
					}
				} break;
				case RS::ARRAY_TANGENT: {
					vd.stride = 0;
					vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
				} break;
				case RS::ARRAY_COLOR: {
					vd.offset = attribute_stride;

					vd.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					attribute_stride += sizeof(int8_t) * 4;
				} break;
				case RS::ARRAY_TEX_UV: {
					vd.offset = attribute_stride;
					if (p_surface_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
						vd.format = RD::DATA_FORMAT_R16G16_UNORM;
						attribute_stride += sizeof(uint16_t) * 2;
					} else {
						vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
						attribute_stride += sizeof(float) * 2;
					}

				} break;
				case RS::ARRAY_TEX_UV2: {
					vd.offset = attribute_stride;
					if (p_surface_format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
						vd.format = RD::DATA_FORMAT_R16G16_UNORM;
						attribute_stride += sizeof(uint16_t) * 2;
					} else {
						vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
						attribute_stride += sizeof(float) * 2;
					}
				} break;
				case RS::ARRAY_CUSTOM0:
				case RS::ARRAY_CUSTOM1:
				case RS::ARRAY_CUSTOM2:
				case RS::ARRAY_CUSTOM3: {
					vd.offset = attribute_stride;

					int idx = i - RS::ARRAY_CUSTOM0;
					const uint32_t fmt_shift[RS::ARRAY_CUSTOM_COUNT] = { RS::ARRAY_FORMAT_CUSTOM0_SHIFT, RS::ARRAY_FORMAT_CUSTOM1_SHIFT, RS::ARRAY_FORMAT_CUSTOM2_SHIFT, RS::ARRAY_FORMAT_CUSTOM3_SHIFT };
					uint32_t fmt = (p_surface_format >> fmt_shift[idx]) & RS::ARRAY_FORMAT_CUSTOM_MASK;
					const uint32_t fmtsize[RS::ARRAY_CUSTOM_MAX] = { 4, 4, 4, 8, 4, 8, 12, 16 };
					const RD::DataFormat fmtrd[RS::ARRAY_CUSTOM_MAX] = { RD::DATA_FORMAT_R8G8B8A8_UNORM, RD::DATA_FORMAT_R8G8B8A8_SNORM, RD::DATA_FORMAT_R16G16_SFLOAT, RD::DATA_FORMAT_R16G16B16A16_SFLOAT, RD::DATA_FORMAT_R32_SFLOAT, RD::DATA_FORMAT_R32G32_SFLOAT, RD::DATA_FORMAT_R32G32B32_SFLOAT, RD::DATA_FORMAT_R32G32B32A32_SFLOAT };
					vd.format = fmtrd[fmt];
					attribute_stride += fmtsize[fmt];
				} break;
				case RS::ARRAY_BONES: {
					vd.offset = skin_stride;

					vd.format = RD::DATA_FORMAT_R16G16B16A16_UINT;
					skin_stride += sizeof(int16_t) * 4;
				} break;
				case RS::ARRAY_WEIGHTS: {
					vd.offset = skin_stride;

					vd.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
					skin_stride += sizeof(int16_t) * 4;
				} break;
			}
		}

		if (!(p_input_mask & (1ULL << i))) {
			continue; // Shader does not need this, skip it (but computing stride was important anyway)
		}

		attributes.push_back(vd);

		if (p_input_motion_vectors) {
			// Since the previous vertex, normal and tangent can't be part of the vertex format but they are required when
			// motion vectors are enabled, we opt to push a copy of the vertex attribute with a different location.
			switch (i) {
				case RS::ARRAY_VERTEX: {
					vd.location = ATTRIBUTE_LOCATION_PREV_VERTEX;
				} break;
				case RS::ARRAY_NORMAL: {
					vd.location = ATTRIBUTE_LOCATION_PREV_NORMAL;
				} break;
				case RS::ARRAY_TANGENT: {
					vd.location = ATTRIBUTE_LOCATION_PREV_TANGENT;
				} break;
			}

			if (int(vd.location) != i) {
				attributes.push_back(vd);
			}
		}
	}

	// Update final stride.
	for (int i = 0; i < attributes.size(); i++) {
		if (attributes[i].stride == 0) {
			// Default location.
			continue;
		}

		int loc = attributes[i].location;
		if (loc == RS::ARRAY_VERTEX || loc == ATTRIBUTE_LOCATION_PREV_VERTEX) {
			attributes.write[i].stride = r_position_stride;
		} else if ((loc < RS::ARRAY_COLOR) || ((loc >= ATTRIBUTE_LOCATION_PREV_NORMAL) && (loc <= ATTRIBUTE_LOCATION_PREV_TANGENT))) {
			attributes.write[i].stride = normal_tangent_stride;
		} else if (loc < RS::ARRAY_BONES) {
			attributes.write[i].stride = attribute_stride;
		} else {
			attributes.write[i].stride = skin_stride;
		}
	}

	return RD::get_singleton()->vertex_format_create(attributes);
}

void MeshStorage::_mesh_surface_generate_version_for_input_mask(Mesh::Surface::Version &v, Mesh::Surface *s, uint64_t p_input_mask, bool p_input_motion_vectors, MeshInstance::Surface *mis, uint32_t p_current_buffer, uint32_t p_previous_buffer) {
	uint32_t position_stride = 0;
	v.vertex_format = _mesh_surface_generate_vertex_format(s->format, p_input_mask, mis != nullptr, p_input_motion_vectors, position_stride);

	Vector<RID> buffers;
	Vector<uint64_t> offsets;
	RID buffer;
	uint64_t offset = 0;
	for (int i = 0; i < RS::ARRAY_INDEX; i++) {
		offset = 0;

		if (!(s->format & (1ULL << i))) {
			// Not supplied by surface, use default buffers.
			buffer = mesh_default_rd_buffers[i];
		} else {
			// Supplied by surface, use buffer.
			switch (i) {
				case RS::ARRAY_VERTEX:
				case RS::ARRAY_NORMAL:
					offset = i == RS::ARRAY_NORMAL ? position_stride * s->vertex_count : 0;
					buffer = mis != nullptr ? mis->vertex_buffer[p_current_buffer] : s->vertex_buffer;
					break;
				case RS::ARRAY_TANGENT:
					buffer = mesh_default_rd_buffers[i];
					break;
				case RS::ARRAY_COLOR:
				case RS::ARRAY_TEX_UV:
				case RS::ARRAY_TEX_UV2:
				case RS::ARRAY_CUSTOM0:
				case RS::ARRAY_CUSTOM1:
				case RS::ARRAY_CUSTOM2:
				case RS::ARRAY_CUSTOM3:
					buffer = s->attribute_buffer;
					break;
				case RS::ARRAY_BONES:
				case RS::ARRAY_WEIGHTS:
					buffer = s->skin_buffer;
					break;
			}
		}

		if (!(p_input_mask & (1ULL << i))) {
			continue; // Shader does not need this, skip it (but computing stride was important anyway)
		}

		buffers.push_back(buffer);
		offsets.push_back(offset);

		if (p_input_motion_vectors) {
			// Push the buffer for motion vector inputs.
			if (i == RS::ARRAY_VERTEX || i == RS::ARRAY_NORMAL || i == RS::ARRAY_TANGENT) {
				if (mis && buffer != mesh_default_rd_buffers[i]) {
					buffers.push_back(mis->vertex_buffer[p_previous_buffer]);
				} else {
					buffers.push_back(buffer);
				}

				offsets.push_back(offset);
			}
		}
	}

	v.input_mask = p_input_mask;
	v.current_buffer = p_current_buffer;
	v.previous_buffer = p_previous_buffer;
	v.input_motion_vectors = p_input_motion_vectors;
	v.vertex_array = RD::get_singleton()->vertex_array_create(s->vertex_count, v.vertex_format, buffers, offsets);
}

////////////////// MULTIMESH

RID MeshStorage::_multimesh_allocate() {
	return multimesh_owner.allocate_rid();
}
void MeshStorage::_multimesh_initialize(RID p_rid) {
	multimesh_owner.initialize_rid(p_rid, MultiMesh());
}

void MeshStorage::_multimesh_free(RID p_rid) {
	// Remove from interpolator.
	_interpolation_data.notify_free_multimesh(p_rid);
	_update_dirty_multimeshes();
	multimesh_allocate_data(p_rid, 0, RS::MULTIMESH_TRANSFORM_2D);
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_rid);
	multimesh->dependency.deleted_notify(p_rid);
	multimesh_owner.free(p_rid);
}

void MeshStorage::_multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);

	if (multimesh->instances == p_instances && multimesh->xform_format == p_transform_format && multimesh->uses_colors == p_use_colors && multimesh->uses_custom_data == p_use_custom_data) {
		return;
	}

	if (multimesh->buffer.is_valid()) {
		RD::get_singleton()->free(multimesh->buffer);
		multimesh->buffer = RID();
		multimesh->uniform_set_2d = RID(); //cleared by dependency
		multimesh->uniform_set_3d = RID(); //cleared by dependency
	}

	if (multimesh->data_cache_dirty_regions) {
		memdelete_arr(multimesh->data_cache_dirty_regions);
		multimesh->data_cache_dirty_regions = nullptr;
		multimesh->data_cache_dirty_region_count = 0;
	}

	if (multimesh->previous_data_cache_dirty_regions) {
		memdelete_arr(multimesh->previous_data_cache_dirty_regions);
		multimesh->previous_data_cache_dirty_regions = nullptr;
		multimesh->previous_data_cache_dirty_region_count = 0;
	}

	multimesh->instances = p_instances;
	multimesh->xform_format = p_transform_format;
	multimesh->uses_colors = p_use_colors;
	multimesh->color_offset_cache = p_transform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12;
	multimesh->uses_custom_data = p_use_custom_data;
	multimesh->custom_data_offset_cache = multimesh->color_offset_cache + (p_use_colors ? 4 : 0);
	multimesh->stride_cache = multimesh->custom_data_offset_cache + (p_use_custom_data ? 4 : 0);
	multimesh->buffer_set = false;

	//print_line("allocate, elements: " + itos(p_instances) + " 2D: " + itos(p_transform_format == RS::MULTIMESH_TRANSFORM_2D) + " colors " + itos(multimesh->uses_colors) + " data " + itos(multimesh->uses_custom_data) + " stride " + itos(multimesh->stride_cache) + " total size " + itos(multimesh->stride_cache * multimesh->instances));
	multimesh->data_cache = Vector<float>();
	multimesh->aabb = AABB();
	multimesh->aabb_dirty = false;
	multimesh->visible_instances = MIN(multimesh->visible_instances, multimesh->instances);
	multimesh->motion_vectors_current_offset = 0;
	multimesh->motion_vectors_previous_offset = 0;
	multimesh->motion_vectors_last_change = -1;
	multimesh->motion_vectors_enabled = false;

	if (multimesh->instances) {
		uint32_t buffer_size = multimesh->instances * multimesh->stride_cache * sizeof(float);
		multimesh->buffer = RD::get_singleton()->storage_buffer_create(buffer_size);
	}

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MULTIMESH);
}

void MeshStorage::_multimesh_enable_motion_vectors(MultiMesh *multimesh) {
	if (multimesh->motion_vectors_enabled) {
		return;
	}

	multimesh->motion_vectors_enabled = true;

	multimesh->motion_vectors_current_offset = 0;
	multimesh->motion_vectors_previous_offset = 0;
	multimesh->motion_vectors_last_change = -1;

	if (!multimesh->data_cache.is_empty()) {
		multimesh->data_cache.append_array(multimesh->data_cache);
	}

	uint32_t buffer_size = multimesh->instances * multimesh->stride_cache * sizeof(float);
	uint32_t new_buffer_size = buffer_size * 2;
	RID new_buffer = RD::get_singleton()->storage_buffer_create(new_buffer_size);

	if (multimesh->buffer_set && multimesh->data_cache.is_empty()) {
		// If the buffer was set but there's no data cached in the CPU, we copy the buffer directly on the GPU.
		RD::get_singleton()->buffer_copy(multimesh->buffer, new_buffer, 0, 0, buffer_size);
		RD::get_singleton()->buffer_copy(multimesh->buffer, new_buffer, 0, buffer_size, buffer_size);
	} else if (!multimesh->data_cache.is_empty()) {
		// Simply upload the data cached in the CPU, which should already be doubled in size.
		ERR_FAIL_COND(multimesh->data_cache.size() * sizeof(float) != size_t(new_buffer_size));
		RD::get_singleton()->buffer_update(new_buffer, 0, new_buffer_size, multimesh->data_cache.ptr());
	}

	if (multimesh->buffer.is_valid()) {
		RD::get_singleton()->free(multimesh->buffer);
	}

	multimesh->buffer = new_buffer;
	multimesh->uniform_set_3d = RID(); // Cleared by dependency.

	// Invalidate any references to the buffer that was released and the uniform set that was pointing to it.
	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MULTIMESH);
}

void MeshStorage::_multimesh_get_motion_vectors_offsets(RID p_multimesh, uint32_t &r_current_offset, uint32_t &r_prev_offset) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	r_current_offset = multimesh->motion_vectors_current_offset;
	if (!_multimesh_uses_motion_vectors(multimesh)) {
		multimesh->motion_vectors_previous_offset = multimesh->motion_vectors_current_offset;
	}
	r_prev_offset = multimesh->motion_vectors_previous_offset;
}

bool MeshStorage::_multimesh_uses_motion_vectors_offsets(RID p_multimesh) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, false);
	return _multimesh_uses_motion_vectors(multimesh);
}

int MeshStorage::_multimesh_get_instance_count(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, 0);
	return multimesh->instances;
}

void MeshStorage::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	if (multimesh->mesh == p_mesh) {
		return;
	}
	multimesh->mesh = p_mesh;

	if (multimesh->instances == 0) {
		return;
	}

	if (multimesh->data_cache.size()) {
		//we have a data cache, just mark it dirt
		_multimesh_mark_all_dirty(multimesh, false, true);
	} else if (multimesh->instances) {
		//need to re-create AABB unfortunately, calling this has a penalty
		if (multimesh->buffer_set) {
			Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(multimesh->buffer);
			const uint8_t *r = buffer.ptr() + multimesh->motion_vectors_current_offset * multimesh->stride_cache * sizeof(float);
			const float *data = reinterpret_cast<const float *>(r);
			_multimesh_re_create_aabb(multimesh, data, multimesh->instances);
		}
	}

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

#define MULTIMESH_DIRTY_REGION_SIZE 512

void MeshStorage::_multimesh_make_local(MultiMesh *multimesh) const {
	if (multimesh->data_cache.size() > 0) {
		return; //already local
	}

	// this means that the user wants to load/save individual elements,
	// for this, the data must reside on CPU, so just copy it there.
	uint32_t buffer_size = multimesh->instances * multimesh->stride_cache;
	if (multimesh->motion_vectors_enabled) {
		buffer_size *= 2;
	}
	multimesh->data_cache.resize(buffer_size);
	{
		float *w = multimesh->data_cache.ptrw();

		if (multimesh->buffer_set) {
			Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(multimesh->buffer);
			{
				const uint8_t *r = buffer.ptr();
				memcpy(w, r, buffer.size());
			}
		} else {
			memset(w, 0, buffer_size * sizeof(float));
		}
	}
	uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);
	multimesh->data_cache_dirty_regions = memnew_arr(bool, data_cache_dirty_region_count);
	memset(multimesh->data_cache_dirty_regions, 0, data_cache_dirty_region_count * sizeof(bool));
	multimesh->data_cache_dirty_region_count = 0;

	multimesh->previous_data_cache_dirty_regions = memnew_arr(bool, data_cache_dirty_region_count);
	memset(multimesh->previous_data_cache_dirty_regions, 0, data_cache_dirty_region_count * sizeof(bool));
	multimesh->previous_data_cache_dirty_region_count = 0;
}

void MeshStorage::_multimesh_update_motion_vectors_data_cache(MultiMesh *multimesh) {
	ERR_FAIL_COND(multimesh->data_cache.is_empty());

	if (!multimesh->motion_vectors_enabled) {
		return;
	}

	uint32_t frame = RSG::rasterizer->get_frame_number();
	if (multimesh->motion_vectors_last_change != frame) {
		multimesh->motion_vectors_previous_offset = multimesh->motion_vectors_current_offset;
		multimesh->motion_vectors_current_offset = multimesh->instances - multimesh->motion_vectors_current_offset;
		multimesh->motion_vectors_last_change = frame;

		if (multimesh->previous_data_cache_dirty_region_count > 0) {
			uint8_t *data = (uint8_t *)multimesh->data_cache.ptrw();
			uint32_t current_ofs = multimesh->motion_vectors_current_offset * multimesh->stride_cache * sizeof(float);
			uint32_t previous_ofs = multimesh->motion_vectors_previous_offset * multimesh->stride_cache * sizeof(float);
			uint32_t visible_instances = multimesh->visible_instances >= 0 ? multimesh->visible_instances : multimesh->instances;
			uint32_t visible_region_count = visible_instances == 0 ? 0 : Math::division_round_up(visible_instances, (uint32_t)MULTIMESH_DIRTY_REGION_SIZE);
			uint32_t region_size = multimesh->stride_cache * MULTIMESH_DIRTY_REGION_SIZE * sizeof(float);
			uint32_t size = multimesh->stride_cache * (uint32_t)multimesh->instances * (uint32_t)sizeof(float);
			for (uint32_t i = 0; i < visible_region_count; i++) {
				if (multimesh->previous_data_cache_dirty_regions[i]) {
					uint32_t offset = i * region_size;
					memcpy(data + current_ofs + offset, data + previous_ofs + offset, MIN(region_size, size - offset));
				}
			}
		}
	}
}

bool MeshStorage::_multimesh_uses_motion_vectors(MultiMesh *multimesh) {
	return (RSG::rasterizer->get_frame_number() - multimesh->motion_vectors_last_change) < 2;
}

void MeshStorage::_multimesh_mark_dirty(MultiMesh *multimesh, int p_index, bool p_aabb) {
	uint32_t region_index = p_index / MULTIMESH_DIRTY_REGION_SIZE;
#ifdef DEBUG_ENABLED
	uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);
	ERR_FAIL_UNSIGNED_INDEX(region_index, data_cache_dirty_region_count); //bug
#endif
	if (!multimesh->data_cache_dirty_regions[region_index]) {
		multimesh->data_cache_dirty_regions[region_index] = true;
		multimesh->data_cache_dirty_region_count++;
	}

	if (p_aabb) {
		multimesh->aabb_dirty = true;
	}

	if (!multimesh->dirty) {
		multimesh->dirty_list = multimesh_dirty_list;
		multimesh_dirty_list = multimesh;
		multimesh->dirty = true;
	}
}

void MeshStorage::_multimesh_mark_all_dirty(MultiMesh *multimesh, bool p_data, bool p_aabb) {
	if (p_data) {
		uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);

		for (uint32_t i = 0; i < data_cache_dirty_region_count; i++) {
			if (!multimesh->data_cache_dirty_regions[i]) {
				multimesh->data_cache_dirty_regions[i] = true;
				multimesh->data_cache_dirty_region_count++;
			}
		}
	}

	if (p_aabb) {
		multimesh->aabb_dirty = true;
	}

	if (!multimesh->dirty) {
		multimesh->dirty_list = multimesh_dirty_list;
		multimesh_dirty_list = multimesh;
		multimesh->dirty = true;
	}
}

void MeshStorage::_multimesh_re_create_aabb(MultiMesh *multimesh, const float *p_data, int p_instances) {
	ERR_FAIL_COND(multimesh->mesh.is_null());
	if (multimesh->custom_aabb != AABB()) {
		return;
	}
	AABB aabb;
	AABB mesh_aabb = mesh_get_aabb(multimesh->mesh);
	for (int i = 0; i < p_instances; i++) {
		const float *data = p_data + multimesh->stride_cache * i;
		Transform3D t;

		if (multimesh->xform_format == RS::MULTIMESH_TRANSFORM_3D) {
			t.basis.rows[0][0] = data[0];
			t.basis.rows[0][1] = data[1];
			t.basis.rows[0][2] = data[2];
			t.origin.x = data[3];
			t.basis.rows[1][0] = data[4];
			t.basis.rows[1][1] = data[5];
			t.basis.rows[1][2] = data[6];
			t.origin.y = data[7];
			t.basis.rows[2][0] = data[8];
			t.basis.rows[2][1] = data[9];
			t.basis.rows[2][2] = data[10];
			t.origin.z = data[11];

		} else {
			t.basis.rows[0][0] = data[0];
			t.basis.rows[0][1] = data[1];
			t.origin.x = data[3];

			t.basis.rows[1][0] = data[4];
			t.basis.rows[1][1] = data[5];
			t.origin.y = data[7];
		}

		if (i == 0) {
			aabb = t.xform(mesh_aabb);
		} else {
			aabb.merge_with(t.xform(mesh_aabb));
		}
	}

	multimesh->aabb = aabb;
}

void MeshStorage::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_3D);

	_multimesh_make_local(multimesh);

	bool uses_motion_vectors = (RSG::viewport->get_num_viewports_with_motion_vectors() > 0) || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	if (uses_motion_vectors) {
		_multimesh_enable_motion_vectors(multimesh);
	}

	_multimesh_update_motion_vectors_data_cache(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache;

		dataptr[0] = p_transform.basis.rows[0][0];
		dataptr[1] = p_transform.basis.rows[0][1];
		dataptr[2] = p_transform.basis.rows[0][2];
		dataptr[3] = p_transform.origin.x;
		dataptr[4] = p_transform.basis.rows[1][0];
		dataptr[5] = p_transform.basis.rows[1][1];
		dataptr[6] = p_transform.basis.rows[1][2];
		dataptr[7] = p_transform.origin.y;
		dataptr[8] = p_transform.basis.rows[2][0];
		dataptr[9] = p_transform.basis.rows[2][1];
		dataptr[10] = p_transform.basis.rows[2][2];
		dataptr[11] = p_transform.origin.z;
	}

	_multimesh_mark_dirty(multimesh, p_index, true);
}

void MeshStorage::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_2D);

	_multimesh_make_local(multimesh);
	_multimesh_update_motion_vectors_data_cache(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache;

		dataptr[0] = p_transform.columns[0][0];
		dataptr[1] = p_transform.columns[1][0];
		dataptr[2] = 0;
		dataptr[3] = p_transform.columns[2][0];
		dataptr[4] = p_transform.columns[0][1];
		dataptr[5] = p_transform.columns[1][1];
		dataptr[6] = 0;
		dataptr[7] = p_transform.columns[2][1];
	}

	_multimesh_mark_dirty(multimesh, p_index, true);
}

void MeshStorage::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_colors);

	_multimesh_make_local(multimesh);
	_multimesh_update_motion_vectors_data_cache(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache + multimesh->color_offset_cache;

		dataptr[0] = p_color.r;
		dataptr[1] = p_color.g;
		dataptr[2] = p_color.b;
		dataptr[3] = p_color.a;
	}

	_multimesh_mark_dirty(multimesh, p_index, false);
}

void MeshStorage::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_custom_data);

	_multimesh_make_local(multimesh);
	_multimesh_update_motion_vectors_data_cache(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache + multimesh->custom_data_offset_cache;

		dataptr[0] = p_color.r;
		dataptr[1] = p_color.g;
		dataptr[2] = p_color.b;
		dataptr[3] = p_color.a;
	}

	_multimesh_mark_dirty(multimesh, p_index, false);
}

RID MeshStorage::_multimesh_get_mesh(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, RID());

	return multimesh->mesh;
}

Dependency *MeshStorage::multimesh_get_dependency(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, nullptr);

	return &multimesh->dependency;
}

Transform3D MeshStorage::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform3D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform3D());
	ERR_FAIL_COND_V(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_3D, Transform3D());

	_multimesh_make_local(multimesh);

	Transform3D t;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache;

		t.basis.rows[0][0] = dataptr[0];
		t.basis.rows[0][1] = dataptr[1];
		t.basis.rows[0][2] = dataptr[2];
		t.origin.x = dataptr[3];
		t.basis.rows[1][0] = dataptr[4];
		t.basis.rows[1][1] = dataptr[5];
		t.basis.rows[1][2] = dataptr[6];
		t.origin.y = dataptr[7];
		t.basis.rows[2][0] = dataptr[8];
		t.basis.rows[2][1] = dataptr[9];
		t.basis.rows[2][2] = dataptr[10];
		t.origin.z = dataptr[11];
	}

	return t;
}

Transform2D MeshStorage::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform2D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform2D());
	ERR_FAIL_COND_V(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_2D, Transform2D());

	_multimesh_make_local(multimesh);

	Transform2D t;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache;

		t.columns[0][0] = dataptr[0];
		t.columns[1][0] = dataptr[1];
		t.columns[2][0] = dataptr[3];
		t.columns[0][1] = dataptr[4];
		t.columns[1][1] = dataptr[5];
		t.columns[2][1] = dataptr[7];
	}

	return t;
}

Color MeshStorage::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	ERR_FAIL_COND_V(!multimesh->uses_colors, Color());

	_multimesh_make_local(multimesh);

	Color c;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache + multimesh->color_offset_cache;

		c.r = dataptr[0];
		c.g = dataptr[1];
		c.b = dataptr[2];
		c.a = dataptr[3];
	}

	return c;
}

Color MeshStorage::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	ERR_FAIL_COND_V(!multimesh->uses_custom_data, Color());

	_multimesh_make_local(multimesh);

	Color c;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + (multimesh->motion_vectors_current_offset + p_index) * multimesh->stride_cache + multimesh->custom_data_offset_cache;

		c.r = dataptr[0];
		c.g = dataptr[1];
		c.b = dataptr[2];
		c.a = dataptr[3];
	}

	return c;
}

void MeshStorage::_multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_COND(p_buffer.size() != (multimesh->instances * (int)multimesh->stride_cache));

	bool used_motion_vectors = multimesh->motion_vectors_enabled;
	bool uses_motion_vectors = (RSG::viewport->get_num_viewports_with_motion_vectors() > 0) || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	if (uses_motion_vectors) {
		_multimesh_enable_motion_vectors(multimesh);
	}

	if (multimesh->motion_vectors_enabled) {
		uint32_t frame = RSG::rasterizer->get_frame_number();

		if (multimesh->motion_vectors_last_change != frame) {
			multimesh->motion_vectors_previous_offset = multimesh->motion_vectors_current_offset;
			multimesh->motion_vectors_current_offset = multimesh->instances - multimesh->motion_vectors_current_offset;
			multimesh->motion_vectors_last_change = frame;
		}
	}

	{
		const float *r = p_buffer.ptr();
		RD::get_singleton()->buffer_update(multimesh->buffer, multimesh->motion_vectors_current_offset * multimesh->stride_cache * sizeof(float), p_buffer.size() * sizeof(float), r);
		if (multimesh->motion_vectors_enabled && !used_motion_vectors) {
			// Motion vectors were just enabled, and the other half of the buffer will be empty.
			// Need to ensure that both halves are filled for correct operation.
			RD::get_singleton()->buffer_update(multimesh->buffer, multimesh->motion_vectors_previous_offset * multimesh->stride_cache * sizeof(float), p_buffer.size() * sizeof(float), r);
		}
		multimesh->buffer_set = true;
	}

	if (multimesh->data_cache.size()) {
		float *cache_data = multimesh->data_cache.ptrw();
		memcpy(cache_data + (multimesh->motion_vectors_current_offset * multimesh->stride_cache), p_buffer.ptr(), p_buffer.size() * sizeof(float));
		_multimesh_mark_all_dirty(multimesh, true, true); //update AABB
	} else if (multimesh->mesh.is_valid()) {
		//if we have a mesh set, we need to re-generate the AABB from the new data
		const float *data = p_buffer.ptr();

		if (multimesh->custom_aabb == AABB()) {
			_multimesh_re_create_aabb(multimesh, data, multimesh->instances);
			multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
		}
	}
}

Vector<float> MeshStorage::_multimesh_get_buffer(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Vector<float>());
	if (multimesh->buffer.is_null()) {
		return Vector<float>();
	} else {
		Vector<float> ret;
		ret.resize(multimesh->instances * multimesh->stride_cache);
		float *w = ret.ptrw();

		if (multimesh->data_cache.size()) {
			const uint8_t *r = (uint8_t *)multimesh->data_cache.ptr() + multimesh->motion_vectors_current_offset * multimesh->stride_cache * sizeof(float);
			memcpy(w, r, ret.size() * sizeof(float));
		} else {
			Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(multimesh->buffer);
			const uint8_t *r = buffer.ptr() + multimesh->motion_vectors_current_offset * multimesh->stride_cache * sizeof(float);
			memcpy(w, r, ret.size() * sizeof(float));
		}
		return ret;
	}
}

void MeshStorage::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_COND(p_visible < -1 || p_visible > multimesh->instances);
	if (multimesh->visible_instances == p_visible) {
		return;
	}

	if (multimesh->data_cache.size()) {
		// There is a data cache, but we may need to update some sections.
		_multimesh_mark_all_dirty(multimesh, false, true);
		int start = multimesh->visible_instances >= 0 ? multimesh->visible_instances : multimesh->instances;
		for (int i = start; i < p_visible; i++) {
			_multimesh_mark_dirty(multimesh, i, true);
		}
	}

	multimesh->visible_instances = p_visible;

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES);
}

int MeshStorage::_multimesh_get_visible_instances(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, 0);
	return multimesh->visible_instances;
}

void MeshStorage::_multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->custom_aabb = p_aabb;
	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB MeshStorage::_multimesh_get_custom_aabb(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, AABB());
	return multimesh->custom_aabb;
}

AABB MeshStorage::_multimesh_get_aabb(RID p_multimesh) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, AABB());
	if (multimesh->custom_aabb != AABB()) {
		return multimesh->custom_aabb;
	}

	if (multimesh->aabb_dirty) {
		_update_dirty_multimeshes();
	}
	return multimesh->aabb;
}

MeshStorage::MultiMeshInterpolator *MeshStorage::_multimesh_get_interpolator(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V_MSG(multimesh, nullptr, "Multimesh not found: " + itos(p_multimesh.get_id()));

	return &multimesh->interpolator;
}

void MeshStorage::_update_dirty_multimeshes() {
	while (multimesh_dirty_list) {
		MultiMesh *multimesh = multimesh_dirty_list;

		if (multimesh->data_cache.size()) { //may have been cleared, so only process if it exists

			uint32_t visible_instances = multimesh->visible_instances >= 0 ? multimesh->visible_instances : multimesh->instances;
			uint32_t buffer_offset = multimesh->motion_vectors_current_offset * multimesh->stride_cache;
			const float *data = multimesh->data_cache.ptr() + buffer_offset;

			uint32_t total_dirty_regions = multimesh->data_cache_dirty_region_count + multimesh->previous_data_cache_dirty_region_count;
			if (total_dirty_regions != 0) {
				uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, (int)MULTIMESH_DIRTY_REGION_SIZE);
				uint32_t visible_region_count = visible_instances == 0 ? 0 : Math::division_round_up(visible_instances, (uint32_t)MULTIMESH_DIRTY_REGION_SIZE);

				uint32_t region_size = multimesh->stride_cache * MULTIMESH_DIRTY_REGION_SIZE * sizeof(float);
				if (total_dirty_regions > 32 || total_dirty_regions > visible_region_count / 2) {
					//if there too many dirty regions, or represent the majority of regions, just copy all, else transfer cost piles up too much
					RD::get_singleton()->buffer_update(multimesh->buffer, buffer_offset * sizeof(float), MIN(visible_region_count * region_size, multimesh->instances * (uint32_t)multimesh->stride_cache * (uint32_t)sizeof(float)), data);
				} else {
					//not that many regions? update them all
					for (uint32_t i = 0; i < visible_region_count; i++) {
						if (multimesh->data_cache_dirty_regions[i] || multimesh->previous_data_cache_dirty_regions[i]) {
							uint32_t offset = i * region_size;
							uint32_t size = multimesh->stride_cache * (uint32_t)multimesh->instances * (uint32_t)sizeof(float);
							uint32_t region_start_index = multimesh->stride_cache * MULTIMESH_DIRTY_REGION_SIZE * i;
							RD::get_singleton()->buffer_update(multimesh->buffer, buffer_offset * sizeof(float) + offset, MIN(region_size, size - offset), &data[region_start_index]);
						}
					}
				}

				memcpy(multimesh->previous_data_cache_dirty_regions, multimesh->data_cache_dirty_regions, data_cache_dirty_region_count * sizeof(bool));
				memset(multimesh->data_cache_dirty_regions, 0, data_cache_dirty_region_count * sizeof(bool));

				multimesh->previous_data_cache_dirty_region_count = multimesh->data_cache_dirty_region_count;
				multimesh->data_cache_dirty_region_count = 0;
			}

			if (multimesh->aabb_dirty) {
				//aabb is dirty..
				multimesh->aabb_dirty = false;
				if (multimesh->custom_aabb == AABB()) {
					_multimesh_re_create_aabb(multimesh, data, visible_instances);
					multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
				}
			}
		}

		multimesh_dirty_list = multimesh->dirty_list;

		multimesh->dirty_list = nullptr;
		multimesh->dirty = false;
	}

	multimesh_dirty_list = nullptr;
}

/* SKELETON API */

RID MeshStorage::skeleton_allocate() {
	return skeleton_owner.allocate_rid();
}
void MeshStorage::skeleton_initialize(RID p_rid) {
	skeleton_owner.initialize_rid(p_rid, Skeleton());
}

void MeshStorage::skeleton_free(RID p_rid) {
	_update_dirty_skeletons();
	skeleton_allocate_data(p_rid, 0);
	Skeleton *skeleton = skeleton_owner.get_or_null(p_rid);
	skeleton->dependency.deleted_notify(p_rid);
	skeleton_owner.free(p_rid);
}

void MeshStorage::_skeleton_make_dirty(Skeleton *skeleton) {
	if (!skeleton->dirty) {
		skeleton->dirty = true;
		skeleton->dirty_list = skeleton_dirty_list;
		skeleton_dirty_list = skeleton;
	}
}

void MeshStorage::skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_COND(p_bones < 0);

	if (skeleton->size == p_bones && skeleton->use_2d == p_2d_skeleton) {
		return;
	}

	skeleton->size = p_bones;
	skeleton->use_2d = p_2d_skeleton;
	skeleton->uniform_set_3d = RID();

	if (skeleton->buffer.is_valid()) {
		RD::get_singleton()->free(skeleton->buffer);
		skeleton->buffer = RID();
		skeleton->data.clear();
		skeleton->uniform_set_mi = RID();
	}

	if (skeleton->size) {
		skeleton->data.resize(skeleton->size * (skeleton->use_2d ? 8 : 12));
		skeleton->buffer = RD::get_singleton()->storage_buffer_create(skeleton->data.size() * sizeof(float));
		memset(skeleton->data.ptrw(), 0, skeleton->data.size() * sizeof(float));

		_skeleton_make_dirty(skeleton);

		{
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.binding = 0;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.append_id(skeleton->buffer);
				uniforms.push_back(u);
			}
			skeleton->uniform_set_mi = RD::get_singleton()->uniform_set_create(uniforms, skeleton_shader.version_shader[0], SkeletonShader::UNIFORM_SET_SKELETON);
		}
	}

	skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_DATA);
}

int MeshStorage::skeleton_get_bone_count(RID p_skeleton) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL_V(skeleton, 0);

	return skeleton->size;
}

void MeshStorage::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(skeleton->use_2d);

	float *dataptr = skeleton->data.ptrw() + p_bone * 12;

	dataptr[0] = p_transform.basis.rows[0][0];
	dataptr[1] = p_transform.basis.rows[0][1];
	dataptr[2] = p_transform.basis.rows[0][2];
	dataptr[3] = p_transform.origin.x;
	dataptr[4] = p_transform.basis.rows[1][0];
	dataptr[5] = p_transform.basis.rows[1][1];
	dataptr[6] = p_transform.basis.rows[1][2];
	dataptr[7] = p_transform.origin.y;
	dataptr[8] = p_transform.basis.rows[2][0];
	dataptr[9] = p_transform.basis.rows[2][1];
	dataptr[10] = p_transform.basis.rows[2][2];
	dataptr[11] = p_transform.origin.z;

	_skeleton_make_dirty(skeleton);
}

Transform3D MeshStorage::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL_V(skeleton, Transform3D());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform3D());
	ERR_FAIL_COND_V(skeleton->use_2d, Transform3D());

	const float *dataptr = skeleton->data.ptr() + p_bone * 12;

	Transform3D t;

	t.basis.rows[0][0] = dataptr[0];
	t.basis.rows[0][1] = dataptr[1];
	t.basis.rows[0][2] = dataptr[2];
	t.origin.x = dataptr[3];
	t.basis.rows[1][0] = dataptr[4];
	t.basis.rows[1][1] = dataptr[5];
	t.basis.rows[1][2] = dataptr[6];
	t.origin.y = dataptr[7];
	t.basis.rows[2][0] = dataptr[8];
	t.basis.rows[2][1] = dataptr[9];
	t.basis.rows[2][2] = dataptr[10];
	t.origin.z = dataptr[11];

	return t;
}

void MeshStorage::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(!skeleton->use_2d);

	float *dataptr = skeleton->data.ptrw() + p_bone * 8;

	dataptr[0] = p_transform.columns[0][0];
	dataptr[1] = p_transform.columns[1][0];
	dataptr[2] = 0;
	dataptr[3] = p_transform.columns[2][0];
	dataptr[4] = p_transform.columns[0][1];
	dataptr[5] = p_transform.columns[1][1];
	dataptr[6] = 0;
	dataptr[7] = p_transform.columns[2][1];

	_skeleton_make_dirty(skeleton);
}

Transform2D MeshStorage::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL_V(skeleton, Transform2D());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform2D());
	ERR_FAIL_COND_V(!skeleton->use_2d, Transform2D());

	const float *dataptr = skeleton->data.ptr() + p_bone * 8;

	Transform2D t;
	t.columns[0][0] = dataptr[0];
	t.columns[1][0] = dataptr[1];
	t.columns[2][0] = dataptr[3];
	t.columns[0][1] = dataptr[4];
	t.columns[1][1] = dataptr[5];
	t.columns[2][1] = dataptr[7];

	return t;
}

void MeshStorage::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_COND(!skeleton->use_2d);

	skeleton->base_transform_2d = p_base_transform;
}

void MeshStorage::_update_dirty_skeletons() {
	while (skeleton_dirty_list) {
		Skeleton *skeleton = skeleton_dirty_list;

		if (skeleton->size) {
			RD::get_singleton()->buffer_update(skeleton->buffer, 0, skeleton->data.size() * sizeof(float), skeleton->data.ptr());
		}

		skeleton_dirty_list = skeleton->dirty_list;

		skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_BONES);

		skeleton->version++;

		skeleton->dirty = false;
		skeleton->dirty_list = nullptr;
	}

	skeleton_dirty_list = nullptr;
}

void MeshStorage::skeleton_update_dependency(RID p_skeleton, DependencyTracker *p_instance) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL(skeleton);

	p_instance->update_dependency(&skeleton->dependency);
}
