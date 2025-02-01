/**************************************************************************/
/*  camera_3d.h                                                           */
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

#include "scene/3d/node_3d.h"
#include "scene/3d/velocity_tracker_3d.h"
#include "scene/resources/camera_attributes.h"
#include "scene/resources/compositor.h"
#include "scene/resources/environment.h"

class Camera3D : public Node3D {
	GDCLASS(Camera3D, Node3D);

public:
	enum ProjectionType {
		PROJECTION_PERSPECTIVE,
		PROJECTION_ORTHOGONAL,
		PROJECTION_FRUSTUM
	};

	enum KeepAspect {
		KEEP_WIDTH,
		KEEP_HEIGHT
	};

	enum DopplerTracking {
		DOPPLER_TRACKING_DISABLED,
		DOPPLER_TRACKING_IDLE_STEP,
		DOPPLER_TRACKING_PHYSICS_STEP
	};

private:
	bool force_change = false;
	bool current = false;
	Viewport *viewport = nullptr;

	ProjectionType mode = PROJECTION_PERSPECTIVE;

	real_t fov = 75.0;
	real_t size = 1.0;
	Vector2 frustum_offset;
	// _ prefix to avoid conflict with Windows defines.
	real_t _near = 0.05;
	real_t _far = 4000.0;
	real_t v_offset = 0.0;
	real_t h_offset = 0.0;
	KeepAspect keep_aspect = KEEP_HEIGHT;

	RID camera;
	RID scenario_id;

	// String camera_group;

	uint32_t layers = 0xfffff;

	Ref<Environment> environment;
	Ref<CameraAttributes> attributes;
	Ref<Compositor> compositor;
	void _attributes_changed();

	// void _camera_make_current(Node *p_camera);
	friend class Viewport;
	void _update_audio_listener_state();
	TypedArray<Plane> _get_frustum() const;

	DopplerTracking doppler_tracking = DOPPLER_TRACKING_DISABLED;
	Ref<VelocityTracker3D> velocity_tracker;

	RID pyramid_shape;
	Vector<Vector3> pyramid_shape_points;

	///////////////////////////////////////////////////////
	// INTERPOLATION FUNCTIONS
	void _physics_interpolation_ensure_transform_calculated(bool p_force = false) const;
	void _physics_interpolation_ensure_data_flipped();

	// These can be set by derived Camera3Ds, if they wish to do processing
	// (while still allowing physics interpolation to function).
	bool _desired_process_internal = false;
	bool _desired_physics_process_internal = false;

	mutable struct InterpolationData {
		Transform3D xform_curr;
		Transform3D xform_prev;
		Transform3D xform_interpolated;
		Transform3D camera_xform_interpolated; // After modification according to camera type.
		uint32_t last_update_physics_tick = 0;
		uint32_t last_update_frame = UINT32_MAX;
	} _interpolation_data;

	void _update_process_mode();

protected:
	// Use from derived classes to set process modes instead of setting directly.
	// This is because physics interpolation may need to request process modes additionally.
	void set_desired_process_modes(bool p_process_internal, bool p_physics_process_internal);

	// Opportunity for derived classes to interpolate extra attributes.
	virtual void physics_interpolation_flip_data() {}

	virtual void _physics_interpolated_changed() override;
	virtual Transform3D _get_adjusted_camera_transform(const Transform3D &p_xform) const;
	///////////////////////////////////////////////////////

	void _update_camera();
	virtual void _request_camera_update();
	void _update_camera_mode();

	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

	Projection _get_camera_projection(real_t p_near) const;

public:
	enum {
		NOTIFICATION_BECAME_CURRENT = 50,
		NOTIFICATION_LOST_CURRENT = 51
	};

	void set_perspective(real_t p_fovy_degrees, real_t p_z_near, real_t p_z_far);
	void set_orthogonal(real_t p_size, real_t p_z_near, real_t p_z_far);
	void set_frustum(real_t p_size, Vector2 p_offset, real_t p_z_near, real_t p_z_far);
	void set_projection(Camera3D::ProjectionType p_mode);

	void make_current();
	void clear_current(bool p_enable_next = true);
	void set_current(bool p_enabled);
	bool is_current() const;

	RID get_camera() const;

	real_t get_fov() const;
	real_t get_size() const;
	real_t get_far() const;
	real_t get_near() const;
	Vector2 get_frustum_offset() const;

	ProjectionType get_projection() const;

	void set_fov(real_t p_fov);
	void set_size(real_t p_size);
	void set_far(real_t p_far);
	void set_near(real_t p_near);
	void set_frustum_offset(Vector2 p_offset);

	virtual Transform3D get_camera_transform() const;
	virtual Projection get_camera_projection() const;

	virtual Vector3 project_ray_normal(const Point2 &p_pos) const;
	virtual Vector3 project_ray_origin(const Point2 &p_pos) const;
	virtual Vector3 project_local_ray_normal(const Point2 &p_pos) const;
	virtual Point2 unproject_position(const Vector3 &p_pos) const;
	bool is_position_behind(const Vector3 &p_pos) const;
	virtual Vector3 project_position(const Point2 &p_point, real_t p_z_depth) const;

	Vector<Vector3> get_near_plane_points() const;

	void set_cull_mask(uint32_t p_layers);
	uint32_t get_cull_mask() const;

	void set_cull_mask_value(int p_layer_number, bool p_enable);
	bool get_cull_mask_value(int p_layer_number) const;

	virtual Vector<Plane> get_frustum() const;
	bool is_position_in_frustum(const Vector3 &p_position) const;

	void set_environment(const Ref<Environment> &p_environment);
	Ref<Environment> get_environment() const;

	void set_attributes(const Ref<CameraAttributes> &p_effects);
	Ref<CameraAttributes> get_attributes() const;

	void set_compositor(const Ref<Compositor> &p_compositor);
	Ref<Compositor> get_compositor() const;

	void set_keep_aspect_mode(KeepAspect p_aspect);
	KeepAspect get_keep_aspect_mode() const;

	void set_v_offset(real_t p_offset);
	real_t get_v_offset() const;

	void set_h_offset(real_t p_offset);
	real_t get_h_offset() const;

	void set_doppler_tracking(DopplerTracking p_tracking);
	DopplerTracking get_doppler_tracking() const;

	Vector3 get_doppler_tracked_velocity() const;

	RID get_pyramid_shape_rid();

	Camera3D();
	~Camera3D();
};

VARIANT_ENUM_CAST(Camera3D::ProjectionType);
VARIANT_ENUM_CAST(Camera3D::KeepAspect);
VARIANT_ENUM_CAST(Camera3D::DopplerTracking);
