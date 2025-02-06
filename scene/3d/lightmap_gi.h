/**************************************************************************/
/*  lightmap_gi.h                                                         */
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
#include "scene/3d/light_3d.h"
#include "scene/3d/lightmapper.h"
#include "scene/3d/visual_instance_3d.h"

class Sky;
class CameraAttributes;

class LightmapGIData : public Resource {
	GDCLASS(LightmapGIData, Resource);
	RES_BASE_EXTENSION("lmbake")

public:
	enum ShadowmaskMode {
		SHADOWMASK_MODE_NONE,
		SHADOWMASK_MODE_REPLACE,
		SHADOWMASK_MODE_OVERLAY,
		SHADOWMASK_MODE_ONLY,
	};

private:
	// The 'merged' texture atlases actually used by the renderer.
	Ref<TextureLayered> combined_light_texture;
	Ref<TextureLayered> combined_shadowmask_texture;

	// The temporary texture atlas arrays which are used for storage.
	// If a single atlas is too large, it's split and recombined during loading.
	TypedArray<TextureLayered> storage_light_textures;
	TypedArray<TextureLayered> storage_shadowmask_textures;

	bool uses_spherical_harmonics = false;
	bool interior = false;

	bool _uses_packed_directional = false;

	RID lightmap;
	AABB bounds;
	float baked_exposure = 1.0;

	struct User {
		NodePath path;
		int32_t sub_instance = 0;
		Rect2 uv_scale;
		int slice_index = 0;
	};

	Vector<User> users;

	void _set_user_data(const Array &p_data);
	Array _get_user_data() const;
	void _set_probe_data(const Dictionary &p_data);
	Dictionary _get_probe_data() const;

	void _reset_lightmap_textures();
	void _reset_shadowmask_textures();

protected:
	static void _bind_methods();

public:
	void add_user(const NodePath &p_path, const Rect2 &p_uv_scale, int p_slice_index, int32_t p_sub_instance = -1);
	int get_user_count() const;
	NodePath get_user_path(int p_user) const;
	int32_t get_user_sub_instance(int p_user) const;
	Rect2 get_user_lightmap_uv_scale(int p_user) const;
	int get_user_lightmap_slice_index(int p_user) const;
	void clear_users();

#ifndef DISABLE_DEPRECATED
	void set_light_texture(const Ref<TextureLayered> &p_light_texture);
	Ref<TextureLayered> get_light_texture() const;

	void _set_light_textures_data(const Array &p_data);
	Array _get_light_textures_data() const;
#endif

	void set_uses_spherical_harmonics(bool p_enable);
	bool is_using_spherical_harmonics() const;

	void _set_uses_packed_directional(bool p_enable);
	bool _is_using_packed_directional() const;

	void update_shadowmask_mode(ShadowmaskMode p_mode);
	ShadowmaskMode get_shadowmask_mode() const;

	bool is_interior() const;
	float get_baked_exposure() const;

	void set_capture_data(const AABB &p_bounds, bool p_interior, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree, float p_baked_exposure);
	PackedVector3Array get_capture_points() const;
	PackedColorArray get_capture_sh() const;
	PackedInt32Array get_capture_tetrahedra() const;
	PackedInt32Array get_capture_bsp_tree() const;
	AABB get_capture_bounds() const;

	void clear();

	void set_lightmap_textures(const TypedArray<TextureLayered> &p_data);
	TypedArray<TextureLayered> get_lightmap_textures() const;

	void set_shadowmask_textures(const TypedArray<TextureLayered> &p_data);
	TypedArray<TextureLayered> get_shadowmask_textures() const;
	void clear_shadowmask_textures();
	bool has_shadowmask_textures();

	virtual RID get_rid() const override;
	LightmapGIData();
	~LightmapGIData();
};

class LightmapGI : public VisualInstance3D {
	GDCLASS(LightmapGI, VisualInstance3D);

public:
	enum BakeQuality {
		BAKE_QUALITY_LOW,
		BAKE_QUALITY_MEDIUM,
		BAKE_QUALITY_HIGH,
		BAKE_QUALITY_ULTRA,
	};

