/**************************************************************************/
/*  rasterizer_scene_gles3.cpp                                            */
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

#include "rasterizer_scene_gles3.h"

#include "drivers/gles3/effects/copy_effects.h"
#include "rasterizer_gles3.h"
#include "storage/config.h"
#include "storage/mesh_storage.h"
#include "storage/particles_storage.h"
#include "storage/texture_storage.h"

#include "core/config/project_settings.h"
#include "core/templates/sort_array.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_server_globals.h"

#ifdef GLES3_ENABLED

RasterizerSceneGLES3 *RasterizerSceneGLES3::singleton = nullptr;

RenderGeometryInstance *RasterizerSceneGLES3::geometry_instance_create(RID p_base) {
	RS::InstanceType type = RSG::utilities->get_base_type(p_base);
	ERR_FAIL_COND_V(!((1 << type) & RS::INSTANCE_GEOMETRY_MASK), nullptr);

	GeometryInstanceGLES3 *ginstance = geometry_instance_alloc.alloc();
	ginstance->data = memnew(GeometryInstanceGLES3::Data);

	ginstance->data->base = p_base;
	ginstance->data->base_type = type;
	ginstance->data->dependency_tracker.userdata = ginstance;
	ginstance->data->dependency_tracker.changed_callback = _geometry_instance_dependency_changed;
	ginstance->data->dependency_tracker.deleted_callback = _geometry_instance_dependency_deleted;

	ginstance->_mark_dirty();

	return ginstance;
}

uint32_t RasterizerSceneGLES3::geometry_instance_get_pair_mask() {
	return ((1 << RS::INSTANCE_LIGHT) | (1 << RS::INSTANCE_REFLECTION_PROBE));
}

void RasterizerSceneGLES3::GeometryInstanceGLES3::pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) {
	GLES3::Config *config = GLES3::Config::get_singleton();

	paired_omni_light_count = 0;
	paired_spot_light_count = 0;
	paired_omni_lights.clear();
	paired_spot_lights.clear();

	for (uint32_t i = 0; i < p_light_instance_count; i++) {
		RS::LightType type = GLES3::LightStorage::get_singleton()->light_instance_get_type(p_light_instances[i]);
		switch (type) {
			case RS::LIGHT_OMNI: {
				if (paired_omni_light_count < (uint32_t)config->max_lights_per_object) {
					paired_omni_lights.push_back(p_light_instances[i]);
					paired_omni_light_count++;
				}
			} break;
			case RS::LIGHT_SPOT: {
				if (paired_spot_light_count < (uint32_t)config->max_lights_per_object) {
					paired_spot_lights.push_back(p_light_instances[i]);
					paired_spot_light_count++;
				}
			} break;
			default:
				break;
		}
	}
}

void RasterizerSceneGLES3::GeometryInstanceGLES3::pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) {
	paired_reflection_probes.clear();

	for (uint32_t i = 0; i < p_reflection_probe_instance_count; i++) {
		paired_reflection_probes.push_back(p_reflection_probe_instances[i]);
	}
}

void RasterizerSceneGLES3::geometry_instance_free(RenderGeometryInstance *p_geometry_instance) {
	GeometryInstanceGLES3 *ginstance = static_cast<GeometryInstanceGLES3 *>(p_geometry_instance);
	ERR_FAIL_NULL(ginstance);
	GeometryInstanceSurface *surf = ginstance->surface_caches;
	while (surf) {
		GeometryInstanceSurface *next = surf->next;
		geometry_instance_surface_alloc.free(surf);
		surf = next;
	}
	memdelete(ginstance->data);
	geometry_instance_alloc.free(ginstance);
}

void RasterizerSceneGLES3::GeometryInstanceGLES3::_mark_dirty() {
	if (dirty_list_element.in_list()) {
		return;
	}

	//clear surface caches
	GeometryInstanceSurface *surf = surface_caches;

	while (surf) {
		GeometryInstanceSurface *next = surf->next;
		RasterizerSceneGLES3::get_singleton()->geometry_instance_surface_alloc.free(surf);
		surf = next;
	}

	surface_caches = nullptr;

	RasterizerSceneGLES3::get_singleton()->geometry_instance_dirty_list.add(&dirty_list_element);
}

void RasterizerSceneGLES3::GeometryInstanceGLES3::set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) {
	lightmap_instance = p_lightmap_instance;
	lightmap_uv_scale = p_lightmap_uv_scale;
	lightmap_slice_index = p_lightmap_slice_index;

	_mark_dirty();
}

void RasterizerSceneGLES3::GeometryInstanceGLES3::set_lightmap_capture(const Color *p_sh9) {
	if (p_sh9) {
		if (lightmap_sh == nullptr) {
			lightmap_sh = memnew(GeometryInstanceLightmapSH);
		}

		memcpy(lightmap_sh->sh, p_sh9, sizeof(Color) * 9);
	} else {
		if (lightmap_sh != nullptr) {
			memdelete(lightmap_sh);
			lightmap_sh = nullptr;
		}
	}
	_mark_dirty();
}

void RasterizerSceneGLES3::_update_dirty_geometry_instances() {
	while (geometry_instance_dirty_list.first()) {
		_geometry_instance_update(geometry_instance_dirty_list.first()->self());
	}
}

void RasterizerSceneGLES3::_geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	switch (p_notification) {
		case Dependency::DEPENDENCY_CHANGED_MATERIAL:
		case Dependency::DEPENDENCY_CHANGED_MESH:
		case Dependency::DEPENDENCY_CHANGED_PARTICLES:
		case Dependency::DEPENDENCY_CHANGED_MULTIMESH:
		case Dependency::DEPENDENCY_CHANGED_SKELETON_DATA: {
			static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
			static_cast<GeometryInstanceGLES3 *>(p_tracker->userdata)->data->dirty_dependencies = true;
		} break;
		case Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES: {
			GeometryInstanceGLES3 *ginstance = static_cast<GeometryInstanceGLES3 *>(p_tracker->userdata);
			if (ginstance->data->base_type == RS::INSTANCE_MULTIMESH) {
				ginstance->instance_count = GLES3::MeshStorage::get_singleton()->multimesh_get_instances_to_draw(ginstance->data->base);
			}
		} break;
		default: {
			//rest of notifications of no interest
		} break;
	}
}

void RasterizerSceneGLES3::_geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker) {
	static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
	static_cast<GeometryInstanceGLES3 *>(p_tracker->userdata)->data->dirty_dependencies = true;
}

void RasterizerSceneGLES3::_geometry_instance_add_surface_with_material(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, GLES3::SceneMaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh) {
	GLES3::MeshStorage *mesh_storage = GLES3::MeshStorage::get_singleton();

	bool has_read_screen_alpha = p_material->shader_data->uses_screen_texture || p_material->shader_data->uses_depth_texture || p_material->shader_data->uses_normal_texture;
	bool has_base_alpha = ((p_material->shader_data->uses_alpha && !p_material->shader_data->uses_alpha_clip) || has_read_screen_alpha);
	bool has_blend_alpha = p_material->shader_data->uses_blend_alpha;
	bool has_alpha = has_base_alpha || has_blend_alpha;

	uint32_t flags = 0;

	if (p_material->shader_data->uses_screen_texture) {
		flags |= GeometryInstanceSurface::FLAG_USES_SCREEN_TEXTURE;
	}

	if (p_material->shader_data->uses_depth_texture) {
		flags |= GeometryInstanceSurface::FLAG_USES_DEPTH_TEXTURE;
	}

	if (p_material->shader_data->uses_normal_texture) {
		flags |= GeometryInstanceSurface::FLAG_USES_NORMAL_TEXTURE;
	}

	if (ginstance->data->cast_double_sided_shadows) {
		flags |= GeometryInstanceSurface::FLAG_USES_DOUBLE_SIDED_SHADOWS;
	}

	if (has_alpha || has_read_screen_alpha || p_material->shader_data->depth_draw == GLES3::SceneShaderData::DEPTH_DRAW_DISABLED || p_material->shader_data->depth_test == GLES3::SceneShaderData::DEPTH_TEST_DISABLED) {
		//material is only meant for alpha pass
		flags |= GeometryInstanceSurface::FLAG_PASS_ALPHA;
		if (p_material->shader_data->uses_depth_prepass_alpha && !(p_material->shader_data->depth_draw == GLES3::SceneShaderData::DEPTH_DRAW_DISABLED || p_material->shader_data->depth_test == GLES3::SceneShaderData::DEPTH_TEST_DISABLED)) {
			flags |= GeometryInstanceSurface::FLAG_PASS_DEPTH;
			flags |= GeometryInstanceSurface::FLAG_PASS_SHADOW;
		}
	} else {
		flags |= GeometryInstanceSurface::FLAG_PASS_OPAQUE;
		flags |= GeometryInstanceSurface::FLAG_PASS_DEPTH;
		flags |= GeometryInstanceSurface::FLAG_PASS_SHADOW;
	}

	GLES3::SceneMaterialData *material_shadow = nullptr;
	void *surface_shadow = nullptr;
	if (!p_material->shader_data->uses_particle_trails && !p_material->shader_data->writes_modelview_or_projection && !p_material->shader_data->uses_vertex && !p_material->shader_data->uses_discard && !p_material->shader_data->uses_depth_prepass_alpha && !p_material->shader_data->uses_alpha_clip && !p_material->shader_data->uses_world_coordinates) {
		flags |= GeometryInstanceSurface::FLAG_USES_SHARED_SHADOW_MATERIAL;
		material_shadow = static_cast<GLES3::SceneMaterialData *>(GLES3::MaterialStorage::get_singleton()->material_get_data(scene_globals.default_material, RS::SHADER_SPATIAL));

		RID shadow_mesh = mesh_storage->mesh_get_shadow_mesh(p_mesh);

		if (shadow_mesh.is_valid()) {
			surface_shadow = mesh_storage->mesh_get_surface(shadow_mesh, p_surface);
		}

	} else {
		material_shadow = p_material;
	}

	GeometryInstanceSurface *sdcache = geometry_instance_surface_alloc.alloc();

	sdcache->flags = flags;

	sdcache->shader = p_material->shader_data;
	sdcache->material = p_material;
	sdcache->surface = mesh_storage->mesh_get_surface(p_mesh, p_surface);
	sdcache->primitive = mesh_storage->mesh_surface_get_primitive(sdcache->surface);
	sdcache->surface_index = p_surface;

	if (ginstance->data->dirty_dependencies) {
		RSG::utilities->base_update_dependency(p_mesh, &ginstance->data->dependency_tracker);
	}

	//shadow
	sdcache->shader_shadow = material_shadow->shader_data;
	sdcache->material_shadow = material_shadow;

	sdcache->surface_shadow = surface_shadow ? surface_shadow : sdcache->surface;

	sdcache->owner = ginstance;

	sdcache->next = ginstance->surface_caches;
	ginstance->surface_caches = sdcache;

	//sortkey

	sdcache->sort.sort_key1 = 0;
	sdcache->sort.sort_key2 = 0;

	sdcache->sort.surface_index = p_surface;
	sdcache->sort.material_id_low = p_material_id & 0x0000FFFF;
	sdcache->sort.material_id_hi = p_material_id >> 16;
	sdcache->sort.shader_id = p_shader_id;
	sdcache->sort.geometry_id = p_mesh.get_local_index();
	sdcache->sort.priority = p_material->priority;

	GLES3::Mesh::Surface *s = reinterpret_cast<GLES3::Mesh::Surface *>(sdcache->surface);
	if (p_material->shader_data->uses_tangent && !(s->format & RS::ARRAY_FORMAT_TANGENT)) {
		String shader_path = p_material->shader_data->path.is_empty() ? "" : "(" + p_material->shader_data->path + ")";
		String mesh_path = mesh_storage->mesh_get_path(p_mesh).is_empty() ? "" : "(" + mesh_storage->mesh_get_path(p_mesh) + ")";
		WARN_PRINT_ED(vformat("Attempting to use a shader %s that requires tangents with a mesh %s that doesn't contain tangents. Ensure that meshes are imported with the 'ensure_tangents' option. If creating your own meshes, add an `ARRAY_TANGENT` array (when using ArrayMesh) or call `generate_tangents()` (when using SurfaceTool).", shader_path, mesh_path));
	}
}

void RasterizerSceneGLES3::_geometry_instance_add_surface_with_material_chain(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, GLES3::SceneMaterialData *p_material_data, RID p_mat_src, RID p_mesh) {
	GLES3::SceneMaterialData *material_data = p_material_data;
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();

	_geometry_instance_add_surface_with_material(ginstance, p_surface, material_data, p_mat_src.get_local_index(), material_storage->material_get_shader_id(p_mat_src), p_mesh);

	while (material_data->next_pass.is_valid()) {
		RID next_pass = material_data->next_pass;
		material_data = static_cast<GLES3::SceneMaterialData *>(material_storage->material_get_data(next_pass, RS::SHADER_SPATIAL));
		if (!material_data || !material_data->shader_data->valid) {
			break;
		}
		if (ginstance->data->dirty_dependencies) {
			material_storage->material_update_dependency(next_pass, &ginstance->data->dependency_tracker);
		}
		_geometry_instance_add_surface_with_material(ginstance, p_surface, material_data, next_pass.get_local_index(), material_storage->material_get_shader_id(next_pass), p_mesh);
	}
}

void RasterizerSceneGLES3::_geometry_instance_add_surface(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, RID p_material, RID p_mesh) {
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	RID m_src;

	m_src = ginstance->data->material_override.is_valid() ? ginstance->data->material_override : p_material;

	GLES3::SceneMaterialData *material_data = nullptr;

	if (m_src.is_valid()) {
		material_data = static_cast<GLES3::SceneMaterialData *>(material_storage->material_get_data(m_src, RS::SHADER_SPATIAL));
		if (!material_data || !material_data->shader_data->valid) {
			material_data = nullptr;
		}
	}

	if (material_data) {
		if (ginstance->data->dirty_dependencies) {
			material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
		}
	} else {
		material_data = static_cast<GLES3::SceneMaterialData *>(material_storage->material_get_data(scene_globals.default_material, RS::SHADER_SPATIAL));
		m_src = scene_globals.default_material;
	}

	ERR_FAIL_NULL(material_data);

	_geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material_data, m_src, p_mesh);

	if (ginstance->data->material_overlay.is_valid()) {
		m_src = ginstance->data->material_overlay;

		material_data = static_cast<GLES3::SceneMaterialData *>(material_storage->material_get_data(m_src, RS::SHADER_SPATIAL));
		if (material_data && material_data->shader_data->valid) {
			if (ginstance->data->dirty_dependencies) {
				material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
			}

			_geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material_data, m_src, p_mesh);
		}
	}
}

void RasterizerSceneGLES3::_geometry_instance_update(RenderGeometryInstance *p_geometry_instance) {
	GLES3::MeshStorage *mesh_storage = GLES3::MeshStorage::get_singleton();
	GLES3::ParticlesStorage *particles_storage = GLES3::ParticlesStorage::get_singleton();

	GeometryInstanceGLES3 *ginstance = static_cast<GeometryInstanceGLES3 *>(p_geometry_instance);

	if (ginstance->data->dirty_dependencies) {
		ginstance->data->dependency_tracker.update_begin();
	}

	//add geometry for drawing
	switch (ginstance->data->base_type) {
		case RS::INSTANCE_MESH: {
			const RID *materials = nullptr;
			uint32_t surface_count;
			RID mesh = ginstance->data->base;

			materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
			if (materials) {
				//if no materials, no surfaces.
				const RID *inst_materials = ginstance->data->surface_materials.ptr();
				uint32_t surf_mat_count = ginstance->data->surface_materials.size();

				for (uint32_t j = 0; j < surface_count; j++) {
					RID material = (j < surf_mat_count && inst_materials[j].is_valid()) ? inst_materials[j] : materials[j];
					_geometry_instance_add_surface(ginstance, j, material, mesh);
				}
			}

			ginstance->instance_count = -1;

		} break;

		case RS::INSTANCE_MULTIMESH: {
			RID mesh = mesh_storage->multimesh_get_mesh(ginstance->data->base);
			if (mesh.is_valid()) {
				const RID *materials = nullptr;
				uint32_t surface_count;

				materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
				if (materials) {
					for (uint32_t j = 0; j < surface_count; j++) {
						_geometry_instance_add_surface(ginstance, j, materials[j], mesh);
					}
				}

				ginstance->instance_count = mesh_storage->multimesh_get_instances_to_draw(ginstance->data->base);
			}

		} break;
		case RS::INSTANCE_PARTICLES: {
			int draw_passes = particles_storage->particles_get_draw_passes(ginstance->data->base);

			for (int j = 0; j < draw_passes; j++) {
				RID mesh = particles_storage->particles_get_draw_pass_mesh(ginstance->data->base, j);
				if (!mesh.is_valid()) {
					continue;
				}

				const RID *materials = nullptr;
				uint32_t surface_count;

				materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
				if (materials) {
					for (uint32_t k = 0; k < surface_count; k++) {
						_geometry_instance_add_surface(ginstance, k, materials[k], mesh);
					}
				}
			}

			ginstance->instance_count = particles_storage->particles_get_amount(ginstance->data->base);
		} break;

		default: {
		}
	}

	bool store_transform = true;
	ginstance->base_flags = 0;

	if (ginstance->data->base_type == RS::INSTANCE_MULTIMESH) {
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH;
		if (mesh_storage->multimesh_get_transform_format(ginstance->data->base) == RS::MULTIMESH_TRANSFORM_2D) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D;
		}
		if (mesh_storage->multimesh_uses_colors(ginstance->data->base)) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR;
		}
		if (mesh_storage->multimesh_uses_custom_data(ginstance->data->base)) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA;
		}

	} else if (ginstance->data->base_type == RS::INSTANCE_PARTICLES) {
		ginstance->base_flags |= INSTANCE_DATA_FLAG_PARTICLES;
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH;

		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR;
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA;

		if (!particles_storage->particles_is_using_local_coords(ginstance->data->base)) {
			store_transform = false;
		}

	} else if (ginstance->data->base_type == RS::INSTANCE_MESH) {
		if (mesh_storage->skeleton_is_valid(ginstance->data->skeleton)) {
			if (ginstance->data->dirty_dependencies) {
				mesh_storage->skeleton_update_dependency(ginstance->data->skeleton, &ginstance->data->dependency_tracker);
			}
		}
	}

	ginstance->store_transform_cache = store_transform;

	if (ginstance->data->dirty_dependencies) {
		ginstance->data->dependency_tracker.update_end();
		ginstance->data->dirty_dependencies = false;
	}

	ginstance->dirty_list_element.remove_from_list();
}

/* SKY API */

void RasterizerSceneGLES3::_free_sky_data(Sky *p_sky) {
	if (p_sky->radiance != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(p_sky->radiance);
		p_sky->radiance = 0;
		GLES3::Utilities::get_singleton()->texture_free_data(p_sky->raw_radiance);
		p_sky->raw_radiance = 0;
		glDeleteFramebuffers(1, &p_sky->radiance_framebuffer);
		p_sky->radiance_framebuffer = 0;
	}
}

RID RasterizerSceneGLES3::sky_allocate() {
	return sky_owner.allocate_rid();
}

void RasterizerSceneGLES3::sky_initialize(RID p_rid) {
	sky_owner.initialize_rid(p_rid);
}

void RasterizerSceneGLES3::sky_set_radiance_size(RID p_sky, int p_radiance_size) {
	Sky *sky = sky_owner.get_or_null(p_sky);
	ERR_FAIL_NULL(sky);
	ERR_FAIL_COND_MSG(p_radiance_size < 32 || p_radiance_size > 2048, "Sky radiance size must be between 32 and 2048");

	if (sky->radiance_size == p_radiance_size) {
		return; // No need to update
	}

	sky->radiance_size = p_radiance_size;

	_free_sky_data(sky);
	_invalidate_sky(sky);
}

void RasterizerSceneGLES3::sky_set_mode(RID p_sky, RS::SkyMode p_mode) {
	Sky *sky = sky_owner.get_or_null(p_sky);
	ERR_FAIL_NULL(sky);

	if (sky->mode == p_mode) {
		return;
	}

	sky->mode = p_mode;
	_invalidate_sky(sky);
}

void RasterizerSceneGLES3::sky_set_material(RID p_sky, RID p_material) {
	Sky *sky = sky_owner.get_or_null(p_sky);
	ERR_FAIL_NULL(sky);

	if (sky->material == p_material) {
		return;
	}

	sky->material = p_material;
	_invalidate_sky(sky);
}

float RasterizerSceneGLES3::sky_get_baked_exposure(RID p_sky) const {
	Sky *sky = sky_owner.get_or_null(p_sky);
	ERR_FAIL_NULL_V(sky, 1.0);

	return sky->baked_exposure;
}

void RasterizerSceneGLES3::_invalidate_sky(Sky *p_sky) {
	if (!p_sky->dirty) {
		p_sky->dirty = true;
		p_sky->dirty_list = dirty_sky_list;
		dirty_sky_list = p_sky;
	}
}

GLuint _init_radiance_texture(int p_size, int p_mipmaps, String p_name) {
	GLuint radiance_id = 0;

	glGenTextures(1, &radiance_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, radiance_id);
#ifdef GL_API_ENABLED
	if (RasterizerGLES3::is_gles_over_gl()) {
		//TODO, on low-end compare this to allocating each face of each mip individually
		// see: https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glTexStorage2D.xhtml
		for (int i = 0; i < 6; i++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB10_A2, p_size, p_size, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
		}

		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}
#endif // GL_API_ENABLED
#ifdef GLES_API_ENABLED
	if (!RasterizerGLES3::is_gles_over_gl()) {
		glTexStorage2D(GL_TEXTURE_CUBE_MAP, p_mipmaps, GL_RGB10_A2, p_size, p_size);
	}
#endif // GLES_API_ENABLED
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, p_mipmaps - 1);

	GLES3::Utilities::get_singleton()->texture_allocated_data(radiance_id, Image::get_image_data_size(p_size, p_size, Image::FORMAT_RGBA8, true), p_name);
	return radiance_id;
}

