/**************************************************************************/
/*  vector2i.cpp                                                          */
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

#include "vector2i.h"

#include "core/math/vector2.h"
#include "core/string/ustring.h"

Vector2i Vector2i::clamp(const Vector2i &p_min, const Vector2i &p_max) const {
	return Vector2i(
			CLAMP(x, p_min.x, p_max.x),
			CLAMP(y, p_min.y, p_max.y));
}

Vector2i Vector2i::clampi(int32_t p_min, int32_t p_max) const {
	return Vector2i(
			CLAMP(x, p_min, p_max),
			CLAMP(y, p_min, p_max));
}

Vector2i Vector2i::snapped(const Vector2i &p_step) const {
	return Vector2i(
			Math::snapped(x, p_step.x),
			Math::snapped(y, p_step.y));
}

Vector2i Vector2i::snappedi(int32_t p_step) const {
	return Vector2i(
			Math::snapped(x, p_step),
			Math::snapped(y, p_step));
}

int64_t Vector2i::length_squared() const {
	return x * (int64_t)x + y * (int64_t)y;
}

double Vector2i::length() const {
	return Math::sqrt((double)length_squared());
}

Vector2i::operator String() const {
	return "(" + itos(x) + ", " + itos(y) + ")";
}

Vector2i::operator Vector2() const {
	return Vector2((int32_t)x, (int32_t)y);
}
