/**************************************************************************/
/*  vector3i.cpp                                                          */
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

#include "vector3i.h"

#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"

Vector3i::Axis Vector3i::min_axis_index() const {
	return x < y ? (x < z ? Vector3i::AXIS_X : Vector3i::AXIS_Z) : (y < z ? Vector3i::AXIS_Y : Vector3i::AXIS_Z);
}

Vector3i::Axis Vector3i::max_axis_index() const {
	return x < y ? (y < z ? Vector3i::AXIS_Z : Vector3i::AXIS_Y) : (x < z ? Vector3i::AXIS_Z : Vector3i::AXIS_X);
}

Vector3i Vector3i::clamp(const Vector3i &p_min, const Vector3i &p_max) const {
	return Vector3i(
			CLAMP(x, p_min.x, p_max.x),
			CLAMP(y, p_min.y, p_max.y),
			CLAMP(z, p_min.z, p_max.z));
}

Vector3i Vector3i::clampi(int32_t p_min, int32_t p_max) const {
	return Vector3i(
			CLAMP(x, p_min, p_max),
			CLAMP(y, p_min, p_max),
			CLAMP(z, p_min, p_max));
}

Vector3i Vector3i::snapped(const Vector3i &p_step) const {
	return Vector3i(
			Math::snapped(x, p_step.x),
			Math::snapped(y, p_step.y),
			Math::snapped(z, p_step.z));
}

Vector3i Vector3i::snappedi(int32_t p_step) const {
	return Vector3i(
			Math::snapped(x, p_step),
			Math::snapped(y, p_step),
			Math::snapped(z, p_step));
}

Vector3i::operator String() const {
	return "(" + itos(x) + ", " + itos(y) + ", " + itos(z) + ")";
}

Vector3i::operator Vector3() const {
	return Vector3(x, y, z);
}

#define SETGET_2D(a, b)                                              \
	Vector2i Vector3i::get_##a##b() const { return Vector2i(a, b); } \
	void Vector3i::set_##a##b(const Vector2i &p_v) {                 \
		a = p_v.x;                                                   \
		b = p_v.y;                                                   \
	}
#define SETGET_2DM(a, b) SETGET_2D(a, b) SETGET_2D(b, a)
SETGET_2DM(x, y)
SETGET_2DM(x, z)
SETGET_2DM(y, z)
SETGET_2D(x, x)
SETGET_2D(y, y)
SETGET_2D(z, z)
#undef SETGET_2D
#undef SETGET_2DM
