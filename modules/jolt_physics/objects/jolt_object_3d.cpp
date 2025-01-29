/**************************************************************************/
/*  jolt_object_3d.cpp                                                    */
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

#include "jolt_object_3d.h"

#include "../jolt_project_settings.h"
#include "../spaces/jolt_layers.h"
#include "../spaces/jolt_space_3d.h"
#include "jolt_group_filter.h"

void JoltObject3D::_remove_from_space() {
	if (unlikely(jolt_id.IsInvalid())) {
		return;
	}

	space->remove_body(jolt_id);

	jolt_id = JPH::BodyID();
}

void JoltObject3D::_reset_space() {
	ERR_FAIL_NULL(space);

	_space_changing();
	_remove_from_space();
	_add_to_space();
	_space_changed();
}

void JoltObject3D::_update_object_layer() {
	if (!in_space()) {
		return;
	}

	space->get_body_iface().SetObjectLayer(jolt_id, _get_object_layer());
}

void JoltObject3D::_collision_layer_changed() {
	_update_object_layer();
}

void JoltObject3D::_collision_mask_changed() {
	_update_object_layer();
}

JoltObject3D::JoltObject3D(ObjectType p_object_type) :
		object_type(p_object_type) {
}

JoltObject3D::~JoltObject3D() = default;

Object *JoltObject3D::get_instance() const {
	return ObjectDB::get_instance(instance_id);
}

void JoltObject3D::set_space(JoltSpace3D *p_space) {
	if (space == p_space) {
		return;
	}

	_space_changing();

	if (space != nullptr) {
		_remove_from_space();
	}

	space = p_space;

	if (space != nullptr) {
		_add_to_space();
	}

	_space_changed();
}

void JoltObject3D::set_collision_layer(uint32_t p_layer) {
	if (p_layer == collision_layer) {
		return;
	}

	collision_layer = p_layer;

	_collision_layer_changed();
}

void JoltObject3D::set_collision_mask(uint32_t p_mask) {
	if (p_mask == collision_mask) {
		return;
	}

	collision_mask = p_mask;

	_collision_mask_changed();
}

bool JoltObject3D::can_collide_with(const JoltObject3D &p_other) const {
	return (collision_mask & p_other.get_collision_layer()) != 0;
}

bool JoltObject3D::can_interact_with(const JoltObject3D &p_other) const {
	if (const JoltBody3D *other_body = p_other.as_body()) {
		return can_interact_with(*other_body);
	} else if (const JoltArea3D *other_area = p_other.as_area()) {
		return can_interact_with(*other_area);
	} else if (const JoltSoftBody3D *other_soft_body = p_other.as_soft_body()) {
		return can_interact_with(*other_soft_body);
	} else {
		ERR_FAIL_V_MSG(false, vformat("Unhandled object type: '%d'. This should not happen. Please report this.", p_other.get_type()));
	}
}

String JoltObject3D::to_string() const {
	Object *instance = get_instance();
	return instance != nullptr ? instance->to_string() : "<unknown>";
}
