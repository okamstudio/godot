/**************************************************************************/
/*  node_3d.cpp                                                           */
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

#include "node_3d.h"

#include "core/math/transform_interpolator.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/main/viewport.h"
#include "scene/property_utils.h"

/*

 possible algorithms:

 Algorithm 1: (current)

 definition of invalidation: global is invalid

 1) If a node sets a LOCAL, it produces an invalidation of everything above
 .  a) If above is invalid, don't keep invalidating upwards
 2) If a node sets a GLOBAL, it is converted to LOCAL (and forces validation of everything pending below)

 drawback: setting/reading globals is useful and used very often, and using affine inverses is slow

---

 Algorithm 2: (no longer current)

 definition of invalidation: NONE dirty, LOCAL dirty, GLOBAL dirty

 1) If a node sets a LOCAL, it must climb the tree and set it as GLOBAL dirty
 .  a) marking GLOBALs as dirty up all the tree must be done always
 2) If a node sets a GLOBAL, it marks local as dirty, and that's all?

 //is clearing the dirty state correct in this case?

 drawback: setting a local down the tree forces many tree walks often

--

future: no idea

 */

Node3DGizmo::Node3DGizmo() {
}

void Node3D::_notify_dirty() {
#ifdef TOOLS_ENABLED
	if ((!data.gizmos.is_empty() || data.notify_transform) && !data.ignore_notification && !xform_change.in_list()) {
#else
	if (data.notify_transform && !data.ignore_notification && !xform_change.in_list()) {

#endif
		get_tree()->xform_change_list.add(&xform_change);
	}
}

void Node3D::_update_local_transform() const {
	// This function is called when the local transform (data.local_transform) is dirty and the right value is contained in the Euler rotation and scale.
	data.local_transform.basis.set_euler_scale(data.euler_rotation, data.scale, data.euler_rotation_order);
	_clear_dirty_bits(DIRTY_LOCAL_TRANSFORM);
}

void Node3D::_update_rotation_and_scale() const {
	// This function is called when the Euler rotation (data.euler_rotation) is dirty and the right value is contained in the local transform

	data.scale = data.local_transform.basis.get_scale();
	data.euler_rotation = data.local_transform.basis.get_euler_normalized(data.euler_rotation_order);
	_clear_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE);
}

void Node3D::_propagate_transform_changed_deferred() {
	if (is_inside_tree() && !xform_change.in_list()) {
		get_tree()->xform_change_list.add(&xform_change);
	}
}

void Node3D::_propagate_transform_changed(Node3D *p_origin) {
	if (!is_inside_tree()) {
		return;
	}

	for (Node3D *&E : data.children) {
		if (E->data.top_level) {
			continue; //don't propagate to a top_level
		}
		E->_propagate_transform_changed(p_origin);
	}
#ifdef TOOLS_ENABLED
	if ((!data.gizmos.is_empty() || data.notify_transform) && !data.ignore_notification && !xform_change.in_list()) {
#else
	if (data.notify_transform && !data.ignore_notification && !xform_change.in_list()) {
#endif
		if (likely(is_accessible_from_caller_thread())) {
			get_tree()->xform_change_list.add(&xform_change);
		} else {
			// This should very rarely happen, but if it does at least make sure the notification is received eventually.
			callable_mp(this, &Node3D::_propagate_transform_changed_deferred).call_deferred();
		}
	}
	_set_dirty_bits(DIRTY_GLOBAL_TRANSFORM);
}

void Node3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ERR_MAIN_THREAD_GUARD;
			ERR_FAIL_NULL(get_tree());

			Node *p = get_parent();
			if (p) {
				data.parent = Object::cast_to<Node3D>(p);
			}

			if (data.parent) {
				data.C = data.parent->data.children.push_back(this);
			} else {
				data.C = nullptr;
			}

			if (data.top_level && !Engine::get_singleton()->is_editor_hint()) {
				if (data.parent) {
					if (!data.top_level) {
						data.local_transform = data.parent->get_global_transform() * get_transform();
					} else {
						data.local_transform = get_transform();
					}
					_replace_dirty_mask(DIRTY_EULER_ROTATION_AND_SCALE); // As local transform was updated, rot/scale should be dirty.
				}
			}

			_set_dirty_bits(DIRTY_GLOBAL_TRANSFORM); // Global is always dirty upon entering a scene.
			_notify_dirty();

			notification(NOTIFICATION_ENTER_WORLD);
			_update_visibility_parent(true);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			ERR_MAIN_THREAD_GUARD;

			notification(NOTIFICATION_EXIT_WORLD, true);
			if (xform_change.in_list()) {
				get_tree()->xform_change_list.remove(&xform_change);
			}
			if (data.C) {
				data.parent->data.children.erase(data.C);
			}
			data.parent = nullptr;
			data.C = nullptr;
			_update_visibility_parent(true);
			_disable_client_physics_interpolation();
		} break;

		case NOTIFICATION_ENTER_WORLD: {
			ERR_MAIN_THREAD_GUARD;

			data.inside_world = true;
			data.viewport = nullptr;
			Node *parent = get_parent();
			while (parent && !data.viewport) {
				data.viewport = Object::cast_to<Viewport>(parent);
				parent = parent->get_parent();
			}

			ERR_FAIL_NULL(data.viewport);

			if (get_script_instance()) {
				get_script_instance()->call(SNAME("_enter_world"));
			}

#ifdef TOOLS_ENABLED
			if (is_part_of_edited_scene()) {
				get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, SceneStringName(_spatial_editor_group), SNAME("_request_gizmo_for_id"), get_instance_id());
			}
#endif
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			ERR_MAIN_THREAD_GUARD;

#ifdef TOOLS_ENABLED
			clear_gizmos();
#endif

			if (get_script_instance()) {
				get_script_instance()->call(SNAME("_exit_world"));
			}

			data.viewport = nullptr;
			data.inside_world = false;
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			ERR_THREAD_GUARD;

