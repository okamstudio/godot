/**************************************************************************/
/*  camera_3d.cpp                                                         */
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

#include "camera_3d.h"

#include "core/math/projection.h"
#include "core/math/transform_interpolator.h"
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_server_constants.h"

void Camera3D::_update_audio_listener_state() {
}

void Camera3D::_request_camera_update() {
	_update_camera();
}

void Camera3D::_update_camera_mode() {
	force_change = true;
	switch (mode) {
		case PROJECTION_PERSPECTIVE: {
			set_perspective(fov, _near, _far);

		} break;
		case PROJECTION_ORTHOGONAL: {
			set_orthogonal(size, _near, _far);
		} break;
		case PROJECTION_FRUSTUM: {
			set_frustum(size, frustum_offset, _near, _far);
		} break;
	}
}

void Camera3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "fov") {
		if (mode != PROJECTION_PERSPECTIVE) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else if (p_property.name == "size") {
		if (mode != PROJECTION_ORTHOGONAL && mode != PROJECTION_FRUSTUM) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else if (p_property.name == "frustum_offset") {
		if (mode != PROJECTION_FRUSTUM) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (attributes.is_valid()) {
		const CameraAttributesPhysical *physical_attributes = Object::cast_to<CameraAttributesPhysical>(attributes.ptr());
		if (physical_attributes) {
			if (p_property.name == "near" || p_property.name == "far" || p_property.name == "fov" || p_property.name == "keep_aspect") {
				p_property.usage = PROPERTY_USAGE_READ_ONLY | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR;
			}
		}
	}

	Node3D::_validate_property(p_property);
}

void Camera3D::_update_camera() {
	if (!is_inside_tree()) {
		return;
	}

	if (!is_physics_interpolated_and_enabled()) {
		RenderingServer::get_singleton()->camera_set_transform(camera, get_camera_transform());
	} else {
		// Ideally we shouldn't be moving a physics interpolated camera within a frame,
		// because it will break smooth interpolation, but it may occur on e.g. level load.
		if (!Engine::get_singleton()->is_in_physics_frame() && camera.is_valid()) {
			_physics_interpolation_ensure_transform_calculated(true);
			RenderingServer::get_singleton()->camera_set_transform(camera, _interpolation_data.camera_xform_interpolated);
		}
	}

	if (is_part_of_edited_scene() || !is_current()) {
		return;
	}

	get_viewport()->_camera_3d_transform_changed_notify();
}

void Camera3D::_physics_interpolated_changed() {
	_update_process_mode();
}

void Camera3D::_physics_interpolation_ensure_data_flipped() {
	// The curr -> previous update can either occur
	// on the INTERNAL_PHYSICS_PROCESS OR
	// on NOTIFICATION_TRANSFORM_CHANGED,
	// if NOTIFICATION_TRANSFORM_CHANGED takes place
	// earlier than INTERNAL_PHYSICS_PROCESS on a tick.
	// This is to ensure that the data keeps flowing, but the new data
	// doesn't overwrite before prev has been set.

	// Keep the data flowing.
	uint64_t tick = Engine::get_singleton()->get_physics_frames();
	if (_interpolation_data.last_update_physics_tick != tick) {
		_interpolation_data.xform_prev = _interpolation_data.xform_curr;
		_interpolation_data.last_update_physics_tick = tick;
		physics_interpolation_flip_data();
	}
}

void Camera3D::_physics_interpolation_ensure_transform_calculated(bool p_force) const {
	DEV_CHECK_ONCE(!Engine::get_singleton()->is_in_physics_frame());

	InterpolationData &id = _interpolation_data;
	uint64_t frame = Engine::get_singleton()->get_frames_drawn();

	if (id.last_update_frame != frame || p_force) {
		id.last_update_frame = frame;

		TransformInterpolator::interpolate_transform_3d(id.xform_prev, id.xform_curr, id.xform_interpolated, Engine::get_singleton()->get_physics_interpolation_fraction());

		Transform3D &tr = id.camera_xform_interpolated;
		tr = _get_adjusted_camera_transform(id.xform_interpolated);
	}
}

void Camera3D::set_desired_process_modes(bool p_process_internal, bool p_physics_process_internal) {
	_desired_process_internal = p_process_internal;
	_desired_physics_process_internal = p_physics_process_internal;
	_update_process_mode();
}

void Camera3D::_update_process_mode() {
	bool process = _desired_process_internal;
	bool physics_process = _desired_physics_process_internal;

	if (is_physics_interpolated_and_enabled()) {
		if (is_current()) {
			process = true;
			physics_process = true;
		}
	}
	set_process_internal(process);
	set_physics_process_internal(physics_process);
}

void Camera3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			// Needs to track the Viewport because it's needed on NOTIFICATION_EXIT_WORLD
			// and Spatial will handle it first, including clearing its reference to the Viewport,
			// therefore making it impossible to subclasses to access it
			viewport = get_viewport();
			ERR_FAIL_NULL(viewport);

			bool first_camera = viewport->_camera_3d_add(this);
			if (current || first_camera) {
				viewport->_camera_3d_set(this);
			}

#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint()) {
				viewport->connect(SNAME("size_changed"), callable_mp((Node3D *)this, &Camera3D::update_gizmos));
			}
#endif
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (is_physics_interpolated_and_enabled() && camera.is_valid()) {
				_physics_interpolation_ensure_transform_calculated();

#ifdef RENDERING_SERVER_DEBUG_PHYSICS_INTERPOLATION
				print_line("\t\tinterpolated Camera3D: " + rtos(_interpolation_data.xform_interpolated.origin.x) + "\t( prev " + rtos(_interpolation_data.xform_prev.origin.x) + ", curr " + rtos(_interpolation_data.xform_curr.origin.x) + " ) on tick " + itos(Engine::get_singleton()->get_physics_frames()));
#endif

				RenderingServer::get_singleton()->camera_set_transform(camera, _interpolation_data.camera_xform_interpolated);
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_physics_interpolated_and_enabled()) {
				_physics_interpolation_ensure_data_flipped();
				_interpolation_data.xform_curr = get_global_transform();
			}
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (is_physics_interpolated_and_enabled()) {
				_physics_interpolation_ensure_data_flipped();
				_interpolation_data.xform_curr = get_global_transform();
#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
				if (!Engine::get_singleton()->is_in_physics_frame()) {
					PHYSICS_INTERPOLATION_NODE_WARNING(get_instance_id(), "Interpolated Camera3D triggered from outside physics process");
				}
#endif
			}
			_request_camera_update();
			if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
				velocity_tracker->update_position(get_global_transform().origin);
			}
			// Allow auto-reset when first adding to the tree, as a convenience.
			if (_is_physics_interpolation_reset_requested() && is_inside_tree()) {
				_notification(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);
				_set_physics_interpolation_reset_requested(false);
			}
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (is_inside_tree()) {
				_interpolation_data.xform_curr = get_global_transform();
				_interpolation_data.xform_prev = _interpolation_data.xform_curr;
			}
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_PAUSED: {
			if (is_physics_interpolated_and_enabled() && is_inside_tree() && is_visible_in_tree()) {
				_physics_interpolation_ensure_transform_calculated(true);
				RenderingServer::get_singleton()->camera_set_transform(camera, _interpolation_data.camera_xform_interpolated);
			}
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			if (!is_part_of_edited_scene()) {
				if (is_current()) {
					clear_current();
					current = true; //keep it true

				} else {
					current = false;
				}
			}

			if (viewport) {
#ifdef TOOLS_ENABLED
				if (Engine::get_singleton()->is_editor_hint()) {
					viewport->disconnect(SNAME("size_changed"), callable_mp((Node3D *)this, &Camera3D::update_gizmos));
				}
#endif
				viewport->_camera_3d_remove(this);
				viewport = nullptr;
			}
		} break;

		case NOTIFICATION_BECAME_CURRENT: {
			if (viewport) {
				viewport->find_world_3d()->_register_camera(this);
			}
			_update_process_mode();
		} break;

		case NOTIFICATION_LOST_CURRENT: {
			if (viewport) {
				viewport->find_world_3d()->_remove_camera(this);
			}
			_update_process_mode();
		} break;
	}
}

