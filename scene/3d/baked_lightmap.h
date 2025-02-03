/**************************************************************************/
/*  baked_lightmap.h                                                      */
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

#ifndef BAKED_LIGHTMAP_H
#define BAKED_LIGHTMAP_H

#include "core/local_vector.h"
#include "multimesh_instance.h"
#include "scene/3d/light.h"
#include "scene/3d/lightmapper.h"
#include "scene/3d/visual_instance.h"

class BakedLightmapData : public Resource {
	GDCLASS(BakedLightmapData, Resource);
	RES_BASE_EXTENSION("lmbake")

	RID baked_light;
	AABB bounds;
	float energy;
	bool interior;
	int cell_subdiv;
	bool preview_bake = false;
	Transform cell_space_xform;

	struct User {
		NodePath path;
		struct {
			Ref<Texture> single;
			Ref<TextureLayered> layered;
		} lightmap;
		int lightmap_slice;
		Rect2 lightmap_uv_rect;
		int instance_index;
	};

	Vector<User> users;

	void _set_user_data(const Array &p_data);
	Array _get_user_data() const;

protected:
	static void _bind_methods();

public:
	void set_bounds(const AABB &p_bounds);
	AABB get_bounds() const;

	void set_octree(const PoolVector<uint8_t> &p_octree);
	PoolVector<uint8_t> get_octree() const;

	void set_cell_space_transform(const Transform &p_xform);
	Transform get_cell_space_transform() const;

	void set_cell_subdiv(int p_cell_subdiv);
	int get_cell_subdiv() const;

	void set_energy(float p_energy);
	float get_energy() const;

	void set_interior(bool p_interior);
	bool is_interior() const;

	void set_preview_bake(bool p_preview_bake);
	bool is_preview_bake() const;

	void add_user(const NodePath &p_path, const Ref<Resource> &p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect, int p_instance);
	int get_user_count() const;
	NodePath get_user_path(int p_user) const;
	Ref<Resource> get_user_lightmap(int p_user) const;
	int get_user_lightmap_slice(int p_user) const;
	Rect2 get_user_lightmap_uv_rect(int p_user) const;
	int get_user_instance(int p_user) const;
	void clear_users();
	void clear_data();

	virtual RID get_rid() const;
	BakedLightmapData();
	~BakedLightmapData();
};

class BakedLightmap : public VisualInstance {
	GDCLASS(BakedLightmap, VisualInstance);

public:
	enum BakeQuality {
		BAKE_QUALITY_LOW,
		BAKE_QUALITY_MEDIUM,
		BAKE_QUALITY_HIGH,
		BAKE_QUALITY_ULTRA
	};

	enum BakeError {
		BAKE_ERROR_OK,
		BAKE_ERROR_NO_SAVE_PATH,
		BAKE_ERROR_NO_MESHES,
		BAKE_ERROR_CANT_CREATE_IMAGE,
		BAKE_ERROR_LIGHTMAP_SIZE,
		BAKE_ERROR_INVALID_MESH,
		BAKE_ERROR_USER_ABORTED,
		BAKE_ERROR_NO_LIGHTMAPPER,
		BAKE_ERROR_NO_ROOT,
	};

	enum EnvironmentMode {
		ENVIRONMENT_MODE_DISABLED,
		ENVIRONMENT_MODE_SCENE,
		ENVIRONMENT_MODE_CUSTOM_SKY,
		ENVIRONMENT_MODE_CUSTOM_COLOR
	};

	struct BakeStepUD {
		Lightmapper::BakeStepFunc func;
		void *ud;
		float from_percent;
		float to_percent;
	};

	struct LightsFound {
		Transform xform;
		Light *light;
	};

	struct MeshesFound {
		Transform xform;
		NodePath node_path;
		int32_t subindex;
		Ref<Mesh> mesh;
		int32_t lightmap_scale;
		Vector<Ref<Material>> overrides;
		bool cast_shadows;
		bool generate_lightmap;
	};

private:
	Vector3 extents;
	float default_texels_per_unit;
	float bias;
	BakeQuality bake_quality;
	bool generate_atlas;
	int max_atlas_size;
	bool capture_enabled;
	int bounces;
	float bounce_indirect_energy;
	bool use_denoiser;
	bool use_hdr;
	bool use_color;

