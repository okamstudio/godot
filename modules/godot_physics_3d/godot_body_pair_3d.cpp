/**************************************************************************/
/*  godot_body_pair_3d.cpp                                                */
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

#include "godot_body_pair_3d.h"

#include "godot_collision_solver_3d.h"
#include "godot_space_3d.h"

#define MIN_VELOCITY 0.0001
#define MAX_BIAS_ROTATION (Math_PI / 8)

void GodotBodyPair3D::_contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata) {
	GodotBodyPair3D *pair = static_cast<GodotBodyPair3D *>(p_userdata);
	pair->contact_added_callback(p_point_A, p_index_A, p_point_B, p_index_B, normal);
}

void GodotBodyPair3D::contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal) {
	Vector3 local_A = A->get_inv_transform().basis.xform(p_point_A);
	Vector3 local_B = B->get_inv_transform().basis.xform(p_point_B - offset_B);

	int new_index = contact_count;

	ERR_FAIL_COND(new_index >= (MAX_CONTACTS + 1));

	Contact contact;
	contact.index_A = p_index_A;
	contact.index_B = p_index_B;
	contact.local_A = local_A;
	contact.local_B = local_B;
	contact.normal = (p_point_A - p_point_B).normalized();
	contact.used = true;

	// Attempt to determine if the contact will be reused.
	real_t contact_recycle_radius = space->get_contact_recycle_radius();

	for (int i = 0; i < contact_count; i++) {
		Contact &c = contacts[i];
		if (c.local_A.distance_squared_to(local_A) < (contact_recycle_radius * contact_recycle_radius) &&
				c.local_B.distance_squared_to(local_B) < (contact_recycle_radius * contact_recycle_radius)) {
			contact.acc_normal_impulse = c.acc_normal_impulse;
			contact.acc_bias_impulse = c.acc_bias_impulse;
			contact.acc_bias_impulse_center_of_mass = c.acc_bias_impulse_center_of_mass;
			contact.acc_tangent_impulse = c.acc_tangent_impulse;
			c = contact;
			return;
		}
	}

	// Figure out if the contact amount must be reduced to fit the new contact.
	if (new_index == MAX_CONTACTS) {
		// Remove the contact with the minimum depth.

		const Basis &basis_A = A->get_transform().basis;
		const Basis &basis_B = B->get_transform().basis;

		int least_deep = -1;
		real_t min_depth;

		// Start with depth for new contact.
		{
			Vector3 global_A = basis_A.xform(contact.local_A);
			Vector3 global_B = basis_B.xform(contact.local_B) + offset_B;

			Vector3 axis = global_A - global_B;
			min_depth = axis.dot(contact.normal);
		}

		for (int i = 0; i < contact_count; i++) {
			const Contact &c = contacts[i];
			Vector3 global_A = basis_A.xform(c.local_A);
			Vector3 global_B = basis_B.xform(c.local_B) + offset_B;

			Vector3 axis = global_A - global_B;
			real_t depth = axis.dot(c.normal);

			if (depth < min_depth) {
				min_depth = depth;
				least_deep = i;
			}
		}

		if (least_deep > -1) {
			// Replace the least deep contact by the new one.
			contacts[least_deep] = contact;
		}

		return;
	}

	contacts[new_index] = contact;
	contact_count++;
}

void GodotBodyPair3D::validate_contacts() {
	// Make sure to erase contacts that are no longer valid.
	real_t max_separation = space->get_contact_max_separation();
	real_t max_separation2 = max_separation * max_separation;

	const Basis &basis_A = A->get_transform().basis;
	const Basis &basis_B = B->get_transform().basis;

	for (int i = 0; i < contact_count; i++) {
		Contact &c = contacts[i];

		bool erase = false;
		if (!c.used) {
			// Was left behind in previous frame.
			erase = true;
		} else {
			c.used = false;

			Vector3 global_A = basis_A.xform(c.local_A);
			Vector3 global_B = basis_B.xform(c.local_B) + offset_B;
			Vector3 axis = global_A - global_B;
			real_t depth = axis.dot(c.normal);

			if (depth < -max_separation || (global_B + c.normal * depth - global_A).length_squared() > max_separation2) {
				erase = true;
			}
		}

		if (erase) {
			// Contact no longer needed, remove.
			if ((i + 1) < contact_count) {
				// Swap with the last one.
				SWAP(contacts[i], contacts[contact_count - 1]);
			}

			i--;
			contact_count--;
		}
	}
}

