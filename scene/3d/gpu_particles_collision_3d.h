/**************************************************************************/
/*  gpu_particles_collision_3d.h                                          */
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
#include "scene/3d/visual_instance_3d.h"

class GPUParticlesCollision3D : public VisualInstance3D {
	GDCLASS(GPUParticlesCollision3D, VisualInstance3D);

	uint32_t cull_mask = 0xFFFFFFFF;
	RID collision;

protected:
	_FORCE_INLINE_ RID _get_collision() { return collision; }
	static void _bind_methods();

	GPUParticlesCollision3D(RS::ParticlesCollisionType p_type);

public:
	void set_cull_mask(uint32_t p_cull_mask);
	uint32_t get_cull_mask() const;

	~GPUParticlesCollision3D();
};

class GPUParticlesCollisionSphere3D : public GPUParticlesCollision3D {
	GDCLASS(GPUParticlesCollisionSphere3D, GPUParticlesCollision3D);

	real_t radius = 1.0;

protected:
	static void _bind_methods();

public:
	void set_radius(real_t p_radius);
	real_t get_radius() const;

	virtual AABB get_aabb() const override;

	GPUParticlesCollisionSphere3D();
	~GPUParticlesCollisionSphere3D();
};

class GPUParticlesCollisionBox3D : public GPUParticlesCollision3D {
	GDCLASS(GPUParticlesCollisionBox3D, GPUParticlesCollision3D);

	Vector3 size = Vector3(2, 2, 2);

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	virtual AABB get_aabb() const override;

	GPUParticlesCollisionBox3D();
	~GPUParticlesCollisionBox3D();
};

class GPUParticlesCollisionSDF3D : public GPUParticlesCollision3D {
	GDCLASS(GPUParticlesCollisionSDF3D, GPUParticlesCollision3D);

public:
	enum Resolution {
		RESOLUTION_16,
		RESOLUTION_32,
		RESOLUTION_64,
		RESOLUTION_128,
		RESOLUTION_256,
		RESOLUTION_512,
		RESOLUTION_MAX,
	};

	typedef void (*BakeBeginFunc)(int);
	typedef void (*BakeStepFunc)(int, const String &);
	typedef void (*BakeEndFunc)();

private:
	Vector3 size = Vector3(2, 2, 2);
	Resolution resolution = RESOLUTION_64;
	uint32_t bake_mask = 0xFFFFFFFF;
	Ref<Texture3D> texture;
	float thickness = 1.0;

	struct PlotMesh {
		Ref<Mesh> mesh;
		Transform3D local_xform;
	};

	void _find_meshes(const AABB &p_aabb, Node *p_at_node, List<PlotMesh> &plot_meshes);

	struct BVH {
		enum {
			LEAF_BIT = 1 << 30,
			LEAF_MASK = LEAF_BIT - 1
		};
		AABB bounds;
		uint32_t children[2] = {};
	};

	struct FacePos {
		Vector3 center;
		uint32_t index = 0;
	};

	struct FaceSort {
		uint32_t axis = 0;
		bool operator()(const FacePos &p_left, const FacePos &p_right) const {
			return p_left.center[axis] < p_right.center[axis];
		}
	};

	uint32_t _create_bvh(LocalVector<BVH> &bvh_tree, FacePos *p_faces, uint32_t p_face_count, const Face3 *p_triangles, float p_thickness);

	struct ComputeSDFParams {
		float *cells = nullptr;
		Vector3i size;
		float cell_size = 0.0;
		Vector3 cell_offset;
		const BVH *bvh = nullptr;
		const Face3 *triangles = nullptr;
		float thickness = 0.0;
	};

	void _find_closest_distance(const Vector3 &p_pos, const BVH *p_bvh, uint32_t p_bvh_cell, const Face3 *p_triangles, float p_thickness, float &r_closest_distance);
	void _compute_sdf_z(uint32_t p_z, ComputeSDFParams *params);
	void _compute_sdf(ComputeSDFParams *params);

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	virtual PackedStringArray get_configuration_warnings() const override;

	void set_thickness(float p_thickness);
	float get_thickness() const;

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_resolution(Resolution p_resolution);
	Resolution get_resolution() const;

	void set_bake_mask(uint32_t p_mask);
	uint32_t get_bake_mask() const;

	void set_bake_mask_value(int p_layer_number, bool p_enable);
	bool get_bake_mask_value(int p_layer_number) const;

	void set_texture(const Ref<Texture3D> &p_texture);
	Ref<Texture3D> get_texture() const;

	Vector3i get_estimated_cell_size() const;
	Ref<Image> bake();