void RasterizerSceneGLES3::_update_dirty_skys() {
	Sky *sky = dirty_sky_list;

	while (sky) {
		if (sky->radiance == 0) {
			sky->mipmap_count = Image::get_image_required_mipmaps(sky->radiance_size, sky->radiance_size, Image::FORMAT_RGBA8) - 1;
			// Left uninitialized, will attach a texture at render time
			glGenFramebuffers(1, &sky->radiance_framebuffer);
			sky->radiance = _init_radiance_texture(sky->radiance_size, sky->mipmap_count, "Sky radiance texture");
			sky->raw_radiance = _init_radiance_texture(sky->radiance_size, sky->mipmap_count, "Sky raw radiance texture");
		}

		sky->reflection_dirty = true;
		sky->processing_layer = 0;

		Sky *next = sky->dirty_list;
		sky->dirty_list = nullptr;
		sky->dirty = false;
		sky = next;
	}

	dirty_sky_list = nullptr;
}

void RasterizerSceneGLES3::_setup_sky(const RenderDataGLES3 *p_render_data, const PagedArray<RID> &p_lights, const Projection &p_projection, const Transform3D &p_transform, const Size2i p_screen_size) {
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	ERR_FAIL_COND(p_render_data->environment.is_null());

	GLES3::SkyMaterialData *material = nullptr;
	Sky *sky = sky_owner.get_or_null(environment_get_sky(p_render_data->environment));

	RID sky_material;

	GLES3::SkyShaderData *shader_data = nullptr;

	if (sky) {
		sky_material = sky->material;

		if (sky_material.is_valid()) {
			material = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
			if (!material || !material->shader_data->valid) {
				material = nullptr;
			}
		}
	}

	if (!material) {
		sky_material = sky_globals.default_material;
		material = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
	}

	ERR_FAIL_NULL(material);

	shader_data = material->shader_data;

	ERR_FAIL_NULL(shader_data);

	if (sky) {
		if (shader_data->uses_time && time - sky->prev_time > 0.00001) {
			sky->prev_time = time;
			sky->reflection_dirty = true;
			RenderingServerDefault::redraw_request();
		}

		if (material != sky->prev_material) {
			sky->prev_material = material;
			sky->reflection_dirty = true;
		}

		if (material->uniform_set_updated) {
			material->uniform_set_updated = false;
			sky->reflection_dirty = true;
		}

		if (!p_transform.origin.is_equal_approx(sky->prev_position) && shader_data->uses_position) {
			sky->prev_position = p_transform.origin;
			sky->reflection_dirty = true;
		}
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, SKY_DIRECTIONAL_LIGHT_UNIFORM_LOCATION, sky_globals.directional_light_buffer);
	if (shader_data->uses_light) {
		sky_globals.directional_light_count = 0;
		for (int i = 0; i < (int)p_lights.size(); i++) {
			GLES3::LightInstance *li = GLES3::LightStorage::get_singleton()->get_light_instance(p_lights[i]);
			if (!li) {
				continue;
			}
			RID base = li->light;

			ERR_CONTINUE(base.is_null());

			RS::LightType type = light_storage->light_get_type(base);
			if (type == RS::LIGHT_DIRECTIONAL && light_storage->light_directional_get_sky_mode(base) != RS::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_ONLY) {
				DirectionalLightData &sky_light_data = sky_globals.directional_lights[sky_globals.directional_light_count];
				Transform3D light_transform = li->transform;
				Vector3 world_direction = light_transform.basis.xform(Vector3(0, 0, 1)).normalized();

				sky_light_data.direction[0] = world_direction.x;
				sky_light_data.direction[1] = world_direction.y;
				sky_light_data.direction[2] = world_direction.z;

				float sign = light_storage->light_is_negative(base) ? -1 : 1;
				sky_light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

				if (is_using_physical_light_units()) {
					sky_light_data.energy *= light_storage->light_get_param(base, RS::LIGHT_PARAM_INTENSITY);
				}

				if (p_render_data->camera_attributes.is_valid()) {
					sky_light_data.energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
				}

				Color linear_col = light_storage->light_get_color(base);
				sky_light_data.color[0] = linear_col.r;
				sky_light_data.color[1] = linear_col.g;
				sky_light_data.color[2] = linear_col.b;

				sky_light_data.enabled = true;

				float angular_diameter = light_storage->light_get_param(base, RS::LIGHT_PARAM_SIZE);
				if (angular_diameter > 0.0) {
					angular_diameter = Math::tan(Math::deg_to_rad(angular_diameter));
				} else {
					angular_diameter = 0.0;
				}
				sky_light_data.size = angular_diameter;
				sky_globals.directional_light_count++;
				if (sky_globals.directional_light_count >= sky_globals.max_directional_lights) {
					break;
				}
			}
		}
		// Check whether the directional_light_buffer changes
		bool light_data_dirty = false;

		// Light buffer is dirty if we have fewer or more lights
		// If we have fewer lights, make sure that old lights are disabled
		if (sky_globals.directional_light_count != sky_globals.last_frame_directional_light_count) {
			light_data_dirty = true;
			for (uint32_t i = sky_globals.directional_light_count; i < sky_globals.max_directional_lights; i++) {
				sky_globals.directional_lights[i].enabled = false;
				sky_globals.last_frame_directional_lights[i].enabled = false;
			}
		}

		if (!light_data_dirty) {
			for (uint32_t i = 0; i < sky_globals.directional_light_count; i++) {
				if (sky_globals.directional_lights[i].direction[0] != sky_globals.last_frame_directional_lights[i].direction[0] ||
						sky_globals.directional_lights[i].direction[1] != sky_globals.last_frame_directional_lights[i].direction[1] ||
						sky_globals.directional_lights[i].direction[2] != sky_globals.last_frame_directional_lights[i].direction[2] ||
						sky_globals.directional_lights[i].energy != sky_globals.last_frame_directional_lights[i].energy ||
						sky_globals.directional_lights[i].color[0] != sky_globals.last_frame_directional_lights[i].color[0] ||
						sky_globals.directional_lights[i].color[1] != sky_globals.last_frame_directional_lights[i].color[1] ||
						sky_globals.directional_lights[i].color[2] != sky_globals.last_frame_directional_lights[i].color[2] ||
						sky_globals.directional_lights[i].enabled != sky_globals.last_frame_directional_lights[i].enabled ||
						sky_globals.directional_lights[i].size != sky_globals.last_frame_directional_lights[i].size) {
					light_data_dirty = true;
					break;
				}
			}
		}

		if (light_data_dirty) {
			glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLightData) * sky_globals.max_directional_lights, sky_globals.directional_lights, GL_STREAM_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			DirectionalLightData *temp = sky_globals.last_frame_directional_lights;
			sky_globals.last_frame_directional_lights = sky_globals.directional_lights;
			sky_globals.directional_lights = temp;
			sky_globals.last_frame_directional_light_count = sky_globals.directional_light_count;
			if (sky) {
				sky->reflection_dirty = true;
			}
		}
	}

	if (p_render_data->view_count > 1) {
		glBindBufferBase(GL_UNIFORM_BUFFER, SKY_MULTIVIEW_UNIFORM_LOCATION, scene_state.multiview_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	if (sky && !sky->radiance) {
		_invalidate_sky(sky);
		_update_dirty_skys();
	}
}

void RasterizerSceneGLES3::_draw_sky(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier, float p_luminance_multiplier, bool p_use_multiview, bool p_flip_y, bool p_apply_color_adjustments_in_post) {
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	ERR_FAIL_COND(p_env.is_null());

	Sky *sky = sky_owner.get_or_null(environment_get_sky(p_env));

	GLES3::SkyMaterialData *material_data = nullptr;
	RID sky_material;

	uint64_t spec_constants = p_use_multiview ? SkyShaderGLES3::USE_MULTIVIEW : 0;
	if (p_flip_y) {
		spec_constants |= SkyShaderGLES3::USE_INVERTED_Y;
	}
	if (!p_apply_color_adjustments_in_post) {
		spec_constants |= SkyShaderGLES3::APPLY_TONEMAPPING;
	}

	RS::EnvironmentBG background = environment_get_background(p_env);

	if (sky) {
		sky_material = sky->material;

		if (sky_material.is_valid()) {
			material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
			if (!material_data || !material_data->shader_data->valid) {
				material_data = nullptr;
			}
		}

		if (!material_data) {
			sky_material = sky_globals.default_material;
			material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
		}
	} else if (background == RS::ENV_BG_CLEAR_COLOR || background == RS::ENV_BG_COLOR) {
		sky_material = sky_globals.fog_material;
		material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
	}

	ERR_FAIL_NULL(material_data);
	material_data->bind_uniforms();

	GLES3::SkyShaderData *shader_data = material_data->shader_data;

	ERR_FAIL_NULL(shader_data);

	// Camera
	Projection camera;

	if (environment_get_sky_custom_fov(p_env)) {
		float near_plane = p_projection.get_z_near();
		float far_plane = p_projection.get_z_far();
		float aspect = p_projection.get_aspect();

		camera.set_perspective(environment_get_sky_custom_fov(p_env), aspect, near_plane, far_plane);
	} else {
		camera = p_projection;
	}

	Projection correction;
	correction.set_depth_correction(false, true, false);
	camera = correction * camera;

	Basis sky_transform = environment_get_sky_orientation(p_env);
	sky_transform.invert();
	sky_transform = sky_transform * p_transform.basis;

	bool success = material_storage->shaders.sky_shader.version_bind_shader(shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	if (!success) {
		return;
	}

	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::ORIENTATION, sky_transform, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::PROJECTION, camera.columns[2][0], camera.columns[0][0], camera.columns[2][1], camera.columns[1][1], shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::POSITION, p_transform.origin, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::TIME, time, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::SKY_ENERGY_MULTIPLIER, p_sky_energy_multiplier, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::LUMINANCE_MULTIPLIER, p_luminance_multiplier, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);

	Color fog_color = environment_get_fog_light_color(p_env).srgb_to_linear() * environment_get_fog_light_energy(p_env);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_ENABLED, environment_get_fog_enabled(p_env), shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_AERIAL_PERSPECTIVE, environment_get_fog_aerial_perspective(p_env), shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_LIGHT_COLOR, fog_color, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_SUN_SCATTER, environment_get_fog_sun_scatter(p_env), shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_DENSITY, environment_get_fog_density(p_env), shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::FOG_SKY_AFFECT, environment_get_fog_sky_affect(p_env), shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);
	material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::DIRECTIONAL_LIGHT_COUNT, sky_globals.directional_light_count, shader_data->version, SkyShaderGLES3::MODE_BACKGROUND, spec_constants);

	if (p_use_multiview) {
		glBindBufferBase(GL_UNIFORM_BUFFER, SKY_MULTIVIEW_UNIFORM_LOCATION, scene_state.multiview_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glBindVertexArray(sky_globals.screen_triangle_array);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void RasterizerSceneGLES3::_update_sky_radiance(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier) {
	GLES3::CubemapFilter *cubemap_filter = GLES3::CubemapFilter::get_singleton();
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	ERR_FAIL_COND(p_env.is_null());

	Sky *sky = sky_owner.get_or_null(environment_get_sky(p_env));
	ERR_FAIL_NULL(sky);

	GLES3::SkyMaterialData *material_data = nullptr;
	RID sky_material;

	RS::EnvironmentBG background = environment_get_background(p_env);

	if (sky) {
		ERR_FAIL_NULL(sky);
		sky_material = sky->material;

		if (sky_material.is_valid()) {
			material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
			if (!material_data || !material_data->shader_data->valid) {
				material_data = nullptr;
			}
		}

		if (!material_data) {
			sky_material = sky_globals.default_material;
			material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
		}
	} else if (background == RS::ENV_BG_CLEAR_COLOR || background == RS::ENV_BG_COLOR) {
		sky_material = sky_globals.fog_material;
		material_data = static_cast<GLES3::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
	}

	ERR_FAIL_NULL(material_data);
	material_data->bind_uniforms();

	GLES3::SkyShaderData *shader_data = material_data->shader_data;

	ERR_FAIL_NULL(shader_data);

	bool update_single_frame = sky->mode == RS::SKY_MODE_REALTIME || sky->mode == RS::SKY_MODE_QUALITY;
	RS::SkyMode sky_mode = sky->mode;

	if (sky_mode == RS::SKY_MODE_AUTOMATIC) {
		if ((shader_data->uses_time || shader_data->uses_position) && sky->radiance_size == 256) {
			update_single_frame = true;
			sky_mode = RS::SKY_MODE_REALTIME;
		} else if (shader_data->uses_light || shader_data->ubo_size > 0) {
			update_single_frame = false;
			sky_mode = RS::SKY_MODE_INCREMENTAL;
		} else {
			update_single_frame = true;
			sky_mode = RS::SKY_MODE_QUALITY;
		}
	}

	if (sky->processing_layer == 0 && sky_mode == RS::SKY_MODE_INCREMENTAL) {
		// On the first frame after creating sky, rebuild in single frame
		update_single_frame = true;
		sky_mode = RS::SKY_MODE_QUALITY;
	}

	int max_processing_layer = sky->mipmap_count;

	// Update radiance cubemap
	if (sky->reflection_dirty && (sky->processing_layer >= max_processing_layer || update_single_frame)) {
		static const Vector3 view_normals[6] = {
			Vector3(+1, 0, 0),
			Vector3(-1, 0, 0),
			Vector3(0, +1, 0),
			Vector3(0, -1, 0),
			Vector3(0, 0, +1),
			Vector3(0, 0, -1)
		};
		static const Vector3 view_up[6] = {
			Vector3(0, -1, 0),
			Vector3(0, -1, 0),
			Vector3(0, 0, +1),
			Vector3(0, 0, -1),
			Vector3(0, -1, 0),
			Vector3(0, -1, 0)
		};

		Projection cm;
		cm.set_perspective(90, 1, 0.01, 10.0);
		Projection correction;
		correction.set_depth_correction(true, true, false);
		cm = correction * cm;

		bool success = material_storage->shaders.sky_shader.version_bind_shader(shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
		if (!success) {
			return;
		}

		material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::POSITION, p_transform.origin, shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
		material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::TIME, time, shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
		material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::PROJECTION, cm.columns[2][0], cm.columns[0][0], cm.columns[2][1], cm.columns[1][1], shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
		material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::SKY_ENERGY_MULTIPLIER, p_sky_energy_multiplier, shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
		material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::LUMINANCE_MULTIPLIER, 1.0, shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);

		glBindVertexArray(sky_globals.screen_triangle_array);

		glViewport(0, 0, sky->radiance_size, sky->radiance_size);
		glBindFramebuffer(GL_FRAMEBUFFER, sky->radiance_framebuffer);

		scene_state.reset_gl_state();
		scene_state.set_gl_cull_mode(GLES3::SceneShaderData::CULL_DISABLED);
		scene_state.enable_gl_blend(false);

		for (int i = 0; i < 6; i++) {
			Basis local_view = Basis::looking_at(view_normals[i], view_up[i]);
			material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES3::ORIENTATION, local_view, shader_data->version, SkyShaderGLES3::MODE_CUBEMAP);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, sky->raw_radiance, 0);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		if (update_single_frame) {
			for (int i = 0; i < max_processing_layer; i++) {
				cubemap_filter->filter_radiance(sky->raw_radiance, sky->radiance, sky->radiance_framebuffer, sky->radiance_size, sky->mipmap_count, i);
			}
		} else {
			cubemap_filter->filter_radiance(sky->raw_radiance, sky->radiance, sky->radiance_framebuffer, sky->radiance_size, sky->mipmap_count, 0); // Just copy over the first mipmap.
		}
		sky->processing_layer = 1;
		sky->baked_exposure = p_sky_energy_multiplier;
		sky->reflection_dirty = false;
	} else {
		if (sky_mode == RS::SKY_MODE_INCREMENTAL && sky->processing_layer < max_processing_layer) {
			scene_state.reset_gl_state();
			scene_state.set_gl_cull_mode(GLES3::SceneShaderData::CULL_DISABLED);
			scene_state.enable_gl_blend(false);

			cubemap_filter->filter_radiance(sky->raw_radiance, sky->radiance, sky->radiance_framebuffer, sky->radiance_size, sky->mipmap_count, sky->processing_layer);
			sky->processing_layer++;
		}
	}
	glViewport(0, 0, sky->screen_size.x, sky->screen_size.y);
}

Ref<Image> RasterizerSceneGLES3::sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) {
	Sky *sky = sky_owner.get_or_null(p_sky);
	ERR_FAIL_NULL_V(sky, Ref<Image>());

	_update_dirty_skys();

	if (sky->radiance == 0) {
		return Ref<Image>();
	}

	GLES3::CopyEffects *copy_effects = GLES3::CopyEffects::get_singleton();
	GLES3::Config *config = GLES3::Config::get_singleton();

	GLuint rad_tex = 0;
	glGenTextures(1, &rad_tex);
	glBindTexture(GL_TEXTURE_2D, rad_tex);
	if (config->float_texture_supported) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, p_size.width, p_size.height, 0, GL_RGBA, GL_FLOAT, nullptr);
		GLES3::Utilities::get_singleton()->texture_allocated_data(rad_tex, p_size.width * p_size.height * 16, "Temp sky panorama");
	} else {
		// Fallback to RGBA8 on devices that don't support rendering to floating point textures. This will look bad, but we have no choice.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, p_size.width, p_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		GLES3::Utilities::get_singleton()->texture_allocated_data(rad_tex, p_size.width * p_size.height * 4, "Temp sky panorama");
	}

	GLuint rad_fbo = 0;
	glGenFramebuffers(1, &rad_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, rad_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rad_tex, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, sky->radiance);
	glViewport(0, 0, p_size.width, p_size.height);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	copy_effects->copy_cube_to_panorama(p_bake_irradiance ? float(sky->mipmap_count) : 0.0);

	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	glDeleteFramebuffers(1, &rad_fbo);
	// Create a dummy texture so we can use texture_2d_get.
	RID tex_rid = GLES3::TextureStorage::get_singleton()->texture_allocate();
	GLES3::Texture texture;
	texture.width = p_size.width;
	texture.height = p_size.height;
	texture.alloc_width = p_size.width;
	texture.alloc_height = p_size.height;
	texture.format = Image::FORMAT_RGBAF;
	texture.real_format = Image::FORMAT_RGBAF;
	texture.gl_format_cache = GL_RGBA;
	texture.gl_type_cache = GL_FLOAT;
	texture.type = GLES3::Texture::TYPE_2D;
	texture.target = GL_TEXTURE_2D;
	texture.active = true;
	texture.tex_id = rad_tex;
	texture.is_render_target = true;

	GLES3::TextureStorage::get_singleton()->texture_2d_initialize_from_texture(tex_rid, texture);
	Ref<Image> img = GLES3::TextureStorage::get_singleton()->texture_2d_get(tex_rid);
	GLES3::Utilities::get_singleton()->texture_free_data(rad_tex);

	texture.is_render_target = false;
	texture.tex_id = 0;
	GLES3::TextureStorage::get_singleton()->texture_free(tex_rid);

	for (int i = 0; i < p_size.width; i++) {
		for (int j = 0; j < p_size.height; j++) {
			Color c = img->get_pixel(i, j);
			c.r *= p_energy;
			c.g *= p_energy;
			c.b *= p_energy;
			img->set_pixel(i, j, c);
		}
	}
	return img;
}

/* ENVIRONMENT API */

void RasterizerSceneGLES3::environment_glow_set_use_bicubic_upscale(bool p_enable) {
	glow_bicubic_upscale = p_enable;
}

void RasterizerSceneGLES3::environment_set_ssr_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality) {
}

void RasterizerSceneGLES3::environment_set_ssao_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) {
}

void RasterizerSceneGLES3::environment_set_ssil_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) {
}

void RasterizerSceneGLES3::environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) {
}

void RasterizerSceneGLES3::environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) {
}

void RasterizerSceneGLES3::environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) {
}

void RasterizerSceneGLES3::environment_set_volumetric_fog_volume_size(int p_size, int p_depth) {
}

void RasterizerSceneGLES3::environment_set_volumetric_fog_filter_active(bool p_enable) {
}

Ref<Image> RasterizerSceneGLES3::environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) {
	ERR_FAIL_COND_V(p_env.is_null(), Ref<Image>());

	RS::EnvironmentBG environment_background = environment_get_background(p_env);

	if (environment_background == RS::ENV_BG_CAMERA_FEED || environment_background == RS::ENV_BG_CANVAS || environment_background == RS::ENV_BG_KEEP) {
		return Ref<Image>(); // Nothing to bake.
	}

	RS::EnvironmentAmbientSource ambient_source = environment_get_ambient_source(p_env);

	bool use_ambient_light = false;
	bool use_cube_map = false;
	if (ambient_source == RS::ENV_AMBIENT_SOURCE_BG && (environment_background == RS::ENV_BG_CLEAR_COLOR || environment_background == RS::ENV_BG_COLOR)) {
		use_ambient_light = true;
	} else {
		use_cube_map = (ambient_source == RS::ENV_AMBIENT_SOURCE_BG && environment_background == RS::ENV_BG_SKY) || ambient_source == RS::ENV_AMBIENT_SOURCE_SKY;
		use_ambient_light = use_cube_map || ambient_source == RS::ENV_AMBIENT_SOURCE_COLOR;
	}

	use_cube_map = use_cube_map || (environment_background == RS::ENV_BG_SKY && environment_get_sky(p_env).is_valid());

	Color ambient_color;
	float ambient_color_sky_mix = 0.0;
	if (use_ambient_light) {
		ambient_color_sky_mix = environment_get_ambient_sky_contribution(p_env);
		const float ambient_energy = environment_get_ambient_light_energy(p_env);
		ambient_color = environment_get_ambient_light(p_env);
		ambient_color = ambient_color.srgb_to_linear();
		ambient_color.r *= ambient_energy;
		ambient_color.g *= ambient_energy;
		ambient_color.b *= ambient_energy;
	}

	if (use_cube_map) {
		Ref<Image> panorama = sky_bake_panorama(environment_get_sky(p_env), environment_get_bg_energy_multiplier(p_env), p_bake_irradiance, p_size);
		if (use_ambient_light) {
			for (int x = 0; x < p_size.width; x++) {
				for (int y = 0; y < p_size.height; y++) {
					panorama->set_pixel(x, y, ambient_color.lerp(panorama->get_pixel(x, y), ambient_color_sky_mix));
				}
			}
		}
		return panorama;
	} else {
		const float bg_energy_multiplier = environment_get_bg_energy_multiplier(p_env);
		Color panorama_color = ((environment_background == RS::ENV_BG_CLEAR_COLOR) ? RSG::texture_storage->get_default_clear_color() : environment_get_bg_color(p_env));
		panorama_color = panorama_color.srgb_to_linear();
		panorama_color.r *= bg_energy_multiplier;
		panorama_color.g *= bg_energy_multiplier;
		panorama_color.b *= bg_energy_multiplier;

		if (use_ambient_light) {
			panorama_color = ambient_color.lerp(panorama_color, ambient_color_sky_mix);
		}

		Ref<Image> panorama = Image::create_empty(p_size.width, p_size.height, false, Image::FORMAT_RGBAF);
		panorama->fill(panorama_color);
		return panorama;
	}
}

