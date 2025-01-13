/**************************************************************************/
/*  skeleton_2d.cpp                                                       */
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

#include "skeleton_2d.h"

#include "core/math/transform_interpolator.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_data.h"
#include "editor/editor_settings.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#endif //TOOLS_ENABLED

bool Bone2D::_set(const StringName &p_path, const Variant &p_value) {
	if (p_path == SNAME("auto_calculate_length_and_angle")) {
		set_autocalculate_length_and_angle(p_value);
	} else if (p_path == SNAME("length")) {
		set_length(p_value);
	} else if (p_path == SNAME("bone_angle")) {
		set_bone_angle(Math::deg_to_rad(real_t(p_value)));
	} else if (p_path == SNAME("default_length")) {
		set_length(p_value);
	}
#ifdef TOOLS_ENABLED
	else if (p_path == SNAME("editor_settings/show_bone_gizmo")) {
		_editor_set_show_bone_gizmo(p_value);
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

bool Bone2D::_get(const StringName &p_path, Variant &r_ret) const {
	if (p_path == SNAME("auto_calculate_length_and_angle")) {
		r_ret = get_autocalculate_length_and_angle();
	} else if (p_path == SNAME("length")) {
		r_ret = get_length();
	} else if (p_path == SNAME("bone_angle")) {
		r_ret = Math::rad_to_deg(get_bone_angle());
	} else if (p_path == SNAME("default_length")) {
		r_ret = get_length();
	}
#ifdef TOOLS_ENABLED
	else if (p_path == SNAME("editor_settings/show_bone_gizmo")) {
		r_ret = _editor_get_show_bone_gizmo();
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

void Bone2D::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("auto_calculate_length_and_angle"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	if (!autocalculate_length_and_angle) {
		p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("length"), PROPERTY_HINT_RANGE, "1, 1024, 1", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("bone_angle"), PROPERTY_HINT_RANGE, "-360, 360, 0.01", PROPERTY_USAGE_DEFAULT));
	}

#ifdef TOOLS_ENABLED
	p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("editor_settings/show_bone_gizmo"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
#endif // TOOLS_ENABLED
}

void Bone2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Node *parent = get_parent();
			parent_bone = Object::cast_to<Bone2D>(parent);
			skeleton = nullptr;
			while (parent) {
				skeleton = Object::cast_to<Skeleton2D>(parent);
				if (skeleton) {
					break;
				}
				if (!Object::cast_to<Bone2D>(parent)) {
					break; //skeletons must be chained to Bone2Ds.
				}

				parent = parent->get_parent();
			}

			if (skeleton) {
				Skeleton2D::Bone bone;
				bone.bone = this;
				skeleton->bones.push_back(bone);
				skeleton->_make_bone_setup_dirty();
				get_parent()->connect(SNAME("child_order_changed"), callable_mp(skeleton, &Skeleton2D::_make_bone_setup_dirty), CONNECT_REFERENCE_COUNTED);
			}

			cache_transform = get_transform();
			copy_transform_to_cache = true;

#ifdef TOOLS_ENABLED
			// Only draw the gizmo in the editor!
			if (Engine::get_singleton()->is_editor_hint() == false) {
				return;
			}

			queue_redraw();
#endif // TOOLS_ENABLED
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (skeleton) {
				skeleton->_make_transform_dirty();
			}
			if (copy_transform_to_cache) {
				cache_transform = get_transform();
			}
#ifdef TOOLS_ENABLED
			// Only draw the gizmo in the editor!
			if (Engine::get_singleton()->is_editor_hint() == false) {
				return;
			}

			queue_redraw();

			if (get_parent()) {
				Bone2D *p_bone = Object::cast_to<Bone2D>(get_parent());
				if (p_bone) {
					p_bone->queue_redraw();
				}
			}
#endif // TOOLS_ENABLED
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (skeleton) {
				for (uint32_t i = 0; i < skeleton->bones.size(); i++) {
					if (skeleton->bones[i].bone == this) {
						skeleton->bones.remove_at(i);
						break;
					}
				}
				skeleton->_make_bone_setup_dirty();
				get_parent()->disconnect(SNAME("child_order_changed"), callable_mp(skeleton, &Skeleton2D::_make_bone_setup_dirty));
			}
			parent_bone = nullptr;
			set_transform(cache_transform);
		} break;

		case NOTIFICATION_READY: {
			if (autocalculate_length_and_angle) {
				calculate_length_and_rotation();
			}
		} break;

#ifdef TOOLS_ENABLED
		case NOTIFICATION_EDITOR_PRE_SAVE:
		case NOTIFICATION_EDITOR_POST_SAVE: {
			Transform2D tmp_trans = get_transform();
			set_transform(cache_transform);
			cache_transform = tmp_trans;
		} break;

		// Bone2D Editor gizmo drawing.
		// TODO: Bone2D gizmo drawing needs to be moved to an editor plugin.
		case NOTIFICATION_DRAW: {
			// Only draw the gizmo in the editor!
			if (Engine::get_singleton()->is_editor_hint() == false) {
				return;
			}

			if (editor_gizmo_rid.is_null()) {
				editor_gizmo_rid = RenderingServer::get_singleton()->canvas_item_create();
				RenderingServer::get_singleton()->canvas_item_set_parent(editor_gizmo_rid, get_canvas_item());
				RenderingServer::get_singleton()->canvas_item_set_z_as_relative_to_parent(editor_gizmo_rid, true);
				RenderingServer::get_singleton()->canvas_item_set_z_index(editor_gizmo_rid, 10);
			}
			RenderingServer::get_singleton()->canvas_item_clear(editor_gizmo_rid);

			if (!_editor_show_bone_gizmo) {
				return;
			}

			// Undo scaling
			Transform2D editor_gizmo_trans;
			editor_gizmo_trans.set_scale(Vector2(1, 1) / get_global_scale());
			RenderingServer::get_singleton()->canvas_item_set_transform(editor_gizmo_rid, editor_gizmo_trans);

			Color bone_color1 = EDITOR_GET("editors/2d/bone_color1");
			Color bone_color2 = EDITOR_GET("editors/2d/bone_color2");
			Color bone_ik_color = EDITOR_GET("editors/2d/bone_ik_color");
			Color bone_outline_color = EDITOR_GET("editors/2d/bone_outline_color");
			Color bone_selected_color = EDITOR_GET("editors/2d/bone_selected_color");

			bool Bone2D_found = false;
			for (int i = 0; i < get_child_count(); i++) {
				Bone2D *child_node = nullptr;
				child_node = Object::cast_to<Bone2D>(get_child(i));
				if (!child_node) {
					continue;
				}
				Bone2D_found = true;

				Vector<Vector2> bone_shape;
				Vector<Vector2> bone_shape_outline;

				_editor_get_bone_shape(&bone_shape, &bone_shape_outline, child_node);

				Vector<Color> colors;
				if (has_meta("_local_pose_override_enabled_")) {
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
				} else {
					colors.push_back(bone_color1);
					colors.push_back(bone_color2);
					colors.push_back(bone_color1);
					colors.push_back(bone_color2);
				}

				Vector<Color> outline_colors;
				if (CanvasItemEditor::get_singleton()->editor_selection->is_selected(this)) {
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
				} else {
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
				}

				RenderingServer::get_singleton()->canvas_item_add_polygon(editor_gizmo_rid, bone_shape_outline, outline_colors);
				RenderingServer::get_singleton()->canvas_item_add_polygon(editor_gizmo_rid, bone_shape, colors);
			}

			if (!Bone2D_found) {
				Vector<Vector2> bone_shape;
				Vector<Vector2> bone_shape_outline;

				_editor_get_bone_shape(&bone_shape, &bone_shape_outline, nullptr);

				Vector<Color> colors;
				if (has_meta("_local_pose_override_enabled_")) {
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
					colors.push_back(bone_ik_color);
				} else {
					colors.push_back(bone_color1);
					colors.push_back(bone_color2);
					colors.push_back(bone_color1);
					colors.push_back(bone_color2);
				}

				Vector<Color> outline_colors;
				if (CanvasItemEditor::get_singleton()->editor_selection->is_selected(this)) {
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
					outline_colors.push_back(bone_selected_color);
				} else {
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
					outline_colors.push_back(bone_outline_color);
				}

				RenderingServer::get_singleton()->canvas_item_add_polygon(editor_gizmo_rid, bone_shape_outline, outline_colors);
				RenderingServer::get_singleton()->canvas_item_add_polygon(editor_gizmo_rid, bone_shape, colors);
			}
		} break;
#endif // TOOLS_ENABLED
	}
}