Transform3D Camera3D::_get_adjusted_camera_transform(const Transform3D &p_xform) const {
	Transform3D tr = p_xform.orthonormalized();
	tr.origin += tr.basis.get_column(1) * v_offset;
	tr.origin += tr.basis.get_column(0) * h_offset;
	return tr;
}

Transform3D Camera3D::get_camera_transform() const {
	if (is_physics_interpolated_and_enabled() && !Engine::get_singleton()->is_in_physics_frame()) {
		_physics_interpolation_ensure_transform_calculated();
		return _interpolation_data.camera_xform_interpolated;
	}

	return _get_adjusted_camera_transform(get_global_transform());
}

Projection Camera3D::_get_camera_projection(real_t p_near) const {
	Size2 viewport_size = get_viewport()->get_visible_rect().size;
	Projection cm;

	switch (mode) {
		case PROJECTION_PERSPECTIVE: {
			cm.set_perspective(fov, viewport_size.aspect(), p_near, _far, keep_aspect == KEEP_WIDTH);
		} break;
		case PROJECTION_ORTHOGONAL: {
			cm.set_orthogonal(size, viewport_size.aspect(), p_near, _far, keep_aspect == KEEP_WIDTH);
		} break;
		case PROJECTION_FRUSTUM: {
			cm.set_frustum(size, viewport_size.aspect(), frustum_offset, p_near, _far);
		} break;
	}

	return cm;
}