	virtual AABB get_aabb() const override;

	static BakeBeginFunc bake_begin_function;
	static BakeStepFunc bake_step_function;
	static BakeEndFunc bake_end_function;

	GPUParticlesCollisionSDF3D();
	~GPUParticlesCollisionSDF3D();
};

VARIANT_ENUM_CAST(GPUParticlesCollisionSDF3D::Resolution)

class GPUParticlesCollisionHeightField3D : public GPUParticlesCollision3D {
	GDCLASS(GPUParticlesCollisionHeightField3D, GPUParticlesCollision3D);

public:
	enum Resolution {
		RESOLUTION_256,
		RESOLUTION_512,
		RESOLUTION_1024,
		RESOLUTION_2048,
		RESOLUTION_4096,
		RESOLUTION_8192,
		RESOLUTION_MAX,
	};

	enum UpdateMode {
		UPDATE_MODE_WHEN_MOVED,
		UPDATE_MODE_ALWAYS,
	};

private:
	uint32_t heightfield_mask = (1 << 20) - 1; // Only the first 20 bits are set by default to ignore editor layers.
	Vector3 size = Vector3(2, 2, 2);
	Resolution resolution = RESOLUTION_1024;
	bool follow_camera_mode = false;

	UpdateMode update_mode = UPDATE_MODE_WHEN_MOVED;

protected:
	void _notification(int p_what);
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_resolution(Resolution p_resolution);
	Resolution get_resolution() const;

	void set_update_mode(UpdateMode p_update_mode);
	UpdateMode get_update_mode() const;

	void set_heightfield_mask(uint32_t p_heightfield_mask);
	uint32_t get_heightfield_mask() const;

	void set_heightfield_mask_value(int p_layer_number, bool p_value);
	bool get_heightfield_mask_value(int p_layer_number) const;

	void set_follow_camera_enabled(bool p_enabled);
	bool is_follow_camera_enabled() const;

	virtual AABB get_aabb() const override;

	GPUParticlesCollisionHeightField3D();
	~GPUParticlesCollisionHeightField3D();
};

VARIANT_ENUM_CAST(GPUParticlesCollisionHeightField3D::Resolution)
VARIANT_ENUM_CAST(GPUParticlesCollisionHeightField3D::UpdateMode)

class GPUParticlesAttractor3D : public VisualInstance3D {
	GDCLASS(GPUParticlesAttractor3D, VisualInstance3D);

	uint32_t cull_mask = 0xFFFFFFFF;
	RID collision;
	real_t strength = 1.0;
	real_t attenuation = 1.0;
	real_t directionality = 0.0;

protected:
	_FORCE_INLINE_ RID _get_collision() { return collision; }
	static void _bind_methods();

	GPUParticlesAttractor3D(RS::ParticlesCollisionType p_type);

public:
	void set_cull_mask(uint32_t p_cull_mask);
	uint32_t get_cull_mask() const;

	void set_strength(real_t p_strength);
	real_t get_strength() const;

	void set_attenuation(real_t p_attenuation);
	real_t get_attenuation() const;

	void set_directionality(real_t p_directionality);
	real_t get_directionality() const;

	~GPUParticlesAttractor3D();
};

class GPUParticlesAttractorSphere3D : public GPUParticlesAttractor3D {
	GDCLASS(GPUParticlesAttractorSphere3D, GPUParticlesAttractor3D);

	real_t radius = 1.0;

protected:
	static void _bind_methods();

public:
	void set_radius(real_t p_radius);
	real_t get_radius() const;

	virtual AABB get_aabb() const override;

	GPUParticlesAttractorSphere3D();
	~GPUParticlesAttractorSphere3D();
};

class GPUParticlesAttractorBox3D : public GPUParticlesAttractor3D {
	GDCLASS(GPUParticlesAttractorBox3D, GPUParticlesAttractor3D);

	Vector3 size = Vector3(2, 2, 2);

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	virtual AABB get_aabb() const override;

	GPUParticlesAttractorBox3D();
	~GPUParticlesAttractorBox3D();
};

class GPUParticlesAttractorVectorField3D : public GPUParticlesAttractor3D {
	GDCLASS(GPUParticlesAttractorVectorField3D, GPUParticlesAttractor3D);

	Vector3 size = Vector3(2, 2, 2);
	Ref<Texture3D> texture;

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_texture(const Ref<Texture3D> &p_texture);
	Ref<Texture3D> get_texture() const;

	virtual AABB get_aabb() const override;

	GPUParticlesAttractorVectorField3D();
	~GPUParticlesAttractorVectorField3D();
};
