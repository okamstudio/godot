/**************************************************************************/
/*  baked_lightmap.cpp                                                    */
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

#include "baked_lightmap.h"
#include "core/io/config_file.h"
#include "core/io/resource_saver.h"
#include "core/math/math_defs.h"
#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "voxel_light_baker.h"

void BakedLightmapData::set_bounds(const AABB &p_bounds) {
	bounds = p_bounds;
	VS::get_singleton()->lightmap_capture_set_bounds(baked_light, p_bounds);
}

AABB BakedLightmapData::get_bounds() const {
	return bounds;
}

void BakedLightmapData::set_octree(const PoolVector<uint8_t> &p_octree) {
	VS::get_singleton()->lightmap_capture_set_octree(baked_light, p_octree);
}

PoolVector<uint8_t> BakedLightmapData::get_octree() const {
	return VS::get_singleton()->lightmap_capture_get_octree(baked_light);
}

void BakedLightmapData::set_cell_space_transform(const Transform &p_xform) {
	cell_space_xform = p_xform;
	VS::get_singleton()->lightmap_capture_set_octree_cell_transform(baked_light, p_xform);
}

Transform BakedLightmapData::get_cell_space_transform() const {
	return cell_space_xform;
}

void BakedLightmapData::set_cell_subdiv(int p_cell_subdiv) {
	cell_subdiv = p_cell_subdiv;
	VS::get_singleton()->lightmap_capture_set_octree_cell_subdiv(baked_light, p_cell_subdiv);
}

int BakedLightmapData::get_cell_subdiv() const {
	return cell_subdiv;
}

void BakedLightmapData::set_energy(float p_energy) {
	energy = p_energy;
	VS::get_singleton()->lightmap_capture_set_energy(baked_light, energy);
}

float BakedLightmapData::get_energy() const {
	return energy;
}

void BakedLightmapData::set_interior(bool p_interior) {
	interior = p_interior;
	VS::get_singleton()->lightmap_capture_set_interior(baked_light, interior);
}

bool BakedLightmapData::is_interior() const {
	return interior;
}

void BakedLightmapData::set_preview_bake(bool p_preview_bake) {
	preview_bake = p_preview_bake;
}

bool BakedLightmapData::is_preview_bake() const {
	return preview_bake;
}

void BakedLightmapData::add_user(const NodePath &p_path, const Ref<Resource> &p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect, int p_instance) {
	ERR_FAIL_COND_MSG(p_lightmap.is_null(), "It's not a reference to a valid Texture object.");
	ERR_FAIL_COND(p_lightmap_slice == -1 && !Object::cast_to<Texture>(p_lightmap.ptr()));
	ERR_FAIL_COND(p_lightmap_slice != -1 && !Object::cast_to<TextureLayered>(p_lightmap.ptr()));

	User user;
	user.path = p_path;
	if (p_lightmap_slice == -1) {
		user.lightmap.single = p_lightmap;
	} else {
		user.lightmap.layered = p_lightmap;
	}
	user.lightmap_slice = p_lightmap_slice;
	user.lightmap_uv_rect = p_lightmap_uv_rect;
	user.instance_index = p_instance;
	users.push_back(user);
}

int BakedLightmapData::get_user_count() const {
	return users.size();
}
NodePath BakedLightmapData::get_user_path(int p_user) const {
	ERR_FAIL_INDEX_V(p_user, users.size(), NodePath());
	return users[p_user].path;
}
Ref<Resource> BakedLightmapData::get_user_lightmap(int p_user) const {
	ERR_FAIL_INDEX_V(p_user, users.size(), Ref<Resource>());
	if (users[p_user].lightmap_slice == -1) {
		return users[p_user].lightmap.single;
	} else {
		return users[p_user].lightmap.layered;
	}
}

int BakedLightmapData::get_user_lightmap_slice(int p_user) const {
	ERR_FAIL_INDEX_V(p_user, users.size(), -1);
	return users[p_user].lightmap_slice;
}

Rect2 BakedLightmapData::get_user_lightmap_uv_rect(int p_user) const {
	ERR_FAIL_INDEX_V(p_user, users.size(), Rect2(0, 0, 1, 1));
	return users[p_user].lightmap_uv_rect;
}

int BakedLightmapData::get_user_instance(int p_user) const {
	ERR_FAIL_INDEX_V(p_user, users.size(), -1);
	return users[p_user].instance_index;
}

void BakedLightmapData::clear_users() {
	users.clear();
}

void BakedLightmapData::clear_data() {
	clear_users();
	if (baked_light.is_valid()) {
		VS::get_singleton()->free(baked_light);
	}
	baked_light = RID_PRIME(VS::get_singleton()->lightmap_capture_create());
}

void BakedLightmapData::_set_user_data(const Array &p_data) {
	ERR_FAIL_COND(p_data.size() <= 0);

	// Detect old lightmapper format
	if (p_data.size() % 3 == 0) {
		bool is_old_format = true;
		for (int i = 0; i < p_data.size(); i += 3) {
			is_old_format = is_old_format && p_data[i + 0].get_type() == Variant::NODE_PATH;
			is_old_format = is_old_format && p_data[i + 1].is_ref();
			is_old_format = is_old_format && p_data[i + 2].get_type() == Variant::INT;
			if (!is_old_format) {
				break;
			}
		}
		if (is_old_format) {
#ifdef DEBUG_ENABLED
			WARN_PRINT("Geometry at path " + String(p_data[0]) + " is using old lightmapper data. Please re-bake.");
#endif
			Array adapted_data;
			adapted_data.resize((p_data.size() / 3) * 5);
			for (int i = 0; i < p_data.size() / 3; i++) {
				adapted_data[i * 5 + 0] = p_data[i * 3 + 0];
				adapted_data[i * 5 + 1] = p_data[i * 3 + 1];
				adapted_data[i * 5 + 2] = -1;
				adapted_data[i * 5 + 3] = Rect2(0, 0, 1, 1);
				adapted_data[i * 5 + 4] = p_data[i * 3 + 2];
			}
			_set_user_data(adapted_data);
			return;
		}
	}

	ERR_FAIL_COND((p_data.size() % 5) != 0);

	for (int i = 0; i < p_data.size(); i += 5) {
		add_user(p_data[i], p_data[i + 1], p_data[i + 2], p_data[i + 3], p_data[i + 4]);
	}
}

Array BakedLightmapData::_get_user_data() const {
	Array ret;
	for (int i = 0; i < users.size(); i++) {
		ret.push_back(users[i].path);
		ret.push_back(users[i].lightmap_slice == -1 ? Ref<Resource>(users[i].lightmap.single) : Ref<Resource>(users[i].lightmap.layered));
		ret.push_back(users[i].lightmap_slice);
		ret.push_back(users[i].lightmap_uv_rect);
		ret.push_back(users[i].instance_index);
	}
	return ret;
}