Projection Camera3D::get_camera_projection() const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Projection(), "Camera is not inside the scene tree.");
	return _get_camera_projection(_near);
}

void Camera3D::set_perspective(real_t p_fovy_degrees, real_t p_z_near, real_t p_z_far) {
	if (!force_change && fov == p_fovy_degrees && p_z_near == _near && p_z_far == _far && mode == PROJECTION_PERSPECTIVE) {
		return;
	}

	fov = p_fovy_degrees;
	_near = p_z_near;
	_far = p_z_far;
	mode = PROJECTION_PERSPECTIVE;

	RenderingServer::get_singleton()->camera_set_perspective(camera, fov, _near, _far);
	update_gizmos();
	force_change = false;
}

void Camera3D::set_orthogonal(real_t p_size, real_t p_z_near, real_t p_z_far) {
	if (!force_change && size == p_size && p_z_near == _near && p_z_far == _far && mode == PROJECTION_ORTHOGONAL) {
		return;
	}

	size = p_size;

	_near = p_z_near;
	_far = p_z_far;
	mode = PROJECTION_ORTHOGONAL;
	force_change = false;

	RenderingServer::get_singleton()->camera_set_orthogonal(camera, size, _near, _far);
	update_gizmos();
}

void Camera3D::set_frustum(real_t p_size, Vector2 p_offset, real_t p_z_near, real_t p_z_far) {
	if (!force_change && size == p_size && frustum_offset == p_offset && p_z_near == _near && p_z_far == _far && mode == PROJECTION_FRUSTUM) {
		return;
	}

	size = p_size;
	frustum_offset = p_offset;

	_near = p_z_near;
	_far = p_z_far;
	mode = PROJECTION_FRUSTUM;
	force_change = false;

	RenderingServer::get_singleton()->camera_set_frustum(camera, size, frustum_offset, _near, _far);
	update_gizmos();
}

void Camera3D::set_projection(ProjectionType p_mode) {
	if (p_mode == PROJECTION_PERSPECTIVE || p_mode == PROJECTION_ORTHOGONAL || p_mode == PROJECTION_FRUSTUM) {
		mode = p_mode;
		_update_camera_mode();
		notify_property_list_changed();
	}
}

RID Camera3D::get_camera() const {
	return camera;
}

void Camera3D::make_current() {
	current = true;

	if (!is_inside_tree()) {
		return;
	}

	get_viewport()->_camera_3d_set(this);
}