void RasterizerSceneGLES3::positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) {
	scene_state.positional_shadow_quality = p_quality;
}

void RasterizerSceneGLES3::directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) {
	scene_state.directional_shadow_quality = p_quality;
}

RID RasterizerSceneGLES3::fog_volume_instance_create(RID p_fog_volume) {
	return RID();
}

void RasterizerSceneGLES3::fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) {
}

void RasterizerSceneGLES3::fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) {
}

RID RasterizerSceneGLES3::fog_volume_instance_get_volume(RID p_fog_volume_instance) const {
	return RID();
}

Vector3 RasterizerSceneGLES3::fog_volume_instance_get_position(RID p_fog_volume_instance) const {
	return Vector3();
}

RID RasterizerSceneGLES3::voxel_gi_instance_create(RID p_voxel_gi) {
	return RID();
}

void RasterizerSceneGLES3::voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) {
}

bool RasterizerSceneGLES3::voxel_gi_needs_update(RID p_probe) const {
	return false;
}

void RasterizerSceneGLES3::voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) {
}

void RasterizerSceneGLES3::voxel_gi_set_quality(RS::VoxelGIQuality) {
}

_FORCE_INLINE_ static uint32_t _indices_to_primitives(RS::PrimitiveType p_primitive, uint32_t p_indices) {
	static const uint32_t divisor[RS::PRIMITIVE_MAX] = { 1, 2, 1, 3, 1 };
	static const uint32_t subtractor[RS::PRIMITIVE_MAX] = { 0, 0, 1, 0, 1 };
	return (p_indices - subtractor[p_primitive]) / divisor[p_primitive];
}
void RasterizerSceneGLES3::_fill_render_list(RenderListType p_render_list, const RenderDataGLES3 *p_render_data, PassMode p_pass_mode, bool p_append) {
	GLES3::MeshStorage *mesh_storage = GLES3::MeshStorage::get_singleton();
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();

	if (p_render_list == RENDER_LIST_OPAQUE) {
		scene_state.used_screen_texture = false;
		scene_state.used_normal_texture = false;
		scene_state.used_depth_texture = false;
	}

	Plane near_plane;
	if (p_render_data->cam_orthogonal) {
		near_plane = Plane(-p_render_data->cam_transform.basis.get_column(Vector3::AXIS_Z), p_render_data->cam_transform.origin);
		near_plane.d += p_render_data->cam_projection.get_z_near();
	}
	float z_max = p_render_data->cam_projection.get_z_far() - p_render_data->cam_projection.get_z_near();

	RenderList *rl = &render_list[p_render_list];

	// Parse any updates on our geometry, updates surface caches and such
	_update_dirty_geometry_instances();

	if (!p_append) {
		rl->clear();
		if (p_render_list == RENDER_LIST_OPAQUE) {
			render_list[RENDER_LIST_ALPHA].clear(); //opaque fills alpha too
		}
	}

	//fill list

	for (int i = 0; i < (int)p_render_data->instances->size(); i++) {
		GeometryInstanceGLES3 *inst = static_cast<GeometryInstanceGLES3 *>((*p_render_data->instances)[i]);

		Vector3 center = inst->transform.origin;
		if (p_render_data->cam_orthogonal) {
			if (inst->use_aabb_center) {
				center = inst->transformed_aabb.get_support(-near_plane.normal);
			}
			inst->depth = near_plane.distance_to(center) - inst->sorting_offset;
		} else {
			if (inst->use_aabb_center) {
				center = inst->transformed_aabb.position + (inst->transformed_aabb.size * 0.5);
			}
			inst->depth = p_render_data->cam_transform.origin.distance_to(center) - inst->sorting_offset;
		}
		uint32_t depth_layer = CLAMP(int(inst->depth * 16 / z_max), 0, 15);

		uint32_t flags = inst->base_flags; //fill flags if appropriate

		if (inst->non_uniform_scale) {
			flags |= INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE;
		}

		// Sets the index values for lookup in the shader
		// This has to be done after _setup_lights was called this frame

		if (p_pass_mode == PASS_MODE_COLOR) {
			inst->light_passes.clear();
			inst->spot_light_gl_cache.clear();
			inst->omni_light_gl_cache.clear();
			inst->reflection_probes_local_transform_cache.clear();
			inst->reflection_probe_rid_cache.clear();
			uint64_t current_frame = RSG::rasterizer->get_frame_number();

			if (inst->paired_omni_light_count) {
				for (uint32_t j = 0; j < inst->paired_omni_light_count; j++) {
					RID light_instance = inst->paired_omni_lights[j];
					if (light_storage->light_instance_get_render_pass(light_instance) != current_frame) {
						continue;
					}
					RID light = light_storage->light_instance_get_base_light(light_instance);
					int32_t shadow_id = light_storage->light_instance_get_shadow_id(light_instance);

					if (light_storage->light_has_shadow(light) && shadow_id >= 0) {
						// Skip static lights when a lightmap is used.
						if (!inst->lightmap_instance.is_valid() || light_storage->light_get_bake_mode(light) != RenderingServer::LIGHT_BAKE_STATIC) {
							GeometryInstanceGLES3::LightPass pass;
							pass.light_id = light_storage->light_instance_get_gl_id(light_instance);
							pass.shadow_id = shadow_id;
							pass.light_instance_rid = light_instance;
							pass.is_omni = true;
							inst->light_passes.push_back(pass);
						}
					} else {
						// Lights without shadow can all go in base pass.
						inst->omni_light_gl_cache.push_back((uint32_t)light_storage->light_instance_get_gl_id(light_instance));
					}
				}
			}

			if (inst->paired_spot_light_count) {
				for (uint32_t j = 0; j < inst->paired_spot_light_count; j++) {
					RID light_instance = inst->paired_spot_lights[j];
					if (light_storage->light_instance_get_render_pass(light_instance) != current_frame) {
						continue;
					}
					RID light = light_storage->light_instance_get_base_light(light_instance);
					int32_t shadow_id = light_storage->light_instance_get_shadow_id(light_instance);

					if (light_storage->light_has_shadow(light) && shadow_id >= 0) {
						// Skip static lights when a lightmap is used.
						if (!inst->lightmap_instance.is_valid() || light_storage->light_get_bake_mode(light) != RenderingServer::LIGHT_BAKE_STATIC) {
							GeometryInstanceGLES3::LightPass pass;
							pass.light_id = light_storage->light_instance_get_gl_id(light_instance);
							pass.shadow_id = shadow_id;
							pass.light_instance_rid = light_instance;
							inst->light_passes.push_back(pass);
						}
					} else {
						// Lights without shadow can all go in base pass.
						inst->spot_light_gl_cache.push_back((uint32_t)light_storage->light_instance_get_gl_id(light_instance));
					}
				}
			}

			if (p_render_data->reflection_probe.is_null() && inst->paired_reflection_probes.size() > 0) {
				// Do not include if we're rendering reflection probes.
				// We only support two probes for now and we handle them first come, first serve.
				// This should be improved one day, at minimum the list should be sorted by priority.

				for (uint32_t pi = 0; pi < inst->paired_reflection_probes.size(); pi++) {
					RID probe_instance = inst->paired_reflection_probes[pi];
					RID atlas = light_storage->reflection_probe_instance_get_atlas(probe_instance);
					RID probe = light_storage->reflection_probe_instance_get_probe(probe_instance);
					uint32_t reflection_mask = light_storage->reflection_probe_get_reflection_mask(probe);
					if (atlas.is_valid() && (inst->layer_mask & reflection_mask)) {
						Transform3D local_matrix = p_render_data->inv_cam_transform * light_storage->reflection_probe_instance_get_transform(probe_instance);
						inst->reflection_probes_local_transform_cache.push_back(local_matrix.affine_inverse());
						inst->reflection_probe_rid_cache.push_back(probe_instance);
					}
				}
			}
		}

		inst->flags_cache = flags;

		GeometryInstanceSurface *surf = inst->surface_caches;

		float lod_distance = 0.0;

		if (p_render_data->cam_orthogonal) {
			lod_distance = 1.0;
		} else {
			Vector3 aabb_min = inst->transformed_aabb.position;
			Vector3 aabb_max = inst->transformed_aabb.position + inst->transformed_aabb.size;
			Vector3 camera_position = p_render_data->main_cam_transform.origin;
			Vector3 surface_distance = Vector3(0.0, 0.0, 0.0).max(aabb_min - camera_position).max(camera_position - aabb_max);

			lod_distance = surface_distance.length();
		}

		while (surf) {
			// LOD

			if (p_render_data->screen_mesh_lod_threshold > 0.0 && mesh_storage->mesh_surface_has_lod(surf->surface)) {
				uint32_t indices = 0;
				surf->lod_index = mesh_storage->mesh_surface_get_lod(surf->surface, inst->lod_model_scale * inst->lod_bias, lod_distance * p_render_data->lod_distance_multiplier, p_render_data->screen_mesh_lod_threshold, indices);
				surf->index_count = indices;

				if (p_render_data->render_info) {
					indices = _indices_to_primitives(surf->primitive, indices);
					if (p_render_list == RENDER_LIST_OPAQUE) { //opaque
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += indices;
					} else if (p_render_list == RENDER_LIST_SECONDARY) { //shadow
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += indices;
					}
				}

			} else {
				surf->lod_index = 0;

				if (p_render_data->render_info) {
					uint32_t to_draw = mesh_storage->mesh_surface_get_vertices_drawn_count(surf->surface);
					to_draw = _indices_to_primitives(surf->primitive, to_draw);
					to_draw *= inst->instance_count > 0 ? inst->instance_count : 1;
					if (p_render_list == RENDER_LIST_OPAQUE) { //opaque
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += to_draw;
					} else if (p_render_list == RENDER_LIST_SECONDARY) { //shadow
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += to_draw;
					}
				}
			}

			// ADD Element
			if (p_pass_mode == PASS_MODE_COLOR) {
#ifdef DEBUG_ENABLED
				bool force_alpha = unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW);
#else
				bool force_alpha = false;
#endif
				if (!force_alpha && (surf->flags & (GeometryInstanceSurface::FLAG_PASS_DEPTH | GeometryInstanceSurface::FLAG_PASS_OPAQUE))) {
					rl->add_element(surf);
				}
				if (force_alpha || (surf->flags & GeometryInstanceSurface::FLAG_PASS_ALPHA)) {
					render_list[RENDER_LIST_ALPHA].add_element(surf);
				}

				if (surf->flags & GeometryInstanceSurface::FLAG_USES_SCREEN_TEXTURE) {
					scene_state.used_screen_texture = true;
				}
				if (surf->flags & GeometryInstanceSurface::FLAG_USES_NORMAL_TEXTURE) {
					scene_state.used_normal_texture = true;
				}
				if (surf->flags & GeometryInstanceSurface::FLAG_USES_DEPTH_TEXTURE) {
					scene_state.used_depth_texture = true;
				}

			} else if (p_pass_mode == PASS_MODE_SHADOW) {
				if (surf->flags & GeometryInstanceSurface::FLAG_PASS_SHADOW) {
					rl->add_element(surf);
				}
			} else {
				if (surf->flags & (GeometryInstanceSurface::FLAG_PASS_DEPTH | GeometryInstanceSurface::FLAG_PASS_OPAQUE)) {
					rl->add_element(surf);
				}
			}

			surf->sort.depth_layer = depth_layer;
			surf->finished_base_pass = false;
			surf->light_pass_index = 0;

			surf = surf->next;
		}
	}
}

// Needs to be called after _setup_lights so that directional_light_count is accurate.
void RasterizerSceneGLES3::_setup_environment(const RenderDataGLES3 *p_render_data, bool p_no_fog, const Size2i &p_screen_size, bool p_flip_y, const Color &p_default_bg_color, bool p_pancake_shadows, float p_shadow_bias) {
	Projection correction;
	correction.set_depth_correction(p_flip_y, true, false);
	Projection projection = correction * p_render_data->cam_projection;
	//store camera into ubo
	GLES3::MaterialStorage::store_camera(projection, scene_state.ubo.projection_matrix);
	GLES3::MaterialStorage::store_camera(projection.inverse(), scene_state.ubo.inv_projection_matrix);
	GLES3::MaterialStorage::store_transform(p_render_data->cam_transform, scene_state.ubo.inv_view_matrix);
	GLES3::MaterialStorage::store_transform(p_render_data->inv_cam_transform, scene_state.ubo.view_matrix);
	GLES3::MaterialStorage::store_transform(p_render_data->main_cam_transform, scene_state.ubo.main_cam_inv_view_matrix);
	scene_state.ubo.camera_visible_layers = p_render_data->camera_visible_layers;

	if (p_render_data->view_count > 1) {
		for (uint32_t v = 0; v < p_render_data->view_count; v++) {
			projection = correction * p_render_data->view_projection[v];
			GLES3::MaterialStorage::store_camera(projection, scene_state.multiview_ubo.projection_matrix_view[v]);
			GLES3::MaterialStorage::store_camera(projection.inverse(), scene_state.multiview_ubo.inv_projection_matrix_view[v]);

			scene_state.multiview_ubo.eye_offset[v][0] = p_render_data->view_eye_offset[v].x;
			scene_state.multiview_ubo.eye_offset[v][1] = p_render_data->view_eye_offset[v].y;
			scene_state.multiview_ubo.eye_offset[v][2] = p_render_data->view_eye_offset[v].z;
			scene_state.multiview_ubo.eye_offset[v][3] = 0.0;
		}
	}

	// Only render the lights without shadows in the base pass.
	scene_state.ubo.directional_light_count = p_render_data->directional_light_count - p_render_data->directional_shadow_count;

	scene_state.ubo.z_far = p_render_data->z_far;
	scene_state.ubo.z_near = p_render_data->z_near;

	scene_state.ubo.viewport_size[0] = p_screen_size.x;
	scene_state.ubo.viewport_size[1] = p_screen_size.y;

	Size2 screen_pixel_size = Vector2(1.0, 1.0) / Size2(p_screen_size);
	scene_state.ubo.screen_pixel_size[0] = screen_pixel_size.x;
	scene_state.ubo.screen_pixel_size[1] = screen_pixel_size.y;

	scene_state.ubo.luminance_multiplier = p_render_data->luminance_multiplier;

	scene_state.ubo.shadow_bias = p_shadow_bias;
	scene_state.ubo.pancake_shadows = p_pancake_shadows;

	//time global variables
	scene_state.ubo.time = time;

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
		scene_state.ubo.use_ambient_light = true;
		scene_state.ubo.ambient_light_color_energy[0] = 1;
		scene_state.ubo.ambient_light_color_energy[1] = 1;
		scene_state.ubo.ambient_light_color_energy[2] = 1;
		scene_state.ubo.ambient_light_color_energy[3] = 1.0;
		scene_state.ubo.use_ambient_cubemap = false;
		scene_state.ubo.use_reflection_cubemap = false;
	} else if (is_environment(p_render_data->environment)) {
		RS::EnvironmentBG env_bg = environment_get_background(p_render_data->environment);
		RS::EnvironmentAmbientSource ambient_src = environment_get_ambient_source(p_render_data->environment);

		float bg_energy_multiplier = environment_get_bg_energy_multiplier(p_render_data->environment);

		scene_state.ubo.ambient_light_color_energy[3] = bg_energy_multiplier;

		scene_state.ubo.ambient_color_sky_mix = environment_get_ambient_sky_contribution(p_render_data->environment);

		//ambient
		if (ambient_src == RS::ENV_AMBIENT_SOURCE_BG && (env_bg == RS::ENV_BG_CLEAR_COLOR || env_bg == RS::ENV_BG_COLOR)) {
			Color color = env_bg == RS::ENV_BG_CLEAR_COLOR ? p_default_bg_color : environment_get_bg_color(p_render_data->environment);
			color = color.srgb_to_linear();

			scene_state.ubo.ambient_light_color_energy[0] = color.r * bg_energy_multiplier;
			scene_state.ubo.ambient_light_color_energy[1] = color.g * bg_energy_multiplier;
			scene_state.ubo.ambient_light_color_energy[2] = color.b * bg_energy_multiplier;
			scene_state.ubo.use_ambient_light = true;
			scene_state.ubo.use_ambient_cubemap = false;
		} else {
			float energy = environment_get_ambient_light_energy(p_render_data->environment);
			Color color = environment_get_ambient_light(p_render_data->environment);
			color = color.srgb_to_linear();
			scene_state.ubo.ambient_light_color_energy[0] = color.r * energy;
			scene_state.ubo.ambient_light_color_energy[1] = color.g * energy;
			scene_state.ubo.ambient_light_color_energy[2] = color.b * energy;

			Basis sky_transform = environment_get_sky_orientation(p_render_data->environment);
			sky_transform = sky_transform.inverse() * p_render_data->cam_transform.basis;
			GLES3::MaterialStorage::store_transform_3x3(sky_transform, scene_state.ubo.radiance_inverse_xform);
			scene_state.ubo.use_ambient_cubemap = (ambient_src == RS::ENV_AMBIENT_SOURCE_BG && env_bg == RS::ENV_BG_SKY) || ambient_src == RS::ENV_AMBIENT_SOURCE_SKY;
			scene_state.ubo.use_ambient_light = scene_state.ubo.use_ambient_cubemap || ambient_src == RS::ENV_AMBIENT_SOURCE_COLOR;
		}

		//specular
		RS::EnvironmentReflectionSource ref_src = environment_get_reflection_source(p_render_data->environment);
		if ((ref_src == RS::ENV_REFLECTION_SOURCE_BG && env_bg == RS::ENV_BG_SKY) || ref_src == RS::ENV_REFLECTION_SOURCE_SKY) {
			scene_state.ubo.use_reflection_cubemap = true;
		} else {
			scene_state.ubo.use_reflection_cubemap = false;
		}

		scene_state.ubo.fog_enabled = environment_get_fog_enabled(p_render_data->environment);
		scene_state.ubo.fog_mode = environment_get_fog_mode(p_render_data->environment);
		scene_state.ubo.fog_density = environment_get_fog_density(p_render_data->environment);
		scene_state.ubo.fog_height = environment_get_fog_height(p_render_data->environment);
		scene_state.ubo.fog_depth_curve = environment_get_fog_depth_curve(p_render_data->environment);
		scene_state.ubo.fog_depth_end = environment_get_fog_depth_end(p_render_data->environment) > 0.0 ? environment_get_fog_depth_end(p_render_data->environment) : scene_state.ubo.z_far;
		scene_state.ubo.fog_depth_begin = MIN(environment_get_fog_depth_begin(p_render_data->environment), scene_state.ubo.fog_depth_end - 0.001);
		scene_state.ubo.fog_height_density = environment_get_fog_height_density(p_render_data->environment);
		scene_state.ubo.fog_aerial_perspective = environment_get_fog_aerial_perspective(p_render_data->environment);

		Color fog_color = environment_get_fog_light_color(p_render_data->environment).srgb_to_linear();
		float fog_energy = environment_get_fog_light_energy(p_render_data->environment);

		scene_state.ubo.fog_light_color[0] = fog_color.r * fog_energy;
		scene_state.ubo.fog_light_color[1] = fog_color.g * fog_energy;
		scene_state.ubo.fog_light_color[2] = fog_color.b * fog_energy;

		scene_state.ubo.fog_sun_scatter = environment_get_fog_sun_scatter(p_render_data->environment);

	} else {
	}

	if (p_render_data->camera_attributes.is_valid()) {
		scene_state.ubo.emissive_exposure_normalization = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
		scene_state.ubo.IBL_exposure_normalization = 1.0;
		if (is_environment(p_render_data->environment)) {
			RID sky_rid = environment_get_sky(p_render_data->environment);
			if (sky_rid.is_valid()) {
				float current_exposure = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes) * environment_get_bg_intensity(p_render_data->environment);
				scene_state.ubo.IBL_exposure_normalization = current_exposure / MAX(0.001, sky_get_baked_exposure(sky_rid));
			}
		}
	} else if (scene_state.ubo.emissive_exposure_normalization > 0.0) {
		// This branch is triggered when using render_material().
		// Emissive is set outside the function, so don't set it.
		// IBL isn't used don't set it.
	} else {
		scene_state.ubo.emissive_exposure_normalization = 1.0;
		scene_state.ubo.IBL_exposure_normalization = 1.0;
	}

	if (scene_state.ubo_buffer == 0) {
		glGenBuffers(1, &scene_state.ubo_buffer);
		glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DATA_UNIFORM_LOCATION, scene_state.ubo_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.ubo_buffer, sizeof(SceneState::UBO), &scene_state.ubo, GL_STREAM_DRAW, "Scene state UBO");
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	} else {
		glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DATA_UNIFORM_LOCATION, scene_state.ubo_buffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::UBO), &scene_state.ubo, GL_STREAM_DRAW);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	if (p_render_data->view_count > 1) {
		if (scene_state.multiview_buffer == 0) {
			glGenBuffers(1, &scene_state.multiview_buffer);
			glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_MULTIVIEW_UNIFORM_LOCATION, scene_state.multiview_buffer);
			GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.multiview_buffer, sizeof(SceneState::MultiviewUBO), &scene_state.multiview_ubo, GL_STREAM_DRAW, "Multiview UBO");
		} else {
			glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_MULTIVIEW_UNIFORM_LOCATION, scene_state.multiview_buffer);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::MultiviewUBO), &scene_state.multiview_ubo, GL_STREAM_DRAW);
		}

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
}