RID BakedLightmapData::get_rid() const {
	return baked_light;
}
void BakedLightmapData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_set_user_data", "data"), &BakedLightmapData::_set_user_data);
	ClassDB::bind_method(D_METHOD("_get_user_data"), &BakedLightmapData::_get_user_data);

	ClassDB::bind_method(D_METHOD("set_bounds", "bounds"), &BakedLightmapData::set_bounds);
	ClassDB::bind_method(D_METHOD("get_bounds"), &BakedLightmapData::get_bounds);

	ClassDB::bind_method(D_METHOD("set_cell_space_transform", "xform"), &BakedLightmapData::set_cell_space_transform);
	ClassDB::bind_method(D_METHOD("get_cell_space_transform"), &BakedLightmapData::get_cell_space_transform);

	ClassDB::bind_method(D_METHOD("set_cell_subdiv", "cell_subdiv"), &BakedLightmapData::set_cell_subdiv);
	ClassDB::bind_method(D_METHOD("get_cell_subdiv"), &BakedLightmapData::get_cell_subdiv);

	ClassDB::bind_method(D_METHOD("set_octree", "octree"), &BakedLightmapData::set_octree);
	ClassDB::bind_method(D_METHOD("get_octree"), &BakedLightmapData::get_octree);

	ClassDB::bind_method(D_METHOD("set_energy", "energy"), &BakedLightmapData::set_energy);
	ClassDB::bind_method(D_METHOD("get_energy"), &BakedLightmapData::get_energy);

	ClassDB::bind_method(D_METHOD("set_interior", "interior"), &BakedLightmapData::set_interior);
	ClassDB::bind_method(D_METHOD("is_interior"), &BakedLightmapData::is_interior);

	ClassDB::bind_method(D_METHOD("set_preview_bake", "preview_bake"), &BakedLightmapData::set_preview_bake);
	ClassDB::bind_method(D_METHOD("is_preview_bake"), &BakedLightmapData::is_preview_bake);

	ClassDB::bind_method(D_METHOD("add_user", "path", "lightmap", "lightmap_slice", "lightmap_uv_rect", "instance"), &BakedLightmapData::add_user);
	ClassDB::bind_method(D_METHOD("get_user_count"), &BakedLightmapData::get_user_count);
	ClassDB::bind_method(D_METHOD("get_user_path", "user_idx"), &BakedLightmapData::get_user_path);
	ClassDB::bind_method(D_METHOD("get_user_lightmap", "user_idx"), &BakedLightmapData::get_user_lightmap);
	ClassDB::bind_method(D_METHOD("clear_users"), &BakedLightmapData::clear_users);
	ClassDB::bind_method(D_METHOD("clear_data"), &BakedLightmapData::clear_data);

	ADD_PROPERTY(PropertyInfo(Variant::AABB, "bounds", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_bounds", "get_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM, "cell_space_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_cell_space_transform", "get_cell_space_transform");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell_subdiv", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_cell_subdiv", "get_cell_subdiv");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "energy", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_energy", "get_energy");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interior"), "set_interior", "is_interior");
	ADD_PROPERTY(PropertyInfo(Variant::POOL_BYTE_ARRAY, "octree", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_octree", "get_octree");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preview_bake", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_preview_bake", "is_preview_bake");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "user_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_user_data", "_get_user_data");
}

BakedLightmapData::BakedLightmapData() {
	baked_light = RID_PRIME(VS::get_singleton()->lightmap_capture_create());
	energy = 1;
	cell_subdiv = 1;
	interior = false;
}

BakedLightmapData::~BakedLightmapData() {
	VS::get_singleton()->free(baked_light);
}

///////////////////////////

Lightmapper::BakeStepFunc BakedLightmap::bake_step_function;
Lightmapper::BakeStepFunc BakedLightmap::bake_substep_function;
Lightmapper::BakeEndFunc BakedLightmap::bake_end_function;

Size2i BakedLightmap::_compute_lightmap_size(const MeshesFound &p_mesh) {
	double area = 0;
	double uv_area = 0;
	for (int i = 0; i < p_mesh.mesh->get_surface_count(); i++) {
		Array arrays = p_mesh.mesh->surface_get_arrays(i);
		PoolVector<Vector3> vertices = arrays[Mesh::ARRAY_VERTEX];
		PoolVector<Vector2> uv2 = arrays[Mesh::ARRAY_TEX_UV2];
		PoolVector<int> indices = arrays[Mesh::ARRAY_INDEX];

		ERR_FAIL_COND_V(vertices.size() == 0, Vector2());
		ERR_FAIL_COND_V(uv2.size() == 0, Vector2());

		int vc = vertices.size();
		PoolVector<Vector3>::Read vr = vertices.read();
		PoolVector<Vector2>::Read u2r = uv2.read();
		PoolVector<int>::Read ir;
		int ic = 0;

		if (indices.size()) {
			ic = indices.size();
			ir = indices.read();
		}

		int faces = ic ? ic / 3 : vc / 3;
		for (int j = 0; j < faces; j++) {
			Vector3 vertex[3];
			Vector2 uv[3];

			for (int k = 0; k < 3; k++) {
				int idx = ic ? ir[j * 3 + k] : j * 3 + k;
				vertex[k] = p_mesh.xform.xform(vr[idx]);
				uv[k] = u2r[idx];
			}

			Vector3 p1 = vertex[0];
			Vector3 p2 = vertex[1];
			Vector3 p3 = vertex[2];
			double a = p1.distance_to(p2);
			double b = p2.distance_to(p3);
			double c = p3.distance_to(p1);
			double halfPerimeter = (a + b + c) / 2.0;
			area += sqrt(halfPerimeter * (halfPerimeter - a) * (halfPerimeter - b) * (halfPerimeter - c));

			Vector2 uv_p1 = uv[0];
			Vector2 uv_p2 = uv[1];
			Vector2 uv_p3 = uv[2];
			double uv_a = uv_p1.distance_to(uv_p2);
			double uv_b = uv_p2.distance_to(uv_p3);
			double uv_c = uv_p3.distance_to(uv_p1);
			double uv_halfPerimeter = (uv_a + uv_b + uv_c) / 2.0;
			uv_area += sqrt(
					uv_halfPerimeter * (uv_halfPerimeter - uv_a) * (uv_halfPerimeter - uv_b) * (uv_halfPerimeter - uv_c));
		}
	}

	if (uv_area < 0.0001f) {
		uv_area = 1.0;
	}

	int pixels = Math::round(ceil((1.0 / sqrt(uv_area)) * sqrt(area * default_texels_per_unit)));
	int size = CLAMP(pixels, 2, 4096);
	return Vector2i(size, size);
}

void BakedLightmap::_find_meshes_and_lights(Node *p_at_node, Vector<MeshesFound> &meshes, Vector<LightsFound> &lights) {
	AABB bounds = AABB(-extents, extents * 2.0);

	MeshInstance *mi = Object::cast_to<MeshInstance>(p_at_node);
	if (mi && mi->get_flag(GeometryInstance::FLAG_USE_BAKED_LIGHT) && mi->is_visible_in_tree()) {
		Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_valid()) {
			bool all_have_uv2_and_normal = true;
			bool surfaces_found = false;
			for (int i = 0; i < mesh->get_surface_count(); i++) {
				if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
					continue;
				}
				if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_TEX_UV2)) {
					all_have_uv2_and_normal = false;
					break;
				}
				if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_NORMAL)) {
					all_have_uv2_and_normal = false;
					break;
				}
				surfaces_found = true;
			}

			if (surfaces_found && all_have_uv2_and_normal) {
				Transform mesh_xform = get_global_transform().affine_inverse() * mi->get_global_transform();

				AABB aabb = mesh_xform.xform(mesh->get_aabb());

				if (bounds.intersects(aabb)) {
					MeshesFound mf;
					mf.cast_shadows = mi->get_cast_shadows_setting() != GeometryInstance::SHADOW_CASTING_SETTING_OFF;
					mf.generate_lightmap = mi->get_generate_lightmap();
					mf.xform = mesh_xform;
					mf.node_path = get_path_to(mi);
					mf.subindex = -1;
					mf.mesh = mesh;

					static const int lightmap_scale[4] = { 1, 2, 4, 8 }; //GeometryInstance3D::LIGHTMAP_SCALE_MAX = { 1, 2, 4, 8 };
					mf.lightmap_scale = lightmap_scale[mi->get_lightmap_scale()];

					Ref<Material> all_override = mi->get_material_override();
					for (int i = 0; i < mesh->get_surface_count(); i++) {
						if (all_override.is_valid()) {
							mf.overrides.push_back(all_override);
						} else {
							mf.overrides.push_back(mi->get_surface_material(i));
						}
					}

					meshes.push_back(mf);
				}
			}
		}
	}

	Spatial *s = Object::cast_to<Spatial>(p_at_node);

	if (!mi && s) {
		Array bmeshes = p_at_node->call("get_bake_meshes");
		if (bmeshes.size() && (bmeshes.size() & 1) == 0) {
			Transform xf = get_global_transform().affine_inverse() * s->get_global_transform();
			Ref<Material> all_override;

			GeometryInstance *gi = Object::cast_to<GeometryInstance>(p_at_node);
			if (gi) {
				all_override = gi->get_material_override();
			}

			for (int i = 0; i < bmeshes.size(); i += 2) {
				Ref<Mesh> mesh = bmeshes[i];
				if (!mesh.is_valid()) {
					continue;
				}

				Transform mesh_xform = xf * bmeshes[i + 1];

				AABB aabb = mesh_xform.xform(mesh->get_aabb());

				if (!bounds.intersects(aabb)) {
					continue;
				}

				MeshesFound mf;
				mf.xform = mesh_xform;
				mf.node_path = get_path_to(s);
				mf.subindex = i / 2;
				mf.lightmap_scale = 1;
				mf.mesh = mesh;

				if (gi) {
					mf.cast_shadows = gi->get_cast_shadows_setting() != GeometryInstance::SHADOW_CASTING_SETTING_OFF;
					mf.generate_lightmap = gi->get_generate_lightmap();
				} else {
					mf.cast_shadows = true;
					mf.generate_lightmap = true;
				}

				for (int j = 0; j < mesh->get_surface_count(); j++) {
					mf.overrides.push_back(all_override);
				}

				meshes.push_back(mf);
			}
		}
	}

	Light *light = Object::cast_to<Light>(p_at_node);

	if (light && light->get_bake_mode() != Light::BAKE_DISABLED) {
		LightsFound lf;
		lf.xform = get_global_transform().affine_inverse() * light->get_global_transform();
		lf.light = light;
		lights.push_back(lf);
	}

	for (int i = 0; i < p_at_node->get_child_count(); i++) {
		Node *child = p_at_node->get_child(i);
		if (!child->get_owner()) {
			continue; //maybe a helper
		}

		_find_meshes_and_lights(child, meshes, lights);
	}
}