#ifdef TOOLS_ENABLED
bool Bone2D::_editor_get_bone_shape(Vector<Vector2> *p_shape, Vector<Vector2> *p_outline_shape, Bone2D *p_other_bone) {
	float bone_width = EDITOR_GET("editors/2d/bone_width");
	float bone_outline_width = EDITOR_GET("editors/2d/bone_outline_size");

	if (!is_inside_tree()) {
		return false; //may have been removed
	}
	if (!p_other_bone && length <= 0) {
		return false;
	}

	Vector2 rel;
	if (p_other_bone) {
		rel = (p_other_bone->get_global_position() - get_global_position());
		rel = rel.rotated(-get_global_rotation()); // Undo Bone2D node's rotation so its drawn correctly regardless of the node's rotation
	} else {
		rel = Vector2(Math::cos(bone_angle), Math::sin(bone_angle)) * length * get_global_scale();
	}

	Vector2 relt = rel.rotated(Math_PI * 0.5).normalized() * bone_width;
	Vector2 reln = rel.normalized();
	Vector2 reltn = relt.normalized();

	if (p_shape) {
		p_shape->clear();
		p_shape->push_back(Vector2(0, 0));
		p_shape->push_back(rel * 0.2 + relt);
		p_shape->push_back(rel);
		p_shape->push_back(rel * 0.2 - relt);
	}

	if (p_outline_shape) {
		p_outline_shape->clear();
		p_outline_shape->push_back((-reln - reltn) * bone_outline_width);
		p_outline_shape->push_back((-reln + reltn) * bone_outline_width);
		p_outline_shape->push_back(rel * 0.2 + relt + reltn * bone_outline_width);
		p_outline_shape->push_back(rel + (reln + reltn) * bone_outline_width);
		p_outline_shape->push_back(rel + (reln - reltn) * bone_outline_width);
		p_outline_shape->push_back(rel * 0.2 - relt - reltn * bone_outline_width);
	}
	return true;
}