#ifdef TOOLS_ENABLED
			for (int i = 0; i < data.gizmos.size(); i++) {
				data.gizmos.write[i]->transform();
			}
#endif
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (data.client_physics_interpolation_data) {
				data.client_physics_interpolation_data->global_xform_prev = data.client_physics_interpolation_data->global_xform_curr;
			}
		} break;
	}
}

void Node3D::set_basis(const Basis &p_basis) {
	ERR_THREAD_GUARD;

	set_transform(Transform3D(p_basis, data.local_transform.origin));
}
void Node3D::set_quaternion(const Quaternion &p_quaternion) {
	ERR_THREAD_GUARD;

	if (_test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		// We need the scale part, so if these are dirty, update it
		data.scale = data.local_transform.basis.get_scale();
		_clear_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE);
	}
	data.local_transform.basis = Basis(p_quaternion, data.scale);
	// Rotscale should not be marked dirty because that would cause precision loss issues with the scale. Instead reconstruct rotation now.
	data.euler_rotation = data.local_transform.basis.get_euler_normalized(data.euler_rotation_order);

	_replace_dirty_mask(DIRTY_NONE);

	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

Vector3 Node3D::get_global_position() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	return get_global_transform().get_origin();
}

Basis Node3D::get_global_basis() const {
	ERR_READ_THREAD_GUARD_V(Basis());
	return get_global_transform().get_basis();
}

void Node3D::set_global_position(const Vector3 &p_position) {
	ERR_THREAD_GUARD;
	Transform3D transform = get_global_transform();
	transform.set_origin(p_position);
	set_global_transform(transform);
}

void Node3D::set_global_basis(const Basis &p_basis) {
	ERR_THREAD_GUARD;
	Transform3D transform = get_global_transform();
	transform.set_basis(p_basis);
	set_global_transform(transform);
}

Vector3 Node3D::get_global_rotation() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	return get_global_transform().get_basis().get_euler();
}

Vector3 Node3D::get_global_rotation_degrees() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	Vector3 radians = get_global_rotation();
	return Vector3(Math::rad_to_deg(radians.x), Math::rad_to_deg(radians.y), Math::rad_to_deg(radians.z));
}

void Node3D::set_global_rotation(const Vector3 &p_euler_rad) {
	ERR_THREAD_GUARD;
	Transform3D transform = get_global_transform();
	transform.basis = Basis::from_euler(p_euler_rad) * Basis::from_scale(transform.basis.get_scale());
	set_global_transform(transform);
}

void Node3D::set_global_rotation_degrees(const Vector3 &p_euler_degrees) {
	ERR_THREAD_GUARD;
	Vector3 radians(Math::deg_to_rad(p_euler_degrees.x), Math::deg_to_rad(p_euler_degrees.y), Math::deg_to_rad(p_euler_degrees.z));
	set_global_rotation(radians);
}

void Node3D::set_transform(const Transform3D &p_transform) {
	ERR_THREAD_GUARD;
	data.local_transform = p_transform;
	_replace_dirty_mask(DIRTY_EULER_ROTATION_AND_SCALE); // Make rot/scale dirty.

	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

Basis Node3D::get_basis() const {
	ERR_READ_THREAD_GUARD_V(Basis());
	return get_transform().basis;
}

Quaternion Node3D::get_quaternion() const {
	return get_transform().basis.get_rotation_quaternion();
}

void Node3D::set_global_transform(const Transform3D &p_transform) {
	ERR_THREAD_GUARD;
	Transform3D xform = (data.parent && !data.top_level)
			? data.parent->get_global_transform().affine_inverse() * p_transform
			: p_transform;

	set_transform(xform);
}

Transform3D Node3D::get_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform3D());
	if (_test_dirty_bits(DIRTY_LOCAL_TRANSFORM)) {
		// This update can happen if needed over multiple threads.
		_update_local_transform();
	}

	return data.local_transform;
}

// Return false to timeout and remove from the client interpolation list.
bool Node3D::update_client_physics_interpolation_data() {
	if (!is_inside_tree() || !_is_physics_interpolated_client_side()) {
		return false;
	}

	ERR_FAIL_NULL_V(data.client_physics_interpolation_data, false);
	ClientPhysicsInterpolationData &pid = *data.client_physics_interpolation_data;

	uint64_t tick = Engine::get_singleton()->get_physics_frames();

	// Has this update been done already this tick?
	// (For instance, get_global_transform_interpolated() could be called multiple times.)
	if (pid.current_physics_tick != tick) {
		// Timeout?
		if (tick >= pid.timeout_physics_tick) {
			return false;
		}

		if (pid.current_physics_tick == (tick - 1)) {
			// Normal interpolation situation, there is a continuous flow of data
			// from one tick to the next...
			pid.global_xform_prev = pid.global_xform_curr;
		} else {
			// There has been a gap, we cannot sensibly offer interpolation over
			// a multitick gap, so we will teleport.
			pid.global_xform_prev = get_global_transform();
		}
		pid.current_physics_tick = tick;
	}

	pid.global_xform_curr = get_global_transform();
	return true;
}

void Node3D::_disable_client_physics_interpolation() {
	// Disable any current client side interpolation.
	// (This can always restart as normal if you later re-attach the node to the SceneTree.)
	if (data.client_physics_interpolation_data) {
		memdelete(data.client_physics_interpolation_data);
		data.client_physics_interpolation_data = nullptr;

		SceneTree *tree = get_tree();
		if (tree && _client_physics_interpolation_node_3d_list.in_list()) {
			tree->client_physics_interpolation_remove_node_3d(&_client_physics_interpolation_node_3d_list);
		}
	}
	_set_physics_interpolated_client_side(false);
}