// `_test_ccd` prevents tunneling by slowing down a high velocity body that is about to collide so
// that next frame it will be at an appropriate location to collide (i.e. slight overlap).
// WARNING: The way velocity is adjusted down to cause a collision means the momentum will be
// weaker than it should for a bounce!
// Process: Only proceed if body A's motion is high relative to its size.
// Cast forward along motion vector to see if A is going to enter/pass B's collider next frame, only proceed if it does.
// Adjust the velocity of A down so that it will just slightly intersect the collider instead of blowing right past it.
bool GodotBodyPair3D::_test_ccd(real_t p_step, GodotBody3D *p_A, int p_shape_A, const Transform3D &p_xform_A, GodotBody3D *p_B, int p_shape_B, const Transform3D &p_xform_B) {
	GodotShape3D *shape_A_ptr = p_A->get_shape(p_shape_A);

	Vector3 motion = p_A->get_linear_velocity() * p_step;
	real_t mlen = motion.length();
	if (mlen < CMP_EPSILON) {
		return false;
	}

	Vector3 mnormal = motion / mlen;

	real_t min = 0.0, max = 0.0;
	shape_A_ptr->project_range(mnormal, p_xform_A, min, max);

	// Did it move enough in this direction to even attempt raycast?
	// Let's say it should move more than 1/3 the size of the object in that axis.
	bool fast_object = mlen > (max - min) * 0.3;
	if (!fast_object) {
		return false; // moving slow enough that there's no chance of tunneling.
	}

	// A is moving fast enough that tunneling might occur. See if it's really about to collide.

	// Roughly predict body B's position in the next frame (ignoring collisions).
	Transform3D predicted_xform_B = p_xform_B.translated(p_B->get_linear_velocity() * p_step);

	// Support points are the farthest forward points on A in the direction of the motion vector.
	// i.e. the candidate points of which one should hit B first if any collision does occur.
	static const int max_supports = 16;
	Vector3 supports_A[max_supports];
	int support_count_A;
	GodotShape3D::FeatureType support_type_A;
	// Convert mnormal into body A's local xform because get_supports requires (and returns) local coordinates.
	shape_A_ptr->get_supports(p_xform_A.basis.xform_inv(mnormal).normalized(), max_supports, supports_A, support_count_A, support_type_A);

	// Cast a segment from each support point of A in the motion direction.
	int segment_support_idx = -1;
	float segment_hit_length = FLT_MAX;
	Vector3 segment_hit_local;
	for (int i = 0; i < support_count_A; i++) {
		supports_A[i] = p_xform_A.xform(supports_A[i]);

		Vector3 from = supports_A[i];
		Vector3 to = from + motion;

		Transform3D from_inv = predicted_xform_B.affine_inverse();

		// Back up 10% of the per-frame motion behind the support point and use that as the beginning of our cast.
		// At high speeds, this may mean we're actually casting from well behind the body instead of inside it, which is odd.
		// But it still works out.
		Vector3 local_from = from_inv.xform(from - motion * 0.1);
		Vector3 local_to = from_inv.xform(to);

		Vector3 rpos, rnorm;
		int fi = -1;
		if (p_B->get_shape(p_shape_B)->intersect_segment(local_from, local_to, rpos, rnorm, fi, true)) {
			float hit_length = local_from.distance_to(rpos);
			if (hit_length < segment_hit_length) {
				segment_support_idx = i;
				segment_hit_length = hit_length;
				segment_hit_local = rpos;
			}
		}
	}

	if (segment_support_idx == -1) {
		// There was no hit. Since the segment is the length of per-frame motion, this means the bodies will not
		// actually collide yet on next frame. We'll probably check again next frame once they're closer.
		return false;
	}

	Vector3 hitpos = predicted_xform_B.xform(segment_hit_local);

	real_t newlen = hitpos.distance_to(supports_A[segment_support_idx]);
	// Adding 1% of body length to the distance between collision and support point
	// should cause body A's support point to arrive just within B's collider next frame.
	newlen += (max - min) * 0.01;
	// FIXME: This doesn't always work well when colliding with a triangle face of a trimesh shape.

	p_A->set_linear_velocity((mnormal * newlen) / p_step);

	return true;
}

real_t combine_bounce(GodotBody3D *A, GodotBody3D *B) {
	return CLAMP(A->get_bounce() + B->get_bounce(), 0, 1);
}

real_t combine_friction(GodotBody3D *A, GodotBody3D *B) {
	return ABS(MIN(A->get_friction(), B->get_friction()));
}

