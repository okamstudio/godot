/**************************************************************************/
/*  godot_collision_object_3d.cpp                                         */
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

#include "godot_collision_object_3d.h"

#include "godot_physics_server_3d.h"
#include "godot_space_3d.h"

void GodotCollisionObject3D::add_shape(GodotShape3D *p_shape, const Transform3D &p_transform, bool p_disabled) {
	Shape s;
	s.shape = p_shape;
	s.xform = p_transform;
	s.xform_inv = s.xform.affine_inverse();
	s.bpid = 0; //needs update
	s.disabled = p_disabled;
	shapes.push_back(s);
	p_shape->add_owner(this);

	if (!pending_shape_update_list.in_list()) {
		GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void GodotCollisionObject3D::set_shape(int p_index, GodotShape3D *p_shape) {
	ERR_FAIL_INDEX(p_index, shapes.size());
	shapes[p_index].shape->remove_owner(this);
	shapes.write[p_index].shape = p_shape;

	p_shape->add_owner(this);
	if (!pending_shape_update_list.in_list()) {
		GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void GodotCollisionObject3D::set_shape_transform(int p_index, const Transform3D &p_transform) {
	ERR_FAIL_INDEX(p_index, shapes.size());

	shapes.write[p_index].xform = p_transform;
	shapes.write[p_index].xform_inv = p_transform.affine_inverse();
	if (!pending_shape_update_list.in_list()) {
		GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void GodotCollisionObject3D::set_shape_disabled(int p_idx, bool p_disabled) {
	ERR_FAIL_INDEX(p_idx, shapes.size());

	GodotCollisionObject3D::Shape &shape = shapes.write[p_idx];
	if (shape.disabled == p_disabled) {
		return;
	}

	shape.disabled = p_disabled;

	if (!space) {
		return;
	}

	if (p_disabled && shape.bpid != 0) {
		space->get_broadphase()->remove(shape.bpid);
		shape.bpid = 0;
		if (!pending_shape_update_list.in_list()) {
			GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
		}
	} else if (!p_disabled && shape.bpid == 0) {
		if (!pending_shape_update_list.in_list()) {
			GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
		}
	}
}

void GodotCollisionObject3D::remove_shape(GodotShape3D *p_shape) {
	//remove a shape, all the times it appears
	for (int i = 0; i < shapes.size(); i++) {
		if (shapes[i].shape == p_shape) {
			remove_shape(i);
			i--;
		}
	}
}

void GodotCollisionObject3D::remove_shape(int p_index) {
	//remove anything from shape to be erased to end, so subindices don't change
	ERR_FAIL_INDEX(p_index, shapes.size());
	for (int i = p_index; i < shapes.size(); i++) {
		if (shapes[i].bpid == 0) {
			continue;
		}
		//should never get here with a null owner
		space->get_broadphase()->remove(shapes[i].bpid);
		shapes.write[i].bpid = 0;
	}
	shapes[p_index].shape->remove_owner(this);
	shapes.remove_at(p_index);

	if (!pending_shape_update_list.in_list()) {
		GodotPhysicsServer3D::godot_singleton->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void GodotCollisionObject3D::_set_static(bool p_static) {
	if (_static == p_static && _area == false && _dynamic == !_static) {
		return;
	}
	_static = p_static;
	_area = false;
	_dynamic = !_static;

	if (!space) {
		return;
	}
	for (int i = 0; i < get_shape_count(); i++) {
		const Shape &s = shapes[i];
		if (s.bpid > 0) {
			space->get_broadphase()->set_type(s.bpid, _static, _area, _dynamic);
		}
	}
}

void GodotCollisionObject3D::_set_type(bool p_static, bool p_area, bool p_dynamic) {
	if (_static == p_static && _area == p_area && _dynamic == p_dynamic) {
		return;
	}
#ifdef DEV_ENABLED
	ERR_FAIL_COND(p_static && p_dynamic);
#endif // DEV_ENABLED
	_static = p_static;
	_area = p_area;
	_dynamic = p_dynamic;

	if (!space) {
		return;
	}
	for (int i = 0; i < get_shape_count(); i++) {
		const Shape &s = shapes[i];
		if (s.bpid > 0) {
			space->get_broadphase()->set_type(s.bpid, _static, _area, _dynamic);
		}
	}
}

void GodotCollisionObject3D::_unregister_shapes() {
	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.bpid > 0) {
			space->get_broadphase()->remove(s.bpid);
			s.bpid = 0;
		}
	}
}

void GodotCollisionObject3D::_update_shapes() {
	if (!space) {
		return;
	}

	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.disabled) {
			continue;
		}

		//not quite correct, should compute the next matrix..
		AABB shape_aabb = s.shape->get_aabb();
		Transform3D xform = transform * s.xform;
		shape_aabb = xform.xform(shape_aabb);
		shape_aabb.grow_by((s.aabb_cache.size.x + s.aabb_cache.size.y) * 0.5 * 0.05);
		s.aabb_cache = shape_aabb;

		Vector3 scale = xform.get_basis().get_scale();
		s.area_cache = s.shape->get_volume() * scale.x * scale.y * scale.z;

		if (s.bpid == 0) {
			s.bpid = space->get_broadphase()->create(this, i, shape_aabb, _static, _dynamic);
			space->get_broadphase()->set_type(s.bpid, _static, _area, _dynamic);
		}

		space->get_broadphase()->move(s.bpid, shape_aabb);
	}
}

void GodotCollisionObject3D::_update_shapes_with_motion(const Vector3 &p_motion) {
	if (!space) {
		return;
	}

	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.disabled) {
			continue;
		}

		//not quite correct, should compute the next matrix..
		AABB shape_aabb = s.shape->get_aabb();
		Transform3D xform = transform * s.xform;
		shape_aabb = xform.xform(shape_aabb);
		shape_aabb.merge_with(AABB(shape_aabb.position + p_motion, shape_aabb.size)); //use motion
		s.aabb_cache = shape_aabb;

		if (s.bpid == 0) {
			s.bpid = space->get_broadphase()->create(this, i, shape_aabb, _static, _dynamic);
			space->get_broadphase()->set_type(s.bpid, _static, _area, _dynamic);
		}

		space->get_broadphase()->move(s.bpid, shape_aabb);
	}
}

void GodotCollisionObject3D::_set_space(GodotSpace3D *p_space) {
	GodotSpace3D *old_space = space;
	space = p_space;

	if (old_space) {
		old_space->remove_object(this);

		for (int i = 0; i < shapes.size(); i++) {
			Shape &s = shapes.write[i];
			if (s.bpid) {
				old_space->get_broadphase()->remove(s.bpid);
				s.bpid = 0;
			}
		}
	}

	if (space) {
		space->add_object(this);
		_update_shapes();
	}
}

void GodotCollisionObject3D::_shape_changed() {
	_update_shapes();
	_shapes_changed();
}

GodotCollisionObject3D::GodotCollisionObject3D(Type p_type) :
		pending_shape_update_list(this) {
	type = p_type;
}