Transform3D Node3D::_get_global_transform_interpolated(real_t p_interpolation_fraction) {
	ERR_FAIL_COND_V(!is_inside_tree(), Transform3D());

	// Set in motion the mechanisms for client side interpolation if not already active.
	if (!_is_physics_interpolated_client_side()) {
		_set_physics_interpolated_client_side(true);

		ERR_FAIL_COND_V(data.client_physics_interpolation_data != nullptr, Transform3D());
		data.client_physics_interpolation_data = memnew(ClientPhysicsInterpolationData);
		data.client_physics_interpolation_data->global_xform_curr = get_global_transform();
		data.client_physics_interpolation_data->global_xform_prev = data.client_physics_interpolation_data->global_xform_curr;
		data.client_physics_interpolation_data->current_physics_tick = Engine::get_singleton()->get_physics_frames();
	}

	// Storing the last tick we requested client interpolation allows us to timeout
	// and remove client interpolated nodes from the list to save processing.
	// We use some arbitrary timeout here, but this could potentially be user defined.

	// Note: This timeout has to be larger than the number of ticks in a frame, otherwise the interpolated
	// data will stop flowing before the next frame is drawn. This should only be relevant at high tick rates.
	// We could alternatively do this by frames rather than ticks and avoid this problem, but then the behavior
	// would be machine dependent.
	data.client_physics_interpolation_data->timeout_physics_tick = Engine::get_singleton()->get_physics_frames() + 256;

	// Make sure data is up to date.
	update_client_physics_interpolation_data();

	// Interpolate the current data.
	const Transform3D &xform_curr = data.client_physics_interpolation_data->global_xform_curr;
	const Transform3D &xform_prev = data.client_physics_interpolation_data->global_xform_prev;

	Transform3D res;
	TransformInterpolator::interpolate_transform_3d(xform_prev, xform_curr, res, p_interpolation_fraction);

	SceneTree *tree = get_tree();

	// This should not happen, as is_inside_tree() is checked earlier.
	ERR_FAIL_NULL_V(tree, res);
	if (!_client_physics_interpolation_node_3d_list.in_list()) {
		tree->client_physics_interpolation_add_node_3d(&_client_physics_interpolation_node_3d_list);
	}

	return res;
}

Transform3D Node3D::get_global_transform_interpolated() {
	// Pass through if physics interpolation is switched off.
	// This is a convenience, as it allows you to easy turn off interpolation
	// without changing any code.
	if (!is_physics_interpolated_and_enabled()) {
		return get_global_transform();
	}

	// If we are in the physics frame, the interpolated global transform is meaningless.
	// However, there is an exception, we may want to use this as a means of starting off the client
	// interpolation pump if not already started (when _is_physics_interpolated_client_side() is false).
	if (Engine::get_singleton()->is_in_physics_frame() && _is_physics_interpolated_client_side()) {
		return get_global_transform();
	}

	return _get_global_transform_interpolated(Engine::get_singleton()->get_physics_interpolation_fraction());
}

Transform3D Node3D::get_global_transform() const {
	ERR_FAIL_COND_V(!is_inside_tree(), Transform3D());

	/* Due to how threads work at scene level, while this global transform won't be able to be changed from outside a thread,
	 * it is possible that multiple threads can access it while it's dirty from previous work. Due to this, we must ensure that
	 * the dirty/update process is thread safe by utilizing atomic copies.
	 */

	uint32_t dirty = _read_dirty_mask();
	if (dirty & DIRTY_GLOBAL_TRANSFORM) {
		if (dirty & DIRTY_LOCAL_TRANSFORM) {
			_update_local_transform(); // Update local transform atomically.
		}

		Transform3D new_global;
		if (data.parent && !data.top_level) {
			new_global = data.parent->get_global_transform() * data.local_transform;
		} else {
			new_global = data.local_transform;
		}

		if (data.disable_scale) {
			new_global.basis.orthonormalize();
		}

		data.global_transform = new_global;
		_clear_dirty_bits(DIRTY_GLOBAL_TRANSFORM);
	}

	return data.global_transform;
}

#ifdef TOOLS_ENABLED
Transform3D Node3D::get_global_gizmo_transform() const {
	return get_global_transform();
}

Transform3D Node3D::get_local_gizmo_transform() const {
	return get_transform();
}
#endif

Node3D *Node3D::get_parent_node_3d() const {
	ERR_READ_THREAD_GUARD_V(nullptr); // This can't be changed on threads anyway.
	if (data.top_level) {
		return nullptr;
	}

	return Object::cast_to<Node3D>(get_parent());
}

Transform3D Node3D::get_relative_transform(const Node *p_parent) const {
	ERR_READ_THREAD_GUARD_V(Transform3D());
	if (p_parent == this) {
		return Transform3D();
	}

	ERR_FAIL_NULL_V(data.parent, Transform3D());

	if (p_parent == data.parent) {
		return get_transform();
	} else {
		return data.parent->get_relative_transform(p_parent) * get_transform();
	}
}