// Puts lights into Uniform Buffers. Needs to be called before _fill_list as this caches the index of each light in the Uniform Buffer
void RasterizerSceneGLES3::_setup_lights(const RenderDataGLES3 *p_render_data, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_omni_light_count, uint32_t &r_spot_light_count, uint32_t &r_directional_shadow_count) {
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();
	GLES3::Config *config = GLES3::Config::get_singleton();

	const Transform3D inverse_transform = p_render_data->inv_cam_transform;

	const PagedArray<RID> &lights = *p_render_data->lights;

	r_directional_light_count = 0;
	r_omni_light_count = 0;
	r_spot_light_count = 0;
	r_directional_shadow_count = 0;

	int num_lights = lights.size();

	for (int i = 0; i < num_lights; i++) {
		GLES3::LightInstance *li = GLES3::LightStorage::get_singleton()->get_light_instance(lights[i]);
		if (!li) {
			continue;
		}
		RID base = li->light;

		ERR_CONTINUE(base.is_null());

		RS::LightType type = light_storage->light_get_type(base);
		switch (type) {
			case RS::LIGHT_DIRECTIONAL: {
				if (r_directional_light_count >= RendererSceneRender::MAX_DIRECTIONAL_LIGHTS || light_storage->light_directional_get_sky_mode(base) == RS::LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY) {
					continue;
				}

				// If a DirectionalLight has shadows, we will add it to the end of the array and work in.
				bool has_shadow = light_storage->light_has_shadow(base);

				int index = r_directional_light_count - r_directional_shadow_count;

				if (has_shadow) {
					// Lights with shadow are incremented from the end of the array.
					index = MAX_DIRECTIONAL_LIGHTS - 1 - r_directional_shadow_count;
				}
				DirectionalLightData &light_data = scene_state.directional_lights[index];

				Transform3D light_transform = li->transform;

				Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, 1))).normalized();

				light_data.direction[0] = direction.x;
				light_data.direction[1] = direction.y;
				light_data.direction[2] = direction.z;

				light_data.bake_mode = light_storage->light_get_bake_mode(base);

				float sign = light_storage->light_is_negative(base) ? -1 : 1;

				light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

				if (is_using_physical_light_units()) {
					light_data.energy *= light_storage->light_get_param(base, RS::LIGHT_PARAM_INTENSITY);
				} else {
					light_data.energy *= Math_PI;
				}

				if (p_render_data->camera_attributes.is_valid()) {
					light_data.energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
				}

				Color linear_col = light_storage->light_get_color(base).srgb_to_linear();
				light_data.color[0] = linear_col.r;
				light_data.color[1] = linear_col.g;
				light_data.color[2] = linear_col.b;

				float size = light_storage->light_get_param(base, RS::LIGHT_PARAM_SIZE);
				light_data.size = 1.0 - Math::cos(Math::deg_to_rad(size)); //angle to cosine offset

				light_data.specular = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPECULAR);

				light_data.shadow_opacity = (p_using_shadows && light_storage->light_has_shadow(base))
						? light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_OPACITY)
						: 0.0;

				if (has_shadow) {
					DirectionalShadowData &shadow_data = scene_state.directional_shadows[MAX_DIRECTIONAL_LIGHTS - 1 - r_directional_shadow_count];

					RS::LightDirectionalShadowMode shadow_mode = light_storage->light_directional_get_shadow_mode(base);

					int limit = shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL ? 0 : (shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS ? 1 : 3);

					shadow_data.shadow_atlas_pixel_size = 1.0 / light_storage->directional_shadow_get_size();

					shadow_data.blend_splits = uint32_t((shadow_mode != RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL) && light_storage->light_directional_get_blend_splits(base));
					for (int j = 0; j < 4; j++) {
						Rect2 atlas_rect = li->shadow_transform[j].atlas_rect;
						Projection correction;
						correction.set_depth_correction(false, true, false);
						Projection matrix = correction * li->shadow_transform[j].camera;
						float split = li->shadow_transform[MIN(limit, j)].split;

						Projection bias;
						bias.set_light_bias();
						Projection rectm;
						rectm.set_light_atlas_rect(atlas_rect);

						Transform3D modelview = (inverse_transform * li->shadow_transform[j].transform).inverse();

						shadow_data.direction[0] = light_data.direction[0];
						shadow_data.direction[1] = light_data.direction[1];
						shadow_data.direction[2] = light_data.direction[2];

						Projection shadow_mtx = rectm * bias * matrix * modelview;
						shadow_data.shadow_split_offsets[j] = split;
						shadow_data.shadow_normal_bias[j] = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS) * li->shadow_transform[j].shadow_texel_size;
						GLES3::MaterialStorage::store_camera(shadow_mtx, shadow_data.shadow_matrices[j]);
					}
					float fade_start = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_FADE_START);
					shadow_data.fade_from = -shadow_data.shadow_split_offsets[3] * MIN(fade_start, 0.999);
					shadow_data.fade_to = -shadow_data.shadow_split_offsets[3];

					r_directional_shadow_count++;
				}

				r_directional_light_count++;
			} break;
			case RS::LIGHT_OMNI: {
				if (r_omni_light_count >= (uint32_t)config->max_renderable_lights) {
					continue;
				}

				const real_t distance = p_render_data->cam_transform.origin.distance_to(li->transform.origin);

				if (light_storage->light_is_distance_fade_enabled(li->light)) {
					const float fade_begin = light_storage->light_get_distance_fade_begin(li->light);
					const float fade_length = light_storage->light_get_distance_fade_length(li->light);

					if (distance > fade_begin) {
						if (distance > fade_begin + fade_length) {
							// Out of range, don't draw this light to improve performance.
							continue;
						}
					}
				}

				scene_state.omni_light_sort[r_omni_light_count].instance = li;
				scene_state.omni_light_sort[r_omni_light_count].depth = distance;
				r_omni_light_count++;
			} break;
			case RS::LIGHT_SPOT: {
				if (r_spot_light_count >= (uint32_t)config->max_renderable_lights) {
					continue;
				}

				const real_t distance = p_render_data->cam_transform.origin.distance_to(li->transform.origin);

				if (light_storage->light_is_distance_fade_enabled(li->light)) {
					const float fade_begin = light_storage->light_get_distance_fade_begin(li->light);
					const float fade_length = light_storage->light_get_distance_fade_length(li->light);

					if (distance > fade_begin) {
						if (distance > fade_begin + fade_length) {
							// Out of range, don't draw this light to improve performance.
							continue;
						}
					}
				}

				scene_state.spot_light_sort[r_spot_light_count].instance = li;
				scene_state.spot_light_sort[r_spot_light_count].depth = distance;
				r_spot_light_count++;
			} break;
		}

		li->last_pass = RSG::rasterizer->get_frame_number();
	}

	if (r_omni_light_count) {
		SortArray<InstanceSort<GLES3::LightInstance>> sorter;
		sorter.sort(scene_state.omni_light_sort, r_omni_light_count);
	}

	if (r_spot_light_count) {
		SortArray<InstanceSort<GLES3::LightInstance>> sorter;
		sorter.sort(scene_state.spot_light_sort, r_spot_light_count);
	}

	int num_positional_shadows = 0;

	for (uint32_t i = 0; i < (r_omni_light_count + r_spot_light_count); i++) {
		uint32_t index = (i < r_omni_light_count) ? i : i - (r_omni_light_count);
		LightData &light_data = (i < r_omni_light_count) ? scene_state.omni_lights[index] : scene_state.spot_lights[index];
		RS::LightType type = (i < r_omni_light_count) ? RS::LIGHT_OMNI : RS::LIGHT_SPOT;
		GLES3::LightInstance *li = (i < r_omni_light_count) ? scene_state.omni_light_sort[index].instance : scene_state.spot_light_sort[index].instance;
		real_t distance = (i < r_omni_light_count) ? scene_state.omni_light_sort[index].depth : scene_state.spot_light_sort[index].depth;
		RID base = li->light;

		li->gl_id = index;

		Transform3D light_transform = li->transform;
		Vector3 pos = inverse_transform.xform(light_transform.origin);

		light_data.position[0] = pos.x;
		light_data.position[1] = pos.y;
		light_data.position[2] = pos.z;

		light_data.bake_mode = light_storage->light_get_bake_mode(base);

		float radius = MAX(0.001, light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE));
		light_data.inv_radius = 1.0 / radius;

		Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, -1))).normalized();

		light_data.direction[0] = direction.x;
		light_data.direction[1] = direction.y;
		light_data.direction[2] = direction.z;

		float size = light_storage->light_get_param(base, RS::LIGHT_PARAM_SIZE);

		light_data.size = size;

		float sign = light_storage->light_is_negative(base) ? -1 : 1;
		Color linear_col = light_storage->light_get_color(base).srgb_to_linear();

		// Reuse fade begin, fade length and distance for shadow LOD determination later.
		float fade_begin = 0.0;
		float fade_shadow = 0.0;
		float fade_length = 0.0;

		float fade = 1.0;
		float shadow_opacity_fade = 1.0;

		if (light_storage->light_is_distance_fade_enabled(base)) {
			fade_begin = light_storage->light_get_distance_fade_begin(base);
			fade_shadow = light_storage->light_get_distance_fade_shadow(base);
			fade_length = light_storage->light_get_distance_fade_length(base);

			if (distance > fade_begin) {
				// Use `smoothstep()` to make opacity changes more gradual and less noticeable to the player.
				fade = Math::smoothstep(0.0f, 1.0f, 1.0f - float(distance - fade_begin) / fade_length);
			}
			if (distance > fade_shadow) {
				shadow_opacity_fade = Math::smoothstep(0.0f, 1.0f, 1.0f - float(distance - fade_shadow) / fade_length);
			}
		}

		float energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY) * fade;

		if (is_using_physical_light_units()) {
			energy *= light_storage->light_get_param(base, RS::LIGHT_PARAM_INTENSITY);

			// Convert from Luminous Power to Luminous Intensity
			if (type == RS::LIGHT_OMNI) {
				energy *= 1.0 / (Math_PI * 4.0);
			} else {
				// Spot Lights are not physically accurate, Luminous Intensity should change in relation to the cone angle.
				// We make this assumption to keep them easy to control.
				energy *= 1.0 / Math_PI;
			}
		} else {
			energy *= Math_PI;
		}

		if (p_render_data->camera_attributes.is_valid()) {
			energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
		}

		light_data.color[0] = linear_col.r * energy;
		light_data.color[1] = linear_col.g * energy;
		light_data.color[2] = linear_col.b * energy;

		light_data.attenuation = light_storage->light_get_param(base, RS::LIGHT_PARAM_ATTENUATION);

		light_data.inv_spot_attenuation = 1.0f / light_storage->light_get_param(base, RS::LIGHT_PARAM_SPOT_ATTENUATION);

		float spot_angle = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPOT_ANGLE);
		light_data.cos_spot_angle = Math::cos(Math::deg_to_rad(spot_angle));

		light_data.specular_amount = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPECULAR) * 2.0;

		// Setup shadows
		const bool needs_shadow =
				p_using_shadows &&
				light_storage->owns_shadow_atlas(p_render_data->shadow_atlas) &&
				light_storage->shadow_atlas_owns_light_instance(p_render_data->shadow_atlas, li->self) &&
				light_storage->light_has_shadow(base);

		bool in_shadow_range = true;
		if (needs_shadow && light_storage->light_is_distance_fade_enabled(base)) {
			if (distance > fade_shadow + fade_length) {
				// Out of range, don't draw shadows to improve performance.
				in_shadow_range = false;
			}
		}

		// Fill in the shadow information.
		if (needs_shadow && in_shadow_range) {
			if (num_positional_shadows >= config->max_renderable_lights) {
				continue;
			}
			ShadowData &shadow_data = scene_state.positional_shadows[num_positional_shadows];
			li->shadow_id = num_positional_shadows;
			num_positional_shadows++;

			light_data.shadow_opacity = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_OPACITY) * shadow_opacity_fade;

			float shadow_texel_size = light_storage->light_instance_get_shadow_texel_size(li->self, p_render_data->shadow_atlas);
			shadow_data.shadow_atlas_pixel_size = shadow_texel_size;
			shadow_data.shadow_normal_bias = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS) * shadow_texel_size * 10.0;

			shadow_data.light_position[0] = light_data.position[0];
			shadow_data.light_position[1] = light_data.position[1];
			shadow_data.light_position[2] = light_data.position[2];

			if (type == RS::LIGHT_OMNI) {
				Transform3D proj = (inverse_transform * light_transform).inverse();

				GLES3::MaterialStorage::store_transform(proj, shadow_data.shadow_matrix);

			} else if (type == RS::LIGHT_SPOT) {
				Transform3D modelview = (inverse_transform * light_transform).inverse();
				Projection bias;
				bias.set_light_bias();

				Projection correction;
				correction.set_depth_correction(false, true, false);
				Projection cm = correction * li->shadow_transform[0].camera;
				Projection shadow_mtx = bias * cm * modelview;
				GLES3::MaterialStorage::store_camera(shadow_mtx, shadow_data.shadow_matrix);
			}
		}
	}

	// TODO, to avoid stalls, should rotate between 3 buffers based on frame index.
	// TODO, consider mapping the buffer as in 2D
	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_OMNILIGHT_UNIFORM_LOCATION, scene_state.omni_light_buffer);
	if (r_omni_light_count) {
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightData) * r_omni_light_count, scene_state.omni_lights);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_SPOTLIGHT_UNIFORM_LOCATION, scene_state.spot_light_buffer);
	if (r_spot_light_count) {
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightData) * r_spot_light_count, scene_state.spot_lights);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION, scene_state.directional_light_buffer);
	if (r_directional_light_count) {
		glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLightData) * MAX_DIRECTIONAL_LIGHTS, scene_state.directional_lights, GL_STREAM_DRAW);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_POSITIONAL_SHADOW_UNIFORM_LOCATION, scene_state.positional_shadow_buffer);
	if (num_positional_shadows) {
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ShadowData) * num_positional_shadows, scene_state.positional_shadows);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DIRECTIONAL_SHADOW_UNIFORM_LOCATION, scene_state.directional_shadow_buffer);
	if (r_directional_shadow_count) {
		glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalShadowData) * MAX_DIRECTIONAL_LIGHTS, scene_state.directional_shadows, GL_STREAM_DRAW);
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// Render shadows
void RasterizerSceneGLES3::_render_shadows(const RenderDataGLES3 *p_render_data, const Size2i &p_viewport_size) {
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();

	LocalVector<int> cube_shadows;
	LocalVector<int> shadows;
	LocalVector<int> directional_shadows;

	float lod_distance_multiplier = p_render_data->cam_projection.get_lod_multiplier();

	// Put lights into buckets for omni (cube shadows), directional, and spot.
	{
		for (int i = 0; i < p_render_data->render_shadow_count; i++) {
			RID li = p_render_data->render_shadows[i].light;
			RID base = light_storage->light_instance_get_base_light(li);

			if (light_storage->light_get_type(base) == RS::LIGHT_DIRECTIONAL) {
				directional_shadows.push_back(i);
			} else if (light_storage->light_get_type(base) == RS::LIGHT_OMNI && light_storage->light_omni_get_shadow_mode(base) == RS::LIGHT_OMNI_SHADOW_CUBE) {
				cube_shadows.push_back(i);
			} else {
				shadows.push_back(i);
			}
		}
		if (directional_shadows.size()) {
			light_storage->update_directional_shadow_atlas();
		}
	}

	bool render_shadows = directional_shadows.size() || shadows.size() || cube_shadows.size();

	if (render_shadows) {
		RENDER_TIMESTAMP("Render Shadows");

		// Render cubemap shadows.
		for (const int &index : cube_shadows) {
			_render_shadow_pass(p_render_data->render_shadows[index].light, p_render_data->shadow_atlas, p_render_data->render_shadows[index].pass, p_render_data->render_shadows[index].instances, lod_distance_multiplier, p_render_data->screen_mesh_lod_threshold, p_render_data->render_info, p_viewport_size, p_render_data->cam_transform);
		}
		// Render directional shadows.
		for (uint32_t i = 0; i < directional_shadows.size(); i++) {
			_render_shadow_pass(p_render_data->render_shadows[directional_shadows[i]].light, p_render_data->shadow_atlas, p_render_data->render_shadows[directional_shadows[i]].pass, p_render_data->render_shadows[directional_shadows[i]].instances, lod_distance_multiplier, p_render_data->screen_mesh_lod_threshold, p_render_data->render_info, p_viewport_size, p_render_data->cam_transform);
		}
		// Render positional shadows (Spotlight and Omnilight with dual-paraboloid).
		for (uint32_t i = 0; i < shadows.size(); i++) {
			_render_shadow_pass(p_render_data->render_shadows[shadows[i]].light, p_render_data->shadow_atlas, p_render_data->render_shadows[shadows[i]].pass, p_render_data->render_shadows[shadows[i]].instances, lod_distance_multiplier, p_render_data->screen_mesh_lod_threshold, p_render_data->render_info, p_viewport_size, p_render_data->cam_transform);
		}
	}
}

void RasterizerSceneGLES3::_render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, float p_lod_distance_multiplier, float p_screen_mesh_lod_threshold, RenderingMethod::RenderInfo *p_render_info, const Size2i &p_viewport_size, const Transform3D &p_main_cam_transform) {
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();

	ERR_FAIL_COND(!light_storage->owns_light_instance(p_light));

	RID base = light_storage->light_instance_get_base_light(p_light);

	float zfar = 0.0;
	bool use_pancake = false;
	float shadow_bias = 0.0;
	bool reverse_cull = false;
	bool needs_clear = false;

	Projection light_projection;
	Transform3D light_transform;
	GLuint shadow_fb = 0;
	Rect2i atlas_rect;

	if (light_storage->light_get_type(base) == RS::LIGHT_DIRECTIONAL) {
		// Set pssm stuff.
		uint64_t last_scene_shadow_pass = light_storage->light_instance_get_shadow_pass(p_light);
		if (last_scene_shadow_pass != get_scene_pass()) {
			light_storage->light_instance_set_directional_rect(p_light, light_storage->get_directional_shadow_rect());
			light_storage->directional_shadow_increase_current_light();
			light_storage->light_instance_set_shadow_pass(p_light, get_scene_pass());
		}

		atlas_rect = light_storage->light_instance_get_directional_rect(p_light);

		if (light_storage->light_directional_get_shadow_mode(base) == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS) {
			atlas_rect.size.width /= 2;
			atlas_rect.size.height /= 2;

			if (p_pass == 1) {
				atlas_rect.position.x += atlas_rect.size.width;
			} else if (p_pass == 2) {
				atlas_rect.position.y += atlas_rect.size.height;
			} else if (p_pass == 3) {
				atlas_rect.position += atlas_rect.size;
			}
		} else if (light_storage->light_directional_get_shadow_mode(base) == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS) {
			atlas_rect.size.height /= 2;

			if (p_pass == 0) {
			} else {
				atlas_rect.position.y += atlas_rect.size.height;
			}
		}

		use_pancake = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_PANCAKE_SIZE) > 0;
		light_projection = light_storage->light_instance_get_shadow_camera(p_light, p_pass);
		light_transform = light_storage->light_instance_get_shadow_transform(p_light, p_pass);

		float directional_shadow_size = light_storage->directional_shadow_get_size();
		Rect2 atlas_rect_norm = atlas_rect;
		atlas_rect_norm.position /= directional_shadow_size;
		atlas_rect_norm.size /= directional_shadow_size;
		light_storage->light_instance_set_directional_shadow_atlas_rect(p_light, p_pass, atlas_rect_norm);

		zfar = RSG::light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
		shadow_fb = light_storage->direction_shadow_get_fb();
		reverse_cull = !light_storage->light_get_reverse_cull_face_mode(base);

		float bias_scale = light_storage->light_instance_get_shadow_bias_scale(p_light, p_pass);
		shadow_bias = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_BIAS) / 100.0 * bias_scale;

	} else {
		// Set from shadow atlas.

		ERR_FAIL_COND(!light_storage->owns_shadow_atlas(p_shadow_atlas));
		ERR_FAIL_COND(!light_storage->shadow_atlas_owns_light_instance(p_shadow_atlas, p_light));

		uint32_t key = light_storage->shadow_atlas_get_light_instance_key(p_shadow_atlas, p_light);

		uint32_t quadrant = (key >> GLES3::LightStorage::QUADRANT_SHIFT) & 0x3;
		uint32_t shadow = key & GLES3::LightStorage::SHADOW_INDEX_MASK;

		ERR_FAIL_INDEX((int)shadow, light_storage->shadow_atlas_get_quadrant_shadows_length(p_shadow_atlas, quadrant));

		int shadow_size = light_storage->shadow_atlas_get_quadrant_shadow_size(p_shadow_atlas, quadrant);

		shadow_fb = light_storage->shadow_atlas_get_quadrant_shadow_fb(p_shadow_atlas, quadrant, shadow);

		zfar = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
		reverse_cull = !light_storage->light_get_reverse_cull_face_mode(base);

		if (light_storage->light_get_type(base) == RS::LIGHT_OMNI) {
			if (light_storage->light_omni_get_shadow_mode(base) == RS::LIGHT_OMNI_SHADOW_CUBE) {
				GLuint shadow_texture = light_storage->shadow_atlas_get_quadrant_shadow_texture(p_shadow_atlas, quadrant, shadow);
				glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);

				static GLenum cube_map_faces[6] = {
					GL_TEXTURE_CUBE_MAP_POSITIVE_X,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
					// Flipped order for Y to match what the RD renderer expects
					// (and thus what is given to us by the Rendering Server).
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				};

				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, cube_map_faces[p_pass], shadow_texture, 0);

				light_projection = light_storage->light_instance_get_shadow_camera(p_light, p_pass);
				light_transform = light_storage->light_instance_get_shadow_transform(p_light, p_pass);
				shadow_size = shadow_size / 2;
			} else {
				ERR_FAIL_MSG("Dual paraboloid shadow mode not supported in GL Compatibility renderer. Please use Cubemap shadow mode instead.");
			}

			shadow_bias = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_BIAS);

		} else if (light_storage->light_get_type(base) == RS::LIGHT_SPOT) {
			light_projection = light_storage->light_instance_get_shadow_camera(p_light, 0);
			light_transform = light_storage->light_instance_get_shadow_transform(p_light, 0);

			shadow_bias = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_BIAS) / 10.0;
			// Prebake range into bias so we can scale based on distance easily.
			shadow_bias *= light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
		}
		atlas_rect.size.x = shadow_size;
		atlas_rect.size.y = shadow_size;

		needs_clear = true;
	}

	RenderDataGLES3 render_data;
	render_data.cam_projection = light_projection;
	render_data.cam_transform = light_transform;
	render_data.inv_cam_transform = light_transform.affine_inverse();
	render_data.z_far = zfar; // Only used by OmniLights.
	render_data.z_near = 0.0;
	render_data.lod_distance_multiplier = p_lod_distance_multiplier;
	render_data.main_cam_transform = p_main_cam_transform;

	render_data.instances = &p_instances;
	render_data.render_info = p_render_info;

	_setup_environment(&render_data, true, p_viewport_size, false, Color(), use_pancake, shadow_bias);

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_DISABLE_LOD) {
		render_data.screen_mesh_lod_threshold = 0.0;
	} else {
		render_data.screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
	}

	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, PASS_MODE_SHADOW);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();

	glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
	glViewport(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.x, atlas_rect.size.y);

	GLuint global_buffer = GLES3::MaterialStorage::get_singleton()->global_shader_parameters_get_uniform_buffer();

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_GLOBALS_UNIFORM_LOCATION, global_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	scene_state.reset_gl_state();
	scene_state.enable_gl_depth_test(true);
	scene_state.enable_gl_depth_draw(true);
	glDepthFunc(GL_GREATER);

	glColorMask(0, 0, 0, 0);
	glDrawBuffers(0, nullptr);
	RasterizerGLES3::clear_depth(0.0);
	if (needs_clear) {
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	uint64_t spec_constant_base_flags = SceneShaderGLES3::DISABLE_LIGHTMAP |
			SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL |
			SceneShaderGLES3::DISABLE_LIGHT_OMNI |
			SceneShaderGLES3::DISABLE_LIGHT_SPOT |
			SceneShaderGLES3::DISABLE_FOG |
			SceneShaderGLES3::RENDER_SHADOWS;

	if (light_storage->light_get_type(base) == RS::LIGHT_OMNI) {
		spec_constant_base_flags |= SceneShaderGLES3::RENDER_SHADOWS_LINEAR;
	}

	RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), reverse_cull, spec_constant_base_flags, false);

	_render_list_template<PASS_MODE_SHADOW>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());

	glColorMask(1, 1, 1, 1);
	scene_state.enable_gl_depth_test(false);
	scene_state.enable_gl_depth_draw(true);
	glDisable(GL_CULL_FACE);
	scene_state.cull_mode = GLES3::SceneShaderData::CULL_DISABLED;
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
}

