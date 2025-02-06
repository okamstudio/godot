/**************************************************************************/
/*  node_3d.h                                                             */
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

#include "scene/main/node.h"
#include "scene/resources/3d/world_3d.h"

class Node3DGizmo : public RefCounted {
	GDCLASS(Node3DGizmo, RefCounted);

public:
	virtual void create() = 0;
	virtual void transform() = 0;
	virtual void clear() = 0;
	virtual void redraw() = 0;
	virtual void free() = 0;

	Node3DGizmo();
	virtual ~Node3DGizmo() {}
};

class Node3D : public Node {
	GDCLASS(Node3D, Node);

public:
	// Edit mode for the rotation.
	// THIS MODE ONLY AFFECTS HOW DATA IS EDITED AND SAVED
	// IT DOES _NOT_ AFFECT THE TRANSFORM LOGIC (see comment in TransformDirty).
	enum RotationEditMode {
		ROTATION_EDIT_MODE_EULER,
		ROTATION_EDIT_MODE_QUATERNION,
		ROTATION_EDIT_MODE_BASIS,
	};

private:
	// For the sake of ease of use, Node3D can operate with Transforms (Basis+Origin), Quaternion/Scale and Euler Rotation/Scale.
	// Transform and Quaternion are stored in data.local_transform Basis (so quaternion is not really stored, but converted back/forth from 3x3 matrix on demand).
	// Euler needs to be kept separate because converting to Basis and back may result in a different vector (which is troublesome for users
	// editing in the inspector, not only because of the numerical precision loss but because they expect these rotations to be consistent, or support
	// "redundant" rotations for animation interpolation, like going from 0 to 720 degrees).
	//
	// As such, the system works in a way where if the local transform is set (via transform/basis/quaternion), the EULER rotation and scale becomes dirty.
	// It will remain dirty until reading back is attempted (for performance reasons). Likewise, if the Euler rotation scale are set, the local transform
	// will become dirty (and again, will not become valid again until read).
	//
	// All this is transparent from outside the Node3D API, which allows everything to works by calling these functions in exchange.
	//
	// Additionally, setting either transform, quaternion, Euler rotation or scale makes the global transform dirty, which will be updated when read again.
	//
	// NOTE: Again, RotationEditMode is _independent_ of this mechanism, it is only meant to expose the right set of properties for editing (editor) and saving
	// (to scene, in order to keep the same values and avoid data loss on conversions). It has zero influence in the logic described above.
	enum TransformDirty {
		DIRTY_NONE = 0,
		DIRTY_EULER_ROTATION_AND_SCALE = 1,
		DIRTY_LOCAL_TRANSFORM = 2,
		DIRTY_GLOBAL_TRANSFORM = 4
	};

	struct ClientPhysicsInterpolationData {
		Transform3D global_xform_curr;
		Transform3D global_xform_prev;
		uint64_t current_physics_tick = 0;
		uint64_t timeout_physics_tick = 0;
	};

	mutable SelfList<Node> xform_change;
	SelfList<Node3D> _client_physics_interpolation_node_3d_list;

	// This Data struct is to avoid namespace pollution in derived classes.

	struct Data {
		mutable Transform3D global_transform;
		mutable Transform3D local_transform;
		mutable EulerOrder euler_rotation_order = EulerOrder::YXZ;
		mutable Vector3 euler_rotation;
		mutable Vector3 scale = Vector3(1, 1, 1);
		mutable RotationEditMode rotation_edit_mode = ROTATION_EDIT_MODE_EULER;

		mutable MTNumeric<uint32_t> dirty;

		Viewport *viewport = nullptr;

		bool top_level : 1;
		bool inside_world : 1;

		// This is cached, and only currently kept up to date in visual instances.
		// This is set if a visual instance is (a) in the tree AND (b) visible via is_visible_in_tree() call.
		bool vi_visible : 1;

		bool ignore_notification : 1;
		bool notify_local_transform : 1;
		bool notify_transform : 1;

		bool visible : 1;
		bool disable_scale : 1;

		RID visibility_parent;

		Node3D *parent = nullptr;
		List<Node3D *> children;
		List<Node3D *>::Element *C = nullptr;

		ClientPhysicsInterpolationData *client_physics_interpolation_data = nullptr;

#ifdef TOOLS_ENABLED
		Vector<Ref<Node3DGizmo>> gizmos;
		bool gizmos_disabled : 1;
		bool gizmos_dirty : 1;
		bool transform_gizmo_visible : 1;
#endif

	} data;

	NodePath visibility_parent_path;

	_FORCE_INLINE_ uint32_t _read_dirty_mask() const { return is_group_processing() ? data.dirty.mt.get() : data.dirty.st; }
	_FORCE_INLINE_ bool _test_dirty_bits(uint32_t p_bits) const { return is_group_processing() ? data.dirty.mt.bit_and(p_bits) : (data.dirty.st & p_bits); }
	void _replace_dirty_mask(uint32_t p_mask) const;
	void _set_dirty_bits(uint32_t p_bits) const;
	void _clear_dirty_bits(uint32_t p_bits) const;

	void _update_gizmos();
	void _notify_dirty();
	void _propagate_transform_changed(Node3D *p_origin);

	void _propagate_visibility_changed();

	void _propagate_visibility_parent();
	void _update_visibility_parent(bool p_update_root);
	void _propagate_transform_changed_deferred();

protected:
	_FORCE_INLINE_ void set_ignore_transform_notification(bool p_ignore) { data.ignore_notification = p_ignore; }

	_FORCE_INLINE_ void _update_local_transform() const;
	_FORCE_INLINE_ void _update_rotation_and_scale() const;