void Bone2D::_editor_set_show_bone_gizmo(bool p_show_gizmo) {
	_editor_show_bone_gizmo = p_show_gizmo;
	queue_redraw();
}

bool Bone2D::_editor_get_show_bone_gizmo() const {
	return _editor_show_bone_gizmo;
}
#endif // TOOLS_ENABLED

void Bone2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rest", "rest"), &Bone2D::set_rest);
	ClassDB::bind_method(D_METHOD("get_rest"), &Bone2D::get_rest);
	ClassDB::bind_method(D_METHOD("apply_rest"), &Bone2D::apply_rest);
	ClassDB::bind_method(D_METHOD("get_skeleton_rest"), &Bone2D::get_skeleton_rest);
	ClassDB::bind_method(D_METHOD("get_index_in_skeleton"), &Bone2D::get_index_in_skeleton);

	ClassDB::bind_method(D_METHOD("set_autocalculate_length_and_angle", "auto_calculate"), &Bone2D::set_autocalculate_length_and_angle);
	ClassDB::bind_method(D_METHOD("get_autocalculate_length_and_angle"), &Bone2D::get_autocalculate_length_and_angle);
	ClassDB::bind_method(D_METHOD("set_length", "length"), &Bone2D::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &Bone2D::get_length);
	ClassDB::bind_method(D_METHOD("set_bone_angle", "angle"), &Bone2D::set_bone_angle);
	ClassDB::bind_method(D_METHOD("get_bone_angle"), &Bone2D::get_bone_angle);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "rest", PROPERTY_HINT_NONE, "suffix:px"), "set_rest", "get_rest");
}