	enum GenerateProbes {
		GENERATE_PROBES_DISABLED,
		GENERATE_PROBES_SUBDIV_4,
		GENERATE_PROBES_SUBDIV_8,
		GENERATE_PROBES_SUBDIV_16,
		GENERATE_PROBES_SUBDIV_32,
	};

	enum BakeError {
		BAKE_ERROR_OK,
		BAKE_ERROR_NO_SCENE_ROOT,
		BAKE_ERROR_FOREIGN_DATA,
		BAKE_ERROR_NO_LIGHTMAPPER,
		BAKE_ERROR_NO_SAVE_PATH,
		BAKE_ERROR_NO_MESHES,
		BAKE_ERROR_MESHES_INVALID,
		BAKE_ERROR_CANT_CREATE_IMAGE,
		BAKE_ERROR_USER_ABORTED,
		BAKE_ERROR_TEXTURE_SIZE_TOO_SMALL,
		BAKE_ERROR_LIGHTMAP_TOO_SMALL,
		BAKE_ERROR_ATLAS_TOO_SMALL,
	};

	enum EnvironmentMode {
		ENVIRONMENT_MODE_DISABLED,
		ENVIRONMENT_MODE_SCENE,
		ENVIRONMENT_MODE_CUSTOM_SKY,
		ENVIRONMENT_MODE_CUSTOM_COLOR,
	};

private:
	BakeQuality bake_quality = BAKE_QUALITY_MEDIUM;
	bool use_denoiser = true;
	float denoiser_strength = 0.1f;
	int denoiser_range = 10;
	int bounces = 3;
	float bounce_indirect_energy = 1.0;
	float bias = 0.0005;
	float texel_scale = 1.0;
	int max_texture_size = 16384;
	bool supersampling_enabled = false;
	float supersampling_factor = 2.0;
	bool interior = false;
	EnvironmentMode environment_mode = ENVIRONMENT_MODE_SCENE;
	Ref<Sky> environment_custom_sky;
	Color environment_custom_color = Color(1, 1, 1);
	float environment_custom_energy = 1.0;
	bool directional = false;
	bool use_texture_for_bounces = true;
	LightmapGIData::ShadowmaskMode shadowmask_mode = LightmapGIData::SHADOWMASK_MODE_NONE;
	GenerateProbes gen_probes = GENERATE_PROBES_SUBDIV_8;
	Ref<CameraAttributes> camera_attributes;

	Ref<LightmapGIData> light_data;
	Node *last_owner = nullptr;

	struct LightsFound {
		Transform3D xform;
		Light3D *light = nullptr;
	};

	struct MeshesFound {
		Transform3D xform;
		NodePath node_path;
		int32_t subindex = 0;
		Ref<Mesh> mesh;
		float lightmap_scale = 0.0;
		Vector<Ref<Material>> overrides;
	};

	void _find_meshes_and_lights(Node *p_at_node, Vector<MeshesFound> &meshes, Vector<LightsFound> &lights, Vector<Vector3> &probes);

	void _assign_lightmaps();
	void _clear_lightmaps();

	struct BakeTimeData {
		String text;
		int pass = 0;
		uint64_t last_step = 0;
	};

	struct BSPSimplex {
		int vertices[4] = {};
		int planes[4] = {};
	};

	struct BSPNode {
		static const int32_t EMPTY_LEAF = INT32_MIN;
		Plane plane;
		int32_t over = EMPTY_LEAF;
		int32_t under = EMPTY_LEAF;
	};

	int _bsp_get_simplex_side(const Vector<Vector3> &p_points, const LocalVector<BSPSimplex> &p_simplices, const Plane &p_plane, uint32_t p_simplex) const;
	int32_t _compute_bsp_tree(const Vector<Vector3> &p_points, const LocalVector<Plane> &p_planes, LocalVector<int32_t> &planes_tested, const LocalVector<BSPSimplex> &p_simplices, const LocalVector<int32_t> &p_simplex_indices, LocalVector<BSPNode> &bsp_nodes);