void Node3D::set_position(const Vector3 &p_position) {
	ERR_THREAD_GUARD;
	data.local_transform.origin = p_position;
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

void Node3D::set_rotation_edit_mode(RotationEditMode p_mode) {
	ERR_THREAD_GUARD;
	if (data.rotation_edit_mode == p_mode) {
		return;
	}

	bool transform_changed = false;
	if (data.rotation_edit_mode == ROTATION_EDIT_MODE_BASIS && !_test_dirty_bits(DIRTY_LOCAL_TRANSFORM)) {
		data.local_transform.orthogonalize();
		transform_changed = true;
	}

	data.rotation_edit_mode = p_mode;

	if (p_mode == ROTATION_EDIT_MODE_EULER && _test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		// If going to Euler mode, ensure that vectors are _not_ dirty, else the retrieved value may be wrong.
		// Otherwise keep what is there, so switching back and forth between modes does not break the vectors.

		_update_rotation_and_scale();
	}

	if (transform_changed) {
		_propagate_transform_changed(this);
		if (data.notify_local_transform) {
			notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
		}
	}

	notify_property_list_changed();
}

Node3D::RotationEditMode Node3D::get_rotation_edit_mode() const {
	ERR_READ_THREAD_GUARD_V(ROTATION_EDIT_MODE_EULER);
	return data.rotation_edit_mode;
}

void Node3D::set_rotation_order(EulerOrder p_order) {
	ERR_THREAD_GUARD;
	if (data.euler_rotation_order == p_order) {
		return;
	}

	ERR_FAIL_INDEX(int32_t(p_order), 6);
	bool transform_changed = false;

	uint32_t dirty = _read_dirty_mask();
	if ((dirty & DIRTY_EULER_ROTATION_AND_SCALE)) {
		_update_rotation_and_scale();
	} else if ((dirty & DIRTY_LOCAL_TRANSFORM)) {
		data.euler_rotation = Basis::from_euler(data.euler_rotation, data.euler_rotation_order).get_euler_normalized(p_order);
		transform_changed = true;
	} else {
		_set_dirty_bits(DIRTY_LOCAL_TRANSFORM);
		transform_changed = true;
	}

	data.euler_rotation_order = p_order;

	if (transform_changed) {
		_propagate_transform_changed(this);
		if (data.notify_local_transform) {
			notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
		}
	}
	notify_property_list_changed(); // Will change the rotation property.
}

EulerOrder Node3D::get_rotation_order() const {
	ERR_READ_THREAD_GUARD_V(EulerOrder::XYZ);
	return data.euler_rotation_order;
}

void Node3D::set_rotation(const Vector3 &p_euler_rad) {
	ERR_THREAD_GUARD;
	if (_test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		// Update scale only if rotation and scale are dirty, as rotation will be overridden.
		data.scale = data.local_transform.basis.get_scale();
		_clear_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE);
	}

	data.euler_rotation = p_euler_rad;
	_replace_dirty_mask(DIRTY_LOCAL_TRANSFORM);
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

void Node3D::set_rotation_degrees(const Vector3 &p_euler_degrees) {
	ERR_THREAD_GUARD;
	Vector3 radians(Math::deg_to_rad(p_euler_degrees.x), Math::deg_to_rad(p_euler_degrees.y), Math::deg_to_rad(p_euler_degrees.z));
	set_rotation(radians);
}

void Node3D::set_scale(const Vector3 &p_scale) {
	ERR_THREAD_GUARD;
	if (_test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		// Update rotation only if rotation and scale are dirty, as scale will be overridden.
		data.euler_rotation = data.local_transform.basis.get_euler_normalized(data.euler_rotation_order);
		_clear_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE);
	}

	data.scale = p_scale;
	_replace_dirty_mask(DIRTY_LOCAL_TRANSFORM);
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

Vector3 Node3D::get_position() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	return data.local_transform.origin;
}

Vector3 Node3D::get_rotation() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	if (_test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		_update_rotation_and_scale();
	}

	return data.euler_rotation;
}

Vector3 Node3D::get_rotation_degrees() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	Vector3 radians = get_rotation();
	return Vector3(Math::rad_to_deg(radians.x), Math::rad_to_deg(radians.y), Math::rad_to_deg(radians.z));
}

Vector3 Node3D::get_scale() const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	if (_test_dirty_bits(DIRTY_EULER_ROTATION_AND_SCALE)) {
		_update_rotation_and_scale();
	}

	return data.scale;
}

void Node3D::update_gizmos() {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	if (!is_inside_world()) {
		return;
	}

	if (data.gizmos.is_empty()) {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, SceneStringName(_spatial_editor_group), SNAME("_request_gizmo_for_id"), get_instance_id());
		return;
	}
	if (data.gizmos_dirty) {
		return;
	}
	data.gizmos_dirty = true;
	callable_mp(this, &Node3D::_update_gizmos).call_deferred();
#endif
}

void Node3D::set_subgizmo_selection(Ref<Node3DGizmo> p_gizmo, int p_id, Transform3D p_transform) {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	if (!is_inside_world()) {
		return;
	}

	if (is_part_of_edited_scene()) {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, SceneStringName(_spatial_editor_group), SNAME("_set_subgizmo_selection"), this, p_gizmo, p_id, p_transform);
	}
#endif
}

void Node3D::clear_subgizmo_selection() {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	if (!is_inside_world()) {
		return;
	}

	if (data.gizmos.is_empty()) {
		return;
	}

	if (is_part_of_edited_scene()) {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, SceneStringName(_spatial_editor_group), SNAME("_clear_subgizmo_selection"), this);
	}
#endif
}

void Node3D::add_gizmo(Ref<Node3DGizmo> p_gizmo) {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	if (data.gizmos_disabled || p_gizmo.is_null()) {
		return;
	}
	data.gizmos.push_back(p_gizmo);

	if (p_gizmo.is_valid() && is_inside_world()) {
		p_gizmo->create();
		if (is_visible_in_tree()) {
			p_gizmo->redraw();
		}
		p_gizmo->transform();
	}
#endif
}

void Node3D::remove_gizmo(Ref<Node3DGizmo> p_gizmo) {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	int idx = data.gizmos.find(p_gizmo);
	if (idx != -1) {
		p_gizmo->free();
		data.gizmos.remove_at(idx);
	}
#endif
}

void Node3D::clear_gizmos() {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	for (int i = 0; i < data.gizmos.size(); i++) {
		data.gizmos.write[i]->free();
	}
	data.gizmos.clear();
#endif
}

TypedArray<Node3DGizmo> Node3D::get_gizmos_bind() const {
	ERR_THREAD_GUARD_V(TypedArray<Node3DGizmo>());
	TypedArray<Node3DGizmo> ret;

#ifdef TOOLS_ENABLED
	for (int i = 0; i < data.gizmos.size(); i++) {
		ret.push_back(Variant(data.gizmos[i].ptr()));
	}
#endif

	return ret;
}

Vector<Ref<Node3DGizmo>> Node3D::get_gizmos() const {
	ERR_THREAD_GUARD_V(Vector<Ref<Node3DGizmo>>());
#ifdef TOOLS_ENABLED
	return data.gizmos;
#else
	return Vector<Ref<Node3DGizmo>>();
#endif
}