void RasterizerSceneGLES3::render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data, RenderingMethod::RenderInfo *r_render_info) {
	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	GLES3::Config *config = GLES3::Config::get_singleton();
	RENDER_TIMESTAMP("Setup 3D Scene");

	bool apply_color_adjustments_in_post = false;
	bool is_reflection_probe = p_reflection_probe.is_valid();

	Ref<RenderSceneBuffersGLES3> rb = p_render_buffers;
	ERR_FAIL_COND(rb.is_null());

	if (rb->get_scaling_3d_mode() != RS::VIEWPORT_SCALING_3D_MODE_OFF) {
		// If we're scaling, we apply tonemapping etc. in post, so disable it during rendering
		apply_color_adjustments_in_post = true;
	}

	GLES3::RenderTarget *rt = nullptr; // No render target for reflection probe
	if (!is_reflection_probe) {
		rt = texture_storage->get_render_target(rb->render_target);
		ERR_FAIL_NULL(rt);
	}

	bool glow_enabled = false;
	if (p_environment.is_valid()) {
		glow_enabled = environment_get_glow_enabled(p_environment);
		if (glow_enabled) {
			// If glow is enabled, we apply tonemapping etc. in post, so disable it during rendering
			apply_color_adjustments_in_post = true;
		}
	}

	// Assign render data
	// Use the format from rendererRD
	RenderDataGLES3 render_data;
	{
		render_data.render_buffers = rb;
		render_data.transparent_bg = rt ? rt->is_transparent : false;
		// Our first camera is used by default
		render_data.cam_transform = p_camera_data->main_transform;
		render_data.inv_cam_transform = render_data.cam_transform.affine_inverse();
		render_data.cam_projection = p_camera_data->main_projection;
		render_data.cam_orthogonal = p_camera_data->is_orthogonal;
		render_data.camera_visible_layers = p_camera_data->visible_layers;
		render_data.main_cam_transform = p_camera_data->main_transform;

		render_data.view_count = p_camera_data->view_count;
		for (uint32_t v = 0; v < p_camera_data->view_count; v++) {
			render_data.view_eye_offset[v] = p_camera_data->view_offset[v].origin;
			render_data.view_projection[v] = p_camera_data->view_projection[v];
		}

		render_data.z_near = p_camera_data->main_projection.get_z_near();
		render_data.z_far = p_camera_data->main_projection.get_z_far();

		render_data.instances = &p_instances;
		render_data.lights = &p_lights;
		render_data.reflection_probes = &p_reflection_probes;
		render_data.environment = p_environment;
		render_data.camera_attributes = p_camera_attributes;
		render_data.shadow_atlas = p_shadow_atlas;
		render_data.reflection_probe = p_reflection_probe;
		render_data.reflection_probe_pass = p_reflection_probe_pass;

		// this should be the same for all cameras..
		render_data.lod_distance_multiplier = p_camera_data->main_projection.get_lod_multiplier();

		if (rt != nullptr && rt->color_type == GL_UNSIGNED_INT_2_10_10_10_REV && glow_enabled) {
			// As our output is in sRGB and we're using 10bit color space, we can fake a little HDR to do glow...
			render_data.luminance_multiplier = 0.25;
		} else {
			render_data.luminance_multiplier = 1.0;
		}

		if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_DISABLE_LOD) {
			render_data.screen_mesh_lod_threshold = 0.0;
		} else {
			render_data.screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
		}
		render_data.render_info = r_render_info;
		render_data.render_shadows = p_render_shadows;
		render_data.render_shadow_count = p_render_shadow_count;
	}

	PagedArray<RID> empty;

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
		render_data.lights = &empty;
		render_data.reflection_probes = &empty;
	}

	bool reverse_cull = render_data.cam_transform.basis.determinant() < 0;

	///////////
	// Fill Light lists here
	//////////

	GLuint global_buffer = GLES3::MaterialStorage::get_singleton()->global_shader_parameters_get_uniform_buffer();
	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_GLOBALS_UNIFORM_LOCATION, global_buffer);

	Color clear_color;
	if (!is_reflection_probe && rb->render_target.is_valid()) {
		clear_color = texture_storage->render_target_get_clear_request_color(rb->render_target);
	} else {
		clear_color = texture_storage->get_default_clear_color();
	}

	bool fb_cleared = false;

	Size2i screen_size = rb->internal_size;

	bool use_wireframe = get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME;

	SceneState::TonemapUBO tonemap_ubo;
	if (render_data.environment.is_valid()) {
		bool use_bcs = environment_get_adjustments_enabled(render_data.environment);
		if (use_bcs) {
			apply_color_adjustments_in_post = true;
		}

		tonemap_ubo.exposure = environment_get_exposure(render_data.environment);
		tonemap_ubo.white = environment_get_white(render_data.environment);
		tonemap_ubo.tonemapper = int32_t(environment_get_tone_mapper(render_data.environment));

		tonemap_ubo.brightness = environment_get_adjustments_brightness(render_data.environment);
		tonemap_ubo.contrast = environment_get_adjustments_contrast(render_data.environment);
		tonemap_ubo.saturation = environment_get_adjustments_saturation(render_data.environment);
	}

	if (scene_state.tonemap_buffer == 0) {
		// Only create if using 3D
		glGenBuffers(1, &scene_state.tonemap_buffer);
		glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_TONEMAP_UNIFORM_LOCATION, scene_state.tonemap_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.tonemap_buffer, sizeof(SceneState::TonemapUBO), &tonemap_ubo, GL_STREAM_DRAW, "Tonemap UBO");
	} else {
		glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_TONEMAP_UNIFORM_LOCATION, scene_state.tonemap_buffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::TonemapUBO), &tonemap_ubo, GL_STREAM_DRAW);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	scene_state.ubo.emissive_exposure_normalization = -1.0; // Use default exposure normalization.

	bool flip_y = !is_reflection_probe;

	if (rt && rt->overridden.color.is_valid()) {
		// If we've overridden the render target's color texture, then don't render upside down.
		// We're probably rendering directly to an XR device.
		flip_y = false;
	}
	if (!flip_y) {
		// If we're rendering right-side up, then we need to change the winding order.
		glFrontFace(GL_CW);
	}
	_render_shadows(&render_data, screen_size);

	_setup_lights(&render_data, true, render_data.directional_light_count, render_data.omni_light_count, render_data.spot_light_count, render_data.directional_shadow_count);
	_setup_environment(&render_data, is_reflection_probe, screen_size, flip_y, clear_color, false);

	_fill_render_list(RENDER_LIST_OPAQUE, &render_data, PASS_MODE_COLOR);
	render_list[RENDER_LIST_OPAQUE].sort_by_key();
	render_list[RENDER_LIST_ALPHA].sort_by_reverse_depth_and_priority();

	bool draw_sky = false;
	bool draw_sky_fog_only = false;
	bool keep_color = false;
	float sky_energy_multiplier = 1.0;

	if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW)) {
		clear_color = Color(0, 0, 0, 1); //in overdraw mode, BG should always be black
	} else if (render_data.environment.is_valid()) {
		RS::EnvironmentBG bg_mode = environment_get_background(render_data.environment);
		float bg_energy_multiplier = environment_get_bg_energy_multiplier(render_data.environment);
		bg_energy_multiplier *= environment_get_bg_intensity(render_data.environment);
		RS::EnvironmentReflectionSource reflection_source = environment_get_reflection_source(render_data.environment);
		RS::EnvironmentAmbientSource ambient_source = environment_get_ambient_source(render_data.environment);

		if (render_data.camera_attributes.is_valid()) {
			bg_energy_multiplier *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(render_data.camera_attributes);
		}

		switch (bg_mode) {
			case RS::ENV_BG_CLEAR_COLOR: {
				clear_color.r *= bg_energy_multiplier;
				clear_color.g *= bg_energy_multiplier;
				clear_color.b *= bg_energy_multiplier;
				if (!render_data.transparent_bg && environment_get_fog_enabled(render_data.environment)) {
					draw_sky_fog_only = true;
					GLES3::MaterialStorage::get_singleton()->material_set_param(sky_globals.fog_material, "clear_color", Variant(clear_color));
				}
			} break;
			case RS::ENV_BG_COLOR: {
				clear_color = environment_get_bg_color(render_data.environment);
				clear_color.r *= bg_energy_multiplier;
				clear_color.g *= bg_energy_multiplier;
				clear_color.b *= bg_energy_multiplier;
				if (!render_data.transparent_bg && environment_get_fog_enabled(render_data.environment)) {
					draw_sky_fog_only = true;
					GLES3::MaterialStorage::get_singleton()->material_set_param(sky_globals.fog_material, "clear_color", Variant(clear_color));
				}
			} break;
			case RS::ENV_BG_SKY: {
				draw_sky = !render_data.transparent_bg;
			} break;
			case RS::ENV_BG_CANVAS: {
				keep_color = true;
			} break;
			case RS::ENV_BG_KEEP: {
				keep_color = true;
			} break;
			case RS::ENV_BG_CAMERA_FEED: {
			} break;
			default: {
			}
		}

		bool sky_reflections = reflection_source == RS::ENV_REFLECTION_SOURCE_SKY;
		sky_reflections |= reflection_source == RS::ENV_REFLECTION_SOURCE_BG && bg_mode == RS::ENV_BG_SKY;
		bool sky_ambient = ambient_source == RS::ENV_AMBIENT_SOURCE_SKY;
		sky_ambient |= ambient_source == RS::ENV_AMBIENT_SOURCE_BG && bg_mode == RS::ENV_BG_SKY;

		// setup sky if used for ambient, reflections, or background
		if (draw_sky || draw_sky_fog_only || sky_reflections || sky_ambient) {
			RENDER_TIMESTAMP("Setup Sky");
			Projection projection = render_data.cam_projection;
			if (is_reflection_probe) {
				Projection correction;
				correction.set_depth_correction(true, true, false);
				projection = correction * render_data.cam_projection;
			}

			sky_energy_multiplier *= bg_energy_multiplier;

			_setup_sky(&render_data, *render_data.lights, projection, render_data.cam_transform, screen_size);

			if (environment_get_sky(render_data.environment).is_valid()) {
				if (sky_reflections || sky_ambient) {
					_update_sky_radiance(render_data.environment, projection, render_data.cam_transform, sky_energy_multiplier);
				}
			} else {
				// do not try to draw sky if invalid
				draw_sky = false;
			}
		}
	}

	GLuint fbo = 0;
	if (is_reflection_probe) {
		fbo = GLES3::LightStorage::get_singleton()->reflection_probe_instance_get_framebuffer(render_data.reflection_probe, render_data.reflection_probe_pass);
	} else {
		rb->set_apply_color_adjustments_in_post(apply_color_adjustments_in_post);
		fbo = rb->get_render_fbo();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, rb->internal_size.x, rb->internal_size.y);

	scene_state.reset_gl_state();

	// Do depth prepass if it's explicitly enabled
	bool use_depth_prepass = config->use_depth_prepass;

	// Don't do depth prepass we are rendering overdraw
	use_depth_prepass = use_depth_prepass && get_debug_draw_mode() != RS::VIEWPORT_DEBUG_DRAW_OVERDRAW;

	if (use_depth_prepass) {
		RENDER_TIMESTAMP("Depth Prepass");
		//pre z pass

		scene_state.enable_gl_depth_test(true);
		scene_state.enable_gl_depth_draw(true);
		scene_state.enable_gl_blend(false);
		glDepthFunc(GL_GEQUAL);
		scene_state.enable_gl_scissor_test(false);

		glColorMask(0, 0, 0, 0);
		RasterizerGLES3::clear_depth(0.0);
		glClear(GL_DEPTH_BUFFER_BIT);
		// Some desktop GL implementations fall apart when using Multiview with GL_NONE.
		GLuint db = p_camera_data->view_count > 1 ? GL_COLOR_ATTACHMENT0 : GL_NONE;
		glDrawBuffers(1, &db);

		uint64_t spec_constant = SceneShaderGLES3::DISABLE_FOG | SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL |
				SceneShaderGLES3::DISABLE_LIGHTMAP | SceneShaderGLES3::DISABLE_LIGHT_OMNI |
				SceneShaderGLES3::DISABLE_LIGHT_SPOT;

		RenderListParameters render_list_params(render_list[RENDER_LIST_OPAQUE].elements.ptr(), render_list[RENDER_LIST_OPAQUE].elements.size(), reverse_cull, spec_constant, use_wireframe);
		_render_list_template<PASS_MODE_DEPTH>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());

		glColorMask(1, 1, 1, 1);

		fb_cleared = true;
		scene_state.used_depth_prepass = true;
	} else {
		scene_state.used_depth_prepass = false;
	}

	glBlendEquation(GL_FUNC_ADD);
	if (render_data.transparent_bg) {
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		scene_state.enable_gl_blend(true);
	} else {
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
		scene_state.enable_gl_blend(false);
	}
	scene_state.current_blend_mode = GLES3::SceneShaderData::BLEND_MODE_MIX;

	scene_state.enable_gl_scissor_test(false);
	scene_state.enable_gl_depth_test(true);
	scene_state.enable_gl_depth_draw(true);
	glDepthFunc(GL_GEQUAL);

	{
		GLuint db = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &db);
	}

	if (!fb_cleared) {
		RasterizerGLES3::clear_depth(0.0);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	if (!keep_color) {
		clear_color.a = render_data.transparent_bg ? 0.0f : 1.0f;
		glClearBufferfv(GL_COLOR, 0, clear_color.components);
	} else if (fbo != rt->fbo) {
		// Need to copy our current contents to our intermediate/MSAA buffer
		GLES3::CopyEffects *copy_effects = GLES3::CopyEffects::get_singleton();

		scene_state.enable_gl_depth_test(false);
		scene_state.enable_gl_depth_draw(false);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(rt->view_count > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, rt->color);

		copy_effects->copy_screen(render_data.luminance_multiplier);

		scene_state.enable_gl_depth_test(true);
		scene_state.enable_gl_depth_draw(true);
	}

	RENDER_TIMESTAMP("Render Opaque Pass");
	uint64_t spec_constant_base_flags = 0;

	{
		// Specialization Constants that apply for entire rendering pass.
		if (render_data.directional_light_count == 0) {
			spec_constant_base_flags |= SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL;
		}

		if (render_data.environment.is_null() || (render_data.environment.is_valid() && !environment_get_fog_enabled(render_data.environment))) {
			spec_constant_base_flags |= SceneShaderGLES3::DISABLE_FOG;
		}

		if (render_data.environment.is_valid() && environment_get_fog_mode(render_data.environment) == RS::EnvironmentFogMode::ENV_FOG_MODE_DEPTH) {
			spec_constant_base_flags |= SceneShaderGLES3::USE_DEPTH_FOG;
		}

		if (!apply_color_adjustments_in_post) {
			spec_constant_base_flags |= SceneShaderGLES3::APPLY_TONEMAPPING;
		}
	}
	// Render Opaque Objects.
	RenderListParameters render_list_params(render_list[RENDER_LIST_OPAQUE].elements.ptr(), render_list[RENDER_LIST_OPAQUE].elements.size(), reverse_cull, spec_constant_base_flags, use_wireframe);

	_render_list_template<PASS_MODE_COLOR>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());

	scene_state.enable_gl_depth_draw(false);

	if (draw_sky || draw_sky_fog_only) {
		RENDER_TIMESTAMP("Render Sky");

		scene_state.enable_gl_depth_test(true);
		scene_state.enable_gl_blend(false);
		scene_state.set_gl_cull_mode(GLES3::SceneShaderData::CULL_BACK);

		Projection projection = render_data.cam_projection;
		if (is_reflection_probe) {
			Projection correction;
			correction.columns[1][1] = -1.0;
			projection = correction * render_data.cam_projection;
		}

		_draw_sky(render_data.environment, projection, render_data.cam_transform, sky_energy_multiplier, render_data.luminance_multiplier, p_camera_data->view_count > 1, flip_y, apply_color_adjustments_in_post);
	}

	if (rt && (scene_state.used_screen_texture || scene_state.used_depth_texture)) {
		Size2i size;
		GLuint backbuffer_fbo = 0;
		GLuint backbuffer = 0;
		GLuint backbuffer_depth = 0;

		if (rb->get_scaling_3d_mode() == RS::VIEWPORT_SCALING_3D_MODE_OFF) {
			texture_storage->check_backbuffer(rt, scene_state.used_screen_texture, scene_state.used_depth_texture); // note, badly names, this just allocates!

			size = rt->size;
			backbuffer_fbo = rt->backbuffer_fbo;
			backbuffer = rt->backbuffer;
			backbuffer_depth = rt->backbuffer_depth;
		} else {
			rb->check_backbuffer(scene_state.used_screen_texture, scene_state.used_depth_texture);
			size = rb->get_internal_size();
			backbuffer_fbo = rb->get_backbuffer_fbo();
			backbuffer = rb->get_backbuffer();
			backbuffer_depth = rb->get_backbuffer_depth();
		}

		if (backbuffer_fbo != 0) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backbuffer_fbo);
			if (scene_state.used_screen_texture) {
				glBlitFramebuffer(0, 0, size.x, size.y,
						0, 0, size.x, size.y,
						GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 5);
				glBindTexture(GL_TEXTURE_2D, backbuffer);
			}
			if (scene_state.used_depth_texture) {
				glBlitFramebuffer(0, 0, size.x, size.y,
						0, 0, size.x, size.y,
						GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 6);
				glBindTexture(GL_TEXTURE_2D, backbuffer_depth);
			}
		}

		// Bound framebuffer may have changed, so change it back
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	}

	RENDER_TIMESTAMP("Render 3D Transparent Pass");
	scene_state.enable_gl_blend(true);

	//Render transparent pass
	RenderListParameters render_list_params_alpha(render_list[RENDER_LIST_ALPHA].elements.ptr(), render_list[RENDER_LIST_ALPHA].elements.size(), reverse_cull, spec_constant_base_flags, use_wireframe);

	_render_list_template<PASS_MODE_COLOR_TRANSPARENT>(&render_list_params_alpha, &render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size(), true);

	if (!flip_y) {
		// Restore the default winding order.
		glFrontFace(GL_CCW);
	}

	if (!is_reflection_probe && rb.is_valid()) {
		_render_buffers_debug_draw(rb, p_shadow_atlas, fbo);
	}

	// Reset stuff that may trip up the next process.
	scene_state.reset_gl_state();
	glUseProgram(0);

	if (!is_reflection_probe) {
		_render_post_processing(&render_data);

		texture_storage->render_target_disable_clear_request(rb->render_target);
	}

	glActiveTexture(GL_TEXTURE0);
}