bool GodotBodyPair3D::setup(real_t p_step) {
	check_ccd = false;

	if (!A->interacts_with(B) || A->has_exception(B->get_self()) || B->has_exception(A->get_self())) {
		collided = false;
		return false;
	}

	collide_A = (A->get_mode() > PhysicsServer3D::BODY_MODE_KINEMATIC) && A->collides_with(B);
	collide_B = (B->get_mode() > PhysicsServer3D::BODY_MODE_KINEMATIC) && B->collides_with(A);

	report_contacts_only = false;
	if (!collide_A && !collide_B) {
		if ((A->get_max_contacts_reported() > 0) || (B->get_max_contacts_reported() > 0)) {
			report_contacts_only = true;
		} else {
			collided = false;
			return false;
		}
	}

	offset_B = B->get_transform().get_origin() - A->get_transform().get_origin();

	validate_contacts();

	const Vector3 &offset_A = A->get_transform().get_origin();
	Transform3D xform_Au = Transform3D(A->get_transform().basis, Vector3());
	Transform3D xform_A = xform_Au * A->get_shape_transform(shape_A);

	Transform3D xform_Bu = B->get_transform();
	xform_Bu.origin -= offset_A;
	Transform3D xform_B = xform_Bu * B->get_shape_transform(shape_B);

	GodotShape3D *shape_A_ptr = A->get_shape(shape_A);
	GodotShape3D *shape_B_ptr = B->get_shape(shape_B);

	collided = GodotCollisionSolver3D::solve_static(shape_A_ptr, xform_A, shape_B_ptr, xform_B, _contact_added_callback, this, &sep_axis);

	if (!collided) {
		if (A->is_continuous_collision_detection_enabled() && collide_A) {
			check_ccd = true;
			return true;
		}

		if (B->is_continuous_collision_detection_enabled() && collide_B) {
			check_ccd = true;
			return true;
		}

		return false;
	}

	return true;
}