void BakedLightmap::_get_material_images(const MeshesFound &p_found_mesh, Lightmapper::MeshData &r_mesh_data, Vector<Ref<Texture>> &r_albedo_textures, Vector<Ref<Texture>> &r_emission_textures) {
	for (int i = 0; i < p_found_mesh.mesh->get_surface_count(); ++i) {
		Ref<Material3D> mat = p_found_mesh.overrides[i];

		if (mat.is_null()) {
			mat = p_found_mesh.mesh->surface_get_material(i);
		}

		Ref<Texture> albedo_texture;
		Color albedo_add = Color(1, 1, 1, 1);
		Color albedo_mul = Color(1, 1, 1, 1);

		Ref<Texture> emission_texture;
		Color emission_add = Color(0, 0, 0, 0);
		Color emission_mul = Color(1, 1, 1, 1);

		if (mat.is_valid()) {
			albedo_texture = mat->get_texture(Material3D::TEXTURE_ALBEDO);

			if (albedo_texture.is_valid()) {
				albedo_mul = mat->get_albedo();
				albedo_add = Color(0, 0, 0, 0);
			} else {
				albedo_add = mat->get_albedo();
			}

			emission_texture = mat->get_texture(Material3D::TEXTURE_EMISSION);
			Color emission_color = mat->get_emission();
			float emission_energy = mat->get_emission_energy();

			if (mat->get_emission_operator() == Material3D::EMISSION_OP_ADD) {
				emission_mul = Color(1, 1, 1) * emission_energy;
				emission_add = emission_color * emission_energy;
			} else {
				emission_mul = emission_color * emission_energy;
				emission_add = Color(0, 0, 0);
			}
		}

		Lightmapper::MeshData::TextureDef albedo;
		albedo.mul = albedo_mul;
		albedo.add = albedo_add;

		if (albedo_texture.is_valid()) {
			albedo.tex_rid = albedo_texture->get_rid();
			r_albedo_textures.push_back(albedo_texture);
		}

		r_mesh_data.albedo.push_back(albedo);

		Lightmapper::MeshData::TextureDef emission;
		emission.mul = emission_mul;
		emission.add = emission_add;

		if (emission_texture.is_valid()) {
			emission.tex_rid = emission_texture->get_rid();
			r_emission_textures.push_back(emission_texture);
		}
		r_mesh_data.emission.push_back(emission);
	}
}

void BakedLightmap::_save_image(String &r_base_path, Ref<Image> r_img, bool p_use_srgb) {
	if (use_hdr) {
		r_base_path += ".exr";
	} else {
		r_base_path += ".png";
	}

	String relative_path = r_base_path;
	if (relative_path.begins_with("res://")) {
		relative_path = relative_path.substr(6, relative_path.length());
	}

	bool hdr_grayscale = use_hdr && !use_color;

	r_img->lock();
	for (int i = 0; i < r_img->get_height(); i++) {
		for (int j = 0; j < r_img->get_width(); j++) {
			Color c = r_img->get_pixel(j, i);

			c.r = MAX(c.r, environment_min_light.r);
			c.g = MAX(c.g, environment_min_light.g);
			c.b = MAX(c.b, environment_min_light.b);

			if (hdr_grayscale) {
				c = Color(c.get_v(), 0.0f, 0.0f);
			}

			if (p_use_srgb) {
				c = c.to_srgb();
			}

			r_img->set_pixel(j, i, c);
		}
	}
	r_img->unlock();

	if (!use_color) {
		if (use_hdr) {
			r_img->convert(Image::FORMAT_RH);
		} else {
			r_img->convert(Image::FORMAT_L8);
		}
	}

	if (use_hdr) {
		r_img->save_exr(relative_path, !use_color);
	} else {
		r_img->save_png(relative_path);
	}
}

bool BakedLightmap::_lightmap_bake_step_function(float p_completion, const String &p_text, void *ud, bool p_refresh) {
	BakeStepUD *bsud = (BakeStepUD *)ud;
	bool ret = false;
	if (bsud->func) {
		ret = bsud->func(bsud->from_percent + p_completion * (bsud->to_percent - bsud->from_percent), p_text, bsud->ud, p_refresh);
	}
	return ret;
}