void RasterizerSceneGLES3::_render_post_processing(const RenderDataGLES3 *p_render_data) {
	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	GLES3::Glow *glow = GLES3::Glow::get_singleton();
	GLES3::PostEffects *post_effects = GLES3::PostEffects::get_singleton();

	Ref<RenderSceneBuffersGLES3> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	RID render_target = rb->get_render_target();
	Size2i internal_size = rb->get_internal_size();
	Size2i target_size = rb->get_target_size();
	uint32_t view_count = rb->get_view_count();

	// bool msaa2d_needs_resolve = texture_storage->render_target_get_msaa(render_target) != RS::VIEWPORT_MSAA_DISABLED && !GLES3::Config::get_singleton()->rt_msaa_supported;
	bool msaa3d_needs_resolve = rb->get_msaa_needs_resolve();
	GLuint fbo_msaa_3d = rb->get_msaa3d_fbo();
	GLuint fbo_int = rb->get_internal_fbo();
	GLuint fbo_rt = texture_storage->render_target_get_fbo(render_target); // TODO if MSAA 2D is enabled and we're not using rt_msaa, get 2D render target here.

	// Check if we have glow enabled and if so, check if our buffers were allocated
	bool glow_enabled = false;
	float glow_intensity = 1.0;
	float glow_bloom = 0.0;
	float glow_hdr_bleed_threshold = 1.0;
	float glow_hdr_bleed_scale = 2.0;
	float glow_hdr_luminance_cap = 12.0;
	if (p_render_data->environment.is_valid()) {
		glow_enabled = environment_get_glow_enabled(p_render_data->environment);
		glow_intensity = environment_get_glow_intensity(p_render_data->environment);
		glow_bloom = environment_get_glow_bloom(p_render_data->environment);
		glow_hdr_bleed_threshold = environment_get_glow_hdr_bleed_threshold(p_render_data->environment);
		glow_hdr_bleed_scale = environment_get_glow_hdr_bleed_scale(p_render_data->environment);
		glow_hdr_luminance_cap = environment_get_glow_hdr_luminance_cap(p_render_data->environment);
	}

	if (glow_enabled) {
		rb->check_glow_buffers();
	}

	uint64_t bcs_spec_constants = 0;
	if (p_render_data->environment.is_valid()) {
		bool use_bcs = environment_get_adjustments_enabled(p_render_data->environment);
		RID color_correction_texture = environment_get_color_correction(p_render_data->environment);
		if (use_bcs) {
			bcs_spec_constants |= PostShaderGLES3::USE_BCS;

			if (color_correction_texture.is_valid()) {
				bcs_spec_constants |= PostShaderGLES3::USE_COLOR_CORRECTION;

				bool use_1d_lut = environment_get_use_1d_color_correction(p_render_data->environment);
				GLenum texture_target = GL_TEXTURE_3D;
				if (use_1d_lut) {
					bcs_spec_constants |= PostShaderGLES3::USE_1D_LUT;
					texture_target = GL_TEXTURE_2D;
				}

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(texture_target, texture_storage->texture_get_texid(color_correction_texture));
				glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(texture_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}
		}
	}

	if (view_count == 1) {
		// Resolve if needed.
		if (fbo_msaa_3d != 0 && msaa3d_needs_resolve) {
			// We can use blit to copy things over
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_msaa_3d);

			if (fbo_int != 0) {
				// We can't combine resolve and scaling, so resolve into our internal buffer
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_int);
			} else {
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_rt);
			}
			glBlitFramebuffer(0, 0, internal_size.x, internal_size.y, 0, 0, internal_size.x, internal_size.y, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		// Rendered to intermediate buffer, must copy to our render target
		if (fbo_int != 0) {
			// Apply glow/bloom if requested? then populate our glow buffers
			GLuint color = fbo_int != 0 ? rb->get_internal_color() : texture_storage->render_target_get_color(render_target);
			const GLES3::Glow::GLOWLEVEL *glow_buffers = nullptr;
			if (glow_enabled) {
				glow_buffers = rb->get_glow_buffers();

				glow->set_luminance_multiplier(p_render_data->luminance_multiplier);

				glow->set_intensity(glow_intensity);
				glow->set_glow_bloom(glow_bloom);
				glow->set_glow_hdr_bleed_threshold(glow_hdr_bleed_threshold);
				glow->set_glow_hdr_bleed_scale(glow_hdr_bleed_scale);
				glow->set_glow_hdr_luminance_cap(glow_hdr_luminance_cap);

				glow->process_glow(color, internal_size, glow_buffers);
			}

			// Copy color buffer
			post_effects->post_copy(fbo_rt, target_size, color, internal_size, p_render_data->luminance_multiplier, glow_buffers, glow_intensity, 0, false, bcs_spec_constants);

			// Copy depth buffer
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_int);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_rt);
			glBlitFramebuffer(0, 0, internal_size.x, internal_size.y, 0, 0, target_size.x, target_size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_rt);
	} else if ((fbo_msaa_3d != 0 && msaa3d_needs_resolve) || (fbo_int != 0)) {
		// TODO investigate if it's smarter to cache these FBOs
		GLuint fbos[3]; // read, write and post
		glGenFramebuffers(3, fbos);

		// Resolve if needed.
		if (fbo_msaa_3d != 0 && msaa3d_needs_resolve) {
			GLuint read_color = rb->get_msaa3d_color();
			GLuint read_depth = rb->get_msaa3d_depth();
			GLuint write_color = 0;
			GLuint write_depth = 0;

			if (fbo_int != 0) {
				write_color = rb->get_internal_color();
				write_depth = rb->get_internal_depth();
			} else {
				write_color = texture_storage->render_target_get_color(render_target);
				write_depth = texture_storage->render_target_get_depth(render_target);
			}

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);

			for (uint32_t v = 0; v < view_count; v++) {
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, read_color, 0, v);
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, read_depth, 0, v);
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, write_color, 0, v);
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, write_depth, 0, v);
				glBlitFramebuffer(0, 0, internal_size.x, internal_size.y, 0, 0, internal_size.x, internal_size.y, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			}
		}

		// Rendered to intermediate buffer, must copy to our render target
		if (fbo_int != 0) {
			// Apply glow/bloom if requested? then populate our glow buffers
			const GLES3::Glow::GLOWLEVEL *glow_buffers = nullptr;
			GLuint source_color = fbo_int != 0 ? rb->get_internal_color() : texture_storage->render_target_get_color(render_target);

			if (glow_enabled) {
				glow_buffers = rb->get_glow_buffers();

				glow->set_luminance_multiplier(p_render_data->luminance_multiplier);

				glow->set_intensity(glow_intensity);
				glow->set_glow_bloom(glow_bloom);
				glow->set_glow_hdr_bleed_threshold(glow_hdr_bleed_threshold);
				glow->set_glow_hdr_bleed_scale(glow_hdr_bleed_scale);
				glow->set_glow_hdr_luminance_cap(glow_hdr_luminance_cap);
			}

			GLuint write_color = texture_storage->render_target_get_color(render_target);

			for (uint32_t v = 0; v < view_count; v++) {
				if (glow_enabled) {
					glow->process_glow(source_color, internal_size, glow_buffers, v, true);
				}

				glBindFramebuffer(GL_FRAMEBUFFER, fbos[2]);
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, write_color, 0, v);
				post_effects->post_copy(fbos[2], target_size, source_color, internal_size, p_render_data->luminance_multiplier, glow_buffers, glow_intensity, v, true, bcs_spec_constants);
			}

			// Copy depth
			GLuint read_depth = rb->get_internal_depth();
			GLuint write_depth = texture_storage->render_target_get_depth(render_target);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);

			for (uint32_t v = 0; v < view_count; v++) {
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, read_depth, 0, v);
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, write_depth, 0, v);

				glBlitFramebuffer(0, 0, internal_size.x, internal_size.y, 0, 0, target_size.x, target_size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_rt);
		glDeleteFramebuffers(3, fbos);
	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
}

template <PassMode p_pass_mode>
void RasterizerSceneGLES3::_render_list_template(RenderListParameters *p_params, const RenderDataGLES3 *p_render_data, uint32_t p_from_element, uint32_t p_to_element, bool p_alpha_pass) {
	GLES3::MeshStorage *mesh_storage = GLES3::MeshStorage::get_singleton();
	GLES3::ParticlesStorage *particles_storage = GLES3::ParticlesStorage::get_singleton();
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();

	GLuint prev_vertex_array_gl = 0;
	GLuint prev_index_array_gl = 0;

	GLES3::SceneMaterialData *prev_material_data = nullptr;
	GLES3::SceneShaderData *prev_shader = nullptr;
	GeometryInstanceGLES3 *prev_inst = nullptr;
	SceneShaderGLES3::ShaderVariant prev_variant = SceneShaderGLES3::ShaderVariant::MODE_COLOR;
	SceneShaderGLES3::ShaderVariant shader_variant = SceneShaderGLES3::MODE_COLOR; // Assigned to silence wrong -Wmaybe-initialized
	uint64_t prev_spec_constants = 0;

	// Specializations constants used by all instances in the scene.
	uint64_t base_spec_constants = p_params->spec_constant_base_flags;

	if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
		GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
		GLES3::Config *config = GLES3::Config::get_singleton();
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 2);
		GLuint texture_to_bind = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES3::DEFAULT_GL_TEXTURE_CUBEMAP_BLACK))->tex_id;
		if (p_render_data->environment.is_valid()) {
			Sky *sky = sky_owner.get_or_null(environment_get_sky(p_render_data->environment));
			if (sky && sky->radiance != 0) {
				texture_to_bind = sky->radiance;
				base_spec_constants |= SceneShaderGLES3::USE_RADIANCE_MAP;
			}
			glBindTexture(GL_TEXTURE_CUBE_MAP, texture_to_bind);
		}

	} else if constexpr (p_pass_mode == PASS_MODE_DEPTH || p_pass_mode == PASS_MODE_SHADOW) {
		shader_variant = SceneShaderGLES3::MODE_DEPTH;
	}

	if (p_render_data->view_count > 1) {
		base_spec_constants |= SceneShaderGLES3::USE_MULTIVIEW;
	}

	bool should_request_redraw = false;
	if constexpr (p_pass_mode != PASS_MODE_DEPTH) {
		// Don't count elements during depth pre-pass to match the RD renderers.
		if (p_render_data->render_info) {
			p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += p_to_element - p_from_element;
		}
	}

	for (uint32_t i = p_from_element; i < p_to_element; i++) {
		GeometryInstanceSurface *surf = p_params->elements[i];
		GeometryInstanceGLES3 *inst = surf->owner;

		if (p_pass_mode == PASS_MODE_COLOR && !(surf->flags & GeometryInstanceSurface::FLAG_PASS_OPAQUE)) {
			continue; // Objects with "Depth-prepass" transparency are included in both render lists, but should only be rendered in the transparent pass
		}

		if (inst->instance_count == 0) {
			continue;
		}

		GLES3::SceneShaderData *shader;
		GLES3::SceneMaterialData *material_data;
		void *mesh_surface;

		if constexpr (p_pass_mode == PASS_MODE_SHADOW) {
			shader = surf->shader_shadow;
			material_data = surf->material_shadow;
			mesh_surface = surf->surface_shadow;
		} else {
			if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW)) {
				material_data = overdraw_material_data_ptr;
				shader = material_data->shader_data;
			} else if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_LIGHTING)) {
				material_data = default_material_data_ptr;
				shader = material_data->shader_data;
			} else {
				shader = surf->shader;
				material_data = surf->material;
			}
			mesh_surface = surf->surface;
		}

		if (!mesh_surface) {
			continue;
		}

		//request a redraw if one of the shaders uses TIME
		if (shader->uses_time) {
			should_request_redraw = true;
		}

		if constexpr (p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
			scene_state.enable_gl_depth_test(shader->depth_test == GLES3::SceneShaderData::DEPTH_TEST_ENABLED);
		}

		if constexpr (p_pass_mode != PASS_MODE_SHADOW) {
			if (shader->depth_draw == GLES3::SceneShaderData::DEPTH_DRAW_OPAQUE) {
				scene_state.enable_gl_depth_draw((p_pass_mode == PASS_MODE_COLOR && !GLES3::Config::get_singleton()->use_depth_prepass) || p_pass_mode == PASS_MODE_DEPTH);
			} else {
				scene_state.enable_gl_depth_draw(shader->depth_draw == GLES3::SceneShaderData::DEPTH_DRAW_ALWAYS);
			}
		}

		bool uses_additive_lighting = (inst->light_passes.size() + p_render_data->directional_shadow_count) > 0;
		uses_additive_lighting = uses_additive_lighting && !shader->unshaded;

		// TODOS
		/*
		 * Still a bug when atlas space is limited. Somehow need to evict light when it doesn't have a spot on the atlas, current check isn't enough
		 * Disable depth draw
		 */

		for (int32_t pass = 0; pass < MAX(1, int32_t(inst->light_passes.size() + p_render_data->directional_shadow_count)); pass++) {
			if constexpr (p_pass_mode == PASS_MODE_DEPTH || p_pass_mode == PASS_MODE_SHADOW) {
				if (pass > 0) {
					// Don't render shadow passes when doing depth or shadow pass.
					break;
				}
			}

			if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
				if (!uses_additive_lighting && pass == 1) {
					// Don't render additive passes if not using additive lighting.
					break;
				}
				if (uses_additive_lighting && pass == 1 && !p_render_data->transparent_bg) {
					// Enable blending if in opaque pass and not already enabled.
					scene_state.enable_gl_blend(true);
				}
				if (pass < int32_t(inst->light_passes.size())) {
					RID light_instance_rid = inst->light_passes[pass].light_instance_rid;
					if (!GLES3::LightStorage::get_singleton()->light_instance_has_shadow_atlas(light_instance_rid, p_render_data->shadow_atlas)) {
						// Shadow wasn't able to get a spot on the atlas. So skip it.
						continue;
					}
				} else if (pass > 0) {
					uint32_t shadow_id = MAX_DIRECTIONAL_LIGHTS - 1 - (pass - int32_t(inst->light_passes.size()));
					if (inst->lightmap_instance.is_valid() && scene_state.directional_lights[shadow_id].bake_mode == RenderingServer::LIGHT_BAKE_STATIC) {
						// Skip shadows for static lights on meshes with a lightmap.
						continue;
					}
				}
			}

			if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
				GLES3::SceneShaderData::BlendMode desired_blend_mode;
				if (pass > 0) {
					desired_blend_mode = GLES3::SceneShaderData::BLEND_MODE_ADD;
				} else {
					desired_blend_mode = shader->blend_mode;
				}

				if (desired_blend_mode != scene_state.current_blend_mode) {
					switch (desired_blend_mode) {
						case GLES3::SceneShaderData::BLEND_MODE_MIX: {
							glBlendEquation(GL_FUNC_ADD);
							if (p_render_data->transparent_bg) {
								glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
							} else {
								glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
							}

						} break;
						case GLES3::SceneShaderData::BLEND_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(p_pass_mode == PASS_MODE_COLOR_TRANSPARENT ? GL_SRC_ALPHA : GL_ONE, GL_ONE);

						} break;
						case GLES3::SceneShaderData::BLEND_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case GLES3::SceneShaderData::BLEND_MODE_MUL: {
							glBlendEquation(GL_FUNC_ADD);
							if (p_render_data->transparent_bg) {
								glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
							} else {
								glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
							}

						} break;
						case GLES3::SceneShaderData::BLEND_MODE_PREMULT_ALPHA: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

						} break;
						case GLES3::SceneShaderData::BLEND_MODE_ALPHA_TO_COVERAGE: {
							// Do nothing for now.
						} break;
					}
					scene_state.current_blend_mode = desired_blend_mode;
				}
			}

			// Find cull variant.
			GLES3::SceneShaderData::Cull cull_mode = shader->cull_mode;

			if (p_pass_mode == PASS_MODE_MATERIAL || (surf->flags & GeometryInstanceSurface::FLAG_USES_DOUBLE_SIDED_SHADOWS)) {
				cull_mode = GLES3::SceneShaderData::CULL_DISABLED;
			} else {
				bool mirror = inst->mirror;
				if (p_params->reverse_cull) {
					mirror = !mirror;
				}
				if (cull_mode == GLES3::SceneShaderData::CULL_FRONT && mirror) {
					cull_mode = GLES3::SceneShaderData::CULL_BACK;
				} else if (cull_mode == GLES3::SceneShaderData::CULL_BACK && mirror) {
					cull_mode = GLES3::SceneShaderData::CULL_FRONT;
				}
			}

			scene_state.set_gl_cull_mode(cull_mode);

			RS::PrimitiveType primitive = surf->primitive;
			if (shader->uses_point_size) {
				primitive = RS::PRIMITIVE_POINTS;
			}
			static const GLenum prim[5] = { GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP };
			GLenum primitive_gl = prim[int(primitive)];

			GLuint vertex_array_gl = 0;
			GLuint index_array_gl = 0;
			uint64_t vertex_input_mask = shader->vertex_input_mask;
			if (inst->lightmap_instance.is_valid() || p_pass_mode == PASS_MODE_MATERIAL) {
				vertex_input_mask |= 1 << RS::ARRAY_TEX_UV2;
			}

			// Skeleton and blend shapes.
			if (surf->owner->mesh_instance.is_valid()) {
				mesh_storage->mesh_instance_surface_get_vertex_arrays_and_format(surf->owner->mesh_instance, surf->surface_index, vertex_input_mask, vertex_array_gl);
			} else {
				mesh_storage->mesh_surface_get_vertex_arrays_and_format(mesh_surface, vertex_input_mask, vertex_array_gl);
			}

			index_array_gl = mesh_storage->mesh_surface_get_index_buffer(mesh_surface, surf->lod_index);

			if (prev_vertex_array_gl != vertex_array_gl) {
				if (vertex_array_gl != 0) {
					glBindVertexArray(vertex_array_gl);
				}
				prev_vertex_array_gl = vertex_array_gl;

				// Invalidate the previous index array
				prev_index_array_gl = 0;
			}

			bool use_wireframe = false;
			if (p_params->force_wireframe) {
				GLuint wireframe_index_array_gl = mesh_storage->mesh_surface_get_index_buffer_wireframe(mesh_surface);
				if (wireframe_index_array_gl) {
					index_array_gl = wireframe_index_array_gl;
					use_wireframe = true;
				}
			}

			bool use_index_buffer = index_array_gl != 0;
			if (prev_index_array_gl != index_array_gl) {
				if (index_array_gl != 0) {
					// Bind index each time so we can use LODs
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_gl);
				}
				prev_index_array_gl = index_array_gl;
			}

			Transform3D world_transform;
			if (inst->store_transform_cache) {
				world_transform = inst->transform;
			}

			if (prev_material_data != material_data) {
				material_data->bind_uniforms();
				prev_material_data = material_data;
			}

			SceneShaderGLES3::ShaderVariant instance_variant = shader_variant;

			if (inst->instance_count > 0) {
				// Will need to use instancing to draw (either MultiMesh or Particles).
				instance_variant = SceneShaderGLES3::ShaderVariant(1 + int(instance_variant));
			}

			uint64_t spec_constants = base_spec_constants;

			// Set up spec constants for lighting.
			if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
				// Only check during color passes as light shader code is compiled out during depth-only pass anyway.

				if (pass == 0) {
					spec_constants |= SceneShaderGLES3::BASE_PASS;

					if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
						spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_OMNI;
						spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_SPOT;
						spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL;
						spec_constants |= SceneShaderGLES3::DISABLE_LIGHTMAP;
					} else {
						if (inst->omni_light_gl_cache.size() == 0) {
							spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_OMNI;
						}

						if (inst->spot_light_gl_cache.size() == 0) {
							spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_SPOT;
						}

						if (p_render_data->directional_light_count == p_render_data->directional_shadow_count) {
							spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL;
						}

						if (inst->reflection_probe_rid_cache.size() == 0) {
							// We don't have any probes.
							spec_constants |= SceneShaderGLES3::DISABLE_REFLECTION_PROBE;
						} else if (inst->reflection_probe_rid_cache.size() > 1) {
							// We have a second probe.
							spec_constants |= SceneShaderGLES3::SECOND_REFLECTION_PROBE;
						}

						if (inst->lightmap_instance.is_valid()) {
							spec_constants |= SceneShaderGLES3::USE_LIGHTMAP;

							GLES3::LightmapInstance *li = GLES3::LightStorage::get_singleton()->get_lightmap_instance(inst->lightmap_instance);
							GLES3::Lightmap *lm = GLES3::LightStorage::get_singleton()->get_lightmap(li->lightmap);

							if (lm->uses_spherical_harmonics) {
								spec_constants |= SceneShaderGLES3::USE_SH_LIGHTMAP;
							}

							if (lightmap_bicubic_upscale) {
								spec_constants |= SceneShaderGLES3::LIGHTMAP_BICUBIC_FILTER;
							}
						} else if (inst->lightmap_sh) {
							spec_constants |= SceneShaderGLES3::USE_LIGHTMAP_CAPTURE;
						} else {
							spec_constants |= SceneShaderGLES3::DISABLE_LIGHTMAP;
						}
					}
				} else {
					// Only base pass uses the radiance map.
					spec_constants &= ~SceneShaderGLES3::USE_RADIANCE_MAP;
					spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_OMNI;
					spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_SPOT;
					spec_constants |= SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL;
					spec_constants |= SceneShaderGLES3::DISABLE_LIGHTMAP;
					spec_constants |= SceneShaderGLES3::DISABLE_REFLECTION_PROBE;
				}

				if (uses_additive_lighting) {
					spec_constants |= SceneShaderGLES3::USE_ADDITIVE_LIGHTING;

					if (pass < int32_t(inst->light_passes.size())) {
						// Rendering positional lights.
						if (inst->light_passes[pass].is_omni) {
							spec_constants |= SceneShaderGLES3::ADDITIVE_OMNI;
						} else {
							spec_constants |= SceneShaderGLES3::ADDITIVE_SPOT;
						}

						if (scene_state.positional_shadow_quality >= RS::SHADOW_QUALITY_SOFT_HIGH) {
							spec_constants |= SceneShaderGLES3::SHADOW_MODE_PCF_13;
						} else if (scene_state.positional_shadow_quality >= RS::SHADOW_QUALITY_SOFT_LOW) {
							spec_constants |= SceneShaderGLES3::SHADOW_MODE_PCF_5;
						}
					} else {
						// Render directional lights.

						uint32_t shadow_id = MAX_DIRECTIONAL_LIGHTS - 1 - (pass - int32_t(inst->light_passes.size()));
						if (pass == 0 && inst->lightmap_instance.is_valid() && scene_state.directional_lights[shadow_id].bake_mode == RenderingServer::LIGHT_BAKE_STATIC) {
							// Disable additive lighting with a static light and a lightmap.
							spec_constants &= ~SceneShaderGLES3::USE_ADDITIVE_LIGHTING;
						}
						if (scene_state.directional_shadows[shadow_id].shadow_split_offsets[0] == scene_state.directional_shadows[shadow_id].shadow_split_offsets[1]) {
							// Orthogonal, do nothing.
						} else if (scene_state.directional_shadows[shadow_id].shadow_split_offsets[1] == scene_state.directional_shadows[shadow_id].shadow_split_offsets[2]) {
							spec_constants |= SceneShaderGLES3::LIGHT_USE_PSSM2;
						} else {
							spec_constants |= SceneShaderGLES3::LIGHT_USE_PSSM4;
						}

						if (scene_state.directional_shadows[shadow_id].blend_splits) {
							spec_constants |= SceneShaderGLES3::LIGHT_USE_PSSM_BLEND;
						}

						if (scene_state.directional_shadow_quality >= RS::SHADOW_QUALITY_SOFT_HIGH) {
							spec_constants |= SceneShaderGLES3::SHADOW_MODE_PCF_13;
						} else if (scene_state.directional_shadow_quality >= RS::SHADOW_QUALITY_SOFT_LOW) {
							spec_constants |= SceneShaderGLES3::SHADOW_MODE_PCF_5;
						}
					}
				}
			}

			if (prev_shader != shader || prev_variant != instance_variant || spec_constants != prev_spec_constants) {
				bool success = material_storage->shaders.scene_shader.version_bind_shader(shader->version, instance_variant, spec_constants);
				if (!success) {
					break;
				}

				float opaque_prepass_threshold = 0.0;
				if constexpr (p_pass_mode == PASS_MODE_DEPTH) {
					opaque_prepass_threshold = 0.99;
				} else if constexpr (p_pass_mode == PASS_MODE_SHADOW) {
					opaque_prepass_threshold = 0.1;
				}

				material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::OPAQUE_PREPASS_THRESHOLD, opaque_prepass_threshold, shader->version, instance_variant, spec_constants);

				prev_shader = shader;
				prev_variant = instance_variant;
				prev_spec_constants = spec_constants;
			}

			// Pass in lighting uniforms.
			if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
				GLES3::Config *config = GLES3::Config::get_singleton();
				// Pass light and shadow index and bind shadow texture.
				if (uses_additive_lighting) {
					if (pass < int32_t(inst->light_passes.size())) {
						int32_t shadow_id = inst->light_passes[pass].shadow_id;
						if (shadow_id >= 0) {
							uint32_t light_id = inst->light_passes[pass].light_id;
							bool is_omni = inst->light_passes[pass].is_omni;
							SceneShaderGLES3::Uniforms uniform_name = is_omni ? SceneShaderGLES3::OMNI_LIGHT_INDEX : SceneShaderGLES3::SPOT_LIGHT_INDEX;
							material_storage->shaders.scene_shader.version_set_uniform(uniform_name, uint32_t(light_id), shader->version, instance_variant, spec_constants);
							material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::POSITIONAL_SHADOW_INDEX, uint32_t(shadow_id), shader->version, instance_variant, spec_constants);

							glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 3);
							RID light_instance_rid = inst->light_passes[pass].light_instance_rid;

							GLuint tex = GLES3::LightStorage::get_singleton()->light_instance_get_shadow_texture(light_instance_rid, p_render_data->shadow_atlas);
							if (is_omni) {
								glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
							} else {
								glBindTexture(GL_TEXTURE_2D, tex);
							}
						}
					} else {
						uint32_t shadow_id = MAX_DIRECTIONAL_LIGHTS - 1 - (pass - int32_t(inst->light_passes.size()));
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::DIRECTIONAL_SHADOW_INDEX, shadow_id, shader->version, instance_variant, spec_constants);

						GLuint tex = GLES3::LightStorage::get_singleton()->directional_shadow_get_texture();
						glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 3);
						glBindTexture(GL_TEXTURE_2D, tex);
					}
				}

				// Pass light count and array of light indices for base pass.
				if ((prev_inst != inst || prev_shader != shader || prev_variant != instance_variant) && pass == 0) {
					// Rebind the light indices.
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::OMNI_LIGHT_COUNT, inst->omni_light_gl_cache.size(), shader->version, instance_variant, spec_constants);
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::SPOT_LIGHT_COUNT, inst->spot_light_gl_cache.size(), shader->version, instance_variant, spec_constants);

					if (inst->omni_light_gl_cache.size()) {
						glUniform1uiv(material_storage->shaders.scene_shader.version_get_uniform(SceneShaderGLES3::OMNI_LIGHT_INDICES, shader->version, instance_variant, spec_constants), inst->omni_light_gl_cache.size(), inst->omni_light_gl_cache.ptr());
					}

					if (inst->spot_light_gl_cache.size()) {
						glUniform1uiv(material_storage->shaders.scene_shader.version_get_uniform(SceneShaderGLES3::SPOT_LIGHT_INDICES, shader->version, instance_variant, spec_constants), inst->spot_light_gl_cache.size(), inst->spot_light_gl_cache.ptr());
					}

					if (inst->lightmap_instance.is_valid()) {
						GLES3::LightmapInstance *li = GLES3::LightStorage::get_singleton()->get_lightmap_instance(inst->lightmap_instance);
						GLES3::Lightmap *lm = GLES3::LightStorage::get_singleton()->get_lightmap(li->lightmap);

						GLuint tex = GLES3::TextureStorage::get_singleton()->texture_get_texid(lm->light_texture);
						glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 4);
						glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::LIGHTMAP_SLICE, inst->lightmap_slice_index, shader->version, instance_variant, spec_constants);

						Vector4 uv_scale(inst->lightmap_uv_scale.position.x, inst->lightmap_uv_scale.position.y, inst->lightmap_uv_scale.size.x, inst->lightmap_uv_scale.size.y);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::LIGHTMAP_UV_SCALE, uv_scale, shader->version, instance_variant, spec_constants);

						if (lightmap_bicubic_upscale) {
							Vector2 light_texture_size(lm->light_texture_size.x, lm->light_texture_size.y);
							material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::LIGHTMAP_TEXTURE_SIZE, light_texture_size, shader->version, instance_variant, spec_constants);
						}

						float exposure_normalization = 1.0;
						if (p_render_data->camera_attributes.is_valid()) {
							float enf = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
							exposure_normalization = enf / lm->baked_exposure;
						}
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::LIGHTMAP_EXPOSURE_NORMALIZATION, exposure_normalization, shader->version, instance_variant, spec_constants);

						if (lm->uses_spherical_harmonics) {
							Basis to_lm = li->transform.basis.inverse() * p_render_data->cam_transform.basis;
							to_lm = to_lm.inverse().transposed();
							GLfloat matrix[9] = {
								(GLfloat)to_lm.rows[0][0],
								(GLfloat)to_lm.rows[1][0],
								(GLfloat)to_lm.rows[2][0],
								(GLfloat)to_lm.rows[0][1],
								(GLfloat)to_lm.rows[1][1],
								(GLfloat)to_lm.rows[2][1],
								(GLfloat)to_lm.rows[0][2],
								(GLfloat)to_lm.rows[1][2],
								(GLfloat)to_lm.rows[2][2],
							};
							glUniformMatrix3fv(material_storage->shaders.scene_shader.version_get_uniform(SceneShaderGLES3::LIGHTMAP_NORMAL_XFORM, shader->version, instance_variant, spec_constants), 1, GL_FALSE, matrix);
						}
					} else if (inst->lightmap_sh) {
						glUniform4fv(material_storage->shaders.scene_shader.version_get_uniform(SceneShaderGLES3::LIGHTMAP_CAPTURES, shader->version, instance_variant, spec_constants), 9, reinterpret_cast<const GLfloat *>(inst->lightmap_sh->sh));
					}

					prev_inst = inst;
				}
			}

			// Pass in reflection probe data
			if constexpr (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
				if (pass == 0 && inst->reflection_probe_rid_cache.size() > 0) {
					GLES3::Config *config = GLES3::Config::get_singleton();
					GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();

					// Setup first probe.
					{
						RID probe_rid = light_storage->reflection_probe_instance_get_probe(inst->reflection_probe_rid_cache[0]);
						GLES3::ReflectionProbe *probe = light_storage->get_reflection_probe(probe_rid);

						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_USE_BOX_PROJECT, probe->box_projection, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_BOX_EXTENTS, probe->size * 0.5, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_BOX_OFFSET, probe->origin_offset, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_EXTERIOR, !probe->interior, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_INTENSITY, probe->intensity, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_AMBIENT_MODE, int(probe->ambient_mode), shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_AMBIENT_COLOR, probe->ambient_color * probe->ambient_color_energy, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE1_LOCAL_MATRIX, inst->reflection_probes_local_transform_cache[0], shader->version, instance_variant, spec_constants);

						glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 7);
						glBindTexture(GL_TEXTURE_CUBE_MAP, light_storage->reflection_probe_instance_get_texture(inst->reflection_probe_rid_cache[0]));
					}

					if (inst->reflection_probe_rid_cache.size() > 1) {
						// Setup second probe.
						RID probe_rid = light_storage->reflection_probe_instance_get_probe(inst->reflection_probe_rid_cache[1]);
						GLES3::ReflectionProbe *probe = light_storage->get_reflection_probe(probe_rid);

						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_USE_BOX_PROJECT, probe->box_projection, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_BOX_EXTENTS, probe->size * 0.5, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_BOX_OFFSET, probe->origin_offset, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_EXTERIOR, !probe->interior, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_INTENSITY, probe->intensity, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_AMBIENT_MODE, int(probe->ambient_mode), shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_AMBIENT_COLOR, probe->ambient_color * probe->ambient_color_energy, shader->version, instance_variant, spec_constants);
						material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::REFPROBE2_LOCAL_MATRIX, inst->reflection_probes_local_transform_cache[1], shader->version, instance_variant, spec_constants);

						glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 8);
						glBindTexture(GL_TEXTURE_CUBE_MAP, light_storage->reflection_probe_instance_get_texture(inst->reflection_probe_rid_cache[1]));

						spec_constants |= SceneShaderGLES3::SECOND_REFLECTION_PROBE;
					}
				}
			}

			material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::WORLD_TRANSFORM, world_transform, shader->version, instance_variant, spec_constants);
			{
				GLES3::Mesh::Surface *s = reinterpret_cast<GLES3::Mesh::Surface *>(surf->surface);
				if (s->format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::COMPRESSED_AABB_POSITION, s->aabb.position, shader->version, instance_variant, spec_constants);
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::COMPRESSED_AABB_SIZE, s->aabb.size, shader->version, instance_variant, spec_constants);
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::UV_SCALE, s->uv_scale, shader->version, instance_variant, spec_constants);
				} else {
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::COMPRESSED_AABB_POSITION, Vector3(0.0, 0.0, 0.0), shader->version, instance_variant, spec_constants);
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::COMPRESSED_AABB_SIZE, Vector3(1.0, 1.0, 1.0), shader->version, instance_variant, spec_constants);
					material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::UV_SCALE, Vector4(0.0, 0.0, 0.0, 0.0), shader->version, instance_variant, spec_constants);
				}
			}

			material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::MODEL_FLAGS, inst->flags_cache, shader->version, instance_variant, spec_constants);

			if (p_pass_mode == PASS_MODE_MATERIAL) {
				material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES3::UV_OFFSET, p_params->uv_offset, shader->version, instance_variant, spec_constants);
			}

			// Can be index count or vertex count
			uint32_t count = 0;
			if (surf->lod_index > 0) {
				count = surf->index_count;
			} else {
				count = mesh_storage->mesh_surface_get_vertices_drawn_count(mesh_surface);
			}

			if (use_wireframe) {
				// In this case we are using index count, and we need double the indices for the wireframe mesh.
				count = count * 2;
			}

			if constexpr (p_pass_mode != PASS_MODE_DEPTH) {
				// Don't count draw calls during depth pre-pass to match the RD renderers.
				if (p_render_data->render_info) {
					p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
				}
			}

			if (inst->instance_count > 0) {
				// Using MultiMesh or Particles.
				// Bind instance buffers.

				GLuint instance_buffer = 0;
				uint32_t stride = 0;
				if (inst->flags_cache & INSTANCE_DATA_FLAG_PARTICLES) {
					instance_buffer = particles_storage->particles_get_gl_buffer(inst->data->base);
					stride = 16; // 12 bytes for instance transform and 4 bytes for packed color and custom.
				} else {
					instance_buffer = mesh_storage->multimesh_get_gl_buffer(inst->data->base);
					stride = mesh_storage->multimesh_get_stride(inst->data->base);
				}

				if (instance_buffer == 0) {
					// Instance buffer not initialized yet. Skip rendering for now.
					break;
				}

				glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);

				glEnableVertexAttribArray(12);
				glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(0));
				glVertexAttribDivisor(12, 1);
				glEnableVertexAttribArray(13);
				glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4));
				glVertexAttribDivisor(13, 1);
				if (!(inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D)) {
					glEnableVertexAttribArray(14);
					glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(sizeof(float) * 8));
					glVertexAttribDivisor(14, 1);
				}

				if ((inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR) || (inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA)) {
					uint32_t color_custom_offset = inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D ? 8 : 12;
					glEnableVertexAttribArray(15);
					glVertexAttribIPointer(15, 4, GL_UNSIGNED_INT, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(color_custom_offset * sizeof(float)));
					glVertexAttribDivisor(15, 1);
				} else {
					// Set all default instance color and custom data values to 1.0 or 0.0 using a compressed format.
					uint16_t zero = Math::make_half_float(0.0f);
					uint16_t one = Math::make_half_float(1.0f);
					GLuint default_color = (uint32_t(one) << 16) | one;
					GLuint default_custom = (uint32_t(zero) << 16) | zero;
					glVertexAttribI4ui(15, default_color, default_color, default_custom, default_custom);
				}

				if (use_wireframe) {
					glDrawElementsInstanced(GL_LINES, count, GL_UNSIGNED_INT, nullptr, inst->instance_count);
				} else {
					if (use_index_buffer) {
						glDrawElementsInstanced(primitive_gl, count, mesh_storage->mesh_surface_get_index_type(mesh_surface), nullptr, inst->instance_count);
					} else {
						glDrawArraysInstanced(primitive_gl, 0, count, inst->instance_count);
					}
				}
			} else {
				// Using regular Mesh.
				if (use_wireframe) {
					glDrawElements(GL_LINES, count, GL_UNSIGNED_INT, nullptr);
				} else {
					if (use_index_buffer) {
						glDrawElements(primitive_gl, count, mesh_storage->mesh_surface_get_index_type(mesh_surface), nullptr);
					} else {
						glDrawArrays(primitive_gl, 0, count);
					}
				}
			}

			if (inst->instance_count > 0) {
				glDisableVertexAttribArray(12);
				glDisableVertexAttribArray(13);
				glDisableVertexAttribArray(14);
				glDisableVertexAttribArray(15);
			}
		}
		if constexpr (p_pass_mode == PASS_MODE_COLOR) {
			if (uses_additive_lighting && !p_render_data->transparent_bg) {
				// Disable additive blending if enabled for additive lights.
				scene_state.enable_gl_blend(false);
			}
		}
	}

	// Make the actual redraw request
	if (should_request_redraw) {
		RenderingServerDefault::redraw_request();
	}
}