bool GodotBodyPair3D::pre_solve(real_t p_step) {
	if (!collided) {
		if (check_ccd) {
			const Vector3 &offset_A = A->get_transform().get_origin();
			Transform3D xform_Au = Transform3D(A->get_transform().basis, Vector3());
			Transform3D xform_A = xform_Au * A->get_shape_transform(shape_A);

			Transform3D xform_Bu = B->get_transform();
			xform_Bu.origin -= offset_A;
			Transform3D xform_B = xform_Bu * B->get_shape_transform(shape_B);

			if (A->is_continuous_collision_detection_enabled() && collide_A) {
				_test_ccd(p_step, A, shape_A, xform_A, B, shape_B, xform_B);
			}

			if (B->is_continuous_collision_detection_enabled() && collide_B) {
				_test_ccd(p_step, B, shape_B, xform_B, A, shape_A, xform_A);
			}
		}

		return false;
	}

	real_t max_penetration = space->get_contact_max_allowed_penetration();

	real_t bias = 0.8;

	GodotShape3D *shape_A_ptr = A->get_shape(shape_A);
	GodotShape3D *shape_B_ptr = B->get_shape(shape_B);

	if (shape_A_ptr->get_custom_bias() || shape_B_ptr->get_custom_bias()) {
		if (shape_A_ptr->get_custom_bias() == 0) {
			bias = shape_B_ptr->get_custom_bias();
		} else if (shape_B_ptr->get_custom_bias() == 0) {
			bias = shape_A_ptr->get_custom_bias();
		} else {
			bias = (shape_B_ptr->get_custom_bias() + shape_A_ptr->get_custom_bias()) * 0.5;
		}
	}

	real_t inv_dt = 1.0 / p_step;

	bool do_process = false;

	const Vector3 &offset_A = A->get_transform().get_origin();

	const Basis &basis_A = A->get_transform().basis;
	const Basis &basis_B = B->get_transform().basis;

	Basis zero_basis;
	zero_basis.set_zero();

	const Basis &inv_inertia_tensor_A = collide_A ? A->get_inv_inertia_tensor() : zero_basis;
	const Basis &inv_inertia_tensor_B = collide_B ? B->get_inv_inertia_tensor() : zero_basis;

	real_t inv_mass_A = collide_A ? A->get_inv_mass() : 0.0;
	real_t inv_mass_B = collide_B ? B->get_inv_mass() : 0.0;

	for (int i = 0; i < contact_count; i++) {
		Contact &c = contacts[i];
		c.active = false;

		Vector3 global_A = basis_A.xform(c.local_A);
		Vector3 global_B = basis_B.xform(c.local_B) + offset_B;

		Vector3 axis = global_A - global_B;
		real_t depth = axis.dot(c.normal);

		if (depth <= 0.0) {
			continue;
		}

#ifdef DEBUG_ENABLED
		if (space->is_debugging_contacts()) {
			space->add_debug_contact(global_A + offset_A);
			space->add_debug_contact(global_B + offset_A);
		}
#endif

		c.rA = global_A - A->get_center_of_mass();
		c.rB = global_B - B->get_center_of_mass() - offset_B;

		// Precompute normal mass, tangent mass, and bias.
		Vector3 inertia_A = inv_inertia_tensor_A.xform(c.rA.cross(c.normal));
		Vector3 inertia_B = inv_inertia_tensor_B.xform(c.rB.cross(c.normal));
		real_t kNormal = inv_mass_A + inv_mass_B;
		kNormal += c.normal.dot(inertia_A.cross(c.rA)) + c.normal.dot(inertia_B.cross(c.rB));
		c.mass_normal = 1.0f / kNormal;

		c.bias = -bias * inv_dt * MIN(0.0f, -depth + max_penetration);
		c.depth = depth;

		Vector3 j_vec = c.normal * c.acc_normal_impulse + c.acc_tangent_impulse;

		c.acc_impulse -= j_vec;

		// contact query reporting...

		if (A->can_report_contacts() || B->can_report_contacts()) {
			Vector3 crB = B->get_angular_velocity().cross(c.rB) + B->get_linear_velocity();
			Vector3 crA = A->get_angular_velocity().cross(c.rA) + A->get_linear_velocity();

			if (A->can_report_contacts()) {
				A->add_contact(global_A + offset_A, -c.normal, depth, shape_A, crA, global_B + offset_A, shape_B, B->get_instance_id(), B->get_self(), crB, c.acc_impulse);
			}

			if (B->can_report_contacts()) {
				B->add_contact(global_B + offset_A, c.normal, depth, shape_B, crB, global_A + offset_A, shape_A, A->get_instance_id(), A->get_self(), crA, -c.acc_impulse);
			}
		}

		if (report_contacts_only) {
			collided = false;
			continue;
		}

		c.active = true;
		do_process = true;

		if (collide_A) {
			A->apply_impulse(-j_vec, c.rA + A->get_center_of_mass());
		}
		if (collide_B) {
			B->apply_impulse(j_vec, c.rB + B->get_center_of_mass());
		}

		c.bounce = combine_bounce(A, B);
		if (c.bounce) {
			Vector3 crA = A->get_prev_angular_velocity().cross(c.rA);
			Vector3 crB = B->get_prev_angular_velocity().cross(c.rB);
			Vector3 dv = B->get_prev_linear_velocity() + crB - A->get_prev_linear_velocity() - crA;
			c.bounce = c.bounce * dv.dot(c.normal);
		}
	}

	return do_process;
}

