/**************************************************************************/
/*  godot_jacobian_entry_3d.h                                             */
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

/*
Adapted to Godot from the Bullet library.
*/

/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "core/math/transform_3d.h"

class GodotJacobianEntry3D {
public:
	GodotJacobianEntry3D() {}
	//constraint between two different rigidbodies
	GodotJacobianEntry3D(
			const Basis &world2A,
			const Basis &world2B,
			const Vector3 &rel_pos1, const Vector3 &rel_pos2,
			const Vector3 &jointAxis,
			const Vector3 &inertiaInvA,
			const real_t massInvA,
			const Vector3 &inertiaInvB,
			const real_t massInvB) :
			m_linearJointAxis(jointAxis) {
		m_aJ = world2A.xform(rel_pos1.cross(m_linearJointAxis));
		m_bJ = world2B.xform(rel_pos2.cross(-m_linearJointAxis));
		m_0MinvJt = inertiaInvA * m_aJ;
		m_1MinvJt = inertiaInvB * m_bJ;
		m_Adiag = massInvA + m_0MinvJt.dot(m_aJ) + massInvB + m_1MinvJt.dot(m_bJ);

		ERR_FAIL_COND(m_Adiag <= real_t(0.0));
	}

	//angular constraint between two different rigidbodies
	GodotJacobianEntry3D(const Vector3 &jointAxis,
			const Basis &world2A,
			const Basis &world2B,
			const Vector3 &inertiaInvA,
			const Vector3 &inertiaInvB) :
			m_linearJointAxis(Vector3(real_t(0.), real_t(0.), real_t(0.))) {
		m_aJ = world2A.xform(jointAxis);
		m_bJ = world2B.xform(-jointAxis);
		m_0MinvJt = inertiaInvA * m_aJ;
		m_1MinvJt = inertiaInvB * m_bJ;
		m_Adiag = m_0MinvJt.dot(m_aJ) + m_1MinvJt.dot(m_bJ);

		ERR_FAIL_COND(m_Adiag <= real_t(0.0));
	}

	//angular constraint between two different rigidbodies
	GodotJacobianEntry3D(const Vector3 &axisInA,
			const Vector3 &axisInB,
			const Vector3 &inertiaInvA,
			const Vector3 &inertiaInvB) :
			m_linearJointAxis(Vector3(real_t(0.), real_t(0.), real_t(0.))),
			m_aJ(axisInA),
			m_bJ(-axisInB) {
		m_0MinvJt = inertiaInvA * m_aJ;
		m_1MinvJt = inertiaInvB * m_bJ;
		m_Adiag = m_0MinvJt.dot(m_aJ) + m_1MinvJt.dot(m_bJ);

		ERR_FAIL_COND(m_Adiag <= real_t(0.0));
	}

	//constraint on one rigidbody
	GodotJacobianEntry3D(
			const Basis &world2A,
			const Vector3 &rel_pos1, const Vector3 &rel_pos2,
			const Vector3 &jointAxis,
			const Vector3 &inertiaInvA,
			const real_t massInvA) :
			m_linearJointAxis(jointAxis) {
		m_aJ = world2A.xform(rel_pos1.cross(jointAxis));
		m_bJ = world2A.xform(rel_pos2.cross(-jointAxis));
		m_0MinvJt = inertiaInvA * m_aJ;
		m_1MinvJt = Vector3(real_t(0.), real_t(0.), real_t(0.));
		m_Adiag = massInvA + m_0MinvJt.dot(m_aJ);

		ERR_FAIL_COND(m_Adiag <= real_t(0.0));
	}

	real_t getDiagonal() const { return m_Adiag; }

	// for two constraints on the same rigidbody (for example vehicle friction)
	real_t getNonDiagonal(const GodotJacobianEntry3D &jacB, const real_t massInvA) const {
		const GodotJacobianEntry3D &jacA = *this;
		real_t lin = massInvA * jacA.m_linearJointAxis.dot(jacB.m_linearJointAxis);
		real_t ang = jacA.m_0MinvJt.dot(jacB.m_aJ);
		return lin + ang;
	}

	// for two constraints on sharing two same rigidbodies (for example two contact points between two rigidbodies)
	real_t getNonDiagonal(const GodotJacobianEntry3D &jacB, const real_t massInvA, const real_t massInvB) const {
		const GodotJacobianEntry3D &jacA = *this;
		Vector3 lin = jacA.m_linearJointAxis * jacB.m_linearJointAxis;
		Vector3 ang0 = jacA.m_0MinvJt * jacB.m_aJ;
		Vector3 ang1 = jacA.m_1MinvJt * jacB.m_bJ;
		Vector3 lin0 = massInvA * lin;
		Vector3 lin1 = massInvB * lin;
		Vector3 sum = ang0 + ang1 + lin0 + lin1;
		return sum[0] + sum[1] + sum[2];
	}

	real_t getRelativeVelocity(const Vector3 &linvelA, const Vector3 &angvelA, const Vector3 &linvelB, const Vector3 &angvelB) {
		Vector3 linrel = linvelA - linvelB;
		Vector3 angvela = angvelA * m_aJ;
		Vector3 angvelb = angvelB * m_bJ;
		linrel *= m_linearJointAxis;
		angvela += angvelb;
		angvela += linrel;
		real_t rel_vel2 = angvela[0] + angvela[1] + angvela[2];
		return rel_vel2 + CMP_EPSILON;
	}
	//private:

	Vector3 m_linearJointAxis;
	Vector3 m_aJ;
	Vector3 m_bJ;
	Vector3 m_0MinvJt;
	Vector3 m_1MinvJt;
	//Optimization: can be stored in the w/last component of one of the vectors
	real_t m_Adiag = 1.0;
};