void Camera3D::clear_current(bool p_enable_next) {
	current = false;
	if (!is_inside_tree()) {
		return;
	}

	if (get_viewport()->get_camera_3d() == this) {
		get_viewport()->_camera_3d_set(nullptr);

		if (p_enable_next && !Engine::get_singleton()->is_editor_hint()) {
			get_viewport()->_camera_3d_make_next_current(this);
		}
	}
}

void Camera3D::set_current(bool p_enabled) {
	if (p_enabled) {
		make_current();
	} else {
		clear_current();
	}
}

bool Camera3D::is_current() const {
	if (is_inside_tree() && !is_part_of_edited_scene()) {
		return get_viewport()->get_camera_3d() == this;
	} else {
		return current;
	}
}

Vector3 Camera3D::project_ray_normal(const Point2 &p_pos) const {
	Vector3 ray = project_local_ray_normal(p_pos);
	return get_camera_transform().basis.xform(ray).normalized();
}

Vector3 Camera3D::project_local_ray_normal(const Point2 &p_pos) const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_camera_rect_size();
	Vector2 cpos = get_viewport()->get_camera_coords(p_pos);
	Vector3 ray;

	if (mode == PROJECTION_ORTHOGONAL) {
		ray = Vector3(0, 0, -1);
	} else {
		Projection cm = _get_camera_projection(_near);
		Vector2 screen_he = cm.get_viewport_half_extents();
		ray = Vector3(((cpos.x / viewport_size.width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (cpos.y / viewport_size.height)) * 2.0 - 1.0) * screen_he.y, -_near).normalized();
	}

	return ray;
}

Vector3 Camera3D::project_ray_origin(const Point2 &p_pos) const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_camera_rect_size();
	Vector2 cpos = get_viewport()->get_camera_coords(p_pos);
	ERR_FAIL_COND_V(viewport_size.y == 0, Vector3());

	if (mode == PROJECTION_ORTHOGONAL) {
		Vector2 pos = cpos / viewport_size;
		real_t vsize, hsize;
		if (keep_aspect == KEEP_WIDTH) {
			vsize = size / viewport_size.aspect();
			hsize = size;
		} else {
			hsize = size * viewport_size.aspect();
			vsize = size;
		}

		Vector3 ray;
		ray.x = pos.x * (hsize)-hsize / 2;
		ray.y = (1.0 - pos.y) * (vsize)-vsize / 2;
		ray.z = -_near;
		ray = get_camera_transform().xform(ray);
		return ray;
	} else {
		return get_camera_transform().origin;
	};
}

bool Camera3D::is_position_behind(const Vector3 &p_pos) const {
	Transform3D t = get_global_transform();
	Vector3 eyedir = -t.basis.get_column(2).normalized();
	return eyedir.dot(p_pos - t.origin) < _near;
}

Vector<Vector3> Camera3D::get_near_plane_points() const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector<Vector3>(), "Camera is not inside scene.");

	Projection cm = _get_camera_projection(_near);

	Vector3 endpoints[8];
	cm.get_endpoints(Transform3D(), endpoints);

	Vector<Vector3> points = {
		Vector3(),
		endpoints[4],
		endpoints[5],
		endpoints[6],
		endpoints[7]
	};
	return points;
}

Point2 Camera3D::unproject_position(const Vector3 &p_pos) const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector2(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_visible_rect().size;

	Projection cm = _get_camera_projection(_near);

	Plane p(get_camera_transform().xform_inv(p_pos), 1.0);

	p = cm.xform4(p);

	// Prevent divide by zero.
	// TODO: Investigate, this was causing NaNs.
	ERR_FAIL_COND_V(p.d == 0, Point2());

	p.normal /= p.d;

	Point2 res;
	res.x = (p.normal.x * 0.5 + 0.5) * viewport_size.x;
	res.y = (-p.normal.y * 0.5 + 0.5) * viewport_size.y;

	return res;
}