void RasterizerSceneGLES3::render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) {
}

void RasterizerSceneGLES3::render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) {
	GLES3::ParticlesStorage *particles_storage = GLES3::ParticlesStorage::get_singleton();

	ERR_FAIL_COND(!particles_storage->particles_collision_is_heightfield(p_collider));
	Vector3 extents = particles_storage->particles_collision_get_extents(p_collider) * p_transform.basis.get_scale();
	Projection cm;
	cm.set_orthogonal(-extents.x, extents.x, -extents.z, extents.z, 0, extents.y * 2.0);

	Vector3 cam_pos = p_transform.origin;
	cam_pos.y += extents.y;

	Transform3D cam_xform;
	cam_xform.set_look_at(cam_pos, cam_pos - p_transform.basis.get_column(Vector3::AXIS_Y), -p_transform.basis.get_column(Vector3::AXIS_Z).normalized());

	GLuint fb = particles_storage->particles_collision_get_heightfield_framebuffer(p_collider);
	Size2i fb_size = particles_storage->particles_collision_get_heightfield_size(p_collider);

	RENDER_TIMESTAMP("Setup GPUParticlesCollisionHeightField3D");

	RenderDataGLES3 render_data;

	render_data.cam_projection = cm;
	render_data.cam_transform = cam_xform;
	render_data.view_projection[0] = cm;
	render_data.inv_cam_transform = render_data.cam_transform.affine_inverse();
	render_data.cam_orthogonal = true;
	render_data.z_near = 0.0;
	render_data.z_far = cm.get_z_far();
	render_data.main_cam_transform = cam_xform;

	render_data.instances = &p_instances;

	_setup_environment(&render_data, true, Vector2(fb_size), true, Color(), false);

	PassMode pass_mode = PASS_MODE_SHADOW;

	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();

	RENDER_TIMESTAMP("Render Collider Heightfield");

	glBindFramebuffer(GL_FRAMEBUFFER, fb);
	glViewport(0, 0, fb_size.width, fb_size.height);

	GLuint global_buffer = GLES3::MaterialStorage::get_singleton()->global_shader_parameters_get_uniform_buffer();

	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_GLOBALS_UNIFORM_LOCATION, global_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	scene_state.reset_gl_state();
	scene_state.enable_gl_depth_test(true);
	scene_state.enable_gl_depth_draw(true);
	glDepthFunc(GL_GREATER);

	glDrawBuffers(0, nullptr);

	glColorMask(0, 0, 0, 0);
	RasterizerGLES3::clear_depth(0.0);

	glClear(GL_DEPTH_BUFFER_BIT);

	RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), false, 31, false);

	_render_list_template<PASS_MODE_SHADOW>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());

	glColorMask(1, 1, 1, 1);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
}

void RasterizerSceneGLES3::_render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, GLuint p_framebuffer, const Rect2i &p_region) {
	RENDER_TIMESTAMP("Setup Rendering UV2");

	RenderDataGLES3 render_data;
	render_data.instances = &p_instances;

	scene_state.ubo.emissive_exposure_normalization = -1.0; // Use default exposure normalization.

	_setup_environment(&render_data, true, Vector2(1, 1), true, Color(), false);

	PassMode pass_mode = PASS_MODE_MATERIAL;

	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();

	RENDER_TIMESTAMP("Render 3D Material");

	{
		glBindFramebuffer(GL_FRAMEBUFFER, p_framebuffer);
		glViewport(p_region.position.x, p_region.position.y, p_region.size.x, p_region.size.y);

		GLuint global_buffer = GLES3::MaterialStorage::get_singleton()->global_shader_parameters_get_uniform_buffer();

		glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_GLOBALS_UNIFORM_LOCATION, global_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		scene_state.reset_gl_state();
		scene_state.enable_gl_depth_test(true);
		scene_state.enable_gl_depth_draw(true);
		glDepthFunc(GL_GREATER);

		TightLocalVector<GLenum> draw_buffers;
		draw_buffers.push_back(GL_COLOR_ATTACHMENT0);
		draw_buffers.push_back(GL_COLOR_ATTACHMENT1);
		draw_buffers.push_back(GL_COLOR_ATTACHMENT2);
		draw_buffers.push_back(GL_COLOR_ATTACHMENT3);
		glDrawBuffers(draw_buffers.size(), draw_buffers.ptr());

		glClearColor(0.0, 0.0, 0.0, 0.0);
		RasterizerGLES3::clear_depth(0.0);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		uint64_t base_spec_constant = 0;
		base_spec_constant |= SceneShaderGLES3::RENDER_MATERIAL;
		base_spec_constant |= SceneShaderGLES3::DISABLE_FOG;
		base_spec_constant |= SceneShaderGLES3::DISABLE_LIGHT_DIRECTIONAL;
		base_spec_constant |= SceneShaderGLES3::DISABLE_LIGHT_OMNI;
		base_spec_constant |= SceneShaderGLES3::DISABLE_LIGHT_SPOT;
		base_spec_constant |= SceneShaderGLES3::DISABLE_LIGHTMAP;

		RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), false, base_spec_constant, true, Vector2(0, 0));

		const int uv_offset_count = 9;
		static const Vector2 uv_offsets[uv_offset_count] = {
			Vector2(-1, 1),
			Vector2(1, 1),
			Vector2(1, -1),
			Vector2(-1, -1),
			Vector2(-1, 0),
			Vector2(1, 0),
			Vector2(0, -1),
			Vector2(0, 1),
			Vector2(0, 0),
		};

		for (int i = 0; i < uv_offset_count; i++) {
			Vector2 ofs = uv_offsets[i];
			ofs.x /= p_region.size.width;
			ofs.y /= p_region.size.height;
			render_list_params.uv_offset = ofs;
			_render_list_template<PASS_MODE_MATERIAL>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());
		}

		render_list_params.uv_offset = Vector2(0, 0);
		render_list_params.force_wireframe = false;
		_render_list_template<PASS_MODE_MATERIAL>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());

		GLuint db = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &db);
		glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	}
}

void RasterizerSceneGLES3::set_time(double p_time, double p_step) {
	time = p_time;
	time_step = p_step;
}

void RasterizerSceneGLES3::set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) {
	debug_draw = p_debug_draw;
}

Ref<RenderSceneBuffers> RasterizerSceneGLES3::render_buffers_create() {
	Ref<RenderSceneBuffersGLES3> rb;
	rb.instantiate();
	return rb;
}

void RasterizerSceneGLES3::_render_buffers_debug_draw(Ref<RenderSceneBuffersGLES3> p_render_buffers, RID p_shadow_atlas, GLuint p_fbo) {
	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	GLES3::LightStorage *light_storage = GLES3::LightStorage::get_singleton();
	GLES3::CopyEffects *copy_effects = GLES3::CopyEffects::get_singleton();

	ERR_FAIL_COND(p_render_buffers.is_null());

	RID render_target = p_render_buffers->render_target;
	GLES3::RenderTarget *rt = texture_storage->get_render_target(render_target);
	ERR_FAIL_NULL(rt);

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_SHADOW_ATLAS) {
		if (p_shadow_atlas.is_valid()) {
			// Get or create debug textures to display shadow maps as an atlas.
			GLuint shadow_atlas_texture = light_storage->shadow_atlas_get_debug_texture(p_shadow_atlas);
			GLuint shadow_atlas_fb = light_storage->shadow_atlas_get_debug_fb(p_shadow_atlas);

			uint32_t shadow_atlas_size = light_storage->shadow_atlas_get_size(p_shadow_atlas);
			uint32_t quadrant_size = shadow_atlas_size >> 1;

			glBindFramebuffer(GL_FRAMEBUFFER, shadow_atlas_fb);
			glViewport(0, 0, shadow_atlas_size, shadow_atlas_size);
			glActiveTexture(GL_TEXTURE0);
			scene_state.enable_gl_depth_draw(true);
			glDepthFunc(GL_ALWAYS);
			scene_state.set_gl_cull_mode(GLES3::SceneShaderData::CULL_DISABLED);

			// Loop through quadrants and copy shadows over.
			for (int quadrant = 0; quadrant < 4; quadrant++) {
				uint32_t subdivision = light_storage->shadow_atlas_get_quadrant_subdivision(p_shadow_atlas, quadrant);
				if (subdivision == 0) {
					continue;
				}

				Rect2i atlas_rect;
				Rect2 atlas_uv_rect;

				uint32_t shadow_size = (quadrant_size / subdivision);
				float size = float(shadow_size) / float(shadow_atlas_size);

				uint32_t length = light_storage->shadow_atlas_get_quadrant_shadows_allocated(p_shadow_atlas, quadrant);
				for (uint32_t shadow_idx = 0; shadow_idx < length; shadow_idx++) {
					bool is_omni = light_storage->shadow_atlas_get_quadrant_shadow_is_omni(p_shadow_atlas, quadrant, shadow_idx);

					// Calculate shadow's position in the debug atlas.
					atlas_rect.position.x = (quadrant & 1) * quadrant_size;
					atlas_rect.position.y = (quadrant >> 1) * quadrant_size;

					atlas_rect.position.x += (shadow_idx % subdivision) * shadow_size;
					atlas_rect.position.y += (shadow_idx / subdivision) * shadow_size;

					atlas_uv_rect.position = Vector2(atlas_rect.position) / float(shadow_atlas_size);

					atlas_uv_rect.size = Vector2(size, size);

					GLuint shadow_tex = light_storage->shadow_atlas_get_quadrant_shadow_texture(p_shadow_atlas, quadrant, shadow_idx);
					// Copy from shadowmap to debug atlas.
					if (is_omni) {
						glBindTexture(GL_TEXTURE_CUBE_MAP, shadow_tex);
						glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);

						copy_effects->copy_cube_to_rect(atlas_uv_rect);

						glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
						glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
					} else {
						glBindTexture(GL_TEXTURE_2D, shadow_tex);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

						copy_effects->copy_to_rect(atlas_uv_rect);

						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
					}
				}
			}

			// Set back to FBO
			glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
			Size2i size = p_render_buffers->get_internal_size();
			glViewport(0, 0, size.width, size.height);
			glBindTexture(GL_TEXTURE_2D, shadow_atlas_texture);

			copy_effects->copy_to_rect(Rect2(Vector2(), Vector2(0.5, 0.5)));
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
		}
	}
	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS) {
		if (light_storage->directional_shadow_get_texture() != 0) {
			GLuint shadow_atlas_texture = light_storage->directional_shadow_get_texture();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, shadow_atlas_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);

			scene_state.enable_gl_depth_test(false);
			scene_state.enable_gl_depth_draw(false);

			copy_effects->copy_to_rect(Rect2(Vector2(), Vector2(0.5, 0.5)));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
}

