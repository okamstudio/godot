/**************************************************************************/
/*  collision_object_2d_sw.cpp                                            */
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

#include "collision_object_2d_sw.h"
#include "servers/physics_2d/physics_2d_server_sw.h"
#include "space_2d_sw.h"

void CollisionObject2DSW::add_shape(Shape2DSW *p_shape, const Transform2D &p_transform, bool p_disabled) {
	Shape s;
	s.shape = p_shape;
	s.xform = p_transform;
	s.xform_inv = s.xform.affine_inverse();
	s.bpid = 0; //needs update
	s.disabled = p_disabled;
	s.one_way_collision = false;
	s.one_way_collision_margin = 0;
	shapes.push_back(s);
	p_shape->add_owner(this);

	if (!pending_shape_update_list.in_list()) {
		Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void CollisionObject2DSW::set_shape(int p_index, Shape2DSW *p_shape) {
	ERR_FAIL_INDEX(p_index, shapes.size());
	shapes[p_index].shape->remove_owner(this);
	shapes.write[p_index].shape = p_shape;

	p_shape->add_owner(this);

	if (!pending_shape_update_list.in_list()) {
		Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void CollisionObject2DSW::set_shape_metadata(int p_index, const Variant &p_metadata) {
	ERR_FAIL_INDEX(p_index, shapes.size());
	shapes.write[p_index].metadata = p_metadata;
}

void CollisionObject2DSW::set_shape_transform(int p_index, const Transform2D &p_transform) {
	ERR_FAIL_INDEX(p_index, shapes.size());

	shapes.write[p_index].xform = p_transform;
	shapes.write[p_index].xform_inv = p_transform.affine_inverse();

	if (!pending_shape_update_list.in_list()) {
		Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void CollisionObject2DSW::set_shape_disabled(int p_idx, bool p_disabled) {
	ERR_FAIL_INDEX(p_idx, shapes.size());

	CollisionObject2DSW::Shape &shape = shapes.write[p_idx];
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
			Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
		}
	} else if (!p_disabled && shape.bpid == 0) {
		if (!pending_shape_update_list.in_list()) {
			Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
		}
	}
}

void CollisionObject2DSW::remove_shape(Shape2DSW *p_shape) {
	//remove a shape, all the times it appears
	for (int i = 0; i < shapes.size(); i++) {
		if (shapes[i].shape == p_shape) {
			remove_shape(i);
			i--;
		}
	}
}

void CollisionObject2DSW::remove_shape(int p_index) {
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
	shapes.remove(p_index);

	if (!pending_shape_update_list.in_list()) {
		Physics2DServerSW::singletonsw->pending_shape_update_list.add(&pending_shape_update_list);
	}
}

void CollisionObject2DSW::_set_static(bool p_static) {
	if (_static == p_static) {
		return;
	}
	_static = p_static;

	if (!space) {
		return;
	}
	for (int i = 0; i < get_shape_count(); i++) {
		const Shape &s = shapes[i];
		if (s.bpid > 0) {
			space->get_broadphase()->set_static(s.bpid, _static, get_type());
		}
	}
}

void CollisionObject2DSW::_unregister_shapes() {
	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.bpid > 0) {
			space->get_broadphase()->remove(s.bpid);
			s.bpid = 0;
		}
	}
}

void CollisionObject2DSW::_update_shapes() {
	if (!space) {
		return;
	}

	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.disabled) {
			continue;
		}

		//not quite correct, should compute the next matrix..
		Rect2 shape_aabb = s.shape->get_aabb();
		Transform2D xform = transform * s.xform;
		shape_aabb = xform.xform(shape_aabb);
		shape_aabb.grow_by((s.aabb_cache.size.x + s.aabb_cache.size.y) * 0.5 * 0.05);
		s.aabb_cache = shape_aabb;

		if (s.bpid == 0) {
			s.bpid = space->get_broadphase()->create(this, i, shape_aabb, _static, get_type());
			space->get_broadphase()->set_static(s.bpid, _static, get_type());
		}

		space->get_broadphase()->move(s.bpid, shape_aabb);
	}
}

void CollisionObject2DSW::_recheck_shapes() {
	if (!space) {
		return;
	}

	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.disabled) {
			continue;
		}

		if (s.bpid != 0) {
			space->get_broadphase()->recheck_pairs(s.bpid);
		}
	}
}

void CollisionObject2DSW::_update_shapes_with_motion(const Vector2 &p_motion) {
	if (!space) {
		return;
	}

	for (int i = 0; i < shapes.size(); i++) {
		Shape &s = shapes.write[i];
		if (s.disabled) {
			continue;
		}

		//not quite correct, should compute the next matrix..
		Rect2 shape_aabb = s.shape->get_aabb();
		Transform2D xform = transform * s.xform;
		shape_aabb = xform.xform(shape_aabb);
		shape_aabb = shape_aabb.merge(Rect2(shape_aabb.position + p_motion, shape_aabb.size)); //use motion
		s.aabb_cache = shape_aabb;

		if (s.bpid == 0) {
			s.bpid = space->get_broadphase()->create(this, i, shape_aabb, _static, get_type());
			space->get_broadphase()->set_static(s.bpid, _static, get_type());
		}

		space->get_broadphase()->move(s.bpid, shape_aabb);
	}
}

void CollisionObject2DSW::_set_space(Space2DSW *p_space) {
	if (space) {
		space->remove_object(this);

		for (int i = 0; i < shapes.size(); i++) {
			Shape &s = shapes.write[i];
			if (s.bpid) {
				space->get_broadphase()->remove(s.bpid);
				s.bpid = 0;
			}
		}
	}

	space = p_space;

	if (space) {
		space->add_object(this);
		_update_shapes();
	}
}

void CollisionObject2DSW::_shape_changed() {
	_update_shapes();
	_shapes_changed();
}

CollisionObject2DSW::CollisionObject2DSW(Type p_type) :
		pending_shape_update_list(this) {
	_static = true;
	type = p_type;
	space = nullptr;
	instance_id = 0;
	canvas_instance_id = 0;
	collision_mask = 1;
	collision_layer = 1;
	pickable = true;
}