Vector3 Camera3D::project_position(const Point2 &p_point, real_t p_z_depth) const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera is not inside scene.");

	if (p_z_depth == 0 && mode != PROJECTION_ORTHOGONAL) {
		return get_global_transform().origin;
	}
	Size2 viewport_size = get_viewport()->get_visible_rect().size;

	Projection cm = _get_camera_projection(_near);

	Plane z_slice(Vector3(0, 0, 1), -p_z_depth);
	Vector3 res;
	z_slice.intersect_3(cm.get_projection_plane(Projection::Planes::PLANE_RIGHT), cm.get_projection_plane(Projection::Planes::PLANE_TOP), &res);
	Vector2 vp_he(res.x, res.y);

	Vector2 point;
	point.x = (p_point.x / viewport_size.x) * 2.0 - 1.0;
	point.y = (1.0 - (p_point.y / viewport_size.y)) * 2.0 - 1.0;
	point *= vp_he;

	Vector3 p(point.x, point.y, -p_z_depth);

	return get_camera_transform().xform(p);
}

void Camera3D::set_environment(const Ref<Environment> &p_environment) {
	environment = p_environment;
	if (environment.is_valid()) {
		RS::get_singleton()->camera_set_environment(camera, environment->get_rid());
	} else {
		RS::get_singleton()->camera_set_environment(camera, RID());
	}
	_update_camera_mode();
}

Ref<Environment> Camera3D::get_environment() const {
	return environment;
}

void Camera3D::set_attributes(const Ref<CameraAttributes> &p_attributes) {
	if (attributes.is_valid()) {
		CameraAttributesPhysical *physical_attributes = Object::cast_to<CameraAttributesPhysical>(attributes.ptr());
		if (physical_attributes) {
			attributes->disconnect_changed(callable_mp(this, &Camera3D::_attributes_changed));
		}
	}

	attributes = p_attributes;

	if (attributes.is_valid()) {
		CameraAttributesPhysical *physical_attributes = Object::cast_to<CameraAttributesPhysical>(attributes.ptr());
		if (physical_attributes) {
			attributes->connect_changed(callable_mp(this, &Camera3D::_attributes_changed));
			_attributes_changed();
		}

		RS::get_singleton()->camera_set_camera_attributes(camera, attributes->get_rid());
	} else {
		RS::get_singleton()->camera_set_camera_attributes(camera, RID());
	}

	notify_property_list_changed();
}

Ref<CameraAttributes> Camera3D::get_attributes() const {
	return attributes;
}

void Camera3D::_attributes_changed() {
	CameraAttributesPhysical *physical_attributes = Object::cast_to<CameraAttributesPhysical>(attributes.ptr());
	ERR_FAIL_NULL(physical_attributes);

	fov = physical_attributes->get_fov();
	_near = physical_attributes->get_near();
	_far = physical_attributes->get_far();
	keep_aspect = KEEP_HEIGHT;
	_update_camera_mode();
}

void Camera3D::set_compositor(const Ref<Compositor> &p_compositor) {
	compositor = p_compositor;
	if (compositor.is_valid()) {
		RS::get_singleton()->camera_set_compositor(camera, compositor->get_rid());
	} else {
		RS::get_singleton()->camera_set_compositor(camera, RID());
	}
	_update_camera_mode();
}

Ref<Compositor> Camera3D::get_compositor() const {
	return compositor;
}

void Camera3D::set_keep_aspect_mode(KeepAspect p_aspect) {
	keep_aspect = p_aspect;
	RenderingServer::get_singleton()->camera_set_use_vertical_aspect(camera, p_aspect == KEEP_WIDTH);
	_update_camera_mode();
	notify_property_list_changed();
}

Camera3D::KeepAspect Camera3D::get_keep_aspect_mode() const {
	return keep_aspect;
}

void Camera3D::set_doppler_tracking(DopplerTracking p_tracking) {
	if (doppler_tracking == p_tracking) {
		return;
	}

	doppler_tracking = p_tracking;
	if (p_tracking != DOPPLER_TRACKING_DISABLED) {
		velocity_tracker->set_track_physics_step(doppler_tracking == DOPPLER_TRACKING_PHYSICS_STEP);
		if (is_inside_tree()) {
			velocity_tracker->reset(get_global_transform().origin);
		}
	}
	_update_camera_mode();
}