void GodotBodyPair3D::solve(real_t p_step) {
	if (!collided) {
		return;
	}

	const real_t max_bias_av = MAX_BIAS_ROTATION / p_step;

	Basis zero_basis;
	zero_basis.set_zero();

	const Basis &inv_inertia_tensor_A = collide_A ? A->get_inv_inertia_tensor() : zero_basis;
	const Basis &inv_inertia_tensor_B = collide_B ? B->get_inv_inertia_tensor() : zero_basis;

	real_t inv_mass_A = collide_A ? A->get_inv_mass() : 0.0;
	real_t inv_mass_B = collide_B ? B->get_inv_mass() : 0.0;

	for (int i = 0; i < contact_count; i++) {
		Contact &c = contacts[i];
		if (!c.active) {
			continue;
		}

		c.active = false; //try to deactivate, will activate itself if still needed

		//bias impulse

		Vector3 crbA = A->get_biased_angular_velocity().cross(c.rA);
		Vector3 crbB = B->get_biased_angular_velocity().cross(c.rB);
		Vector3 dbv = B->get_biased_linear_velocity() + crbB - A->get_biased_linear_velocity() - crbA;

		real_t vbn = dbv.dot(c.normal);

		if (Math::abs(-vbn + c.bias) > MIN_VELOCITY) {
			real_t jbn = (-vbn + c.bias) * c.mass_normal;
			real_t jbnOld = c.acc_bias_impulse;
			c.acc_bias_impulse = MAX(jbnOld + jbn, 0.0f);

			Vector3 jb = c.normal * (c.acc_bias_impulse - jbnOld);

			if (collide_A) {
				A->apply_bias_impulse(-jb, c.rA + A->get_center_of_mass(), max_bias_av);
			}
			if (collide_B) {
				B->apply_bias_impulse(jb, c.rB + B->get_center_of_mass(), max_bias_av);
			}

			crbA = A->get_biased_angular_velocity().cross(c.rA);
			crbB = B->get_biased_angular_velocity().cross(c.rB);
			dbv = B->get_biased_linear_velocity() + crbB - A->get_biased_linear_velocity() - crbA;

			vbn = dbv.dot(c.normal);

			if (Math::abs(-vbn + c.bias) > MIN_VELOCITY) {
				real_t jbn_com = (-vbn + c.bias) / (inv_mass_A + inv_mass_B);
				real_t jbnOld_com = c.acc_bias_impulse_center_of_mass;
				c.acc_bias_impulse_center_of_mass = MAX(jbnOld_com + jbn_com, 0.0f);

				Vector3 jb_com = c.normal * (c.acc_bias_impulse_center_of_mass - jbnOld_com);

				if (collide_A) {
					A->apply_bias_impulse(-jb_com, A->get_center_of_mass(), 0.0f);
				}
				if (collide_B) {
					B->apply_bias_impulse(jb_com, B->get_center_of_mass(), 0.0f);
				}
			}

			c.active = true;
		}

		Vector3 crA = A->get_angular_velocity().cross(c.rA);
		Vector3 crB = B->get_angular_velocity().cross(c.rB);
		Vector3 dv = B->get_linear_velocity() + crB - A->get_linear_velocity() - crA;

		//normal impulse
		real_t vn = dv.dot(c.normal);

		if (Math::abs(vn) > MIN_VELOCITY) {
			real_t jn = -(c.bounce + vn) * c.mass_normal;
			real_t jnOld = c.acc_normal_impulse;
			c.acc_normal_impulse = MAX(jnOld + jn, 0.0f);

			Vector3 j = c.normal * (c.acc_normal_impulse - jnOld);

			if (collide_A) {
				A->apply_impulse(-j, c.rA + A->get_center_of_mass());
			}
			if (collide_B) {
				B->apply_impulse(j, c.rB + B->get_center_of_mass());
			}
			c.acc_impulse -= j;

			c.active = true;
		}

		//friction impulse

		real_t friction = combine_friction(A, B);

		Vector3 lvA = A->get_linear_velocity() + A->get_angular_velocity().cross(c.rA);
		Vector3 lvB = B->get_linear_velocity() + B->get_angular_velocity().cross(c.rB);

		Vector3 dtv = lvB - lvA;
		real_t tn = c.normal.dot(dtv);

		// tangential velocity
		Vector3 tv = dtv - c.normal * tn;
		real_t tvl = tv.length();

		if (tvl > MIN_VELOCITY) {
			tv /= tvl;

			Vector3 temp1 = inv_inertia_tensor_A.xform(c.rA.cross(tv));
			Vector3 temp2 = inv_inertia_tensor_B.xform(c.rB.cross(tv));

			real_t t = -tvl / (inv_mass_A + inv_mass_B + tv.dot(temp1.cross(c.rA) + temp2.cross(c.rB)));

			Vector3 jt = t * tv;

			Vector3 jtOld = c.acc_tangent_impulse;
			c.acc_tangent_impulse += jt;

			real_t fi_len = c.acc_tangent_impulse.length();
			real_t jtMax = c.acc_normal_impulse * friction;

			if (fi_len > CMP_EPSILON && fi_len > jtMax) {
				c.acc_tangent_impulse *= jtMax / fi_len;
			}

			jt = c.acc_tangent_impulse - jtOld;

			if (collide_A) {
				A->apply_impulse(-jt, c.rA + A->get_center_of_mass());
			}
			if (collide_B) {
				B->apply_impulse(jt, c.rB + B->get_center_of_mass());
			}
			c.acc_impulse -= jt;

			c.active = true;
		}
	}
}

GodotBodyPair3D::GodotBodyPair3D(GodotBody3D *p_A, int p_shape_A, GodotBody3D *p_B, int p_shape_B) :
		GodotBodyContact3D(_arr, 2) {
	A = p_A;
	B = p_B;
	shape_A = p_shape_A;
	shape_B = p_shape_B;
	space = A->get_space();
	A->add_constraint(this, 0);
	B->add_constraint(this, 1);
}

GodotBodyPair3D::~GodotBodyPair3D() {
	A->remove_constraint(this);
	B->remove_constraint(this);
}