	struct BakeStepUD {
		Lightmapper::BakeStepFunc func;
		void *ud = nullptr;
		float from_percent = 0.0;
		float to_percent = 0.0;
	};

	static bool _lightmap_bake_step_function(float p_completion, const String &p_text, void *ud, bool p_refresh);

	struct GenProbesOctree {
		Vector3i offset;
		uint32_t size = 0;
		GenProbesOctree *children[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		~GenProbesOctree() {
			for (int i = 0; i < 8; i++) {
				if (children[i] != nullptr) {
					memdelete(children[i]);
				}
			}
		}
	};

	void _plot_triangle_into_octree(GenProbesOctree *p_cell, float p_cell_size, const Vector3 *p_triangle);
	void _gen_new_positions_from_octree(const GenProbesOctree *p_cell, float p_cell_size, const Vector<Vector3> &probe_positions, LocalVector<Vector3> &new_probe_positions, HashMap<Vector3i, bool> &positions_used, const AABB &p_bounds);

	BakeError _save_and_reimport_atlas_textures(const Ref<Lightmapper> p_lightmapper, const String &p_base_name, TypedArray<TextureLayered> &r_textures, bool p_is_shadowmask = false) const;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_light_data(const Ref<LightmapGIData> &p_data);
	Ref<LightmapGIData> get_light_data() const;

	void set_bake_quality(BakeQuality p_quality);
	BakeQuality get_bake_quality() const;

	void set_use_denoiser(bool p_enable);
	bool is_using_denoiser() const;

	void set_denoiser_strength(float p_denoiser_strength);
	float get_denoiser_strength() const;

	void set_denoiser_range(int p_denoiser_range);
	int get_denoiser_range() const;

	void set_directional(bool p_enable);
	bool is_directional() const;

	void set_shadowmask_mode(LightmapGIData::ShadowmaskMode p_mode);
	LightmapGIData::ShadowmaskMode get_shadowmask_mode() const;

	void set_use_texture_for_bounces(bool p_enable);
	bool is_using_texture_for_bounces() const;

	void set_interior(bool p_interior);
	bool is_interior() const;

	void set_environment_mode(EnvironmentMode p_mode);
	EnvironmentMode get_environment_mode() const;

	void set_environment_custom_sky(const Ref<Sky> &p_sky);
	Ref<Sky> get_environment_custom_sky() const;

	void set_environment_custom_color(const Color &p_color);
	Color get_environment_custom_color() const;

	void set_environment_custom_energy(float p_energy);
	float get_environment_custom_energy() const;

	void set_bounces(int p_bounces);
	int get_bounces() const;

	void set_bounce_indirect_energy(float p_indirect_energy);
	float get_bounce_indirect_energy() const;

	void set_bias(float p_bias);
	float get_bias() const;

	void set_texel_scale(float p_multiplier);
	float get_texel_scale() const;

	void set_max_texture_size(int p_size);
	int get_max_texture_size() const;

	void set_supersampling_enabled(bool p_enable);
	bool is_supersampling_enabled() const;

	void set_supersampling_factor(float p_factor);
	float get_supersampling_factor() const;

	void set_generate_probes(GenerateProbes p_generate_probes);
	GenerateProbes get_generate_probes() const;

	void set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes);
	Ref<CameraAttributes> get_camera_attributes() const;

	AABB get_aabb() const override;

	BakeError bake(Node *p_from_node, String p_image_data_path = "", Lightmapper::BakeStepFunc p_bake_step = nullptr, void *p_bake_userdata = nullptr);

	virtual PackedStringArray get_configuration_warnings() const override;

	LightmapGI();
};

VARIANT_ENUM_CAST(LightmapGIData::ShadowmaskMode);
VARIANT_ENUM_CAST(LightmapGI::BakeQuality);
VARIANT_ENUM_CAST(LightmapGI::GenerateProbes);
VARIANT_ENUM_CAST(LightmapGI::BakeError);
VARIANT_ENUM_CAST(LightmapGI::EnvironmentMode);