Camera3D::DopplerTracking Camera3D::get_doppler_tracking() const {
	return doppler_tracking;
}

void Camera3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("project_ray_normal", "screen_point"), &Camera3D::project_ray_normal);
	ClassDB::bind_method(D_METHOD("project_local_ray_normal", "screen_point"), &Camera3D::project_local_ray_normal);
	ClassDB::bind_method(D_METHOD("project_ray_origin", "screen_point"), &Camera3D::project_ray_origin);
	ClassDB::bind_method(D_METHOD("unproject_position", "world_point"), &Camera3D::unproject_position);
	ClassDB::bind_method(D_METHOD("is_position_behind", "world_point"), &Camera3D::is_position_behind);
	ClassDB::bind_method(D_METHOD("project_position", "screen_point", "z_depth"), &Camera3D::project_position);
	ClassDB::bind_method(D_METHOD("set_perspective", "fov", "z_near", "z_far"), &Camera3D::set_perspective);
	ClassDB::bind_method(D_METHOD("set_orthogonal", "size", "z_near", "z_far"), &Camera3D::set_orthogonal);
	ClassDB::bind_method(D_METHOD("set_frustum", "size", "offset", "z_near", "z_far"), &Camera3D::set_frustum);
	ClassDB::bind_method(D_METHOD("make_current"), &Camera3D::make_current);
	ClassDB::bind_method(D_METHOD("clear_current", "enable_next"), &Camera3D::clear_current, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_current", "enabled"), &Camera3D::set_current);
	ClassDB::bind_method(D_METHOD("is_current"), &Camera3D::is_current);
	ClassDB::bind_method(D_METHOD("get_camera_transform"), &Camera3D::get_camera_transform);
	ClassDB::bind_method(D_METHOD("get_camera_projection"), &Camera3D::get_camera_projection);
	ClassDB::bind_method(D_METHOD("get_fov"), &Camera3D::get_fov);
	ClassDB::bind_method(D_METHOD("get_frustum_offset"), &Camera3D::get_frustum_offset);
	ClassDB::bind_method(D_METHOD("get_size"), &Camera3D::get_size);
	ClassDB::bind_method(D_METHOD("get_far"), &Camera3D::get_far);
	ClassDB::bind_method(D_METHOD("get_near"), &Camera3D::get_near);
	ClassDB::bind_method(D_METHOD("set_fov", "fov"), &Camera3D::set_fov);
	ClassDB::bind_method(D_METHOD("set_frustum_offset", "offset"), &Camera3D::set_frustum_offset);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &Camera3D::set_size);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &Camera3D::set_far);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &Camera3D::set_near);
	ClassDB::bind_method(D_METHOD("get_projection"), &Camera3D::get_projection);
	ClassDB::bind_method(D_METHOD("set_projection", "mode"), &Camera3D::set_projection);
	ClassDB::bind_method(D_METHOD("set_h_offset", "offset"), &Camera3D::set_h_offset);
	ClassDB::bind_method(D_METHOD("get_h_offset"), &Camera3D::get_h_offset);
	ClassDB::bind_method(D_METHOD("set_v_offset", "offset"), &Camera3D::set_v_offset);
	ClassDB::bind_method(D_METHOD("get_v_offset"), &Camera3D::get_v_offset);
	ClassDB::bind_method(D_METHOD("set_cull_mask", "mask"), &Camera3D::set_cull_mask);
	ClassDB::bind_method(D_METHOD("get_cull_mask"), &Camera3D::get_cull_mask);
	ClassDB::bind_method(D_METHOD("set_environment", "env"), &Camera3D::set_environment);
	ClassDB::bind_method(D_METHOD("get_environment"), &Camera3D::get_environment);
	ClassDB::bind_method(D_METHOD("set_attributes", "env"), &Camera3D::set_attributes);
	ClassDB::bind_method(D_METHOD("get_attributes"), &Camera3D::get_attributes);
	ClassDB::bind_method(D_METHOD("set_compositor", "compositor"), &Camera3D::set_compositor);
	ClassDB::bind_method(D_METHOD("get_compositor"), &Camera3D::get_compositor);
	ClassDB::bind_method(D_METHOD("set_keep_aspect_mode", "mode"), &Camera3D::set_keep_aspect_mode);
	ClassDB::bind_method(D_METHOD("get_keep_aspect_mode"), &Camera3D::get_keep_aspect_mode);
	ClassDB::bind_method(D_METHOD("set_doppler_tracking", "mode"), &Camera3D::set_doppler_tracking);
	ClassDB::bind_method(D_METHOD("get_doppler_tracking"), &Camera3D::get_doppler_tracking);
	ClassDB::bind_method(D_METHOD("get_frustum"), &Camera3D::_get_frustum);
	ClassDB::bind_method(D_METHOD("is_position_in_frustum", "world_point"), &Camera3D::is_position_in_frustum);
	ClassDB::bind_method(D_METHOD("get_camera_rid"), &Camera3D::get_camera);
	ClassDB::bind_method(D_METHOD("get_pyramid_shape_rid"), &Camera3D::get_pyramid_shape_rid);

	ClassDB::bind_method(D_METHOD("set_cull_mask_value", "layer_number", "value"), &Camera3D::set_cull_mask_value);
	ClassDB::bind_method(D_METHOD("get_cull_mask_value", "layer_number"), &Camera3D::get_cull_mask_value);

	//ClassDB::bind_method(D_METHOD("_camera_make_current"),&Camera::_camera_make_current );

	ADD_PROPERTY(PropertyInfo(Variant::INT, "keep_aspect", PROPERTY_HINT_ENUM, "Keep Width,Keep Height"), "set_keep_aspect_mode", "get_keep_aspect_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cull_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_cull_mask", "get_cull_mask");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_environment", "get_environment");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "attributes", PROPERTY_HINT_RESOURCE_TYPE, "CameraAttributesPractical,CameraAttributesPhysical"), "set_attributes", "get_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "compositor", PROPERTY_HINT_RESOURCE_TYPE, "Compositor"), "set_compositor", "get_compositor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "h_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_h_offset", "get_h_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "v_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_v_offset", "get_v_offset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "doppler_tracking", PROPERTY_HINT_ENUM, "Disabled,Idle,Physics"), "set_doppler_tracking", "get_doppler_tracking");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "projection", PROPERTY_HINT_ENUM, "Perspective,Orthogonal,Frustum"), "set_projection", "get_projection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "current"), "set_current", "is_current");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fov", PROPERTY_HINT_RANGE, "1,179,0.1,degrees"), "set_fov", "get_fov");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "size", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater,suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "frustum_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_frustum_offset", "get_frustum_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,exp,suffix:m"), "set_near", "get_near");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far", PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,exp,suffix:m"), "set_far", "get_far");

	BIND_ENUM_CONSTANT(PROJECTION_PERSPECTIVE);
	BIND_ENUM_CONSTANT(PROJECTION_ORTHOGONAL);
	BIND_ENUM_CONSTANT(PROJECTION_FRUSTUM);

	BIND_ENUM_CONSTANT(KEEP_WIDTH);
	BIND_ENUM_CONSTANT(KEEP_HEIGHT);

	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_DISABLED);
	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_IDLE_STEP);
	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_PHYSICS_STEP);
}