void GodotBodySoftBodyPair3D::_contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata) {
	GodotBodySoftBodyPair3D *pair = static_cast<GodotBodySoftBodyPair3D *>(p_userdata);
	pair->contact_added_callback(p_point_A, p_index_A, p_point_B, p_index_B, normal);
}

void GodotBodySoftBodyPair3D::contact_added_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal) {
	Vector3 local_A = body->get_inv_transform().xform(p_point_A);
	Vector3 local_B = p_point_B - soft_body->get_node_position(p_index_B);

	Contact contact;
	contact.index_A = p_index_A;
	contact.index_B = p_index_B;
	contact.local_A = local_A;
	contact.local_B = local_B;
	contact.normal = (normal.dot((p_point_A - p_point_B)) < 0 ? -normal : normal);
	contact.used = true;

	// Attempt to determine if the contact will be reused.
	real_t contact_recycle_radius = space->get_contact_recycle_radius();

	uint32_t contact_count = contacts.size();
	for (uint32_t contact_index = 0; contact_index < contact_count; ++contact_index) {
		Contact &c = contacts[contact_index];
		if (c.index_B == p_index_B) {
			if (c.local_A.distance_squared_to(local_A) < (contact_recycle_radius * contact_recycle_radius) &&
					c.local_B.distance_squared_to(local_B) < (contact_recycle_radius * contact_recycle_radius)) {
				contact.acc_normal_impulse = c.acc_normal_impulse;
				contact.acc_bias_impulse = c.acc_bias_impulse;
				contact.acc_bias_impulse_center_of_mass = c.acc_bias_impulse_center_of_mass;
				contact.acc_tangent_impulse = c.acc_tangent_impulse;
			}
			c = contact;
			return;
		}
	}

	contacts.push_back(contact);
}

void GodotBodySoftBodyPair3D::validate_contacts() {
	// Make sure to erase contacts that are no longer valid.
	real_t max_separation = space->get_contact_max_separation();
	real_t max_separation2 = max_separation * max_separation;

	const Transform3D &transform_A = body->get_transform();

	uint32_t contact_count = contacts.size();
	for (uint32_t contact_index = 0; contact_index < contact_count; ++contact_index) {
		Contact &c = contacts[contact_index];

		bool erase = false;
		if (!c.used) {
			// Was left behind in previous frame.
			erase = true;
		} else {
			c.used = false;

			Vector3 global_A = transform_A.xform(c.local_A);
			Vector3 global_B = soft_body->get_node_position(c.index_B) + c.local_B;
			Vector3 axis = global_A - global_B;
			real_t depth = axis.dot(c.normal);

			if (depth < -max_separation || (global_B + c.normal * depth - global_A).length_squared() > max_separation2) {
				erase = true;
			}
		}

		if (erase) {
			// Contact no longer needed, remove.
			if ((contact_index + 1) < contact_count) {
				// Swap with the last one.
				SWAP(c, contacts[contact_count - 1]);
			}

			contact_index--;
			contact_count--;
		}
	}

	contacts.resize(contact_count);
}

bool GodotBodySoftBodyPair3D::setup(real_t p_step) {
	if (!body->interacts_with(soft_body) || body->has_exception(soft_body->get_self()) || soft_body->has_exception(body->get_self())) {
		collided = false;
		return false;
	}

	body_collides = (body->get_mode() > PhysicsServer3D::BODY_MODE_KINEMATIC) && body->collides_with(soft_body);
	soft_body_collides = soft_body->collides_with(body);

	if (!body_collides && !soft_body_collides) {
		if (body->get_max_contacts_reported() > 0) {
			report_contacts_only = true;
		} else {
			collided = false;
			return false;
		}
	}

	const Transform3D &xform_Au = body->get_transform();
	Transform3D xform_A = xform_Au * body->get_shape_transform(body_shape);

	Transform3D xform_Bu = soft_body->get_transform();
	Transform3D xform_B = xform_Bu * soft_body->get_shape_transform(0);

	validate_contacts();

	GodotShape3D *shape_A_ptr = body->get_shape(body_shape);
	GodotShape3D *shape_B_ptr = soft_body->get_shape(0);

	collided = GodotCollisionSolver3D::solve_static(shape_A_ptr, xform_A, shape_B_ptr, xform_B, _contact_added_callback, this, &sep_axis);

	return collided;
}

