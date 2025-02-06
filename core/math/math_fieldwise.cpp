/**************************************************************************/
/*  math_fieldwise.cpp                                                    */
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

#ifdef DEBUG_ENABLED

#include "math_fieldwise.h"

#define SETUP_TYPE(m_type)    \
	m_type source = p_source; \
	m_type target = p_target;
#define TRY_TRANSFER_FIELD(m_name, m_member) \
	if (p_field == m_name) {                 \
		target.m_member = source.m_member;   \
	}

Variant fieldwise_assign(const Variant &p_target, const Variant &p_source, const String &p_field) {
	ERR_FAIL_COND_V(p_target.get_type() != p_source.get_type(), p_target);

	/* clang-format makes a mess of this macro usage */
	/* clang-format off */

	switch (p_source.get_type()) {
		case Variant::VECTOR2: {
			SETUP_TYPE(Vector2)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)

			return target;
		}

		case Variant::VECTOR2I: {
			SETUP_TYPE(Vector2i)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)

			return target;
		}

		case Variant::RECT2: {
			SETUP_TYPE(Rect2)

			/**/ TRY_TRANSFER_FIELD("x", position.x)
			else TRY_TRANSFER_FIELD("y", position.y)
			else TRY_TRANSFER_FIELD("w", size.x)
			else TRY_TRANSFER_FIELD("h", size.y)

			return target;
		}

		case Variant::RECT2I: {
			SETUP_TYPE(Rect2i)

			/**/ TRY_TRANSFER_FIELD("x", position.x)
			else TRY_TRANSFER_FIELD("y", position.y)
			else TRY_TRANSFER_FIELD("w", size.x)
			else TRY_TRANSFER_FIELD("h", size.y)

			return target;
		}

		case Variant::VECTOR3: {
			SETUP_TYPE(Vector3)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)
			else TRY_TRANSFER_FIELD("z", z)

			return target;
		}

		case Variant::VECTOR3I: {
			SETUP_TYPE(Vector3i)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)
			else TRY_TRANSFER_FIELD("z", z)

			return target;
		}

		case Variant::VECTOR4: {
			SETUP_TYPE(Vector4)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)
			else TRY_TRANSFER_FIELD("z", z)
			else TRY_TRANSFER_FIELD("w", w)

			return target;
		}

		case Variant::VECTOR4I: {
			SETUP_TYPE(Vector4i)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)
			else TRY_TRANSFER_FIELD("z", z)
			else TRY_TRANSFER_FIELD("w", w)

			return target;
		}

		case Variant::PLANE: {
			SETUP_TYPE(Plane)

			/**/ TRY_TRANSFER_FIELD("x", normal.x)
			else TRY_TRANSFER_FIELD("y", normal.y)
			else TRY_TRANSFER_FIELD("z", normal.z)
			else TRY_TRANSFER_FIELD("d", d)

			return target;
		}

		case Variant::QUATERNION: {
			SETUP_TYPE(Quaternion)

			/**/ TRY_TRANSFER_FIELD("x", x)
			else TRY_TRANSFER_FIELD("y", y)
			else TRY_TRANSFER_FIELD("z", z)
			else TRY_TRANSFER_FIELD("w", w)

			return target;
		}

		case Variant::AABB: {
			SETUP_TYPE(AABB)

			/**/ TRY_TRANSFER_FIELD("px", position.x)
			else TRY_TRANSFER_FIELD("py", position.y)
			else TRY_TRANSFER_FIELD("pz", position.z)
			else TRY_TRANSFER_FIELD("sx", size.x)
			else TRY_TRANSFER_FIELD("sy", size.y)
			else TRY_TRANSFER_FIELD("sz", size.z)

			return target;
		}

		case Variant::TRANSFORM2D: {
			SETUP_TYPE(Transform2D)

			/**/ TRY_TRANSFER_FIELD("xx", columns[0][0])
			else TRY_TRANSFER_FIELD("xy", columns[0][1])
			else TRY_TRANSFER_FIELD("yx", columns[1][0])
			else TRY_TRANSFER_FIELD("yy", columns[1][1])
			else TRY_TRANSFER_FIELD("ox", columns[2][0])
			else TRY_TRANSFER_FIELD("oy", columns[2][1])

			return target;
		}

		case Variant::BASIS: {
			SETUP_TYPE(Basis)

			/**/ TRY_TRANSFER_FIELD("xx", rows[0][0])
			else TRY_TRANSFER_FIELD("xy", rows[0][1])
			else TRY_TRANSFER_FIELD("xz", rows[0][2])
			else TRY_TRANSFER_FIELD("yx", rows[1][0])
			else TRY_TRANSFER_FIELD("yy", rows[1][1])
			else TRY_TRANSFER_FIELD("yz", rows[1][2])
			else TRY_TRANSFER_FIELD("zx", rows[2][0])
			else TRY_TRANSFER_FIELD("zy", rows[2][1])
			else TRY_TRANSFER_FIELD("zz", rows[2][2])

			return target;
		}

		case Variant::TRANSFORM3D: {
			SETUP_TYPE(Transform3D)

			/**/ TRY_TRANSFER_FIELD("xx", basis.rows[0][0])
			else TRY_TRANSFER_FIELD("xy", basis.rows[0][1])
			else TRY_TRANSFER_FIELD("xz", basis.rows[0][2])
			else TRY_TRANSFER_FIELD("yx", basis.rows[1][0])
			else TRY_TRANSFER_FIELD("yy", basis.rows[1][1])
			else TRY_TRANSFER_FIELD("yz", basis.rows[1][2])
			else TRY_TRANSFER_FIELD("zx", basis.rows[2][0])
			else TRY_TRANSFER_FIELD("zy", basis.rows[2][1])
			else TRY_TRANSFER_FIELD("zz", basis.rows[2][2])
			else TRY_TRANSFER_FIELD("xo", origin.x)
			else TRY_TRANSFER_FIELD("yo", origin.y)
			else TRY_TRANSFER_FIELD("zo", origin.z)

			return target;
		}

		case Variant::PROJECTION: {
			SETUP_TYPE(Projection)

			/**/ TRY_TRANSFER_FIELD("xx", columns[0].x)
			else TRY_TRANSFER_FIELD("xy", columns[0].y)
			else TRY_TRANSFER_FIELD("xz", columns[0].z)
			else TRY_TRANSFER_FIELD("xw", columns[0].w)
			else TRY_TRANSFER_FIELD("yx", columns[1].x)
			else TRY_TRANSFER_FIELD("yy", columns[1].y)
			else TRY_TRANSFER_FIELD("yz", columns[1].z)
			else TRY_TRANSFER_FIELD("yw", columns[1].w)
			else TRY_TRANSFER_FIELD("zx", columns[2].x)
			else TRY_TRANSFER_FIELD("zy", columns[2].y)
			else TRY_TRANSFER_FIELD("zz", columns[2].z)
			else TRY_TRANSFER_FIELD("zw", columns[2].w)
			else TRY_TRANSFER_FIELD("xo", columns[3].x)
			else TRY_TRANSFER_FIELD("yo", columns[3].y)
			else TRY_TRANSFER_FIELD("zo", columns[3].z)
			else TRY_TRANSFER_FIELD("wo", columns[3].w)

			return target;
		}

		default: {
			ERR_FAIL_V(p_target);
		}
	}
	/* clang-format on */
}

#endif // DEBUG_ENABLED