BakedLightmap::BakeError BakedLightmap::bake(Node *p_from_node, String p_data_save_path, bool p_preview) {
	if (!p_from_node && !get_parent()) {
		return BAKE_ERROR_NO_ROOT;
	}

	bool no_save_path = false;
	if (p_data_save_path == "" && (get_light_data().is_null() || !get_light_data()->get_path().is_resource_file())) {
		no_save_path = true;
	}

	if (p_data_save_path == "") {
		if (get_light_data().is_null()) {
			no_save_path = true;
		} else {
			p_data_save_path = get_light_data()->get_path();
			if (!p_data_save_path.is_resource_file()) {
				no_save_path = true;
			}
		}
	}

	if (no_save_path) {
		if (image_path == "") {
			return BAKE_ERROR_NO_SAVE_PATH;
		} else {
			p_data_save_path = image_path;
		}

		WARN_PRINT("Using the deprecated property \"image_path\" as a save path, consider providing a better save path via the \"data_save_path\" parameter");
		p_data_save_path = image_path.plus_file("BakedLightmap.lmbake");
	}

	{
		//check for valid save path
		DirAccessRef d = DirAccess::open(p_data_save_path.get_base_dir());
		if (!d) {
			ERR_FAIL_V_MSG(BAKE_ERROR_NO_SAVE_PATH, "Invalid save path '" + p_data_save_path + "'.");
		}
	}

	uint32_t time_started = OS::get_singleton()->get_ticks_msec();

	if (bake_step_function) {
		bool cancelled = bake_step_function(0.0, TTR("Finding meshes and lights"), nullptr, true);
		if (cancelled) {
			if (bake_end_function) {
				bake_end_function(time_started);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	Ref<Lightmapper> lightmapper = Lightmapper::create();
	if (lightmapper.is_null()) {
		if (bake_end_function) {
			bake_end_function(time_started);
		}
		return BAKE_ERROR_NO_LIGHTMAPPER;
	}

	Vector<LightsFound> lights_found;
	Vector<MeshesFound> meshes_found;

	_find_meshes_and_lights(p_from_node ? p_from_node : get_parent(), meshes_found, lights_found);

	if (meshes_found.size() == 0) {
		if (bake_end_function) {
			bake_end_function(time_started);
		}
		return BAKE_ERROR_NO_MESHES;
	}

	for (int m_i = 0; m_i < meshes_found.size(); m_i++) {
		if (bake_step_function) {
			float p = (float)(m_i) / meshes_found.size();
			bool cancelled = bake_step_function(p * 0.05, vformat(TTR("Preparing geometry (%d/%d)"), m_i + 1, meshes_found.size()), nullptr, false);
			if (cancelled) {
				if (bake_end_function) {
					bake_end_function(time_started);
				}
				return BAKE_ERROR_USER_ABORTED;
			}
		}

		MeshesFound &mf = meshes_found.write[m_i];

		Size2i lightmap_size = mf.mesh->get_lightmap_size_hint();

		if (lightmap_size == Vector2i(0, 0)) {
			lightmap_size = _compute_lightmap_size(mf);
		}
		lightmap_size *= mf.lightmap_scale;

		Lightmapper::MeshData md;

		{
			Dictionary d;
			d["path"] = mf.node_path;
			if (mf.subindex >= 0) {
				d["subindex"] = mf.subindex;
			}
			d["cast_shadows"] = mf.cast_shadows;
			d["generate_lightmap"] = mf.generate_lightmap;
			d["node_name"] = mf.node_path.get_name(mf.node_path.get_name_count() - 1);
			md.userdata = d;
		}

		Basis normal_xform = mf.xform.basis.inverse().transposed();

		for (int i = 0; i < mf.mesh->get_surface_count(); i++) {
			if (mf.mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
				continue;
			}
			Array a = mf.mesh->surface_get_arrays(i);

			Vector<Vector3> vertices = a[Mesh::ARRAY_VERTEX];
			const Vector3 *vr = vertices.ptr();
			Vector<Vector2> uv2 = a[Mesh::ARRAY_TEX_UV2];
			const Vector2 *uv2r = nullptr;
			Vector<Vector2> uv = a[Mesh::ARRAY_TEX_UV];
			const Vector2 *uvr = nullptr;
			Vector<Vector3> normals = a[Mesh::ARRAY_NORMAL];
			const Vector3 *nr = nullptr;
			Vector<int> index = a[Mesh::ARRAY_INDEX];

			ERR_CONTINUE(uv2.size() == 0);
			ERR_CONTINUE(normals.size() == 0);

			if (!uv.empty()) {
				uvr = uv.ptr();
			}

			uv2r = uv2.ptr();
			nr = normals.ptr();

			int facecount;
			const int *ir = nullptr;

			if (index.size()) {
				facecount = index.size() / 3;
				ir = index.ptr();
			} else {
				facecount = vertices.size() / 3;
			}

			md.surface_facecounts.push_back(facecount);

			for (int j = 0; j < facecount; j++) {
				uint32_t vidx[3];

				if (ir) {
					for (int k = 0; k < 3; k++) {
						vidx[k] = ir[j * 3 + k];
					}
				} else {
					for (int k = 0; k < 3; k++) {
						vidx[k] = j * 3 + k;
					}
				}

				for (int k = 0; k < 3; k++) {
					Vector3 v = mf.xform.xform(vr[vidx[k]]);
					md.points.push_back(v);

					md.uv2.push_back(uv2r[vidx[k]]);
					md.normal.push_back(normal_xform.xform(nr[vidx[k]]).normalized());

					if (uvr != nullptr) {
						md.uv.push_back(uvr[vidx[k]]);
					}
				}
			}
		}

		Vector<Ref<Texture>> albedo_textures;
		Vector<Ref<Texture>> emission_textures;

		_get_material_images(mf, md, albedo_textures, emission_textures);

		for (int j = 0; j < albedo_textures.size(); j++) {
			lightmapper->add_albedo_texture(albedo_textures[j]);
		}

		for (int j = 0; j < emission_textures.size(); j++) {
			lightmapper->add_emission_texture(emission_textures[j]);
		}

		lightmapper->add_mesh(md, lightmap_size);
	}

	for (int i = 0; i < lights_found.size(); i++) {
		Light *light = lights_found[i].light;
		Transform xf = lights_found[i].xform;

		if (Object::cast_to<DirectionalLight>(light)) {
			DirectionalLight *l = Object::cast_to<DirectionalLight>(light);
			lightmapper->add_directional_light(light->get_bake_mode() == Light::BAKE_ALL, -xf.basis.get_axis(Vector3::AXIS_Z).normalized(), l->get_color(), l->get_param(Light::PARAM_ENERGY), l->get_param(Light::PARAM_INDIRECT_ENERGY), l->get_param(Light::PARAM_SIZE));
		} else if (Object::cast_to<OmniLight>(light)) {
			OmniLight *l = Object::cast_to<OmniLight>(light);
			lightmapper->add_omni_light(light->get_bake_mode() == Light::BAKE_ALL, xf.origin, l->get_color(), l->get_param(Light::PARAM_ENERGY), l->get_param(Light::PARAM_INDIRECT_ENERGY), l->get_param(Light::PARAM_RANGE), l->get_param(Light::PARAM_ATTENUATION), l->get_param(Light::PARAM_SIZE));
		} else if (Object::cast_to<SpotLight>(light)) {
			SpotLight *l = Object::cast_to<SpotLight>(light);
			lightmapper->add_spot_light(light->get_bake_mode() == Light::BAKE_ALL, xf.origin, -xf.basis.get_axis(Vector3::AXIS_Z).normalized(), l->get_color(), l->get_param(Light::PARAM_ENERGY), l->get_param(Light::PARAM_INDIRECT_ENERGY), l->get_param(Light::PARAM_RANGE), l->get_param(Light::PARAM_ATTENUATION), l->get_param(Light::PARAM_SPOT_ANGLE), l->get_param(Light::PARAM_SPOT_ATTENUATION), l->get_param(Light::PARAM_SIZE));
		}
	}

	Ref<Image> environment_image;
	Basis environment_xform;

	if (environment_mode != ENVIRONMENT_MODE_DISABLED) {
		if (bake_step_function) {
			bake_step_function(0.1, TTR("Preparing environment"), nullptr, true);
		}

		switch (environment_mode) {
			case ENVIRONMENT_MODE_DISABLED: {
				//nothing
			} break;
			case ENVIRONMENT_MODE_SCENE: {
				Ref<World> world = get_world();
				if (world.is_valid()) {
					Ref<Environment> env = world->get_environment();
					if (env.is_null()) {
						env = world->get_fallback_environment();
					}

					if (env.is_valid()) {
						environment_image = _get_irradiance_map(env, Vector2i(128, 64));
						environment_xform = get_global_transform().affine_inverse().basis * env->get_sky_orientation();

						float ambient_sky = env->get_ambient_light_sky_contribution();
						float energy = env->get_ambient_light_energy();
						if (ambient_sky != 1.0 || energy != 1.0) {
							Color ambient_light = env->get_ambient_light_color().to_linear() * (1.0 - ambient_sky) * energy;
							environment_image->lock();
							for (int i = 0; i < 128; i++) {
								for (int j = 0; j < 64; j++) {
									Color c = ambient_light + environment_image->get_pixel(i, j) * ambient_sky * energy;
									environment_image->set_pixel(i, j, c);
								}
							}
							environment_image->unlock();
						}
					}
				}
			} break;
			case ENVIRONMENT_MODE_CUSTOM_SKY: {
				if (environment_custom_sky.is_valid()) {
					environment_image = _get_irradiance_from_sky(environment_custom_sky, environment_custom_energy, Vector2i(128, 64));
					environment_xform.set_euler(environment_custom_sky_rotation_degrees * Math_PI / 180.0);
				}

			} break;
			case ENVIRONMENT_MODE_CUSTOM_COLOR: {
				environment_image.instance();
				environment_image->create(128, 64, false, Image::FORMAT_RGBF);
				Color c = environment_custom_color;
				c.r *= environment_custom_energy;
				c.g *= environment_custom_energy;
				c.b *= environment_custom_energy;
				environment_image->lock();
				for (int i = 0; i < 128; i++) {
					for (int j = 0; j < 64; j++) {
						environment_image->set_pixel(i, j, c);
					}
				}
				environment_image->unlock();
			} break;
		}
	}

	BakeStepUD bsud;
	bsud.func = bake_step_function;
	bsud.ud = nullptr;
	bsud.from_percent = 0.1;
	bsud.to_percent = 0.9;

	bool gen_atlas = OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2 ? false : generate_atlas;

	Lightmapper::BakeError bake_err;
	if (p_preview) {
		// Use low-quality settings for quick iteration.
		bake_err = lightmapper->bake(
				Lightmapper::BakeQuality(int(GLOBAL_GET("rendering/cpu_lightmapper/preview_bake/quality"))),
				use_denoiser,
				MIN(bounces, int(GLOBAL_GET("rendering/cpu_lightmapper/preview_bake/max_bounces"))),
				bounce_indirect_energy,
				bias,
				gen_atlas,
				max_atlas_size,
				environment_image,
				environment_xform,
				_lightmap_bake_step_function,
				&bsud,
				bake_substep_function);
	} else {
		bake_err = lightmapper->bake(
				Lightmapper::BakeQuality(bake_quality),
				use_denoiser,
				bounces,
				bounce_indirect_energy,
				bias,
				gen_atlas,
				max_atlas_size,
				environment_image,
				environment_xform,
				_lightmap_bake_step_function,
				&bsud,
				bake_substep_function);
	}

	if (bake_err != Lightmapper::BAKE_OK) {
		if (bake_end_function) {
			bake_end_function(time_started);
		}
		switch (bake_err) {
			case Lightmapper::BAKE_ERROR_USER_ABORTED: {
				return BAKE_ERROR_USER_ABORTED;
			}
			case Lightmapper::BAKE_ERROR_LIGHTMAP_TOO_SMALL: {
				return BAKE_ERROR_LIGHTMAP_SIZE;
			}
			case Lightmapper::BAKE_ERROR_NO_MESHES: {
				return BAKE_ERROR_NO_MESHES;
			}
			default: {
			}
		}
		return BAKE_ERROR_NO_LIGHTMAPPER;
	}

	Ref<BakedLightmapData> data;
	if (get_light_data().is_valid()) {
		data = get_light_data();
		set_light_data(Ref<BakedLightmapData>()); //clear
		data->clear_data();
	} else {
		data.instance();
	}

	if (capture_enabled && (p_preview || GLOBAL_GET("rendering/cpu_lightmapper/preview_bake/allow_capture"))) {
		if (bake_step_function) {
			bool cancelled = bake_step_function(0.85, TTR("Generating capture"), nullptr, true);
			if (cancelled) {
				if (bake_end_function) {
					bake_end_function(time_started);
				}
				return BAKE_ERROR_USER_ABORTED;
			}
		}

		VoxelLightBaker voxel_baker;

		int bake_subdiv;
		int capture_subdiv;
		AABB bake_bounds;
		{
			bake_bounds = AABB(-extents, extents * 2.0);
			int subdiv = nearest_power_of_2_templated(int(bake_bounds.get_longest_axis_size() / capture_cell_size));
			bake_bounds.size[bake_bounds.get_longest_axis_index()] = subdiv * capture_cell_size;
			bake_subdiv = nearest_shift(subdiv) + 1;

			capture_subdiv = bake_subdiv;
			float css = capture_cell_size;
			while (css < capture_cell_size && capture_subdiv > 2) {
				capture_subdiv--;
				css *= 2.0;
			}
		}

		voxel_baker.begin_bake(capture_subdiv + 1, bake_bounds);

		for (int mesh_id = 0; mesh_id < meshes_found.size(); mesh_id++) {
			MeshesFound &mf = meshes_found.write[mesh_id];
			voxel_baker.plot_mesh(mf.xform, mf.mesh, mf.overrides, Ref<Material>());
		}

		VoxelLightBaker::BakeQuality capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_HIGH;
		if (capture_quality == BakedLightmap::BakeQuality::BAKE_QUALITY_LOW) {
			capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_LOW;
		} else if (capture_quality == BakedLightmap::BakeQuality::BAKE_QUALITY_MEDIUM) {
			capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_MEDIUM;
		}

		if (p_preview) {
			voxel_baker.begin_bake_light(VoxelLightBaker::BakeQuality(int(GLOBAL_GET("rendering/cpu_lightmapper/preview_bake/capture_quality"))), capture_propagation);
		} else {
			voxel_baker.begin_bake_light(capt_quality, capture_propagation);
		}

		for (int i = 0; i < lights_found.size(); i++) {
			LightsFound &lf = lights_found.write[i];
			switch (lf.light->get_light_type()) {
				case VS::LIGHT_DIRECTIONAL: {
					voxel_baker.plot_light_directional(-lf.xform.basis.get_axis(2), lf.light->get_color(), lf.light->get_param(Light::PARAM_ENERGY), lf.light->get_param(Light::PARAM_INDIRECT_ENERGY), lf.light->get_bake_mode() == Light::BAKE_ALL);
				} break;
				case VS::LIGHT_OMNI: {
					voxel_baker.plot_light_omni(lf.xform.origin, lf.light->get_color(), lf.light->get_param(Light::PARAM_ENERGY), lf.light->get_param(Light::PARAM_INDIRECT_ENERGY), lf.light->get_param(Light::PARAM_RANGE), lf.light->get_param(Light::PARAM_ATTENUATION), lf.light->get_bake_mode() == Light::BAKE_ALL);
				} break;
				case VS::LIGHT_SPOT: {
					voxel_baker.plot_light_spot(lf.xform.origin, lf.xform.basis.get_axis(2), lf.light->get_color(), lf.light->get_param(Light::PARAM_ENERGY), lf.light->get_param(Light::PARAM_INDIRECT_ENERGY), lf.light->get_param(Light::PARAM_RANGE), lf.light->get_param(Light::PARAM_ATTENUATION), lf.light->get_param(Light::PARAM_SPOT_ANGLE), lf.light->get_param(Light::PARAM_SPOT_ATTENUATION), lf.light->get_bake_mode() == Light::BAKE_ALL);

				} break;
			}
		}

		voxel_baker.end_bake();

		AABB bounds = AABB(-extents, extents * 2);
		data->set_cell_subdiv(capture_subdiv);
		data->set_bounds(bounds);
		data->set_octree(voxel_baker.create_capture_octree(capture_subdiv));

		{
			float bake_bound_size = bake_bounds.get_longest_axis_size();
			Transform to_bounds;
			to_bounds.basis.scale(Vector3(bake_bound_size, bake_bound_size, bake_bound_size));
			to_bounds.origin = bounds.position;

			Transform to_grid;
			to_grid.basis.scale(Vector3(1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1)));

			Transform to_cell_space = to_grid * to_bounds.affine_inverse();
			data->set_cell_space_transform(to_cell_space);
		}
	}

	if (bake_step_function) {
		bool cancelled = bake_step_function(0.9, TTR("Saving lightmaps"), nullptr, true);
		if (cancelled) {
			if (bake_end_function) {
				bake_end_function(time_started);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	Vector<Ref<Image>> images;
	for (int i = 0; i < lightmapper->get_bake_texture_count(); i++) {
		images.push_back(lightmapper->get_bake_texture(i));
	}

	bool use_srgb = use_color && !use_hdr;

	if (gen_atlas) {
		int slice_count = images.size();
		int slice_width = images[0]->get_width();
		int slice_height = images[0]->get_height();

		int slices_per_texture = Image::MAX_HEIGHT / slice_height;
		int texture_count = Math::ceil(slice_count / (float)slices_per_texture);

		Vector<Ref<TextureLayered>> textures;
		textures.resize(texture_count);
		String base_path = p_data_save_path.get_basename();

		int last_count = slice_count % slices_per_texture;
		for (int i = 0; i < texture_count; i++) {
			String texture_path = texture_count > 1 ? base_path + "_" + itos(i) : base_path;
			int texture_slice_count = (i == texture_count - 1 && last_count != 0) ? last_count : slices_per_texture;

			Ref<Image> large_image;
			large_image.instance();
			large_image->create(slice_width, slice_height * texture_slice_count, false, images[0]->get_format());

			for (int j = 0; j < texture_slice_count; j++) {
				large_image->blit_rect(images[i * slices_per_texture + j], Rect2(0, 0, slice_width, slice_height), Point2(0, slice_height * j));
			}
			_save_image(texture_path, large_image, use_srgb);

			Ref<ConfigFile> config;
			config.instance();
			if (FileAccess::exists(texture_path + ".import")) {
				config->load(texture_path + ".import");
			} else {
				// Set only if settings don't exist, to keep user choice
				config->set_value("params", "compress/mode", 0);
			}
			config->set_value("remap", "importer", "texture_array");
			config->set_value("remap", "type", "TextureArray");
			config->set_value("params", "detect_3d", false);
			config->set_value("params", "flags/repeat", false);
			config->set_value("params", "flags/filter", true);
			config->set_value("params", "flags/mipmaps", false);
			config->set_value("params", "flags/srgb", use_srgb);
			config->set_value("params", "slices/horizontal", 1);
			config->set_value("params", "slices/vertical", texture_slice_count);

			config->save(texture_path + ".import");

			ResourceLoader::import(texture_path);
			textures.write[i] = ResourceLoader::load(texture_path); //if already loaded, it will be updated on refocus?
		}

		for (int i = 0; i < lightmapper->get_bake_mesh_count(); i++) {
			if (!meshes_found[i].generate_lightmap) {
				continue;
			}
			Dictionary d = lightmapper->get_bake_mesh_userdata(i);
			NodePath np = d["path"];
			int32_t subindex = -1;
			if (d.has("subindex")) {
				subindex = d["subindex"];
			}

			Rect2 uv_rect = lightmapper->get_bake_mesh_uv_scale(i);
			int slice_index = lightmapper->get_bake_mesh_texture_slice(i);
			data->add_user(np, textures[slice_index / slices_per_texture], slice_index % slices_per_texture, uv_rect, subindex);
		}
	} else {
		for (int i = 0; i < lightmapper->get_bake_mesh_count(); i++) {
			if (!meshes_found[i].generate_lightmap) {
				continue;
			}

			Ref<Texture> texture;
			String base_path = p_data_save_path.get_base_dir().plus_file(images[i]->get_name());

			if (ResourceLoader::import) {
				_save_image(base_path, images[i], use_srgb);

				Ref<ConfigFile> config;
				config.instance();
				if (FileAccess::exists(base_path + ".import")) {
					config->load(base_path + ".import");
				} else {
					// Set only if settings don't exist, to keep user choice
					config->set_value("params", "compress/mode", 0);
				}
				config->set_value("remap", "importer", "texture");
				config->set_value("remap", "type", "StreamTexture");
				config->set_value("params", "detect_3d", false);
				config->set_value("params", "flags/repeat", false);
				config->set_value("params", "flags/filter", true);
				config->set_value("params", "flags/mipmaps", false);
				config->set_value("params", "flags/srgb", use_srgb);

				config->save(base_path + ".import");

				ResourceLoader::import(base_path);
				texture = ResourceLoader::load(base_path); //if already loaded, it will be updated on refocus?
			} else {
				base_path += ".tex";
				Ref<ImageTexture> tex;
				bool set_path = true;
				if (ResourceCache::has(base_path)) {
					tex = Ref<Resource>((Resource *)ResourceCache::get(base_path));
					set_path = false;
				}

				if (!tex.is_valid()) {
					tex.instance();
				}

				tex->create_from_image(images[i], Texture::FLAGS_DEFAULT);

				ResourceSaver::save(base_path, tex, ResourceSaver::FLAG_CHANGE_PATH);
				if (set_path) {
					tex->set_path(base_path);
				}
				texture = tex;
			}

			Dictionary d = lightmapper->get_bake_mesh_userdata(i);
			NodePath np = d["path"];
			int32_t subindex = -1;
			if (d.has("subindex")) {
				subindex = d["subindex"];
			}

			Rect2 uv_rect = Rect2(0, 0, 1, 1);
			int slice_index = -1;
			data->add_user(np, texture, slice_index, uv_rect, subindex);
		}
	}

	if (bake_step_function) {
		bool cancelled = bake_step_function(1.0, TTR("Done"), nullptr, true);
		if (cancelled) {
			if (bake_end_function) {
				bake_end_function(time_started);
			}
			return BAKE_ERROR_USER_ABORTED;
		}
	}

	Error err = ResourceSaver::save(p_data_save_path, data);
	data->set_path(p_data_save_path);

	if (err != OK) {
		if (bake_end_function) {
			bake_end_function(time_started);
		}
		return BAKE_ERROR_CANT_CREATE_IMAGE;
	}

	// Only change the preview bake status when everything is completed.
	// If the user cancels the bake, the preview bake status shouldn't change.
	data->set_preview_bake(p_preview);
	set_light_data(data);
	if (bake_end_function) {
		bake_end_function(time_started);
	}

	return BAKE_ERROR_OK;
}

void BakedLightmap::set_capture_cell_size(float p_cell_size) {
	capture_cell_size = MAX(0.1, p_cell_size);
}

float BakedLightmap::get_capture_cell_size() const {
	return capture_cell_size;
}

void BakedLightmap::set_extents(const Vector3 &p_extents) {
	extents = p_extents;
	update_gizmo();
	_change_notify("extents");
}

Vector3 BakedLightmap::get_extents() const {
	return extents;
}

void BakedLightmap::set_default_texels_per_unit(const float &p_bake_texels_per_unit) {
	default_texels_per_unit = MAX(0.0, p_bake_texels_per_unit);
}

float BakedLightmap::get_default_texels_per_unit() const {
	return default_texels_per_unit;
}

void BakedLightmap::set_capture_enabled(bool p_enable) {
	capture_enabled = p_enable;
	_change_notify();
}

bool BakedLightmap::get_capture_enabled() const {
	return capture_enabled;
}

void BakedLightmap::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (light_data.is_valid()) {
			_assign_lightmaps();
		}
		request_ready(); //will need ready again if re-enters tree
	}

	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (light_data.is_valid()) {
			_clear_lightmaps();
		}
	}
}

void BakedLightmap::_assign_lightmaps() {
	ERR_FAIL_COND(!light_data.is_valid());

	bool atlassed_on_gles2 = false;

	for (int i = 0; i < light_data->get_user_count(); i++) {
		Ref<Resource> lightmap = light_data->get_user_lightmap(i);
		ERR_CONTINUE(!lightmap.is_valid());
		ERR_CONTINUE(!Object::cast_to<Texture>(lightmap.ptr()) && !Object::cast_to<TextureLayered>(lightmap.ptr()));

		Node *node = get_node(light_data->get_user_path(i));
		int instance_idx = light_data->get_user_instance(i);
		if (instance_idx >= 0) {
			RID instance = node->call("get_bake_mesh_instance", instance_idx);
			if (instance.is_valid()) {
				int slice = light_data->get_user_lightmap_slice(i);
				atlassed_on_gles2 = atlassed_on_gles2 || (slice != -1 && OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2);
				VS::get_singleton()->instance_set_use_lightmap(instance, get_instance(), lightmap->get_rid(), slice, light_data->get_user_lightmap_uv_rect(i));
			}
		} else {
			VisualInstance *vi = Object::cast_to<VisualInstance>(node);
			ERR_CONTINUE(!vi);
			int slice = light_data->get_user_lightmap_slice(i);
			atlassed_on_gles2 = atlassed_on_gles2 || (slice != -1 && OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2);
			VS::get_singleton()->instance_set_use_lightmap(vi->get_instance(), get_instance(), lightmap->get_rid(), slice, light_data->get_user_lightmap_uv_rect(i));
		}
	}

	if (atlassed_on_gles2) {
		ERR_PRINT("GLES2 doesn't support layered textures, so lightmap atlassing is not supported. Please re-bake the lightmap or switch to GLES3.");
	}
}

void BakedLightmap::_clear_lightmaps() {
	ERR_FAIL_COND(!light_data.is_valid());
	for (int i = 0; i < light_data->get_user_count(); i++) {
		Node *node = get_node(light_data->get_user_path(i));
		int instance_idx = light_data->get_user_instance(i);
		if (instance_idx >= 0) {
			RID instance = node->call("get_bake_mesh_instance", instance_idx);
			if (instance.is_valid()) {
				VS::get_singleton()->instance_set_use_lightmap(instance, get_instance(), RID(), -1, Rect2(0, 0, 1, 1));
			}
		} else {
			VisualInstance *vi = Object::cast_to<VisualInstance>(node);
			ERR_CONTINUE(!vi);
			VS::get_singleton()->instance_set_use_lightmap(vi->get_instance(), get_instance(), RID(), -1, Rect2(0, 0, 1, 1));
		}
	}
}

Ref<Image> BakedLightmap::_get_irradiance_from_sky(Ref<Sky> p_sky, float p_energy, Vector2i p_size) {
	if (p_sky.is_null()) {
		return Ref<Image>();
	}

	Ref<Image> sky_image;
	Ref<PanoramaSky> panorama = p_sky;
	if (panorama.is_valid()) {
		sky_image = panorama->get_panorama()->get_data();
	}
	Ref<ProceduralSky> procedural = p_sky;
	if (procedural.is_valid()) {
		sky_image = procedural->get_data();
	}

	if (sky_image.is_null()) {
		return Ref<Image>();
	}

	sky_image->convert(Image::FORMAT_RGBF);
	sky_image->resize(p_size.x, p_size.y, Image::INTERPOLATE_CUBIC);

	if (p_energy != 1.0) {
		sky_image->lock();
		for (int i = 0; i < p_size.y; i++) {
			for (int j = 0; j < p_size.x; j++) {
				sky_image->set_pixel(j, i, sky_image->get_pixel(j, i) * p_energy);
			}
		}
		sky_image->unlock();
	}

	return sky_image;
}

Ref<Image> BakedLightmap::_get_irradiance_map(Ref<Environment> p_env, Vector2i p_size) {
	Environment::BGMode bg_mode = p_env->get_background();
	switch (bg_mode) {
		case Environment::BG_SKY: {
			return _get_irradiance_from_sky(p_env->get_sky(), p_env->get_bg_energy(), Vector2i(128, 64));
		}
		case Environment::BG_CLEAR_COLOR:
		case Environment::BG_COLOR: {
			Color c = bg_mode == Environment::BG_CLEAR_COLOR ? Color(GLOBAL_GET("rendering/environment/default_clear_color")) : p_env->get_bg_color();
			c.r *= p_env->get_bg_energy();
			c.g *= p_env->get_bg_energy();
			c.b *= p_env->get_bg_energy();

			Ref<Image> ret;
			ret.instance();
			ret->create(p_size.x, p_size.y, false, Image::FORMAT_RGBF);
			ret->fill(c);
			return ret;
		}
		default: {
		}
	}
	return Ref<Image>();
}

void BakedLightmap::set_light_data(const Ref<BakedLightmapData> &p_data) {
	if (light_data.is_valid()) {
		if (is_inside_tree()) {
			_clear_lightmaps();
		}
		set_base(RID());
	}
	light_data = p_data;
	_change_notify();
	update_configuration_warning();

	if (light_data.is_valid()) {
		set_base(light_data->get_rid());
		if (is_inside_tree()) {
			_assign_lightmaps();
		}
	}
}

Ref<BakedLightmapData> BakedLightmap::get_light_data() const {
	return light_data;
}

void BakedLightmap::set_capture_propagation(float p_propagation) {
	capture_propagation = p_propagation;
}

float BakedLightmap::get_capture_propagation() const {
	return capture_propagation;
}

void BakedLightmap::set_capture_quality(BakeQuality p_quality) {
	capture_quality = p_quality;
}

BakedLightmap::BakeQuality BakedLightmap::get_capture_quality() const {
	return capture_quality;
}

void BakedLightmap::set_generate_atlas(bool p_enabled) {
	generate_atlas = p_enabled;
}

bool BakedLightmap::is_generate_atlas_enabled() const {
	return generate_atlas;
}

void BakedLightmap::set_max_atlas_size(int p_size) {
	ERR_FAIL_COND(p_size < 2048);
	max_atlas_size = p_size;
}

int BakedLightmap::get_max_atlas_size() const {
	return max_atlas_size;
}

void BakedLightmap::set_bake_quality(BakeQuality p_quality) {
	bake_quality = p_quality;
	_change_notify();
}

BakedLightmap::BakeQuality BakedLightmap::get_bake_quality() const {
	return bake_quality;
}

#ifndef DISABLE_DEPRECATED
void BakedLightmap::set_image_path(const String &p_path) {
	image_path = p_path;
}

String BakedLightmap::get_image_path() const {
	return image_path;
}
#endif

void BakedLightmap::set_use_denoiser(bool p_enable) {
	use_denoiser = p_enable;
}

bool BakedLightmap::is_using_denoiser() const {
	return use_denoiser;
}

void BakedLightmap::set_use_hdr(bool p_enable) {
	use_hdr = p_enable;
}

bool BakedLightmap::is_using_hdr() const {
	return use_hdr;
}

void BakedLightmap::set_use_color(bool p_enable) {
	use_color = p_enable;
}

bool BakedLightmap::is_using_color() const {
	return use_color;
}

void BakedLightmap::set_environment_mode(EnvironmentMode p_mode) {
	environment_mode = p_mode;
	_change_notify();
}

BakedLightmap::EnvironmentMode BakedLightmap::get_environment_mode() const {
	return environment_mode;
}

void BakedLightmap::set_environment_custom_sky(const Ref<Sky> &p_sky) {
	environment_custom_sky = p_sky;
}

Ref<Sky> BakedLightmap::get_environment_custom_sky() const {
	return environment_custom_sky;
}

void BakedLightmap::set_environment_custom_sky_rotation_degrees(const Vector3 &p_rotation) {
	environment_custom_sky_rotation_degrees = p_rotation;
}

Vector3 BakedLightmap::get_environment_custom_sky_rotation_degrees() const {
	return environment_custom_sky_rotation_degrees;
}

void BakedLightmap::set_environment_custom_color(const Color &p_color) {
	environment_custom_color = p_color;
}
Color BakedLightmap::get_environment_custom_color() const {
	return environment_custom_color;
}

void BakedLightmap::set_environment_custom_energy(float p_energy) {
	environment_custom_energy = p_energy;
}
float BakedLightmap::get_environment_custom_energy() const {
	return environment_custom_energy;
}

void BakedLightmap::set_environment_min_light(Color p_min_light) {
	environment_min_light = p_min_light;
}

Color BakedLightmap::get_environment_min_light() const {
	return environment_min_light;
}

void BakedLightmap::set_bounces(int p_bounces) {
	ERR_FAIL_COND(p_bounces < 0 || p_bounces > 16);
	bounces = p_bounces;
}

int BakedLightmap::get_bounces() const {
	return bounces;
}

void BakedLightmap::set_bounce_indirect_energy(float p_indirect_energy) {
	ERR_FAIL_COND(p_indirect_energy < 0.0);
	bounce_indirect_energy = p_indirect_energy;
}

float BakedLightmap::get_bounce_indirect_energy() const {
	return bounce_indirect_energy;
}

void BakedLightmap::set_bias(float p_bias) {
	ERR_FAIL_COND(p_bias < 0.00001f);
	bias = p_bias;
}

float BakedLightmap::get_bias() const {
	return bias;
}

AABB BakedLightmap::get_aabb() const {
	return AABB(-extents, extents * 2);
}
PoolVector<Face3> BakedLightmap::get_faces(uint32_t p_usage_flags) const {
	return PoolVector<Face3>();
}

void BakedLightmap::_validate_property(PropertyInfo &property) const {
	if (property.name.begins_with("environment_custom_sky") && environment_mode != ENVIRONMENT_MODE_CUSTOM_SKY) {
		property.usage = 0;
	}

	if (property.name == "environment_custom_color" && environment_mode != ENVIRONMENT_MODE_CUSTOM_COLOR) {
		property.usage = 0;
	}

	if (property.name == "environment_custom_energy" && environment_mode != ENVIRONMENT_MODE_CUSTOM_COLOR && environment_mode != ENVIRONMENT_MODE_CUSTOM_SKY) {
		property.usage = 0;
	}

	if (property.name.begins_with("atlas") && OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2) {
		property.usage = PROPERTY_USAGE_NOEDITOR;
	}

	if (property.name.begins_with("capture") && property.name != "capture_enabled" && !capture_enabled) {
		property.usage = 0;
	}
}

String BakedLightmap::get_configuration_warning() const {
	String warning = VisualInstance::get_configuration_warning();
	if (get_light_data().is_valid() && get_light_data()->is_preview_bake()) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("This BakedLightmap's data uses a preview bake which has lower quality than a \"final\" bake.\nRemember to use the Bake Lightmaps button at the top of the 3D editor viewport (instead of Preview Bake) before distributing your project.");
	}

	return warning;
}

void BakedLightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_light_data", "data"), &BakedLightmap::set_light_data);
	ClassDB::bind_method(D_METHOD("get_light_data"), &BakedLightmap::get_light_data);

	ClassDB::bind_method(D_METHOD("set_bake_quality", "quality"), &BakedLightmap::set_bake_quality);
	ClassDB::bind_method(D_METHOD("get_bake_quality"), &BakedLightmap::get_bake_quality);

	ClassDB::bind_method(D_METHOD("set_bounces", "bounces"), &BakedLightmap::set_bounces);
	ClassDB::bind_method(D_METHOD("get_bounces"), &BakedLightmap::get_bounces);

	ClassDB::bind_method(D_METHOD("set_bounce_indirect_energy", "bounce_indirect_energy"), &BakedLightmap::set_bounce_indirect_energy);
	ClassDB::bind_method(D_METHOD("get_bounce_indirect_energy"), &BakedLightmap::get_bounce_indirect_energy);

	ClassDB::bind_method(D_METHOD("set_bias", "bias"), &BakedLightmap::set_bias);
	ClassDB::bind_method(D_METHOD("get_bias"), &BakedLightmap::get_bias);

	ClassDB::bind_method(D_METHOD("set_environment_mode", "mode"), &BakedLightmap::set_environment_mode);
	ClassDB::bind_method(D_METHOD("get_environment_mode"), &BakedLightmap::get_environment_mode);

	ClassDB::bind_method(D_METHOD("set_environment_custom_sky", "sky"), &BakedLightmap::set_environment_custom_sky);
	ClassDB::bind_method(D_METHOD("get_environment_custom_sky"), &BakedLightmap::get_environment_custom_sky);

	ClassDB::bind_method(D_METHOD("set_environment_custom_sky_rotation_degrees", "rotation"), &BakedLightmap::set_environment_custom_sky_rotation_degrees);
	ClassDB::bind_method(D_METHOD("get_environment_custom_sky_rotation_degrees"), &BakedLightmap::get_environment_custom_sky_rotation_degrees);

	ClassDB::bind_method(D_METHOD("set_environment_custom_color", "color"), &BakedLightmap::set_environment_custom_color);
	ClassDB::bind_method(D_METHOD("get_environment_custom_color"), &BakedLightmap::get_environment_custom_color);

	ClassDB::bind_method(D_METHOD("set_environment_custom_energy", "energy"), &BakedLightmap::set_environment_custom_energy);
	ClassDB::bind_method(D_METHOD("get_environment_custom_energy"), &BakedLightmap::get_environment_custom_energy);

	ClassDB::bind_method(D_METHOD("set_environment_min_light", "min_light"), &BakedLightmap::set_environment_min_light);
	ClassDB::bind_method(D_METHOD("get_environment_min_light"), &BakedLightmap::get_environment_min_light);

	ClassDB::bind_method(D_METHOD("set_use_denoiser", "use_denoiser"), &BakedLightmap::set_use_denoiser);
	ClassDB::bind_method(D_METHOD("is_using_denoiser"), &BakedLightmap::is_using_denoiser);

	ClassDB::bind_method(D_METHOD("set_use_hdr", "use_denoiser"), &BakedLightmap::set_use_hdr);
	ClassDB::bind_method(D_METHOD("is_using_hdr"), &BakedLightmap::is_using_hdr);

	ClassDB::bind_method(D_METHOD("set_use_color", "use_denoiser"), &BakedLightmap::set_use_color);
	ClassDB::bind_method(D_METHOD("is_using_color"), &BakedLightmap::is_using_color);

	ClassDB::bind_method(D_METHOD("set_generate_atlas", "enabled"), &BakedLightmap::set_generate_atlas);
	ClassDB::bind_method(D_METHOD("is_generate_atlas_enabled"), &BakedLightmap::is_generate_atlas_enabled);

	ClassDB::bind_method(D_METHOD("set_max_atlas_size", "max_atlas_size"), &BakedLightmap::set_max_atlas_size);
	ClassDB::bind_method(D_METHOD("get_max_atlas_size"), &BakedLightmap::get_max_atlas_size);

	ClassDB::bind_method(D_METHOD("set_capture_quality", "capture_quality"), &BakedLightmap::set_capture_quality);
	ClassDB::bind_method(D_METHOD("get_capture_quality"), &BakedLightmap::get_capture_quality);

	ClassDB::bind_method(D_METHOD("set_extents", "extents"), &BakedLightmap::set_extents);
	ClassDB::bind_method(D_METHOD("get_extents"), &BakedLightmap::get_extents);

	ClassDB::bind_method(D_METHOD("set_default_texels_per_unit", "texels"), &BakedLightmap::set_default_texels_per_unit);
	ClassDB::bind_method(D_METHOD("get_default_texels_per_unit"), &BakedLightmap::get_default_texels_per_unit);

	ClassDB::bind_method(D_METHOD("set_capture_propagation", "propagation"), &BakedLightmap::set_capture_propagation);
	ClassDB::bind_method(D_METHOD("get_capture_propagation"), &BakedLightmap::get_capture_propagation);

	ClassDB::bind_method(D_METHOD("set_capture_enabled", "enabled"), &BakedLightmap::set_capture_enabled);
	ClassDB::bind_method(D_METHOD("get_capture_enabled"), &BakedLightmap::get_capture_enabled);

	ClassDB::bind_method(D_METHOD("set_capture_cell_size", "capture_cell_size"), &BakedLightmap::set_capture_cell_size);
	ClassDB::bind_method(D_METHOD("get_capture_cell_size"), &BakedLightmap::get_capture_cell_size);
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_image_path", "image_path"), &BakedLightmap::set_image_path);
	ClassDB::bind_method(D_METHOD("get_image_path"), &BakedLightmap::get_image_path);
#endif
	ClassDB::bind_method(D_METHOD("bake", "from_node", "data_save_path", "preview"), &BakedLightmap::bake, DEFVAL(Variant()), DEFVAL(""), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "extents"), "set_extents", "get_extents");

	ADD_GROUP("Tweaks", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "quality", PROPERTY_HINT_ENUM, "Low,Medium,High,Ultra"), "set_bake_quality", "get_bake_quality");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bounces", PROPERTY_HINT_RANGE, "0,16,1"), "set_bounces", "get_bounces");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "bounce_indirect_energy", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_bounce_indirect_energy", "get_bounce_indirect_energy");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_denoiser"), "set_use_denoiser", "is_using_denoiser");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_color"), "set_use_color", "is_using_color");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "bias", PROPERTY_HINT_RANGE, "0.00001,0.1,0.00001,or_greater"), "set_bias", "get_bias");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "default_texels_per_unit", PROPERTY_HINT_RANGE, "0.0,64.0,0.01,or_greater"), "set_default_texels_per_unit", "get_default_texels_per_unit");

	ADD_GROUP("Atlas", "atlas_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "atlas_generate"), "set_generate_atlas", "is_generate_atlas_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "atlas_max_size"), "set_max_atlas_size", "get_max_atlas_size");

	ADD_GROUP("Environment", "environment_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "environment_mode", PROPERTY_HINT_ENUM, "Disabled,Scene,Custom Sky,Custom Color"), "set_environment_mode", "get_environment_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment_custom_sky", PROPERTY_HINT_RESOURCE_TYPE, "Sky"), "set_environment_custom_sky", "get_environment_custom_sky");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "environment_custom_sky_rotation_degrees", PROPERTY_HINT_NONE), "set_environment_custom_sky_rotation_degrees", "get_environment_custom_sky_rotation_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "environment_custom_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_environment_custom_color", "get_environment_custom_color");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "environment_custom_energy", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_environment_custom_energy", "get_environment_custom_energy");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "environment_min_light", PROPERTY_HINT_COLOR_NO_ALPHA), "set_environment_min_light", "get_environment_min_light");

	ADD_GROUP("Capture", "capture_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "capture_enabled"), "set_capture_enabled", "get_capture_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "capture_cell_size", PROPERTY_HINT_RANGE, "0.25,2.0,0.05,or_greater"), "set_capture_cell_size", "get_capture_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "capture_quality", PROPERTY_HINT_ENUM, "Low,Medium,High"), "set_capture_quality", "get_capture_quality");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "capture_propagation", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_capture_propagation", "get_capture_propagation");

	ADD_GROUP("Data", "");