	void _set_vi_visible(bool p_visible) { data.vi_visible = p_visible; }
	bool _is_vi_visible() const { return data.vi_visible; }
	Transform3D _get_global_transform_interpolated(real_t p_interpolation_fraction);
	void _disable_client_physics_interpolation();

	void _notification(int p_what);
	static void _bind_methods();

	void _validate_property(PropertyInfo &p_property) const;

	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

public:
	enum {
		NOTIFICATION_TRANSFORM_CHANGED = SceneTree::NOTIFICATION_TRANSFORM_CHANGED,
		NOTIFICATION_ENTER_WORLD = 41,
		NOTIFICATION_EXIT_WORLD = 42,
		NOTIFICATION_VISIBILITY_CHANGED = 43,
		NOTIFICATION_LOCAL_TRANSFORM_CHANGED = 44,
	};

	Node3D *get_parent_node_3d() const;

	Ref<World3D> get_world_3d() const;

	void set_position(const Vector3 &p_position);

	void set_rotation_edit_mode(RotationEditMode p_mode);
	RotationEditMode get_rotation_edit_mode() const;

	void set_rotation_order(EulerOrder p_order);
	void set_rotation(const Vector3 &p_euler_rad);
	void set_rotation_degrees(const Vector3 &p_euler_degrees);
	void set_scale(const Vector3 &p_scale);

	void set_global_position(const Vector3 &p_position);
	void set_global_basis(const Basis &p_basis);
	void set_global_rotation(const Vector3 &p_euler_rad);
	void set_global_rotation_degrees(const Vector3 &p_euler_degrees);

	Vector3 get_position() const;

	EulerOrder get_rotation_order() const;
	Vector3 get_rotation() const;
	Vector3 get_rotation_degrees() const;
	Vector3 get_scale() const;

	Vector3 get_global_position() const;
	Basis get_global_basis() const;
	Vector3 get_global_rotation() const;
	Vector3 get_global_rotation_degrees() const;

	void set_transform(const Transform3D &p_transform);
	void set_basis(const Basis &p_basis);
	void set_quaternion(const Quaternion &p_quaternion);
	void set_global_transform(const Transform3D &p_transform);

	Transform3D get_transform() const;
	Basis get_basis() const;
	Quaternion get_quaternion() const;
	Transform3D get_global_transform() const;

	Transform3D get_global_transform_interpolated();
	bool update_client_physics_interpolation_data();

#ifdef TOOLS_ENABLED
	virtual Transform3D get_global_gizmo_transform() const;
	virtual Transform3D get_local_gizmo_transform() const;
	virtual void set_transform_gizmo_visible(bool p_enabled) { data.transform_gizmo_visible = p_enabled; }
	virtual bool is_transform_gizmo_visible() const { return data.transform_gizmo_visible; }
#endif
	virtual void reparent(Node *p_parent, bool p_keep_global_transform = true) override;

	void set_disable_gizmos(bool p_enabled);
	void update_gizmos();
	void set_subgizmo_selection(Ref<Node3DGizmo> p_gizmo, int p_id, Transform3D p_transform = Transform3D());
	void clear_subgizmo_selection();
	Vector<Ref<Node3DGizmo>> get_gizmos() const;
	TypedArray<Node3DGizmo> get_gizmos_bind() const;
	void add_gizmo(Ref<Node3DGizmo> p_gizmo);
	void remove_gizmo(Ref<Node3DGizmo> p_gizmo);
	void clear_gizmos();

	void set_as_top_level(bool p_enabled);
	void set_as_top_level_keep_local(bool p_enabled);
	bool is_set_as_top_level() const;

	void set_disable_scale(bool p_enabled);
	bool is_scale_disabled() const;

	_FORCE_INLINE_ bool is_inside_world() const { return data.inside_world; }

	Transform3D get_relative_transform(const Node *p_parent) const;

	void rotate(const Vector3 &p_axis, real_t p_angle);
	void rotate_x(real_t p_angle);
	void rotate_y(real_t p_angle);
	void rotate_z(real_t p_angle);
	void translate(const Vector3 &p_offset);
	void scale(const Vector3 &p_ratio);

	void rotate_object_local(const Vector3 &p_axis, real_t p_angle);
	void scale_object_local(const Vector3 &p_scale);
	void translate_object_local(const Vector3 &p_offset);

	void global_rotate(const Vector3 &p_axis, real_t p_angle);
	void global_scale(const Vector3 &p_scale);
	void global_translate(const Vector3 &p_offset);

	void look_at(const Vector3 &p_target, const Vector3 &p_up = Vector3(0, 1, 0), bool p_use_model_front = false);
	void look_at_from_position(const Vector3 &p_pos, const Vector3 &p_target, const Vector3 &p_up = Vector3(0, 1, 0), bool p_use_model_front = false);

	Vector3 to_local(Vector3 p_global) const;
	Vector3 to_global(Vector3 p_local) const;

	void set_notify_transform(bool p_enabled);
	bool is_transform_notification_enabled() const;

	void set_notify_local_transform(bool p_enabled);
	bool is_local_transform_notification_enabled() const;

	void orthonormalize();
	void set_identity();

	void set_visible(bool p_visible);
	void show();
	void hide();
	bool is_visible() const;
	bool is_visible_in_tree() const;

	void force_update_transform();

	void set_visibility_parent(const NodePath &p_path);
	NodePath get_visibility_parent() const;

	Node3D();
	~Node3D();
};

VARIANT_ENUM_CAST(Node3D::RotationEditMode)