void Bone2D::set_rest(const Transform2D &p_rest) {
	rest = p_rest;
	if (skeleton) {
		skeleton->_make_bone_setup_dirty();
	}

	update_configuration_warnings();
}

Transform2D Bone2D::get_rest() const {
	return rest;
}

Transform2D Bone2D::get_skeleton_rest() const {
	if (parent_bone) {
		return parent_bone->get_skeleton_rest() * rest;
	} else {
		return rest;
	}
}

void Bone2D::apply_rest() {
	set_transform(rest);
}

int Bone2D::get_index_in_skeleton() const {
	ERR_FAIL_NULL_V(skeleton, -1);
	skeleton->_update_bone_setup();
	return skeleton_index;
}

PackedStringArray Bone2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();
	if (!skeleton) {
		if (parent_bone) {
			warnings.push_back(RTR("This Bone2D chain should end at a Skeleton2D node."));
		} else {
			warnings.push_back(RTR("A Bone2D only works with a Skeleton2D or another Bone2D as parent node."));
		}
	}

	if (rest == Transform2D(0, 0, 0, 0, 0, 0)) {
		warnings.push_back(RTR("This bone lacks a proper REST pose. Go to the Skeleton2D node and set one."));
	}

	return warnings;
}

void Bone2D::calculate_length_and_rotation() {
	// If there is at least a single child Bone2D node, we can calculate
	// the length and direction. We will always just use the first Bone2D for this.
	int child_count = get_child_count();
	Transform2D global_inv = get_global_transform().affine_inverse();

	for (int i = 0; i < child_count; i++) {
		Bone2D *child = Object::cast_to<Bone2D>(get_child(i));
		if (child) {
			Vector2 child_local_pos = global_inv.xform(child->get_global_position());
			length = child_local_pos.length();
			bone_angle = child_local_pos.angle();
			return; // Finished!
		}
	}

	WARN_PRINT("No Bone2D children of node " + get_name() + ". Cannot calculate bone length or angle reliably.\nUsing transform rotation for bone angle.");
	bone_angle = get_transform().get_rotation();
}

void Bone2D::set_autocalculate_length_and_angle(bool p_autocalculate) {
	autocalculate_length_and_angle = p_autocalculate;
	if (autocalculate_length_and_angle) {
		calculate_length_and_rotation();
	}
	notify_property_list_changed();
}

bool Bone2D::get_autocalculate_length_and_angle() const {
	return autocalculate_length_and_angle;
}

void Bone2D::set_length(real_t p_length) {
	length = p_length;

#ifdef TOOLS_ENABLED
	queue_redraw();
#endif // TOOLS_ENABLED
}

real_t Bone2D::get_length() const {
	return length;
}

void Bone2D::set_bone_angle(real_t p_angle) {
	bone_angle = p_angle;

#ifdef TOOLS_ENABLED
	queue_redraw();
#endif // TOOLS_ENABLED
}

real_t Bone2D::get_bone_angle() const {
	return bone_angle;
}

Bone2D::Bone2D() {
	skeleton = nullptr;
	parent_bone = nullptr;
	skeleton_index = -1;
	length = 16;
	bone_angle = 0;
	autocalculate_length_and_angle = true;
	set_notify_local_transform(true);
	set_hide_clip_children(true);
	//this is a clever hack so the bone knows no rest has been set yet, allowing to show an error.
	for (int i = 0; i < 3; i++) {
		rest[i] = Vector2(0, 0);
	}
	copy_transform_to_cache = true;
}