void RasterizerSceneGLES3::gi_set_use_half_resolution(bool p_enable) {
}

void RasterizerSceneGLES3::screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_curve) {
}

bool RasterizerSceneGLES3::screen_space_roughness_limiter_is_active() const {
	return false;
}

void RasterizerSceneGLES3::sub_surface_scattering_set_quality(RS::SubSurfaceScatteringQuality p_quality) {
}

void RasterizerSceneGLES3::sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) {
}

TypedArray<Image> RasterizerSceneGLES3::bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) {
	GLES3::Config *config = GLES3::Config::get_singleton();
	ERR_FAIL_COND_V_MSG(p_image_size.width <= 0, TypedArray<Image>(), "Image width must be greater than 0.");
	ERR_FAIL_COND_V_MSG(p_image_size.height <= 0, TypedArray<Image>(), "Image height must be greater than 0.");

	GLuint albedo_alpha_tex = 0;
	GLuint normal_tex = 0;
	GLuint orm_tex = 0;
	GLuint emission_tex = 0;
	GLuint depth_tex = 0;
	glGenTextures(1, &albedo_alpha_tex);
	glGenTextures(1, &normal_tex);
	glGenTextures(1, &orm_tex);
	glGenTextures(1, &emission_tex);
	glGenTextures(1, &depth_tex);

	glBindTexture(GL_TEXTURE_2D, albedo_alpha_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_image_size.width, p_image_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	GLES3::Utilities::get_singleton()->texture_allocated_data(albedo_alpha_tex, p_image_size.width * p_image_size.height * 4, "Lightmap albedo texture");

	glBindTexture(GL_TEXTURE_2D, normal_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_image_size.width, p_image_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	GLES3::Utilities::get_singleton()->texture_allocated_data(normal_tex, p_image_size.width * p_image_size.height * 4, "Lightmap normal texture");

	glBindTexture(GL_TEXTURE_2D, orm_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_image_size.width, p_image_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	GLES3::Utilities::get_singleton()->texture_allocated_data(orm_tex, p_image_size.width * p_image_size.height * 4, "Lightmap ORM texture");

	// Consider rendering to RGBA8 encoded as RGBE, then manually convert to RGBAH on CPU.
	glBindTexture(GL_TEXTURE_2D, emission_tex);
	if (config->float_texture_supported) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, p_image_size.width, p_image_size.height, 0, GL_RGBA, GL_FLOAT, nullptr);
		GLES3::Utilities::get_singleton()->texture_allocated_data(emission_tex, p_image_size.width * p_image_size.height * 16, "Lightmap emission texture");
	} else {
		// Fallback to RGBA8 on devices that don't support rendering to floating point textures. This will look bad, but we have no choice.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, p_image_size.width, p_image_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		GLES3::Utilities::get_singleton()->texture_allocated_data(emission_tex, p_image_size.width * p_image_size.height * 4, "Lightmap emission texture");
	}

	glBindTexture(GL_TEXTURE_2D, depth_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, p_image_size.width, p_image_size.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	GLES3::Utilities::get_singleton()->texture_allocated_data(depth_tex, p_image_size.width * p_image_size.height * 3, "Lightmap depth texture");

	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedo_alpha_tex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_tex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, orm_tex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, emission_tex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		glDeleteFramebuffers(1, &fbo);
		GLES3::Utilities::get_singleton()->texture_free_data(albedo_alpha_tex);
		GLES3::Utilities::get_singleton()->texture_free_data(normal_tex);
		GLES3::Utilities::get_singleton()->texture_free_data(orm_tex);
		GLES3::Utilities::get_singleton()->texture_free_data(emission_tex);
		GLES3::Utilities::get_singleton()->texture_free_data(depth_tex);

		WARN_PRINT("Could not create render target, status: " + GLES3::TextureStorage::get_singleton()->get_framebuffer_error(status));
		return TypedArray<Image>();
	}

	RenderGeometryInstance *gi_inst = geometry_instance_create(p_base);
	ERR_FAIL_NULL_V(gi_inst, TypedArray<Image>());

	uint32_t sc = RSG::mesh_storage->mesh_get_surface_count(p_base);
	Vector<RID> materials;
	materials.resize(sc);

	for (uint32_t i = 0; i < sc; i++) {
		if (i < (uint32_t)p_material_overrides.size()) {
			materials.write[i] = p_material_overrides[i];
		}
	}

	gi_inst->set_surface_materials(materials);

	if (cull_argument.size() == 0) {
		cull_argument.push_back(nullptr);
	}
	cull_argument[0] = gi_inst;
	_render_uv2(cull_argument, fbo, Rect2i(0, 0, p_image_size.width, p_image_size.height));

	geometry_instance_free(gi_inst);

	TypedArray<Image> ret;

	// Create a dummy texture so we can use texture_2d_get.
	RID tex_rid = GLES3::TextureStorage::get_singleton()->texture_allocate();
	GLES3::Texture texture;
	texture.width = p_image_size.width;
	texture.height = p_image_size.height;
	texture.alloc_width = p_image_size.width;
	texture.alloc_height = p_image_size.height;
	texture.format = Image::FORMAT_RGBA8;
	texture.real_format = Image::FORMAT_RGBA8;
	texture.gl_format_cache = GL_RGBA;
	texture.gl_type_cache = GL_UNSIGNED_BYTE;
	texture.type = GLES3::Texture::TYPE_2D;
	texture.target = GL_TEXTURE_2D;
	texture.active = true;
	texture.is_render_target = true; // Enable this so the texture isn't cached in the editor.

	GLES3::TextureStorage::get_singleton()->texture_2d_initialize_from_texture(tex_rid, texture);
	GLES3::Texture *tex = GLES3::TextureStorage::get_singleton()->get_texture(tex_rid);

	{
		tex->tex_id = albedo_alpha_tex;
		Ref<Image> img = GLES3::TextureStorage::get_singleton()->texture_2d_get(tex_rid);
		GLES3::Utilities::get_singleton()->texture_free_data(albedo_alpha_tex);
		ret.push_back(img);
	}

	{
		tex->tex_id = normal_tex;
		Ref<Image> img = GLES3::TextureStorage::get_singleton()->texture_2d_get(tex_rid);
		GLES3::Utilities::get_singleton()->texture_free_data(normal_tex);
		ret.push_back(img);
	}

	{
		tex->tex_id = orm_tex;
		Ref<Image> img = GLES3::TextureStorage::get_singleton()->texture_2d_get(tex_rid);
		GLES3::Utilities::get_singleton()->texture_free_data(orm_tex);
		ret.push_back(img);
	}

	{
		tex->tex_id = emission_tex;
		if (config->float_texture_supported) {
			tex->format = Image::FORMAT_RGBAF;
			tex->real_format = Image::FORMAT_RGBAH;
			tex->gl_type_cache = GL_FLOAT;
		}
		Ref<Image> img = GLES3::TextureStorage::get_singleton()->texture_2d_get(tex_rid);
		GLES3::Utilities::get_singleton()->texture_free_data(emission_tex);
		ret.push_back(img);
	}

	tex->is_render_target = false;
	tex->tex_id = 0;
	GLES3::TextureStorage::get_singleton()->texture_free(tex_rid);

	GLES3::Utilities::get_singleton()->texture_free_data(depth_tex);
	glDeleteFramebuffers(1, &fbo);
	return ret;
}

bool RasterizerSceneGLES3::free(RID p_rid) {
	if (is_environment(p_rid)) {
		environment_free(p_rid);
	} else if (sky_owner.owns(p_rid)) {
		Sky *sky = sky_owner.get_or_null(p_rid);
		ERR_FAIL_NULL_V(sky, false);
		_free_sky_data(sky);
		sky_owner.free(p_rid);
	} else if (GLES3::LightStorage::get_singleton()->owns_light_instance(p_rid)) {
		GLES3::LightStorage::get_singleton()->light_instance_free(p_rid);
	} else if (RSG::camera_attributes->owns_camera_attributes(p_rid)) {
		//not much to delete, just free it
		RSG::camera_attributes->camera_attributes_free(p_rid);
	} else if (is_compositor(p_rid)) {
		compositor_free(p_rid);
	} else if (is_compositor_effect(p_rid)) {
		compositor_effect_free(p_rid);
	} else {
		return false;
	}
	return true;
}

void RasterizerSceneGLES3::update() {
	_update_dirty_skys();
}

void RasterizerSceneGLES3::sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) {
}

void RasterizerSceneGLES3::decals_set_filter(RS::DecalFilter p_filter) {
}

void RasterizerSceneGLES3::light_projectors_set_filter(RS::LightProjectorFilter p_filter) {
}

void RasterizerSceneGLES3::lightmaps_set_bicubic_filter(bool p_enable) {
	lightmap_bicubic_upscale = p_enable;
}

RasterizerSceneGLES3::RasterizerSceneGLES3() {
	singleton = this;

	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	GLES3::Config *config = GLES3::Config::get_singleton();

	cull_argument.set_page_pool(&cull_argument_pool);

	// Quality settings.
	use_physical_light_units = GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units");

	positional_soft_shadow_filter_set_quality((RS::ShadowQuality)(int)GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality"));
	directional_soft_shadow_filter_set_quality((RS::ShadowQuality)(int)GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality"));
	lightmaps_set_bicubic_filter(GLOBAL_GET("rendering/lightmapping/lightmap_gi/use_bicubic_filter"));

	{
		// Setup Lights

		config->max_renderable_lights = MIN(config->max_renderable_lights, config->max_uniform_buffer_size / (int)sizeof(RasterizerSceneGLES3::LightData));
		config->max_lights_per_object = MIN(config->max_lights_per_object, config->max_renderable_lights);

		uint32_t light_buffer_size = config->max_renderable_lights * sizeof(LightData);
		scene_state.omni_lights = memnew_arr(LightData, config->max_renderable_lights);
		scene_state.omni_light_sort = memnew_arr(InstanceSort<GLES3::LightInstance>, config->max_renderable_lights);
		glGenBuffers(1, &scene_state.omni_light_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, scene_state.omni_light_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.omni_light_buffer, light_buffer_size, nullptr, GL_STREAM_DRAW, "OmniLight UBO");

		scene_state.spot_lights = memnew_arr(LightData, config->max_renderable_lights);
		scene_state.spot_light_sort = memnew_arr(InstanceSort<GLES3::LightInstance>, config->max_renderable_lights);
		glGenBuffers(1, &scene_state.spot_light_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, scene_state.spot_light_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.spot_light_buffer, light_buffer_size, nullptr, GL_STREAM_DRAW, "SpotLight UBO");

		uint32_t directional_light_buffer_size = MAX_DIRECTIONAL_LIGHTS * sizeof(DirectionalLightData);
		scene_state.directional_lights = memnew_arr(DirectionalLightData, MAX_DIRECTIONAL_LIGHTS);
		glGenBuffers(1, &scene_state.directional_light_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, scene_state.directional_light_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.directional_light_buffer, directional_light_buffer_size, nullptr, GL_STREAM_DRAW, "DirectionalLight UBO");

		uint32_t shadow_buffer_size = config->max_renderable_lights * sizeof(ShadowData) * 2;
		scene_state.positional_shadows = memnew_arr(ShadowData, config->max_renderable_lights * 2);
		glGenBuffers(1, &scene_state.positional_shadow_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, scene_state.positional_shadow_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.positional_shadow_buffer, shadow_buffer_size, nullptr, GL_STREAM_DRAW, "Positional Shadow UBO");

		uint32_t directional_shadow_buffer_size = MAX_DIRECTIONAL_LIGHTS * sizeof(DirectionalShadowData);
		scene_state.directional_shadows = memnew_arr(DirectionalShadowData, MAX_DIRECTIONAL_LIGHTS);
		glGenBuffers(1, &scene_state.directional_shadow_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, scene_state.directional_shadow_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, scene_state.directional_shadow_buffer, directional_shadow_buffer_size, nullptr, GL_STREAM_DRAW, "Directional Shadow UBO");

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	{
		sky_globals.max_directional_lights = 4;
		uint32_t directional_light_buffer_size = sky_globals.max_directional_lights * sizeof(DirectionalLightData);
		sky_globals.directional_lights = memnew_arr(DirectionalLightData, sky_globals.max_directional_lights);
		sky_globals.last_frame_directional_lights = memnew_arr(DirectionalLightData, sky_globals.max_directional_lights);
		sky_globals.last_frame_directional_light_count = sky_globals.max_directional_lights + 1;
		glGenBuffers(1, &sky_globals.directional_light_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, sky_globals.directional_light_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, sky_globals.directional_light_buffer, directional_light_buffer_size, nullptr, GL_STREAM_DRAW, "Sky DirectionalLight UBO");

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	{
		String global_defines;
		global_defines += "#define MAX_GLOBAL_SHADER_UNIFORMS 256\n"; // TODO: this is arbitrary for now
		global_defines += "\n#define MAX_LIGHT_DATA_STRUCTS " + itos(config->max_renderable_lights) + "\n";
		global_defines += "\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS " + itos(MAX_DIRECTIONAL_LIGHTS) + "\n";
		global_defines += "\n#define MAX_FORWARD_LIGHTS " + itos(config->max_lights_per_object) + "u\n";
		global_defines += "\n#define MAX_ROUGHNESS_LOD " + itos(sky_globals.roughness_layers - 1) + ".0\n";
		if (config->force_vertex_shading) {
			global_defines += "\n#define USE_VERTEX_LIGHTING\n";
		}
		material_storage->shaders.scene_shader.initialize(global_defines);
		scene_globals.shader_default_version = material_storage->shaders.scene_shader.version_create();
		material_storage->shaders.scene_shader.version_bind_shader(scene_globals.shader_default_version, SceneShaderGLES3::MODE_COLOR);
	}

	{
		//default material and shader
		scene_globals.default_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(scene_globals.default_shader);
		material_storage->shader_set_code(scene_globals.default_shader, R"(
// Default 3D material shader (Compatibility).

shader_type spatial;

void vertex() {
	ROUGHNESS = 0.8;
}

void fragment() {
	ALBEDO = vec3(0.6);
	ROUGHNESS = 0.8;
	METALLIC = 0.2;
}
)");
		scene_globals.default_material = material_storage->material_allocate();
		material_storage->material_initialize(scene_globals.default_material);
		material_storage->material_set_shader(scene_globals.default_material, scene_globals.default_shader);
		default_material_data_ptr = static_cast<GLES3::SceneMaterialData *>(GLES3::MaterialStorage::get_singleton()->material_get_data(scene_globals.default_material, RS::SHADER_SPATIAL));
	}

	{
		// Overdraw material and shader.
		scene_globals.overdraw_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(scene_globals.overdraw_shader);
		material_storage->shader_set_code(scene_globals.overdraw_shader, R"(
// 3D editor Overdraw debug draw mode shader (Compatibility).

shader_type spatial;

render_mode blend_add, unshaded, fog_disabled;

void fragment() {
	ALBEDO = vec3(0.4, 0.8, 0.8);
	ALPHA = 0.2;
}
)");
		scene_globals.overdraw_material = material_storage->material_allocate();
		material_storage->material_initialize(scene_globals.overdraw_material);
		material_storage->material_set_shader(scene_globals.overdraw_material, scene_globals.overdraw_shader);
		overdraw_material_data_ptr = static_cast<GLES3::SceneMaterialData *>(GLES3::MaterialStorage::get_singleton()->material_get_data(scene_globals.overdraw_material, RS::SHADER_SPATIAL));
	}

	{
		// Initialize Sky stuff
		sky_globals.roughness_layers = GLOBAL_GET("rendering/reflections/sky_reflections/roughness_layers");

		String global_defines;
		global_defines += "#define MAX_GLOBAL_SHADER_UNIFORMS 256\n"; // TODO: this is arbitrary for now
		global_defines += "\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS " + itos(sky_globals.max_directional_lights) + "\n";
		material_storage->shaders.sky_shader.initialize(global_defines);
		sky_globals.shader_default_version = material_storage->shaders.sky_shader.version_create();
	}

	{
		sky_globals.default_shader = material_storage->shader_allocate();

		material_storage->shader_initialize(sky_globals.default_shader);

		material_storage->shader_set_code(sky_globals.default_shader, R"(
// Default sky shader.

shader_type sky;

void sky() {
	COLOR = vec3(0.0);
}
)");
		sky_globals.default_material = material_storage->material_allocate();
		material_storage->material_initialize(sky_globals.default_material);

		material_storage->material_set_shader(sky_globals.default_material, sky_globals.default_shader);
	}
	{
		sky_globals.fog_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(sky_globals.fog_shader);

		material_storage->shader_set_code(sky_globals.fog_shader, R"(
// Default clear color sky shader.

shader_type sky;

uniform vec4 clear_color;

void sky() {
	COLOR = clear_color.rgb;
}
)");
		sky_globals.fog_material = material_storage->material_allocate();
		material_storage->material_initialize(sky_globals.fog_material);

		material_storage->material_set_shader(sky_globals.fog_material, sky_globals.fog_shader);
	}

	{
		glGenVertexArrays(1, &sky_globals.screen_triangle_array);
		glBindVertexArray(sky_globals.screen_triangle_array);
		glGenBuffers(1, &sky_globals.screen_triangle);
		glBindBuffer(GL_ARRAY_BUFFER, sky_globals.screen_triangle);

		const float qv[6] = {
			-1.0f,
			-1.0f,
			3.0f,
			-1.0f,
			-1.0f,
			3.0f,
		};

		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, sky_globals.screen_triangle, sizeof(float) * 6, qv, GL_STATIC_DRAW, "Screen triangle vertex buffer");

		glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(RS::ARRAY_VERTEX);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}

#ifdef GL_API_ENABLED
	if (RasterizerGLES3::is_gles_over_gl()) {
		glEnable(_EXT_TEXTURE_CUBE_MAP_SEAMLESS);
	}
#endif // GL_API_ENABLED

	// MultiMesh may read from color when color is disabled, so make sure that the color defaults to white instead of black;
	glVertexAttrib4f(RS::ARRAY_COLOR, 1.0, 1.0, 1.0, 1.0);
}

RasterizerSceneGLES3::~RasterizerSceneGLES3() {
	GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.directional_light_buffer);
	GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.omni_light_buffer);
	GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.spot_light_buffer);
	GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.positional_shadow_buffer);
	GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.directional_shadow_buffer);
	memdelete_arr(scene_state.directional_lights);
	memdelete_arr(scene_state.omni_lights);
	memdelete_arr(scene_state.spot_lights);
	memdelete_arr(scene_state.omni_light_sort);
	memdelete_arr(scene_state.spot_light_sort);
	memdelete_arr(scene_state.positional_shadows);
	memdelete_arr(scene_state.directional_shadows);

	// Scene Shader
	GLES3::MaterialStorage::get_singleton()->shaders.scene_shader.version_free(scene_globals.shader_default_version);
	RSG::material_storage->material_free(scene_globals.default_material);
	RSG::material_storage->shader_free(scene_globals.default_shader);

	// Overdraw Shader
	RSG::material_storage->material_free(scene_globals.overdraw_material);
	RSG::material_storage->shader_free(scene_globals.overdraw_shader);

	// Sky Shader
	GLES3::MaterialStorage::get_singleton()->shaders.sky_shader.version_free(sky_globals.shader_default_version);
	RSG::material_storage->material_free(sky_globals.default_material);
	RSG::material_storage->shader_free(sky_globals.default_shader);
	RSG::material_storage->material_free(sky_globals.fog_material);
	RSG::material_storage->shader_free(sky_globals.fog_shader);
	GLES3::Utilities::get_singleton()->buffer_free_data(sky_globals.screen_triangle);
	glDeleteVertexArrays(1, &sky_globals.screen_triangle_array);
	GLES3::Utilities::get_singleton()->buffer_free_data(sky_globals.directional_light_buffer);
	memdelete_arr(sky_globals.directional_lights);
	memdelete_arr(sky_globals.last_frame_directional_lights);

	// UBOs
	if (scene_state.ubo_buffer != 0) {
		GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.ubo_buffer);
	}

	if (scene_state.multiview_buffer != 0) {
		GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.multiview_buffer);
	}

	if (scene_state.tonemap_buffer != 0) {
		GLES3::Utilities::get_singleton()->buffer_free_data(scene_state.tonemap_buffer);
	}

	singleton = nullptr;
}

#endif // GLES3_ENABLED