	EnvironmentMode environment_mode;
	Ref<Sky> environment_custom_sky;
	Vector3 environment_custom_sky_rotation_degrees;
	Color environment_custom_color;
	float environment_custom_energy;
	Color environment_min_light;

	BakeQuality capture_quality;
	float capture_propagation;
	float capture_cell_size;

	String image_path; // (Deprecated property)

	Ref<BakedLightmapData> light_data;

	void _assign_lightmaps();
	void _clear_lightmaps();

	void _get_material_images(const MeshesFound &p_found_mesh, Lightmapper::MeshData &r_mesh_data, Vector<Ref<Texture>> &r_albedo_textures, Vector<Ref<Texture>> &r_emission_textures);
	Ref<Image> _get_irradiance_from_sky(Ref<Sky> p_sky, float p_energy, Vector2i p_size);
	Ref<Image> _get_irradiance_map(Ref<Environment> p_env, Vector2i p_size);
	void _find_meshes_and_lights(Node *p_at_node, Vector<MeshesFound> &meshes, Vector<LightsFound> &lights);
	Vector2i _compute_lightmap_size(const MeshesFound &p_mesh);

	static bool _lightmap_bake_step_function(float p_completion, const String &p_text, void *ud, bool p_refresh);
	void _save_image(String &r_base_path, Ref<Image> p_img, bool p_use_srgb);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &property) const;
	void _notification(int p_what);

public:
	static Lightmapper::BakeStepFunc bake_step_function;
	static Lightmapper::BakeStepFunc bake_substep_function;
	static Lightmapper::BakeEndFunc bake_end_function;

	void set_light_data(const Ref<BakedLightmapData> &p_data);
	Ref<BakedLightmapData> get_light_data() const;

	void set_capture_cell_size(float p_cell_size);
	float get_capture_cell_size() const;

	void set_extents(const Vector3 &p_extents);
	Vector3 get_extents() const;

	void set_default_texels_per_unit(const float &p_extents);
	float get_default_texels_per_unit() const;

	void set_capture_propagation(float p_propagation);
	float get_capture_propagation() const;

	void set_capture_quality(BakeQuality p_quality);
	BakeQuality get_capture_quality() const;

	void set_bake_quality(BakeQuality p_quality);
	BakeQuality get_bake_quality() const;

	void set_generate_atlas(bool p_enabled);
	bool is_generate_atlas_enabled() const;

	void set_max_atlas_size(int p_size);
	int get_max_atlas_size() const;

	void set_capture_enabled(bool p_enable);
	bool get_capture_enabled() const;

	void set_image_path(const String &p_path);
	String get_image_path() const;

	void set_environment_mode(EnvironmentMode p_mode);
	EnvironmentMode get_environment_mode() const;

	void set_environment_custom_sky(const Ref<Sky> &p_sky);
	Ref<Sky> get_environment_custom_sky() const;

	void set_environment_custom_sky_rotation_degrees(const Vector3 &p_rotation);
	Vector3 get_environment_custom_sky_rotation_degrees() const;

	void set_environment_custom_color(const Color &p_color);
	Color get_environment_custom_color() const;

	void set_environment_custom_energy(float p_energy);
	float get_environment_custom_energy() const;

	void set_environment_min_light(Color p_min_light);
	Color get_environment_min_light() const;

	void set_use_denoiser(bool p_enable);
	bool is_using_denoiser() const;

	void set_use_hdr(bool p_enable);
	bool is_using_hdr() const;

	void set_use_color(bool p_enable);
	bool is_using_color() const;

	void set_bounces(int p_bounces);
	int get_bounces() const;

	void set_bounce_indirect_energy(float p_indirect_energy);
	float get_bounce_indirect_energy() const;

	void set_bias(float p_bias);
	float get_bias() const;

	AABB get_aabb() const;
	PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

	virtual String get_configuration_warning() const;

	BakeError bake(Node *p_from_node, String p_data_save_path = "", bool p_preview = false);
	BakedLightmap();
};

VARIANT_ENUM_CAST(BakedLightmap::BakeQuality);
VARIANT_ENUM_CAST(BakedLightmap::BakeError);
VARIANT_ENUM_CAST(BakedLightmap::EnvironmentMode);

#endif // BAKED_LIGHTMAP_H