bool GodotBodySoftBodyPair3D::pre_solve(real_t p_step) {
	if (!collided) {
		return false;
	}

	real_t max_penetration = space->get_contact_max_allowed_penetration();

	real_t bias = space->get_contact_bias();

	GodotShape3D *shape_A_ptr = body->get_shape(body_shape);

	if (shape_A_ptr->get_custom_bias()) {
		bias = shape_A_ptr->get_custom_bias();
	}

	real_t inv_dt = 1.0 / p_step;

	bool do_process = false;

	const Transform3D &transform_A = body->get_transform();

	Basis zero_basis;
	zero_basis.set_zero();

	const Basis &body_inv_inertia_tensor = body_collides ? body->get_inv_inertia_tensor() : zero_basis;

	real_t body_inv_mass = body_collides ? body->get_inv_mass() : 0.0;

	uint32_t contact_count = contacts.size();
	for (uint32_t contact_index = 0; contact_index < contact_count; ++contact_index) {
		Contact &c = contacts[contact_index];
		c.active = false;

		real_t node_inv_mass = soft_body_collides ? soft_body->get_node_inv_mass(c.index_B) : 0.0;
		if ((node_inv_mass == 0.0) && (body_inv_mass == 0.0)) {
			continue;
		}

		Vector3 global_A = transform_A.xform(c.local_A);
		Vector3 global_B = soft_body->get_node_position(c.index_B) + c.local_B;
		Vector3 axis = global_A - global_B;
		real_t depth = axis.dot(c.normal);

		if (depth <= 0.0) {
			continue;
		}

#ifdef DEBUG_ENABLED
		if (space->is_debugging_contacts()) {
			space->add_debug_contact(global_A);
			space->add_debug_contact(global_B);
		}
#endif

		c.rA = global_A - transform_A.origin - body->get_center_of_mass();
		c.rB = global_B;

		// Precompute normal mass, tangent mass, and bias.
		Vector3 inertia_A = body_inv_inertia_tensor.xform(c.rA.cross(c.normal));
		real_t kNormal = body_inv_mass + node_inv_mass;
		kNormal += c.normal.dot(inertia_A.cross(c.rA));
		c.mass_normal = 1.0f / kNormal;

		c.bias = -bias * inv_dt * MIN(0.0f, -depth + max_penetration);
		c.depth = depth;

		Vector3 j_vec = c.normal * c.acc_normal_impulse + c.acc_tangent_impulse;
		if (body_collides) {
			body->apply_impulse(-j_vec, c.rA + body->get_center_of_mass());
		}
		if (soft_body_collides) {
			soft_body->apply_node_impulse(c.index_B, j_vec);
		}
		c.acc_impulse -= j_vec;

		if (body->can_report_contacts()) {
			Vector3 crA = body->get_angular_velocity().cross(c.rA) + body->get_linear_velocity();
			Vector3 crB = soft_body->get_node_velocity(c.index_B);
			body->add_contact(global_A, -c.normal, depth, body_shape, crA, global_B, 0, soft_body->get_instance_id(), soft_body->get_self(), crB, c.acc_impulse);
		}
		if (report_contacts_only) {
			collided = false;
			continue;
		}

		c.active = true;
		do_process = true;

		if (body_collides) {
			body->set_active(true);
		}

		c.bounce = body->get_bounce();

		if (c.bounce) {
			Vector3 crA = body->get_angular_velocity().cross(c.rA);
			Vector3 dv = soft_body->get_node_velocity(c.index_B) - body->get_linear_velocity() - crA;

			// Normal impulse.
			c.bounce = c.bounce * dv.dot(c.normal);
		}
	}

	return do_process;
}

