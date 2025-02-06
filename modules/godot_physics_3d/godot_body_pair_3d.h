/**************************************************************************/
/*  godot_body_pair_3d.h                                                  */
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

#include "godot_body_3d.h"
#include "godot_constraint_3d.h"
#include "godot_soft_body_3d.h"

#include "core/templates/local_vector.h"

class GodotBodyContact3D : public GodotConstraint3D {
protected:
	struct Contact {
		Vector3 position;
		Vector3 normal;
		int index_A = 0, index_B = 0;
		Vector3 local_A, local_B;
		Vector3 acc_impulse; // accumulated impulse - only one of the object's impulse is needed as impulse_a == -impulse_b
		real_t acc_normal_impulse = 0.0; // accumulated normal impulse (Pn)
		Vector3 acc_tangent_impulse; // accumulated tangent impulse (Pt)
		real_t acc_bias_impulse = 0.0; // accumulated normal impulse for position bias (Pnb)
		real_t acc_bias_impulse_center_of_mass = 0.0; // accumulated normal impulse for position bias applied to com
		real_t mass_normal = 0.0;
		real_t bias = 0.0;
		real_t bounce = 0.0;

		real_t depth = 0.0;
		bool active = false;
		bool used = false;
		Vector3 rA, rB; // Offset in world orientation with respect to center of mass
	};

	Vector3 sep_axis;
	bool collided = false;
	bool check_ccd = false;

	GodotSpace3D *space = nullptr;

	GodotBodyContact3D(GodotBody3D **p_body_ptr = nullptr, int p_body_count = 0) :
			GodotConstraint3D(p_body_ptr, p_body_count) {
	}
};

class GodotBodyPair3D : public GodotBodyContact3D {
	enum {
		MAX_CONTACTS = 4
	};

	union {
		struct {
			GodotBody3D *A;
			GodotBody3D *B;
		};

		GodotBody3D *_arr[2] = { nullptr, nullptr };
	};

	int shape_A = 0;
	int shape_B = 0;

	bool collide_A = false;
	bool collide_B = false;

	bool report_contacts_only = false;

	Vector3 offset_B; //use local A coordinates to avoid numerical issues on collision detection

	Contact contacts[MAX_CONTACTS];
	int contact_count = 0;

	static void _contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata);

	void contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal);

	void validate_contacts();
	bool _test_ccd(real_t p_step, GodotBody3D *p_A, int p_shape_A, const Transform3D &p_xform_A, GodotBody3D *p_B, int p_shape_B, const Transform3D &p_xform_B);

public:
	virtual bool setup(real_t p_step) override;
	virtual bool pre_solve(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	GodotBodyPair3D(GodotBody3D *p_A, int p_shape_A, GodotBody3D *p_B, int p_shape_B);
	~GodotBodyPair3D();
};

class GodotBodySoftBodyPair3D : public GodotBodyContact3D {
	GodotBody3D *body = nullptr;
	GodotSoftBody3D *soft_body = nullptr;

	int body_shape = 0;

	bool body_collides = false;
	bool soft_body_collides = false;

	bool report_contacts_only = false;

	LocalVector<Contact> contacts;

	static void _contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata);

	void contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal);

	void validate_contacts();

public:
	virtual bool setup(real_t p_step) override;
	virtual bool pre_solve(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	virtual GodotSoftBody3D *get_soft_body_ptr(int p_index) const override { return soft_body; }
	virtual int get_soft_body_count() const override { return 1; }

	GodotBodySoftBodyPair3D(GodotBody3D *p_A, int p_shape_A, GodotSoftBody3D *p_B);
	~GodotBodySoftBodyPair3D();
};