void Node3D::_replace_dirty_mask(uint32_t p_mask) const {
	if (is_group_processing()) {
		data.dirty.mt.set(p_mask);
	} else {
		data.dirty.st = p_mask;
	}
}

void Node3D::_set_dirty_bits(uint32_t p_bits) const {
	if (is_group_processing()) {
		data.dirty.mt.bit_or(p_bits);
	} else {
		data.dirty.st |= p_bits;
	}
}

void Node3D::_clear_dirty_bits(uint32_t p_bits) const {
	if (is_group_processing()) {
		data.dirty.mt.bit_and(~p_bits);
	} else {
		data.dirty.st &= ~p_bits;
	}
}

void Node3D::_update_gizmos() {
#ifdef TOOLS_ENABLED
	if (data.gizmos_disabled || !is_inside_world() || !data.gizmos_dirty) {
		data.gizmos_dirty = false;
		return;
	}
	data.gizmos_dirty = false;
	for (int i = 0; i < data.gizmos.size(); i++) {
		if (is_visible_in_tree()) {
			data.gizmos.write[i]->redraw();
		} else {
			data.gizmos.write[i]->clear();
		}
	}
#endif
}

void Node3D::set_disable_gizmos(bool p_enabled) {
	ERR_THREAD_GUARD;
#ifdef TOOLS_ENABLED
	data.gizmos_disabled = p_enabled;
	if (!p_enabled) {
		clear_gizmos();
	}
#endif
}

void Node3D::reparent(Node *p_parent, bool p_keep_global_transform) {
	ERR_THREAD_GUARD;
	if (p_keep_global_transform) {
		Transform3D temp = get_global_transform();
		Node::reparent(p_parent, p_keep_global_transform);
		set_global_transform(temp);
	} else {
		Node::reparent(p_parent, p_keep_global_transform);
	}
}

void Node3D::set_disable_scale(bool p_enabled) {
	ERR_THREAD_GUARD;
	data.disable_scale = p_enabled;
}

bool Node3D::is_scale_disabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return data.disable_scale;
}

void Node3D::set_as_top_level(bool p_enabled) {
	ERR_THREAD_GUARD;
	if (data.top_level == p_enabled) {
		return;
	}
	if (is_inside_tree()) {
		if (p_enabled) {
			set_transform(get_global_transform());
		} else if (data.parent) {
			set_transform(data.parent->get_global_transform().affine_inverse() * get_global_transform());
		}
	}
	data.top_level = p_enabled;
}

void Node3D::set_as_top_level_keep_local(bool p_enabled) {
	ERR_THREAD_GUARD;
	if (data.top_level == p_enabled) {
		return;
	}
	data.top_level = p_enabled;
	_propagate_transform_changed(this);
}

bool Node3D::is_set_as_top_level() const {
	ERR_READ_THREAD_GUARD_V(false);
	return data.top_level;
}

Ref<World3D> Node3D::get_world_3d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World3D>()); // World3D can only be set from main thread, so it's safe to obtain on threads.
	ERR_FAIL_COND_V(!is_inside_world(), Ref<World3D>());
	ERR_FAIL_NULL_V(data.viewport, Ref<World3D>());

	return data.viewport->find_world_3d();
}

void Node3D::_propagate_visibility_changed() {
	notification(NOTIFICATION_VISIBILITY_CHANGED);
	emit_signal(SceneStringName(visibility_changed));

#ifdef TOOLS_ENABLED
	if (!data.gizmos.is_empty()) {
		data.gizmos_dirty = true;
		_update_gizmos();
	}
#endif

	for (Node3D *c : data.children) {
		if (!c || !c->data.visible) {
			continue;
		}
		c->_propagate_visibility_changed();
	}
}

void Node3D::show() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(true);
}

void Node3D::hide() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(false);
}

void Node3D::set_visible(bool p_visible) {
	ERR_MAIN_THREAD_GUARD;
	if (data.visible == p_visible) {
		return;
	}

	data.visible = p_visible;

	if (!is_inside_tree()) {
		return;
	}
	_propagate_visibility_changed();
}

bool Node3D::is_visible() const {
	ERR_READ_THREAD_GUARD_V(false);
	return data.visible;
}

bool Node3D::is_visible_in_tree() const {
	ERR_READ_THREAD_GUARD_V(false); // Since visibility can only be changed from main thread, this is safe to call.
	const Node3D *s = this;

	while (s) {
		if (!s->data.visible) {
			return false;
		}
		s = s->data.parent;
	}

	return true;
}

void Node3D::rotate_object_local(const Vector3 &p_axis, real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.rotate_local(p_axis, p_angle);
	set_transform(t);
}

void Node3D::rotate(const Vector3 &p_axis, real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.rotate(p_axis, p_angle);
	set_transform(t);
}

void Node3D::rotate_x(real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.rotate(Vector3(1, 0, 0), p_angle);
	set_transform(t);
}

void Node3D::rotate_y(real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.rotate(Vector3(0, 1, 0), p_angle);
	set_transform(t);
}

void Node3D::rotate_z(real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.rotate(Vector3(0, 0, 1), p_angle);
	set_transform(t);
}

void Node3D::translate(const Vector3 &p_offset) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.translate_local(p_offset);
	set_transform(t);
}

void Node3D::translate_object_local(const Vector3 &p_offset) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();

	Transform3D s;
	s.translate_local(p_offset);
	set_transform(t * s);
}

void Node3D::scale(const Vector3 &p_ratio) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.scale(p_ratio);
	set_transform(t);
}

void Node3D::scale_object_local(const Vector3 &p_scale) {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.basis.scale_local(p_scale);
	set_transform(t);
}

void Node3D::global_rotate(const Vector3 &p_axis, real_t p_angle) {
	ERR_THREAD_GUARD;
	Transform3D t = get_global_transform();
	t.basis.rotate(p_axis, p_angle);
	set_global_transform(t);
}