Bone2D::~Bone2D() {
#ifdef TOOLS_ENABLED
	if (!editor_gizmo_rid.is_null()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(editor_gizmo_rid);
	}
#endif // TOOLS_ENABLED
}

//////////////////////////////////////

bool Skeleton2D::_set(const StringName &p_path, const Variant &p_value) {
	if (p_path == SNAME("modification_stack")) {
		set_modification_stack(p_value);
		return true;
	}
	return false;
}

bool Skeleton2D::_get(const StringName &p_path, Variant &r_ret) const {
	if (p_path == SNAME("modification_stack")) {
		r_ret = get_modification_stack();
		return true;
	}
	return false;
}

void Skeleton2D::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(
			PropertyInfo(Variant::OBJECT, PNAME("modification_stack"),
					PROPERTY_HINT_RESOURCE_TYPE,
					"SkeletonModificationStack2D",
					PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ALWAYS_DUPLICATE));
}

void Skeleton2D::_make_bone_setup_dirty() {
	if (bone_setup_dirty) {
		return;
	}
	bone_setup_dirty = true;
	if (is_inside_tree()) {
		callable_mp(this, &Skeleton2D::_update_bone_setup).call_deferred();
	}
}

void Skeleton2D::_update_bone_setup() {
	if (!bone_setup_dirty) {
		return;
	}

	bone_setup_dirty = false;
	RS::get_singleton()->skeleton_allocate_data(skeleton, bones.size(), true);

	bones.sort(); //sorting so that they are always in the same order/index

	for (uint32_t i = 0; i < bones.size(); i++) {
		bones[i].rest_inverse = bones[i].bone->get_skeleton_rest().affine_inverse(); //bind pose
		bones[i].bone->skeleton_index = i;
		Bone2D *parent_bone = Object::cast_to<Bone2D>(bones[i].bone->get_parent());
		if (parent_bone) {
			bones[i].parent_index = parent_bone->skeleton_index;
		} else {
			bones[i].parent_index = -1;
		}

		bones[i].local_pose_override = bones[i].bone->get_skeleton_rest();
	}

	transform_dirty = true;
	_update_transform();
	emit_signal(SNAME("bone_setup_changed"));
}

void Skeleton2D::_make_transform_dirty() {
	if (transform_dirty) {
		return;
	}
	transform_dirty = true;
	if (is_inside_tree()) {
		callable_mp(this, &Skeleton2D::_update_transform).call_deferred();
	}
}

void Skeleton2D::_update_transform() {
	if (bone_setup_dirty) {
		_update_bone_setup();
		return; //above will update transform anyway
	}
	if (!transform_dirty) {
		return;
	}

	transform_dirty = false;

	for (uint32_t i = 0; i < bones.size(); i++) {
		ERR_CONTINUE(bones[i].parent_index >= (int)i);
		if (bones[i].parent_index >= 0) {
			bones[i].accum_transform = bones[bones[i].parent_index].accum_transform * bones[i].bone->get_transform();
		} else {
			bones[i].accum_transform = bones[i].bone->get_transform();
		}
	}

	for (uint32_t i = 0; i < bones.size(); i++) {
		Transform2D final_xform = bones[i].accum_transform * bones[i].rest_inverse;
		RS::get_singleton()->skeleton_bone_set_transform_2d(skeleton, i, final_xform);
	}
}

int Skeleton2D::get_bone_count() const {
	ERR_FAIL_COND_V(!is_inside_tree(), 0);

	if (bone_setup_dirty) {
		// TODO: Is this necessary? It doesn't seem to change bones.size()
		const_cast<Skeleton2D *>(this)->_update_bone_setup();
	}

	return bones.size();
}

Bone2D *Skeleton2D::get_bone(int p_idx) {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);
	ERR_FAIL_INDEX_V(p_idx, (int)bones.size(), nullptr);

	return bones[p_idx].bone;
}