real_t Camera3D::get_fov() const {
	return fov;
}

real_t Camera3D::get_size() const {
	return size;
}

real_t Camera3D::get_near() const {
	return _near;
}

Vector2 Camera3D::get_frustum_offset() const {
	return frustum_offset;
}

real_t Camera3D::get_far() const {
	return _far;
}

Camera3D::ProjectionType Camera3D::get_projection() const {
	return mode;
}

void Camera3D::set_fov(real_t p_fov) {
	ERR_FAIL_COND(p_fov < 1 || p_fov > 179);
	fov = p_fov;
	_update_camera_mode();
}

void Camera3D::set_size(real_t p_size) {
	ERR_FAIL_COND(p_size <= CMP_EPSILON);
	size = p_size;
	_update_camera_mode();
}

void Camera3D::set_near(real_t p_near) {
	_near = p_near;
	_update_camera_mode();
}

void Camera3D::set_frustum_offset(Vector2 p_offset) {
	frustum_offset = p_offset;
	_update_camera_mode();
}

void Camera3D::set_far(real_t p_far) {
	_far = p_far;
	_update_camera_mode();
}

void Camera3D::set_cull_mask(uint32_t p_layers) {
	layers = p_layers;
	RenderingServer::get_singleton()->camera_set_cull_mask(camera, layers);
	_update_camera_mode();
}

