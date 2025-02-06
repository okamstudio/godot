/**************************************************************************/
/*  ik_ray_3d.cpp                                                         */
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

#include "ik_ray_3d.h"

IKRay3D::IKRay3D() {
}

IKRay3D::IKRay3D(Vector3 p_p1, Vector3 p_p2) {
	working_vector = p_p1;
	point_1 = p_p1;
	point_2 = p_p2;
}

Vector3 IKRay3D::get_heading() {
	working_vector = point_2;
	return working_vector - point_1;
}

void IKRay3D::set_heading(const Vector3 &p_new_head) {
	point_2 = point_1;
	point_2 = p_new_head;
}

real_t IKRay3D::get_scaled_projection(const Vector3 p_input) {
	working_vector = p_input;
	working_vector = working_vector - point_1;
	Vector3 heading = get_heading();
	real_t headingMag = heading.length();
	real_t workingVectorMag = working_vector.length();
	if (workingVectorMag == 0 || headingMag == 0) {
		return 0;
	}
	return (working_vector.dot(heading) / (headingMag * workingVectorMag)) * (workingVectorMag / headingMag);
}

void IKRay3D::elongate(real_t amt) {
	Vector3 midPoint = (point_1 + point_2) * 0.5f;
	Vector3 p1Heading = point_1 - midPoint;
	Vector3 p2Heading = point_2 - midPoint;
	Vector3 p1Add = p1Heading.normalized() * amt;
	Vector3 p2Add = p2Heading.normalized() * amt;

	point_1 = p1Heading + p1Add + midPoint;
	point_2 = p2Heading + p2Add + midPoint;
}

Vector3 IKRay3D::get_intersects_plane(Vector3 ta, Vector3 tb, Vector3 tc) {
	Vector3 uvw;
	tta = ta;
	ttb = tb;
	ttc = tc;
	tta -= point_1;
	ttb -= point_1;
	ttc -= point_1;
	Vector3 result = plane_intersect_test(tta, ttb, ttc, &uvw);
	return result + point_1;
}

int IKRay3D::intersects_sphere(Vector3 sphereCenter, real_t radius, Vector3 *S1, Vector3 *S2) {
	Vector3 tp1 = point_1 - sphereCenter;
	Vector3 tp2 = point_2 - sphereCenter;
	int result = intersects_sphere(tp1, tp2, radius, S1, S2);
	*S1 += sphereCenter;
	*S2 += sphereCenter;
	return result;
}

void IKRay3D::set_point_1(Vector3 in) {
	point_1 = in;
}

void IKRay3D::set_point_2(Vector3 in) {
	point_2 = in;
}

Vector3 IKRay3D::get_point_2() {
	return point_2;
}

Vector3 IKRay3D::get_point_1() {
	return point_1;
}

int IKRay3D::intersects_sphere(Vector3 rp1, Vector3 rp2, real_t radius, Vector3 *S1, Vector3 *S2) {
	Vector3 direction = rp2 - rp1;
	Vector3 e = direction; // e=ray.dir
	e.normalize(); // e=g/|g|
	Vector3 h = point_1;
	h = Vector3(0.0f, 0.0f, 0.0f);
	h = h - rp1; // h=r.o-c.M
	real_t lf = e.dot(h); // lf=e.h
	real_t radpow = radius * radius;
	real_t hdh = h.length_squared();
	real_t lfpow = lf * lf;
	real_t s = radpow - hdh + lfpow; // s=r^2-h^2+lf^2
	if (s < 0.0f) {
		return 0; // no intersection points ?
	}
	s = Math::sqrt(s); // s=sqrt(r^2-h^2+lf^2)

	int result = 0;
	if (lf < s) {
		if (lf + s >= 0) {
			s = -s; // swap S1 <-> S2}
			result = 1; // one intersection point
		}
	} else {
		result = 2; // 2 intersection points
	}

	*S1 = e * (lf - s);
	*S1 += rp1; // S1=A+e*(lf-s)
	*S2 = e * (lf + s);
	*S2 += rp1; // S2=A+e*(lf+s)
	return result;
}

Vector3 IKRay3D::plane_intersect_test(Vector3 ta, Vector3 tb, Vector3 tc, Vector3 *uvw) {
	u = tb;
	v = tc;
	n = Vector3(0, 0, 0);
	dir = get_heading();
	w0 = Vector3(0, 0, 0);
	real_t r, a, b;
	u -= ta;
	v -= ta;

	n = u.cross(v).normalized();

	w0 -= ta;
	a = -(n.dot(w0));
	b = n.dot(dir);
	r = a / b;
	I = dir;
	I *= r;
	barycentric(ta, tb, tc, I, uvw);
	return I;
}

real_t IKRay3D::triangle_area_2d(real_t x1, real_t y1, real_t x2, real_t y2, real_t x3, real_t y3) {
	return (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
}

void IKRay3D::barycentric(Vector3 a, Vector3 b, Vector3 c, Vector3 p, Vector3 *uvw) {
	bc = b;
	ca = a;
	at = a;
	bt = b;
	ct = c;
	pt = p;

	m = Vector3(bc - ct).cross(ca - at).normalized();

	real_t nu;
	real_t nv;
	real_t ood;

	real_t x = Math::abs(m.x);
	real_t y = Math::abs(m.y);
	real_t z = Math::abs(m.z);

	if (x >= y && x >= z) {
		nu = triangle_area_2d(pt.y, pt.z, bt.y, bt.z, ct.y, ct.z);
		nv = triangle_area_2d(pt.y, pt.z, ct.y, ct.z, at.y, at.z);
		ood = 1.0f / m.x;
	} else if (y >= x && y >= z) {
		nu = triangle_area_2d(pt.x, pt.z, bt.x, bt.z, ct.x, ct.z);
		nv = triangle_area_2d(pt.x, pt.z, ct.x, ct.z, at.x, at.z);
		ood = 1.0f / -m.y;
	} else {
		nu = triangle_area_2d(pt.x, pt.y, bt.x, bt.y, ct.x, ct.y);
		nv = triangle_area_2d(pt.x, pt.y, ct.x, ct.y, at.x, at.y);
		ood = 1.0f / m.z;
	}
	(*uvw)[0] = nu * ood;
	(*uvw)[1] = nv * ood;
	(*uvw)[2] = 1.0f - (*uvw)[0] - (*uvw)[1];
}

void IKRay3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_heading"), &IKRay3D::get_heading);
	ClassDB::bind_method(D_METHOD("get_scaled_projection", "input"), &IKRay3D::get_scaled_projection);
	ClassDB::bind_method(D_METHOD("get_intersects_plane", "a", "b", "c"), &IKRay3D::get_intersects_plane);
}
