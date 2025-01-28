/**************************************************************************/
/*  skeleton_modification_2d.h                                            */
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

#ifndef SKELETON_MODIFICATION_2D_H
#define SKELETON_MODIFICATION_2D_H

#include "scene/resources/2d/skeleton/skeleton_modification_stack_2d.h"

///////////////////////////////////////
// SkeletonModification2D
///////////////////////////////////////

class Bone2D;

class SkeletonModification2D : public Resource {
	GDCLASS(SkeletonModification2D, Resource);
	friend class Skeleton2D;
	friend class Bone2D;

protected:
	static void _bind_methods();

	SkeletonModificationStack2D *stack = nullptr;
	int execution_mode = 0; // 0 = process

	bool enabled = true;
	bool is_setup = false;

	bool _print_execution_error(bool p_condition, String p_message);

	virtual void reset_state() override;

	GDVIRTUAL1(_execute, double)
	GDVIRTUAL1(_setup_modification, Ref<SkeletonModificationStack2D>)
	GDVIRTUAL0(_draw_editor_gizmo)

public:
	virtual void _execute(float _delta);
	virtual void _setup_modification(SkeletonModificationStack2D *p_stack);
	virtual void _draw_editor_gizmo();

	bool editor_draw_gizmo = false;
	void set_editor_draw_gizmo(bool p_draw_gizmo);
	bool get_editor_draw_gizmo() const;

	void set_enabled(bool p_enabled);
	bool get_enabled();

	Ref<SkeletonModificationStack2D> get_modification_stack();
	void set_is_setup(bool p_setup);
	bool get_is_setup() const;

	void set_execution_mode(int p_mode);
	int get_execution_mode() const;

	float clamp_angle(float p_angle, float p_min_bound, float p_max_bound, bool p_invert_clamp = false);
	void editor_draw_angle_constraints(Bone2D *p_operation_bone, float p_min_bound, float p_max_bound, bool p_constraint_enabled, bool p_constraint_in_localspace, bool p_constraint_inverted);

	SkeletonModification2D();
};

#endif // SKELETON_MODIFICATION_2D_H