void Node3D::global_scale(const Vector3 &p_scale) {
	ERR_THREAD_GUARD;
	Transform3D t = get_global_transform();
	t.basis.scale(p_scale);
	set_global_transform(t);
}

void Node3D::global_translate(const Vector3 &p_offset) {
	ERR_THREAD_GUARD;
	Transform3D t = get_global_transform();
	t.origin += p_offset;
	set_global_transform(t);
}

void Node3D::orthonormalize() {
	ERR_THREAD_GUARD;
	Transform3D t = get_transform();
	t.orthonormalize();
	set_transform(t);
}

void Node3D::set_identity() {
	ERR_THREAD_GUARD;
	set_transform(Transform3D());
}

void Node3D::look_at(const Vector3 &p_target, const Vector3 &p_up, bool p_use_model_front) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND_MSG(!is_inside_tree(), "Node not inside tree. Use look_at_from_position() instead.");
	Vector3 origin = get_global_transform().origin;
	look_at_from_position(origin, p_target, p_up, p_use_model_front);
}

void Node3D::look_at_from_position(const Vector3 &p_pos, const Vector3 &p_target, const Vector3 &p_up, bool p_use_model_front) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND_MSG(p_pos.is_equal_approx(p_target), "Node origin and target are in the same position, look_at() failed.");
	ERR_FAIL_COND_MSG(p_up.is_zero_approx(), "The up vector can't be zero, look_at() failed.");

	Vector3 forward = p_target - p_pos;
	Basis lookat_basis = Basis::looking_at(forward, p_up, p_use_model_front);
	Vector3 original_scale = get_scale();
	set_global_transform(Transform3D(lookat_basis, p_pos));
	set_scale(original_scale);
}

Vector3 Node3D::to_local(Vector3 p_global) const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	return get_global_transform().affine_inverse().xform(p_global);
}

Vector3 Node3D::to_global(Vector3 p_local) const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	return get_global_transform().xform(p_local);
}

void Node3D::set_notify_transform(bool p_enabled) {
	ERR_THREAD_GUARD;
	data.notify_transform = p_enabled;
}

bool Node3D::is_transform_notification_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return data.notify_transform;
}

void Node3D::set_notify_local_transform(bool p_enabled) {
	ERR_THREAD_GUARD;
	data.notify_local_transform = p_enabled;
}

bool Node3D::is_local_transform_notification_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return data.notify_local_transform;
}

void Node3D::force_update_transform() {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	if (!xform_change.in_list()) {
		return; //nothing to update
	}
	get_tree()->xform_change_list.remove(&xform_change);

	notification(NOTIFICATION_TRANSFORM_CHANGED);
}

void Node3D::_update_visibility_parent(bool p_update_root) {
	RID new_parent;

	if (!visibility_parent_path.is_empty()) {
		if (!p_update_root) {
			return;
		}
		Node *parent = get_node_or_null(visibility_parent_path);
		ERR_FAIL_NULL_MSG(parent, "Can't find visibility parent node at path: " + visibility_parent_path);
		ERR_FAIL_COND_MSG(parent == this, "The visibility parent can't be the same node.");
		GeometryInstance3D *gi = Object::cast_to<GeometryInstance3D>(parent);
		ERR_FAIL_NULL_MSG(gi, "The visibility parent node must be a GeometryInstance3D, at path: " + visibility_parent_path);
		new_parent = gi ? gi->get_instance() : RID();
	} else if (data.parent) {
		new_parent = data.parent->data.visibility_parent;
	}

	if (new_parent == data.visibility_parent) {
		return;
	}

	data.visibility_parent = new_parent;

	VisualInstance3D *vi = Object::cast_to<VisualInstance3D>(this);
	if (vi) {
		RS::get_singleton()->instance_set_visibility_parent(vi->get_instance(), data.visibility_parent);
	}

	for (Node3D *c : data.children) {
		c->_update_visibility_parent(false);
	}
}

void Node3D::set_visibility_parent(const NodePath &p_path) {
	ERR_MAIN_THREAD_GUARD;
	visibility_parent_path = p_path;
	if (is_inside_tree()) {
		_update_visibility_parent(true);
	}
}

NodePath Node3D::get_visibility_parent() const {
	ERR_READ_THREAD_GUARD_V(NodePath());
	return visibility_parent_path;
}

void Node3D::_validate_property(PropertyInfo &p_property) const {
	if (data.rotation_edit_mode != ROTATION_EDIT_MODE_BASIS && p_property.name == "basis") {
		p_property.usage = 0;
	}
	if (data.rotation_edit_mode == ROTATION_EDIT_MODE_BASIS && p_property.name == "scale") {
		p_property.usage = 0;
	}
	if (data.rotation_edit_mode != ROTATION_EDIT_MODE_QUATERNION && p_property.name == "quaternion") {
		p_property.usage = 0;
	}
	if (data.rotation_edit_mode != ROTATION_EDIT_MODE_EULER && p_property.name == "rotation") {
		p_property.usage = 0;
	}
	if (data.rotation_edit_mode != ROTATION_EDIT_MODE_EULER && p_property.name == "rotation_order") {
		p_property.usage = 0;
	}
}

bool Node3D::_property_can_revert(const StringName &p_name) const {
	const String sname = p_name;
	if (sname == "basis") {
		return true;
	} else if (sname == "scale") {
		return true;
	} else if (sname == "quaternion") {
		return true;
	} else if (sname == "rotation") {
		return true;
	} else if (sname == "position") {
		return true;
	}
	return false;
}

