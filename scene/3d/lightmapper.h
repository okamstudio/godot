/**************************************************************************/
/*  lightmapper.h                                                         */
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

#ifndef LIGHTMAPPER_H
#define LIGHTMAPPER_H

#include "core/object/ref_counted.h"

class Image;

#if !defined(__aligned)

#if defined(_WIN32) && defined(_MSC_VER)
#define __aligned(...) __declspec(align(__VA_ARGS__))
#else
#define __aligned(...) __attribute__((aligned(__VA_ARGS__)))
#endif

#endif

class LightmapDenoiser : public RefCounted {
	GDCLASS(LightmapDenoiser, RefCounted)
protected:
	static LightmapDenoiser *(*create_function)();

public:
	virtual Ref<Image> denoise_image(const Ref<Image> &p_image) = 0;
	static Ref<LightmapDenoiser> create();
};

class LightmapRaycaster : public RefCounted {
	GDCLASS(LightmapRaycaster, RefCounted)
protected:
	static LightmapRaycaster *(*create_function)();

public:
	// Compatible with embree4 rays.
	struct __aligned(16) Ray {
		const static unsigned int INVALID_GEOMETRY_ID = ((unsigned int)-1); // from rtcore_common.h

		/*! Default construction does nothing. */
		_FORCE_INLINE_ Ray() :
				geomID(INVALID_GEOMETRY_ID) {}

		/*! Constructs a ray from origin, direction, and ray segment. Near
		 *  has to be smaller than far. */
		_FORCE_INLINE_ Ray(const Vector3 &p_org,
				const Vector3 &p_dir,
				float p_tnear = 0.0f,
				float p_tfar = INFINITY) :
				org(p_org),
				tnear(p_tnear),
				dir(p_dir),
				time(0.0f),
				tfar(p_tfar),
				mask(-1),
				u(0.0),
				v(0.0),
				primID(INVALID_GEOMETRY_ID),
				geomID(INVALID_GEOMETRY_ID),
				instID(INVALID_GEOMETRY_ID) {}

		/*! Tests if we hit something. */
		_FORCE_INLINE_ explicit operator bool() const {
			return geomID != INVALID_GEOMETRY_ID;
		}

	public:
		Vector3 org; //!< Ray origin + tnear
		float tnear; //!< Start of ray segment
		Vector3 dir; //!< Ray direction + tfar
		float time; //!< Time of this ray for motion blur.
		float tfar; //!< End of ray segment
		unsigned int mask; //!< used to mask out objects during traversal
		unsigned int id; //!< ray ID
		unsigned int flags; //!< ray flags

		Vector3 normal; //!< Not normalized geometry normal
		float u; //!< Barycentric u coordinate of hit
		float v; //!< Barycentric v coordinate of hit
		unsigned int primID; //!< primitive ID
		unsigned int geomID; //!< geometry ID
		unsigned int instID; //!< instance ID
	};

	virtual bool intersect(Ray &p_ray) = 0;

	virtual void intersect(Vector<Ray> &r_rays) = 0;

	virtual void add_mesh(const Vector<Vector3> &p_vertices, const Vector<Vector3> &p_normals, const Vector<Vector2> &p_uv2s, unsigned int p_id) = 0;
	virtual void set_mesh_alpha_texture(Ref<Image> p_alpha_texture, unsigned int p_id) = 0;
	virtual void commit() = 0;

	virtual void set_mesh_filter(const HashSet<int> &p_mesh_ids) = 0;
	virtual void clear_mesh_filter() = 0;

	static Ref<LightmapRaycaster> create();
};

class Lightmapper : public RefCounted {
	GDCLASS(Lightmapper, RefCounted)
public:
	enum GenerateProbes {
		GENERATE_PROBES_DISABLED,
		GENERATE_PROBES_SUBDIV_4,
		GENERATE_PROBES_SUBDIV_8,
		GENERATE_PROBES_SUBDIV_16,
		GENERATE_PROBES_SUBDIV_32,
	};

	enum LightType {
		LIGHT_TYPE_DIRECTIONAL,
		LIGHT_TYPE_OMNI,
		LIGHT_TYPE_SPOT
	};

	enum BakeError {
		BAKE_OK,
		BAKE_ERROR_TEXTURE_EXCEEDS_MAX_SIZE,
		BAKE_ERROR_LIGHTMAP_CANT_PRE_BAKE_MESHES,
		BAKE_ERROR_ATLAS_TOO_SMALL,
		BAKE_ERROR_USER_ABORTED,
	};

	enum BakeQuality {
		BAKE_QUALITY_LOW,
		BAKE_QUALITY_MEDIUM,
		BAKE_QUALITY_HIGH,
		BAKE_QUALITY_ULTRA,
	};

	typedef Lightmapper *(*CreateFunc)();

	static CreateFunc create_custom;
	static CreateFunc create_gpu;
	static CreateFunc create_cpu;

protected:
public:
	typedef bool (*BakeStepFunc)(float, const String &, void *, bool); //step index, step total, step description, userdata

	struct MeshData {
		//triangle data
		Vector<Vector3> points;
		Vector<Vector2> uv2;
		Vector<Vector3> normal;
		Ref<Image> albedo_on_uv2;
		Ref<Image> emission_on_uv2;
		Variant userdata;
	};

	virtual void add_mesh(const MeshData &p_mesh) = 0;
	virtual void add_directional_light(const String &p_name, bool p_static, const Vector3 &p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_angular_distance, float p_shadow_blur) = 0;
	virtual void add_omni_light(const String &p_name, bool p_static, const Vector3 &p_position, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_size, float p_shadow_blur) = 0;
	virtual void add_spot_light(const String &p_name, bool p_static, const Vector3 &p_position, const Vector3 p_direction, const Color &p_color, float p_energy, float p_indirect_energy, float p_range, float p_attenuation, float p_spot_angle, float p_spot_attenuation, float p_size, float p_shadow_blur) = 0;
	virtual void add_probe(const Vector3 &p_position) = 0;
	virtual BakeError bake(BakeQuality p_quality, bool p_use_denoiser, float p_denoiser_strength, int p_denoiser_range, int p_bounces, float p_bounce_indirect_energy, float p_bias, int p_max_texture_size, bool p_bake_sh, bool p_bake_shadowmask, bool p_texture_for_bounces, GenerateProbes p_generate_probes, const Ref<Image> &p_environment_panorama, const Basis &p_environment_transform, BakeStepFunc p_step_function = nullptr, void *p_step_userdata = nullptr, float p_exposure_normalization = 1.0) = 0;

	virtual int get_bake_texture_count() const = 0;
	virtual Ref<Image> get_bake_texture(int p_index) const = 0;
	virtual int get_shadowmask_texture_count() const = 0;
	virtual Ref<Image> get_shadowmask_texture(int p_index) const = 0;
	virtual int get_bake_mesh_count() const = 0;
	virtual Variant get_bake_mesh_userdata(int p_index) const = 0;
	virtual Rect2 get_bake_mesh_uv_scale(int p_index) const = 0;
	virtual int get_bake_mesh_texture_slice(int p_index) const = 0;
	virtual int get_bake_probe_count() const = 0;
	virtual Vector3 get_bake_probe_point(int p_probe) const = 0;
	virtual Vector<Color> get_bake_probe_sh(int p_probe) const = 0;

	static Ref<Lightmapper> create();

	Lightmapper();
};

#endif // LIGHTMAPPER_H