void GodotBodySoftBodyPair3D::solve(real_t p_step) {
	if (!collided) {
		return;
	}

	const real_t max_bias_av = MAX_BIAS_ROTATION / p_step;

	Basis zero_basis;
	zero_basis.set_zero();

	const Basis &body_inv_inertia_tensor = body_collides ? body->get_inv_inertia_tensor() : zero_basis;

	real_t body_inv_mass = body_collides ? body->get_inv_mass() : 0.0;

	uint32_t contact_count = contacts.size();
	for (uint32_t contact_index = 0; contact_index < contact_count; ++contact_index) {
		Contact &c = contacts[contact_index];
		if (!c.active) {
			continue;
		}

		c.active = false;

		real_t node_inv_mass = soft_body_collides ? soft_body->get_node_inv_mass(c.index_B) : 0.0;

		// Bias impulse.
		Vector3 crbA = body->get_biased_angular_velocity().cross(c.rA);
		Vector3 dbv = soft_body->get_node_biased_velocity(c.index_B) - body->get_biased_linear_velocity() - crbA;

		real_t vbn = dbv.dot(c.normal);

		if (Math::abs(-vbn + c.bias) > MIN_VELOCITY) {
			real_t jbn = (-vbn + c.bias) * c.mass_normal;
			real_t jbnOld = c.acc_bias_impulse;
			c.acc_bias_impulse = MAX(jbnOld + jbn, 0.0f);

			Vector3 jb = c.normal * (c.acc_bias_impulse - jbnOld);

			if (body_collides) {
				body->apply_bias_impulse(-jb, c.rA + body->get_center_of_mass(), max_bias_av);
			}
			if (soft_body_collides) {
				soft_body->apply_node_bias_impulse(c.index_B, jb);
			}

			crbA = body->get_biased_angular_velocity().cross(c.rA);
			dbv = soft_body->get_node_biased_velocity(c.index_B) - body->get_biased_linear_velocity() - crbA;

			vbn = dbv.dot(c.normal);

			if (Math::abs(-vbn + c.bias) > MIN_VELOCITY) {
				real_t jbn_com = (-vbn + c.bias) / (body_inv_mass + node_inv_mass);
				real_t jbnOld_com = c.acc_bias_impulse_center_of_mass;
				c.acc_bias_impulse_center_of_mass = MAX(jbnOld_com + jbn_com, 0.0f);

				Vector3 jb_com = c.normal * (c.acc_bias_impulse_center_of_mass - jbnOld_com);

				if (body_collides) {
					body->apply_bias_impulse(-jb_com, body->get_center_of_mass(), 0.0f);
				}
				if (soft_body_collides) {
					soft_body->apply_node_bias_impulse(c.index_B, jb_com);
				}
			}

			c.active = true;
		}

		Vector3 crA = body->get_angular_velocity().cross(c.rA);
		Vector3 dv = soft_body->get_node_velocity(c.index_B) - body->get_linear_velocity() - crA;

		// Normal impulse.
		real_t vn = dv.dot(c.normal);

		if (Math::abs(vn) > MIN_VELOCITY) {
			real_t jn = -(c.bounce + vn) * c.mass_normal;
			real_t jnOld = c.acc_normal_impulse;
			c.acc_normal_impulse = MAX(jnOld + jn, 0.0f);

			Vector3 j = c.normal * (c.acc_normal_impulse - jnOld);

			if (body_collides) {
				body->apply_impulse(-j, c.rA + body->get_center_of_mass());
			}
			if (soft_body_collides) {
				soft_body->apply_node_impulse(c.index_B, j);
			}
			c.acc_impulse -= j;

			c.active = true;
		}

		// Friction impulse.
		real_t friction = body->get_friction();

		Vector3 lvA = body->get_linear_velocity() + body->get_angular_velocity().cross(c.rA);
		Vector3 lvB = soft_body->get_node_velocity(c.index_B);
		Vector3 dtv = lvB - lvA;

		real_t tn = c.normal.dot(dtv);

		// Tangential velocity.
		Vector3 tv = dtv - c.normal * tn;
		real_t tvl = tv.length();

		if (tvl > MIN_VELOCITY) {
			tv /= tvl;

			Vector3 temp1 = body_inv_inertia_tensor.xform(c.rA.cross(tv));

			real_t t = -tvl / (body_inv_mass + node_inv_mass + tv.dot(temp1.cross(c.rA)));

			Vector3 jt = t * tv;

			Vector3 jtOld = c.acc_tangent_impulse;
			c.acc_tangent_impulse += jt;

			real_t fi_len = c.acc_tangent_impulse.length();
			real_t jtMax = c.acc_normal_impulse * friction;

			if (fi_len > CMP_EPSILON && fi_len > jtMax) {
				c.acc_tangent_impulse *= jtMax / fi_len;
			}

			jt = c.acc_tangent_impulse - jtOld;

			if (body_collides) {
				body->apply_impulse(-jt, c.rA + body->get_center_of_mass());
			}
			if (soft_body_collides) {
				soft_body->apply_node_impulse(c.index_B, jt);
			}
			c.acc_impulse -= jt;

			c.active = true;
		}
	}
}

GodotBodySoftBodyPair3D::GodotBodySoftBodyPair3D(GodotBody3D *p_A, int p_shape_A, GodotSoftBody3D *p_B) :
		GodotBodyContact3D(&body, 1) {
	body = p_A;
	soft_body = p_B;
	body_shape = p_shape_A;
	space = p_A->get_space();
	body->add_constraint(this, 0);
	soft_body->add_constraint(this);
}

GodotBodySoftBodyPair3D::~GodotBodySoftBodyPair3D() {
	body->remove_constraint(this);
	soft_body->remove_constraint(this);
}