bool Node3D::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	bool valid = false;

	const String sname = p_name;
	if (sname == "basis") {
		Variant variant = PropertyUtils::get_property_default_value(this, "transform", &valid);
		if (valid && variant.get_type() == Variant::Type::TRANSFORM3D) {
			r_property = Transform3D(variant).get_basis();
		} else {
			r_property = Basis();
		}
	} else if (sname == "scale") {
		Variant variant = PropertyUtils::get_property_default_value(this, "transform", &valid);
		if (valid && variant.get_type() == Variant::Type::TRANSFORM3D) {
			r_property = Transform3D(variant).get_basis().get_scale();
		} else {
			r_property = Vector3(1.0, 1.0, 1.0);
		}
	} else if (sname == "quaternion") {
		Variant variant = PropertyUtils::get_property_default_value(this, "transform", &valid);
		if (valid && variant.get_type() == Variant::Type::TRANSFORM3D) {
			r_property = Quaternion(Transform3D(variant).get_basis().get_rotation_quaternion());
		} else {
			r_property = Quaternion();
		}
	} else if (sname == "rotation") {
		Variant variant = PropertyUtils::get_property_default_value(this, "transform", &valid);
		if (valid && variant.get_type() == Variant::Type::TRANSFORM3D) {
			r_property = Transform3D(variant).get_basis().get_euler_normalized(data.euler_rotation_order);
		} else {
			r_property = Vector3();
		}
	} else if (sname == "position") {
		Variant variant = PropertyUtils::get_property_default_value(this, "transform", &valid);
		if (valid) {
			r_property = Transform3D(variant).get_origin();
		} else {
			r_property = Vector3();
		}
	} else {
		return false;
	}
	return true;
}