uint32_t Camera3D::get_cull_mask() const {
	return layers;
}

void Camera3D::set_cull_mask_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Render layer number must be between 1 and 20 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 20, "Render layer number must be between 1 and 20 inclusive.");
	uint32_t mask = get_cull_mask();
	if (p_value) {
		mask |= 1 << (p_layer_number - 1);
	} else {
		mask &= ~(1 << (p_layer_number - 1));
	}
	set_cull_mask(mask);
}

bool Camera3D::get_cull_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Render layer number must be between 1 and 20 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 20, false, "Render layer number must be between 1 and 20 inclusive.");
	return layers & (1 << (p_layer_number - 1));
}

Vector<Plane> Camera3D::get_frustum() const {
	ERR_FAIL_COND_V(!is_inside_world(), Vector<Plane>());

	Projection cm = _get_camera_projection(_near);

	return cm.get_projection_planes(get_camera_transform());
}

TypedArray<Plane> Camera3D::_get_frustum() const {
	Variant ret = get_frustum();
	return ret;
}

bool Camera3D::is_position_in_frustum(const Vector3 &p_position) const {
	Vector<Plane> frustum = get_frustum();
	for (int i = 0; i < frustum.size(); i++) {
		if (frustum[i].is_point_over(p_position)) {
			return false;
		}
	}
	return true;
}

void Camera3D::set_v_offset(real_t p_offset) {
	v_offset = p_offset;
	_update_camera();
}

real_t Camera3D::get_v_offset() const {
	return v_offset;
}

void Camera3D::set_h_offset(real_t p_offset) {
	h_offset = p_offset;
	_update_camera();
}

real_t Camera3D::get_h_offset() const {
	return h_offset;
}

Vector3 Camera3D::get_doppler_tracked_velocity() const {
	if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
		return velocity_tracker->get_tracked_linear_velocity();
	} else {
		return Vector3();
	}
}

RID Camera3D::get_pyramid_shape_rid() {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), RID(), "Camera is not inside scene.");
	if (pyramid_shape == RID()) {
		pyramid_shape_points = get_near_plane_points();
		pyramid_shape = PhysicsServer3D::get_singleton()->convex_polygon_shape_create();
		PhysicsServer3D::get_singleton()->shape_set_data(pyramid_shape, pyramid_shape_points);

	} else { //check if points changed
		Vector<Vector3> local_points = get_near_plane_points();

		bool all_equal = true;

		for (int i = 0; i < 5; i++) {
			if (local_points[i] != pyramid_shape_points[i]) {
				all_equal = false;
				break;
			}
		}

		if (!all_equal) {
			PhysicsServer3D::get_singleton()->shape_set_data(pyramid_shape, local_points);
			pyramid_shape_points = local_points;
		}
	}

	return pyramid_shape;
}

Camera3D::Camera3D() {
	camera = RenderingServer::get_singleton()->camera_create();
	set_perspective(75.0, 0.05, 4000.0);
	RenderingServer::get_singleton()->camera_set_cull_mask(camera, layers);
	//active=false;
	velocity_tracker.instantiate();
	set_notify_transform(true);
	set_disable_scale(true);
}

Camera3D::~Camera3D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(camera);
	if (pyramid_shape.is_valid()) {
		ERR_FAIL_NULL(PhysicsServer3D::get_singleton());
		PhysicsServer3D::get_singleton()->free(pyramid_shape);
	}
}
