/**************************************************************************/
/*  animatable_body_3d.h                                                  */
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

#include "scene/3d/physics/static_body_3d.h"

class AnimatableBody3D : public StaticBody3D {
	GDCLASS(AnimatableBody3D, StaticBody3D);

private:
	Vector3 linear_velocity;
	Vector3 angular_velocity;

	bool sync_to_physics = true;

	Transform3D last_valid_transform;

	static void _body_state_changed_callback(void *p_instance, PhysicsDirectBodyState3D *p_state);
	void _body_state_changed(PhysicsDirectBodyState3D *p_state);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual Vector3 get_linear_velocity() const override;
	virtual Vector3 get_angular_velocity() const override;

	AnimatableBody3D();

private:
	void _update_kinematic_motion();

	void set_sync_to_physics(bool p_enable);
	bool is_sync_to_physics_enabled() const;
};
