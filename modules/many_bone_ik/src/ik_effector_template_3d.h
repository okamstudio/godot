/**************************************************************************/
/*  ik_effector_template_3d.h                                             */
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

#ifndef IK_EFFECTOR_TEMPLATE_3D_H
#define IK_EFFECTOR_TEMPLATE_3D_H

#include "core/io/resource.h"
#include "core/string/node_path.h"

class IKEffectorTemplate3D : public Resource {
	GDCLASS(IKEffectorTemplate3D, Resource);

	StringName root_bone;
	NodePath target_node;
	bool target_static = false;
	real_t motion_propagation_factor = 1.0f;
	real_t weight = 0.0f;
	Vector3 priority_direction = Vector3(0.2f, 0.0f, 0.2f); // Purported ideal values are 1.0 / 3.0 for one direction, 1.0 / 5.0 for two directions and 1.0 / 7.0 for three directions.
protected:
	static void _bind_methods();

public:
	String get_root_bone() const;
	void set_root_bone(String p_root_bone);
	NodePath get_target_node() const;
	void set_target_node(NodePath p_node_path);
	float get_motion_propagation_factor() const;
	void set_motion_propagation_factor(float p_motion_propagation_factor);
	real_t get_weight() const { return weight; }
	void set_weight(real_t p_weight) { weight = p_weight; }
	Vector3 get_direction_priorities() const { return priority_direction; }
	void set_direction_priorities(Vector3 p_priority_direction) { priority_direction = p_priority_direction; }

	IKEffectorTemplate3D();
};

#endif // IK_EFFECTOR_TEMPLATE_3D_H
