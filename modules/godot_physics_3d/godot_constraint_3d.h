/**************************************************************************/
/*  godot_constraint_3d.h                                                 */
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

class GodotBody3D;
class GodotSoftBody3D;

class GodotConstraint3D {
	GodotBody3D **_body_ptr;
	int _body_count;
	uint64_t island_step;
	int priority;
	bool disabled_collisions_between_bodies;

	RID self;

protected:
	GodotConstraint3D(GodotBody3D **p_body_ptr = nullptr, int p_body_count = 0) {
		_body_ptr = p_body_ptr;
		_body_count = p_body_count;
		island_step = 0;
		priority = 1;
		disabled_collisions_between_bodies = true;
	}

public:
	_FORCE_INLINE_ void set_self(const RID &p_self) { self = p_self; }
	_FORCE_INLINE_ RID get_self() const { return self; }

	_FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
	_FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

	_FORCE_INLINE_ GodotBody3D **get_body_ptr() const { return _body_ptr; }
	_FORCE_INLINE_ int get_body_count() const { return _body_count; }

	virtual GodotSoftBody3D *get_soft_body_ptr(int p_index) const { return nullptr; }
	virtual int get_soft_body_count() const { return 0; }

	_FORCE_INLINE_ void set_priority(int p_priority) { priority = p_priority; }
	_FORCE_INLINE_ int get_priority() const { return priority; }

	_FORCE_INLINE_ void disable_collisions_between_bodies(const bool p_disabled) { disabled_collisions_between_bodies = p_disabled; }
	_FORCE_INLINE_ bool is_disabled_collisions_between_bodies() const { return disabled_collisions_between_bodies; }

	virtual bool setup(real_t p_step) = 0;
	virtual bool pre_solve(real_t p_step) = 0;
	virtual void solve(real_t p_step) = 0;

	virtual ~GodotConstraint3D() {}
};