void Node3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_transform", "local"), &Node3D::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &Node3D::get_transform);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &Node3D::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &Node3D::get_position);
	ClassDB::bind_method(D_METHOD("set_rotation", "euler_radians"), &Node3D::set_rotation);
	ClassDB::bind_method(D_METHOD("get_rotation"), &Node3D::get_rotation);
	ClassDB::bind_method(D_METHOD("set_rotation_degrees", "euler_degrees"), &Node3D::set_rotation_degrees);
	ClassDB::bind_method(D_METHOD("get_rotation_degrees"), &Node3D::get_rotation_degrees);
	ClassDB::bind_method(D_METHOD("set_rotation_order", "order"), &Node3D::set_rotation_order);
	ClassDB::bind_method(D_METHOD("get_rotation_order"), &Node3D::get_rotation_order);
	ClassDB::bind_method(D_METHOD("set_rotation_edit_mode", "edit_mode"), &Node3D::set_rotation_edit_mode);
	ClassDB::bind_method(D_METHOD("get_rotation_edit_mode"), &Node3D::get_rotation_edit_mode);
	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &Node3D::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &Node3D::get_scale);
	ClassDB::bind_method(D_METHOD("set_quaternion", "quaternion"), &Node3D::set_quaternion);
	ClassDB::bind_method(D_METHOD("get_quaternion"), &Node3D::get_quaternion);
	ClassDB::bind_method(D_METHOD("set_basis", "basis"), &Node3D::set_basis);
	ClassDB::bind_method(D_METHOD("get_basis"), &Node3D::get_basis);

	ClassDB::bind_method(D_METHOD("set_global_transform", "global"), &Node3D::set_global_transform);
	ClassDB::bind_method(D_METHOD("get_global_transform"), &Node3D::get_global_transform);
	ClassDB::bind_method(D_METHOD("get_global_transform_interpolated"), &Node3D::get_global_transform_interpolated);
	ClassDB::bind_method(D_METHOD("set_global_position", "position"), &Node3D::set_global_position);
	ClassDB::bind_method(D_METHOD("get_global_position"), &Node3D::get_global_position);
	ClassDB::bind_method(D_METHOD("set_global_basis", "basis"), &Node3D::set_global_basis);
	ClassDB::bind_method(D_METHOD("get_global_basis"), &Node3D::get_global_basis);
	ClassDB::bind_method(D_METHOD("set_global_rotation", "euler_radians"), &Node3D::set_global_rotation);
	ClassDB::bind_method(D_METHOD("get_global_rotation"), &Node3D::get_global_rotation);
	ClassDB::bind_method(D_METHOD("set_global_rotation_degrees", "euler_degrees"), &Node3D::set_global_rotation_degrees);
	ClassDB::bind_method(D_METHOD("get_global_rotation_degrees"), &Node3D::get_global_rotation_degrees);

	ClassDB::bind_method(D_METHOD("get_parent_node_3d"), &Node3D::get_parent_node_3d);
	ClassDB::bind_method(D_METHOD("set_ignore_transform_notification", "enabled"), &Node3D::set_ignore_transform_notification);
	ClassDB::bind_method(D_METHOD("set_as_top_level", "enable"), &Node3D::set_as_top_level);
	ClassDB::bind_method(D_METHOD("is_set_as_top_level"), &Node3D::is_set_as_top_level);
	ClassDB::bind_method(D_METHOD("set_disable_scale", "disable"), &Node3D::set_disable_scale);
	ClassDB::bind_method(D_METHOD("is_scale_disabled"), &Node3D::is_scale_disabled);
	ClassDB::bind_method(D_METHOD("get_world_3d"), &Node3D::get_world_3d);

	ClassDB::bind_method(D_METHOD("force_update_transform"), &Node3D::force_update_transform);

	ClassDB::bind_method(D_METHOD("set_visibility_parent", "path"), &Node3D::set_visibility_parent);
	ClassDB::bind_method(D_METHOD("get_visibility_parent"), &Node3D::get_visibility_parent);

	ClassDB::bind_method(D_METHOD("update_gizmos"), &Node3D::update_gizmos);
	ClassDB::bind_method(D_METHOD("add_gizmo", "gizmo"), &Node3D::add_gizmo);
	ClassDB::bind_method(D_METHOD("get_gizmos"), &Node3D::get_gizmos_bind);
	ClassDB::bind_method(D_METHOD("clear_gizmos"), &Node3D::clear_gizmos);
	ClassDB::bind_method(D_METHOD("set_subgizmo_selection", "gizmo", "id", "transform"), &Node3D::set_subgizmo_selection);
	ClassDB::bind_method(D_METHOD("clear_subgizmo_selection"), &Node3D::clear_subgizmo_selection);

	ClassDB::bind_method(D_METHOD("set_visible", "visible"), &Node3D::set_visible);
	ClassDB::bind_method(D_METHOD("is_visible"), &Node3D::is_visible);
	ClassDB::bind_method(D_METHOD("is_visible_in_tree"), &Node3D::is_visible_in_tree);
	ClassDB::bind_method(D_METHOD("show"), &Node3D::show);
	ClassDB::bind_method(D_METHOD("hide"), &Node3D::hide);

	ClassDB::bind_method(D_METHOD("set_notify_local_transform", "enable"), &Node3D::set_notify_local_transform);
	ClassDB::bind_method(D_METHOD("is_local_transform_notification_enabled"), &Node3D::is_local_transform_notification_enabled);

	ClassDB::bind_method(D_METHOD("set_notify_transform", "enable"), &Node3D::set_notify_transform);
	ClassDB::bind_method(D_METHOD("is_transform_notification_enabled"), &Node3D::is_transform_notification_enabled);

	ClassDB::bind_method(D_METHOD("rotate", "axis", "angle"), &Node3D::rotate);
	ClassDB::bind_method(D_METHOD("global_rotate", "axis", "angle"), &Node3D::global_rotate);
	ClassDB::bind_method(D_METHOD("global_scale", "scale"), &Node3D::global_scale);
	ClassDB::bind_method(D_METHOD("global_translate", "offset"), &Node3D::global_translate);
	ClassDB::bind_method(D_METHOD("rotate_object_local", "axis", "angle"), &Node3D::rotate_object_local);
	ClassDB::bind_method(D_METHOD("scale_object_local", "scale"), &Node3D::scale_object_local);
	ClassDB::bind_method(D_METHOD("translate_object_local", "offset"), &Node3D::translate_object_local);
	ClassDB::bind_method(D_METHOD("rotate_x", "angle"), &Node3D::rotate_x);
	ClassDB::bind_method(D_METHOD("rotate_y", "angle"), &Node3D::rotate_y);
	ClassDB::bind_method(D_METHOD("rotate_z", "angle"), &Node3D::rotate_z);
	ClassDB::bind_method(D_METHOD("translate", "offset"), &Node3D::translate);
	ClassDB::bind_method(D_METHOD("orthonormalize"), &Node3D::orthonormalize);
	ClassDB::bind_method(D_METHOD("set_identity"), &Node3D::set_identity);

	ClassDB::bind_method(D_METHOD("look_at", "target", "up", "use_model_front"), &Node3D::look_at, DEFVAL(Vector3(0, 1, 0)), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("look_at_from_position", "position", "target", "up", "use_model_front"), &Node3D::look_at_from_position, DEFVAL(Vector3(0, 1, 0)), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("to_local", "global_point"), &Node3D::to_local);
	ClassDB::bind_method(D_METHOD("to_global", "local_point"), &Node3D::to_global);

	BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
	BIND_CONSTANT(NOTIFICATION_ENTER_WORLD);
	BIND_CONSTANT(NOTIFICATION_EXIT_WORLD);
	BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
	BIND_CONSTANT(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);

	BIND_ENUM_CONSTANT(ROTATION_EDIT_MODE_EULER);
	BIND_ENUM_CONSTANT(ROTATION_EDIT_MODE_QUATERNION);
	BIND_ENUM_CONSTANT(ROTATION_EDIT_MODE_BASIS);

	ADD_GROUP("Transform", "");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "transform", PROPERTY_HINT_NONE, "suffix:m", PROPERTY_USAGE_NO_EDITOR), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "global_transform", PROPERTY_HINT_NONE, "suffix:m", PROPERTY_USAGE_NONE), "set_global_transform", "get_global_transform");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position", PROPERTY_HINT_RANGE, "-99999,99999,or_greater,or_less,hide_slider,suffix:m", PROPERTY_USAGE_EDITOR), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation", PROPERTY_HINT_RANGE, "-360,360,0.1,or_less,or_greater,radians_as_degrees", PROPERTY_USAGE_EDITOR), "set_rotation", "get_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation_degrees", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_rotation_degrees", "get_rotation_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::QUATERNION, "quaternion", PROPERTY_HINT_HIDE_QUATERNION_EDIT, "", PROPERTY_USAGE_EDITOR), "set_quaternion", "get_quaternion");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS, "basis", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_basis", "get_basis");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "scale", PROPERTY_HINT_LINK, "", PROPERTY_USAGE_EDITOR), "set_scale", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_edit_mode", PROPERTY_HINT_ENUM, "Euler,Quaternion,Basis"), "set_rotation_edit_mode", "get_rotation_edit_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_order", PROPERTY_HINT_ENUM, "XYZ,XZY,YXZ,YZX,ZXY,ZYX"), "set_rotation_order", "get_rotation_order");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "top_level"), "set_as_top_level", "is_set_as_top_level");

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "global_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_position", "get_global_position");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS, "global_basis", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_basis", "get_global_basis");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "global_rotation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_rotation", "get_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "global_rotation_degrees", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_rotation_degrees", "get_global_rotation_degrees");
	ADD_GROUP("Visibility", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "is_visible");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "visibility_parent", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "GeometryInstance3D"), "set_visibility_parent", "get_visibility_parent");

	ADD_SIGNAL(MethodInfo("visibility_changed"));
}

Node3D::Node3D() :
		xform_change(this), _client_physics_interpolation_node_3d_list(this) {
	// Default member initializer for bitfield is a C++20 extension, so:

	data.top_level = false;
	data.inside_world = false;

	data.ignore_notification = false;
	data.notify_local_transform = false;
	data.notify_transform = false;

	data.visible = true;
	data.disable_scale = false;
	data.vi_visible = true;

#ifdef TOOLS_ENABLED
	data.gizmos_disabled = false;
	data.gizmos_dirty = false;
	data.transform_gizmo_visible = true;
#endif
}

Node3D::~Node3D() {
	_disable_client_physics_interpolation();
}