#ifndef DISABLE_DEPRECATED
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "image_path", PROPERTY_HINT_DIR, "", 0), "set_image_path", "get_image_path");
#endif
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "light_data", PROPERTY_HINT_RESOURCE_TYPE, "BakedLightmapData"), "set_light_data", "get_light_data");

	BIND_ENUM_CONSTANT(BAKE_QUALITY_LOW);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_HIGH);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_ULTRA);

	BIND_ENUM_CONSTANT(BAKE_ERROR_OK);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_SAVE_PATH);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_MESHES);
	BIND_ENUM_CONSTANT(BAKE_ERROR_CANT_CREATE_IMAGE);
	BIND_ENUM_CONSTANT(BAKE_ERROR_LIGHTMAP_SIZE);
	BIND_ENUM_CONSTANT(BAKE_ERROR_INVALID_MESH);
	BIND_ENUM_CONSTANT(BAKE_ERROR_USER_ABORTED);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_LIGHTMAPPER);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_ROOT);

	BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_DISABLED);
	BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_SCENE);
	BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_CUSTOM_SKY);
	BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_CUSTOM_COLOR);
}

BakedLightmap::BakedLightmap() {
	extents = Vector3(10, 10, 10);

	default_texels_per_unit = 16.0f;
	bake_quality = BAKE_QUALITY_MEDIUM;
	capture_quality = BAKE_QUALITY_MEDIUM;
	capture_propagation = 1;
	capture_enabled = true;
	bounces = 3;
	bounce_indirect_energy = 1.0;
	image_path = "";
	set_disable_scale(true);
	capture_cell_size = 0.5;

	environment_mode = ENVIRONMENT_MODE_DISABLED;
	environment_custom_color = Color(0.2, 0.7, 1.0);
	environment_custom_energy = 1.0;
	environment_min_light = Color(0.0, 0.0, 0.0);

	use_denoiser = true;
	use_hdr = true;
	use_color = true;
	bias = 0.005;

	generate_atlas = true;
	max_atlas_size = 4096;
}