void Skeleton2D::_update_process_mode() {
	bool process = modification_stack.is_valid() && is_inside_tree();
	if (!process) {
		// We might have another reason to process.
		process = is_physics_interpolated_and_enabled() && is_visible_in_tree();
	}

	set_process_internal(process);
	set_physics_process_internal(process);
}

void Skeleton2D::_ensure_update_interpolation_data() {
	uint64_t tick = Engine::get_singleton()->get_physics_frames();

	if (_interpolation_data.last_update_physics_tick != tick) {
		_interpolation_data.xform_prev = _interpolation_data.xform_curr;
		_interpolation_data.last_update_physics_tick = tick;
	}
}

void Skeleton2D::_physics_interpolated_changed() {
	_update_process_mode();
}

void Skeleton2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (bone_setup_dirty) {
				_update_bone_setup();
			}
			if (transform_dirty) {
				_update_transform();
			}
			request_ready();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_update_process_mode();

			if (is_physics_interpolated_and_enabled()) {
				_interpolation_data.xform_curr = get_global_transform();
				_interpolation_data.xform_prev = _interpolation_data.xform_curr;
			}
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (is_physics_interpolated_and_enabled()) {
				_ensure_update_interpolation_data();
				if (Engine::get_singleton()->is_in_physics_frame()) {
					_interpolation_data.xform_curr = get_global_transform();
				}
			} else {
				RS::get_singleton()->skeleton_set_base_transform_2d(skeleton, get_global_transform());
			}
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			_interpolation_data.xform_curr = get_global_transform();
			_interpolation_data.xform_prev = _interpolation_data.xform_curr;
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (is_physics_interpolated_and_enabled()) {
				Transform2D res;
				TransformInterpolator::interpolate_transform_2d(_interpolation_data.xform_prev, _interpolation_data.xform_curr, res, Engine::get_singleton()->get_physics_interpolation_fraction());
				RS::get_singleton()->skeleton_set_base_transform_2d(skeleton, res);
			}
			if (modification_stack.is_valid()) {
				execute_modifications(get_process_delta_time(), SkeletonModificationStack2D::EXECUTION_MODE::execution_mode_process);
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_physics_interpolated_and_enabled()) {
				_ensure_update_interpolation_data();
				_interpolation_data.xform_curr = get_global_transform();
			}
			if (modification_stack.is_valid()) {
				execute_modifications(get_physics_process_delta_time(), SkeletonModificationStack2D::EXECUTION_MODE::execution_mode_physics_process);
			}
		} break;

		case NOTIFICATION_POST_ENTER_TREE: {
			set_modification_stack(modification_stack);
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			_update_process_mode();
		} break;

#ifdef TOOLS_ENABLED
		case NOTIFICATION_DRAW: {
			if (Engine::get_singleton()->is_editor_hint()) {
				if (modification_stack.is_valid()) {
					modification_stack->draw_editor_gizmos();
				}
			}
		} break;
#endif // TOOLS_ENABLED
	}
}

RID Skeleton2D::get_skeleton() const {
	return skeleton;
}

void Skeleton2D::set_bone_local_pose_override(int p_bone_idx, Transform2D p_override, real_t p_amount, bool p_persistent) {
	ERR_FAIL_INDEX_MSG(p_bone_idx, (int)bones.size(), "Bone index is out of range!");
	bones[p_bone_idx].local_pose_override = p_override;
	bones[p_bone_idx].local_pose_override_amount = p_amount;
	bones[p_bone_idx].local_pose_override_persistent = p_persistent;
}

Transform2D Skeleton2D::get_bone_local_pose_override(int p_bone_idx) {
	ERR_FAIL_INDEX_V_MSG(p_bone_idx, (int)bones.size(), Transform2D(), "Bone index is out of range!");
	return bones[p_bone_idx].local_pose_override;
}

void Skeleton2D::set_modification_stack(Ref<SkeletonModificationStack2D> p_stack) {
	if (modification_stack.is_valid()) {
		modification_stack->is_setup = false;
		modification_stack->set_skeleton(nullptr);
	}
	modification_stack = p_stack;
	if (modification_stack.is_valid() && is_inside_tree()) {
		modification_stack->set_skeleton(this);
		modification_stack->setup();

#ifdef TOOLS_ENABLED
		modification_stack->set_editor_gizmos_dirty(true);
#endif // TOOLS_ENABLED
	}
	_update_process_mode();
}

Ref<SkeletonModificationStack2D> Skeleton2D::get_modification_stack() const {
	return modification_stack;
}

void Skeleton2D::execute_modifications(real_t p_delta, int p_execution_mode) {
	if (!modification_stack.is_valid()) {
		return;
	}

	// Do not cache the transform changes caused by the modifications!
	for (uint32_t i = 0; i < bones.size(); i++) {
		bones[i].bone->copy_transform_to_cache = false;
	}

	if (modification_stack->skeleton != this) {
		modification_stack->set_skeleton(this);
	}

	modification_stack->execute(p_delta, p_execution_mode);

	// Only apply the local pose override on _process. Otherwise, just calculate the local_pose_override and reset the transform.
	if (p_execution_mode == SkeletonModificationStack2D::EXECUTION_MODE::execution_mode_process) {
		for (uint32_t i = 0; i < bones.size(); i++) {
			if (bones[i].local_pose_override_amount > 0) {
				bones[i].bone->set_meta("_local_pose_override_enabled_", true);

				Transform2D final_trans = bones[i].bone->cache_transform;
				final_trans = final_trans.interpolate_with(bones[i].local_pose_override, bones[i].local_pose_override_amount);
				bones[i].bone->set_transform(final_trans);
				bones[i].bone->propagate_call("force_update_transform");

				if (bones[i].local_pose_override_persistent) {
					bones[i].local_pose_override_amount = 0.0;
				}
			} else {
				// TODO: see if there is a way to undo the override without having to resort to setting every bone's transform.
				bones[i].bone->remove_meta("_local_pose_override_enabled_");
				bones[i].bone->set_transform(bones[i].bone->cache_transform);
			}
		}
	}

	// Cache any future transform changes
	for (uint32_t i = 0; i < bones.size(); i++) {
		bones[i].bone->copy_transform_to_cache = true;
	}

#ifdef TOOLS_ENABLED
	modification_stack->set_editor_gizmos_dirty(true);
#endif // TOOLS_ENABLED
}

void Skeleton2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bone_count"), &Skeleton2D::get_bone_count);
	ClassDB::bind_method(D_METHOD("get_bone", "idx"), &Skeleton2D::get_bone);

	ClassDB::bind_method(D_METHOD("get_skeleton"), &Skeleton2D::get_skeleton);

	ClassDB::bind_method(D_METHOD("set_modification_stack", "modification_stack"), &Skeleton2D::set_modification_stack);
	ClassDB::bind_method(D_METHOD("get_modification_stack"), &Skeleton2D::get_modification_stack);
	ClassDB::bind_method(D_METHOD("execute_modifications", "delta", "execution_mode"), &Skeleton2D::execute_modifications);

	ClassDB::bind_method(D_METHOD("set_bone_local_pose_override", "bone_idx", "override_pose", "strength", "persistent"), &Skeleton2D::set_bone_local_pose_override);
	ClassDB::bind_method(D_METHOD("get_bone_local_pose_override", "bone_idx"), &Skeleton2D::get_bone_local_pose_override);

	ADD_SIGNAL(MethodInfo("bone_setup_changed"));
}

Skeleton2D::Skeleton2D() {
	skeleton = RS::get_singleton()->skeleton_create();
	set_notify_transform(true);
	set_hide_clip_children(true);
}

Skeleton2D::~Skeleton2D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(skeleton);
}
