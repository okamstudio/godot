/**************************************************************************/
/*  variant_op.cpp                                                        */
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

#include "variant.h"

#include "core/core_string_names.h"
#include "core/object.h"
#include "core/object_rc.h"
#include "core/script_language.h"

#define CASE_TYPE_ALL(PREFIX, OP) \
	CASE_TYPE(PREFIX, OP, INT)    \
	CASE_TYPE_ALL_BUT_INT(PREFIX, OP)

#define CASE_TYPE_ALL_BUT_INT(PREFIX, OP)     \
	CASE_TYPE(PREFIX, OP, NIL)                \
	CASE_TYPE(PREFIX, OP, BOOL)               \
	CASE_TYPE(PREFIX, OP, REAL)               \
	CASE_TYPE(PREFIX, OP, STRING)             \
	CASE_TYPE(PREFIX, OP, VECTOR2)            \
	CASE_TYPE(PREFIX, OP, RECT2)              \
	CASE_TYPE(PREFIX, OP, VECTOR3)            \
	CASE_TYPE(PREFIX, OP, TRANSFORM2D)        \
	CASE_TYPE(PREFIX, OP, PLANE)              \
	CASE_TYPE(PREFIX, OP, QUAT)               \
	CASE_TYPE(PREFIX, OP, AABB)               \
	CASE_TYPE(PREFIX, OP, BASIS)              \
	CASE_TYPE(PREFIX, OP, TRANSFORM)          \
	CASE_TYPE(PREFIX, OP, COLOR)              \
	CASE_TYPE(PREFIX, OP, NODE_PATH)          \
	CASE_TYPE(PREFIX, OP, _RID)               \
	CASE_TYPE(PREFIX, OP, OBJECT)             \
	CASE_TYPE(PREFIX, OP, DICTIONARY)         \
	CASE_TYPE(PREFIX, OP, ARRAY)              \
	CASE_TYPE(PREFIX, OP, POOL_BYTE_ARRAY)    \
	CASE_TYPE(PREFIX, OP, POOL_INT_ARRAY)     \
	CASE_TYPE(PREFIX, OP, POOL_REAL_ARRAY)    \
	CASE_TYPE(PREFIX, OP, POOL_STRING_ARRAY)  \
	CASE_TYPE(PREFIX, OP, POOL_VECTOR2_ARRAY) \
	CASE_TYPE(PREFIX, OP, POOL_VECTOR3_ARRAY) \
	CASE_TYPE(PREFIX, OP, POOL_COLOR_ARRAY)

#ifdef __GNUC__
#define TYPE(PREFIX, OP, TYPE) &&PREFIX##_##OP##_##TYPE

/* clang-format off */
#define TYPES(PREFIX, OP) {                   \
		TYPE(PREFIX, OP, NIL),                \
		TYPE(PREFIX, OP, BOOL),               \
		TYPE(PREFIX, OP, INT),                \
		TYPE(PREFIX, OP, REAL),               \
		TYPE(PREFIX, OP, STRING),             \
		TYPE(PREFIX, OP, VECTOR2),            \
		TYPE(PREFIX, OP, RECT2),              \
		TYPE(PREFIX, OP, VECTOR3),            \
		TYPE(PREFIX, OP, TRANSFORM2D),        \
		TYPE(PREFIX, OP, PLANE),              \
		TYPE(PREFIX, OP, QUAT),               \
		TYPE(PREFIX, OP, AABB),              \
		TYPE(PREFIX, OP, BASIS),              \
		TYPE(PREFIX, OP, TRANSFORM),          \
		TYPE(PREFIX, OP, COLOR),              \
		TYPE(PREFIX, OP, NODE_PATH),          \
		TYPE(PREFIX, OP, _RID),               \
		TYPE(PREFIX, OP, OBJECT),             \
		TYPE(PREFIX, OP, DICTIONARY),         \
		TYPE(PREFIX, OP, ARRAY),              \
		TYPE(PREFIX, OP, POOL_BYTE_ARRAY),    \
		TYPE(PREFIX, OP, POOL_INT_ARRAY),     \
		TYPE(PREFIX, OP, POOL_REAL_ARRAY),    \
		TYPE(PREFIX, OP, POOL_STRING_ARRAY),  \
		TYPE(PREFIX, OP, POOL_VECTOR2_ARRAY), \
		TYPE(PREFIX, OP, POOL_VECTOR3_ARRAY), \
		TYPE(PREFIX, OP, POOL_COLOR_ARRAY),   \
}
/* clang-format on */

#define CASES(PREFIX) static const void *switch_table_##PREFIX[25][27] = { \
	TYPES(PREFIX, OP_EQUAL),                                               \
	TYPES(PREFIX, OP_NOT_EQUAL),                                           \
	TYPES(PREFIX, OP_LESS),                                                \
	TYPES(PREFIX, OP_LESS_EQUAL),                                          \
	TYPES(PREFIX, OP_GREATER),                                             \
	TYPES(PREFIX, OP_GREATER_EQUAL),                                       \
	TYPES(PREFIX, OP_ADD),                                                 \
	TYPES(PREFIX, OP_SUBTRACT),                                            \
	TYPES(PREFIX, OP_MULTIPLY),                                            \
	TYPES(PREFIX, OP_DIVIDE),                                              \
	TYPES(PREFIX, OP_NEGATE),                                              \
	TYPES(PREFIX, OP_POSITIVE),                                            \
	TYPES(PREFIX, OP_MODULE),                                              \
	TYPES(PREFIX, OP_STRING_CONCAT),                                       \
	TYPES(PREFIX, OP_SHIFT_LEFT),                                          \
	TYPES(PREFIX, OP_SHIFT_RIGHT),                                         \
	TYPES(PREFIX, OP_BIT_AND),                                             \
	TYPES(PREFIX, OP_BIT_OR),                                              \
	TYPES(PREFIX, OP_BIT_XOR),                                             \
	TYPES(PREFIX, OP_BIT_NEGATE),                                          \
	TYPES(PREFIX, OP_AND),                                                 \
	TYPES(PREFIX, OP_OR),                                                  \
	TYPES(PREFIX, OP_XOR),                                                 \
	TYPES(PREFIX, OP_NOT),                                                 \
	TYPES(PREFIX, OP_IN),                                                  \
}

#define SWITCH(PREFIX, op, val) goto *switch_table_##PREFIX[op][val];
#define SWITCH_OP(PREFIX, OP, val)
#define CASE_TYPE(PREFIX, OP, TYPE) PREFIX##_##OP##_##TYPE:

#else
#define CASES(PREFIX)
#define SWITCH(PREFIX, op, val) switch (op)
#define SWITCH_OP(PREFIX, OP, val) \
	case OP:                       \
		switch (val)
#define CASE_TYPE(PREFIX, OP, TYPE) case TYPE:
#endif

Variant::operator bool() const {
	return booleanize();
}

// We consider all uninitialized or empty types to be false based on the type's
// zeroiness.
bool Variant::booleanize() const {
	return !is_zero();
}

#define _RETURN(m_what) \
	{                   \
		r_ret = m_what; \
		return;         \
	}

#define _RETURN_FAIL     \
	{                    \
		r_valid = false; \
		return;          \
	}

#define DEFAULT_OP_NUM(m_prefix, m_op_name, m_name, m_op, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                      \
		if (p_b.type == INT)                                      \
			_RETURN(p_a._data.m_type m_op p_b._data._int);        \
		if (p_b.type == REAL)                                     \
			_RETURN(p_a._data.m_type m_op p_b._data._real);       \
                                                                  \
		_RETURN_FAIL                                              \
	};

#define DEFAULT_OP_NUM_NULL(m_prefix, m_op_name, m_name, m_op, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                           \
		if (p_b.type == INT)                                           \
			_RETURN(p_a._data.m_type m_op p_b._data._int);             \
		if (p_b.type == REAL)                                          \
			_RETURN(p_a._data.m_type m_op p_b._data._real);            \
		if (p_b.type == NIL)                                           \
			_RETURN(!(p_b.type m_op NIL));                             \
                                                                       \
		_RETURN_FAIL                                                   \
	};

#ifdef DEBUG_ENABLED
#define DEFAULT_OP_NUM_DIV(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                    \
		if (p_b.type == INT) {                                  \
			if (p_b._data._int == 0) {                          \
				r_valid = false;                                \
				_RETURN("Division By Zero");                    \
			}                                                   \
			_RETURN(p_a._data.m_type / p_b._data._int);         \
		}                                                       \
		if (p_b.type == REAL) {                                 \
			if (p_b._data._real == 0) {                         \
				r_valid = false;                                \
				_RETURN("Division By Zero");                    \
			}                                                   \
			_RETURN(p_a._data.m_type / p_b._data._real);        \
		}                                                       \
                                                                \
		_RETURN_FAIL                                            \
	};
#else
#define DEFAULT_OP_NUM_DIV(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                    \
		if (p_b.type == INT)                                    \
			_RETURN(p_a._data.m_type / p_b._data._int);         \
		if (p_b.type == REAL)                                   \
			_RETURN(p_a._data.m_type / p_b._data._real);        \
                                                                \
		_RETURN_FAIL                                            \
	};
#endif

#define DEFAULT_OP_NUM_NEG(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                    \
		_RETURN(-p_a._data.m_type);                             \
	};

#define DEFAULT_OP_NUM_POS(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                    \
		_RETURN(p_a._data.m_type);                              \
	};

#define DEFAULT_OP_NUM_VEC(m_prefix, m_op_name, m_name, m_op, m_type)                          \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                   \
		if (p_b.type == INT)                                                                   \
			_RETURN(p_a._data.m_type m_op p_b._data._int);                                     \
		if (p_b.type == REAL)                                                                  \
			_RETURN(p_a._data.m_type m_op p_b._data._real);                                    \
		if (p_b.type == VECTOR2)                                                               \
			_RETURN(p_a._data.m_type m_op *reinterpret_cast<const Vector2 *>(p_b._data._mem)); \
		if (p_b.type == VECTOR3)                                                               \
			_RETURN(p_a._data.m_type m_op *reinterpret_cast<const Vector3 *>(p_b._data._mem)); \
                                                                                               \
		_RETURN_FAIL                                                                           \
	};

#define DEFAULT_OP_STR_REV(m_prefix, m_op_name, m_name, m_op, m_type)                                                            \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                     \
		if (p_b.type == STRING)                                                                                                  \
			_RETURN(*reinterpret_cast<const m_type *>(p_b._data._mem) m_op *reinterpret_cast<const String *>(p_a._data._mem));   \
		if (p_b.type == NODE_PATH)                                                                                               \
			_RETURN(*reinterpret_cast<const m_type *>(p_b._data._mem) m_op *reinterpret_cast<const NodePath *>(p_a._data._mem)); \
                                                                                                                                 \
		_RETURN_FAIL                                                                                                             \
	};

#define DEFAULT_OP_STR(m_prefix, m_op_name, m_name, m_op, m_type)                                                                \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                     \
		if (p_b.type == STRING)                                                                                                  \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const String *>(p_b._data._mem));   \
		if (p_b.type == NODE_PATH)                                                                                               \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const NodePath *>(p_b._data._mem)); \
                                                                                                                                 \
		_RETURN_FAIL                                                                                                             \
	};

#define DEFAULT_OP_STR_NULL(m_prefix, m_op_name, m_name, m_op, m_type)                                                           \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                     \
		if (p_b.type == STRING)                                                                                                  \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const String *>(p_b._data._mem));   \
		if (p_b.type == NODE_PATH)                                                                                               \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const NodePath *>(p_b._data._mem)); \
		if (p_b.type == NIL)                                                                                                     \
			_RETURN(!(p_b.type m_op NIL));                                                                                       \
                                                                                                                                 \
		_RETURN_FAIL                                                                                                             \
	};

#define DEFAULT_OP_LOCALMEM_REV(m_prefix, m_op_name, m_name, m_op, m_type)                                                     \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
		if (p_b.type == m_name)                                                                                                \
			_RETURN(*reinterpret_cast<const m_type *>(p_b._data._mem) m_op *reinterpret_cast<const m_type *>(p_a._data._mem)); \
                                                                                                                               \
		_RETURN_FAIL                                                                                                           \
	};

#define DEFAULT_OP_LOCALMEM(m_prefix, m_op_name, m_name, m_op, m_type)                                                         \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
		if (p_b.type == m_name)                                                                                                \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const m_type *>(p_b._data._mem)); \
                                                                                                                               \
		_RETURN_FAIL                                                                                                           \
	};

#define DEFAULT_OP_LOCALMEM_NULL(m_prefix, m_op_name, m_name, m_op, m_type)                                                    \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
		if (p_b.type == m_name)                                                                                                \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const m_type *>(p_b._data._mem)); \
		if (p_b.type == NIL)                                                                                                   \
			_RETURN(!(p_b.type m_op NIL));                                                                                     \
                                                                                                                               \
		_RETURN_FAIL                                                                                                           \
	};

#define DEFAULT_OP_LOCALMEM_NEG(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                         \
		_RETURN(-*reinterpret_cast<const m_type *>(p_a._data._mem)); \
	}

#define DEFAULT_OP_LOCALMEM_POS(m_prefix, m_op_name, m_name, m_type) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                         \
		_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem));  \
	}

#define DEFAULT_OP_LOCALMEM_NUM(m_prefix, m_op_name, m_name, m_op, m_type)                                                     \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
		if (p_b.type == m_name)                                                                                                \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const m_type *>(p_b._data._mem)); \
		if (p_b.type == INT)                                                                                                   \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op p_b._data._int);                                    \
		if (p_b.type == REAL)                                                                                                  \
			_RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op p_b._data._real);                                   \
                                                                                                                               \
		_RETURN_FAIL                                                                                                           \
	}

#define DEFAULT_OP_PTR(m_op, m_name, m_sub)                \
	CASE_TYPE(m_prefix, m_op_name, m_name) {               \
		if (p_b.type == m_name)                            \
			_RETURN(p_a._data.m_sub m_op p_b._data.m_sub); \
                                                           \
		_RETURN_FAIL                                       \
	}

#define DEFAULT_OP_PTRREF(m_prefix, m_op_name, m_name, m_op, m_sub) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                        \
		if (p_b.type == m_name)                                     \
			_RETURN(*p_a._data.m_sub m_op *p_b._data.m_sub);        \
                                                                    \
		_RETURN_FAIL                                                \
	}

#define DEFAULT_OP_PTRREF_NULL(m_prefix, m_op_name, m_name, m_op, m_sub) \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                             \
		if (p_b.type == m_name)                                          \
			_RETURN(*p_a._data.m_sub m_op *p_b._data.m_sub);             \
		if (p_b.type == NIL)                                             \
			_RETURN(!(p_b.type m_op NIL));                               \
                                                                         \
		_RETURN_FAIL                                                     \
	}

#define DEFAULT_OP_ARRAY_EQ(m_prefix, m_op_name, m_name, m_type)                                  \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                      \
		if (p_b.type == NIL)                                                                      \
			_RETURN(false)                                                                        \
		DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, !=, !=, true, false, false) \
	}

#define DEFAULT_OP_ARRAY_NEQ(m_prefix, m_op_name, m_name, m_type)                                \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                     \
		if (p_b.type == NIL)                                                                     \
			_RETURN(true)                                                                        \
		DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, !=, !=, false, true, true) \
	}

#define DEFAULT_OP_ARRAY_LT(m_prefix, m_op_name, m_name, m_type) \
	DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, <, !=, false, a_len < array_b.size(), true)

#define DEFAULT_OP_ARRAY_GT(m_prefix, m_op_name, m_name, m_type) \
	DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, >, !=, false, a_len < array_b.size(), true)

#define DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f)      \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                     \
		DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f) \
	}

#define DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f) \
	if (p_a.type != p_b.type)                                                                                    \
		_RETURN_FAIL                                                                                             \
                                                                                                                 \
	const PoolVector<m_type> &array_a = *reinterpret_cast<const PoolVector<m_type> *>(p_a._data._mem);           \
	const PoolVector<m_type> &array_b = *reinterpret_cast<const PoolVector<m_type> *>(p_b._data._mem);           \
                                                                                                                 \
	int a_len = array_a.size();                                                                                  \
	if (a_len m_opa array_b.size()) {                                                                            \
		_RETURN(m_ret_s);                                                                                        \
	} else {                                                                                                     \
		PoolVector<m_type>::Read ra = array_a.read();                                                            \
		PoolVector<m_type>::Read rb = array_b.read();                                                            \
                                                                                                                 \
		for (int i = 0; i < a_len; i++) {                                                                        \
			if (ra[i] m_opb rb[i])                                                                               \
				_RETURN(m_ret_f);                                                                                \
		}                                                                                                        \
                                                                                                                 \
		_RETURN(m_ret_def);                                                                                      \
	}

#define DEFAULT_OP_ARRAY_ADD(m_prefix, m_op_name, m_name, m_type)                                          \
	CASE_TYPE(m_prefix, m_op_name, m_name) {                                                               \
		if (p_a.type != p_b.type)                                                                          \
			_RETURN_FAIL;                                                                                  \
                                                                                                           \
		const PoolVector<m_type> &array_a = *reinterpret_cast<const PoolVector<m_type> *>(p_a._data._mem); \
		const PoolVector<m_type> &array_b = *reinterpret_cast<const PoolVector<m_type> *>(p_b._data._mem); \
		PoolVector<m_type> sum = array_a;                                                                  \
		sum.append_array(array_b);                                                                         \
		_RETURN(sum);                                                                                      \
	}

// Stores a set of axis mappings for 2 component vector swizzling.
// e.g. for `yz`, `x = AXIS_Y` and `y = AXIS_Z`.
struct SwizzleAxes2 {
	Vector3::Axis x;
	Vector3::Axis y;
	SwizzleAxes2(Vector3::Axis x, Vector3::Axis y) :
			x(x), y(y) {}
};
// Gets the `SwizzleAxes2` for the given `StringName`.
static SwizzleAxes2 get_swizzle2(const StringName p_swizzle, bool &r_valid) {
	r_valid = true;
	if (p_swizzle == CoreStringNames::singleton->xy) {
		return SwizzleAxes2(Vector3::AXIS_X, Vector3::AXIS_Y);
	} else if (p_swizzle == CoreStringNames::singleton->xz) {
		return SwizzleAxes2(Vector3::AXIS_X, Vector3::AXIS_Z);
	} else if (p_swizzle == CoreStringNames::singleton->yx) {
		return SwizzleAxes2(Vector3::AXIS_Y, Vector3::AXIS_X);
	} else if (p_swizzle == CoreStringNames::singleton->yz) {
		return SwizzleAxes2(Vector3::AXIS_Y, Vector3::AXIS_Z);
	} else if (p_swizzle == CoreStringNames::singleton->zx) {
		return SwizzleAxes2(Vector3::AXIS_Z, Vector3::AXIS_X);
	} else if (p_swizzle == CoreStringNames::singleton->zy) {
		return SwizzleAxes2(Vector3::AXIS_Z, Vector3::AXIS_Y);
	} else {
		r_valid = false;
		return SwizzleAxes2(Vector3::AXIS_X, Vector3::AXIS_X);
	}
}

void Variant::evaluate(const Operator &p_op, const Variant &p_a,
		const Variant &p_b, Variant &r_ret, bool &r_valid) {
	CASES(math);
	r_valid = true;

	SWITCH(math, p_op, p_a.type) {
		SWITCH_OP(math, OP_EQUAL, p_a.type) {
			CASE_TYPE(math, OP_EQUAL, NIL) {
				if (p_b.type == NIL)
					_RETURN(true);
				if (p_b.type == OBJECT)
					_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_b) == nullptr);

				_RETURN(false);
			}

			CASE_TYPE(math, OP_EQUAL, BOOL) {
				if (p_b.type != BOOL) {
					if (p_b.type == NIL)
						_RETURN(false);
					_RETURN_FAIL;
				}

				_RETURN(p_a._data._bool == p_b._data._bool);
			}

			CASE_TYPE(math, OP_EQUAL, OBJECT) {
				if (p_b.type == OBJECT)
					_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) == _UNSAFE_OBJ_PROXY_PTR(p_b));
				if (p_b.type == NIL)
					_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) == nullptr);

				_RETURN_FAIL;
			}

			CASE_TYPE(math, OP_EQUAL, DICTIONARY) {
				if (p_b.type != DICTIONARY) {
					if (p_b.type == NIL)
						_RETURN(false);
					_RETURN_FAIL;
				}

				const Dictionary *arr_a = reinterpret_cast<const Dictionary *>(p_a._data._mem);
				const Dictionary *arr_b = reinterpret_cast<const Dictionary *>(p_b._data._mem);

				_RETURN(*arr_a == *arr_b);
			}

			CASE_TYPE(math, OP_EQUAL, ARRAY) {
				if (p_b.type != ARRAY) {
					if (p_b.type == NIL)
						_RETURN(false);
					_RETURN_FAIL;
				}
				const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
				const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

				int l = arr_a->size();
				if (arr_b->size() != l)
					_RETURN(false);
				for (int i = 0; i < l; i++) {
					if (!((*arr_a)[i] == (*arr_b)[i])) {
						_RETURN(false);
					}
				}

				_RETURN(true);
			}

			DEFAULT_OP_NUM_NULL(math, OP_EQUAL, INT, ==, _int);
			DEFAULT_OP_NUM_NULL(math, OP_EQUAL, REAL, ==, _real);
			DEFAULT_OP_STR_NULL(math, OP_EQUAL, STRING, ==, String);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, VECTOR2, ==, Vector2);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, RECT2, ==, Rect2);
			DEFAULT_OP_PTRREF_NULL(math, OP_EQUAL, TRANSFORM2D, ==, _transform2d);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, VECTOR3, ==, Vector3);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, PLANE, ==, Plane);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, QUAT, ==, Quat);
			DEFAULT_OP_PTRREF_NULL(math, OP_EQUAL, AABB, ==, _aabb);
			DEFAULT_OP_PTRREF_NULL(math, OP_EQUAL, BASIS, ==, _basis);
			DEFAULT_OP_PTRREF_NULL(math, OP_EQUAL, TRANSFORM, ==, _transform);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, COLOR, ==, Color);
			DEFAULT_OP_STR_NULL(math, OP_EQUAL, NODE_PATH, ==, NodePath);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_EQUAL, _RID, ==, RID);

			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_BYTE_ARRAY, uint8_t);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_INT_ARRAY, int);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_REAL_ARRAY, real_t);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_STRING_ARRAY, String);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_VECTOR2_ARRAY, Vector2);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_VECTOR3_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_EQ(math, OP_EQUAL, POOL_COLOR_ARRAY, Color);
		}

		SWITCH_OP(math, OP_NOT_EQUAL, p_a.type) {
			CASE_TYPE(math, OP_NOT_EQUAL, NIL) {
				if (p_b.type == NIL)
					_RETURN(false);
				if (p_b.type == OBJECT)
					_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_b) != nullptr);

				_RETURN(true);
			}

			CASE_TYPE(math, OP_NOT_EQUAL, BOOL) {
				if (p_b.type != BOOL) {
					if (p_b.type == NIL)
						_RETURN(true);

					_RETURN_FAIL;
				}

				_RETURN(p_a._data._bool != p_b._data._bool);
			}

			CASE_TYPE(math, OP_NOT_EQUAL, OBJECT) {
				if (p_b.type == OBJECT)
					_RETURN((_UNSAFE_OBJ_PROXY_PTR(p_a) != _UNSAFE_OBJ_PROXY_PTR(p_b)));
				if (p_b.type == NIL)
					_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) != nullptr);

				_RETURN_FAIL;
			}

			CASE_TYPE(math, OP_NOT_EQUAL, DICTIONARY) {
				if (p_b.type != DICTIONARY) {
					if (p_b.type == NIL)
						_RETURN(true);
					_RETURN_FAIL;
				}

				const Dictionary *arr_a = reinterpret_cast<const Dictionary *>(p_a._data._mem);
				const Dictionary *arr_b = reinterpret_cast<const Dictionary *>(p_b._data._mem);

				_RETURN(*arr_a != *arr_b);
			}

			CASE_TYPE(math, OP_NOT_EQUAL, ARRAY) {
				if (p_b.type != ARRAY) {
					if (p_b.type == NIL)
						_RETURN(true);

					_RETURN_FAIL;
				}

				const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
				const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

				int l = arr_a->size();
				if (arr_b->size() != l)
					_RETURN(true);
				for (int i = 0; i < l; i++) {
					if (((*arr_a)[i] != (*arr_b)[i])) {
						_RETURN(true);
					}
				}

				_RETURN(false);
			}

			DEFAULT_OP_NUM_NULL(math, OP_NOT_EQUAL, INT, !=, _int);
			DEFAULT_OP_NUM_NULL(math, OP_NOT_EQUAL, REAL, !=, _real);
			DEFAULT_OP_STR_NULL(math, OP_NOT_EQUAL, STRING, !=, String);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, VECTOR2, !=, Vector2);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, RECT2, !=, Rect2);
			DEFAULT_OP_PTRREF_NULL(math, OP_NOT_EQUAL, TRANSFORM2D, !=, _transform2d);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, VECTOR3, !=, Vector3);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, PLANE, !=, Plane);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, QUAT, !=, Quat);
			DEFAULT_OP_PTRREF_NULL(math, OP_NOT_EQUAL, AABB, !=, _aabb);
			DEFAULT_OP_PTRREF_NULL(math, OP_NOT_EQUAL, BASIS, !=, _basis);
			DEFAULT_OP_PTRREF_NULL(math, OP_NOT_EQUAL, TRANSFORM, !=, _transform);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, COLOR, !=, Color);
			DEFAULT_OP_STR_NULL(math, OP_NOT_EQUAL, NODE_PATH, !=, NodePath);
			DEFAULT_OP_LOCALMEM_NULL(math, OP_NOT_EQUAL, _RID, !=, RID);

			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_BYTE_ARRAY, uint8_t);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_INT_ARRAY, int);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_REAL_ARRAY, real_t);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_STRING_ARRAY, String);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_VECTOR2_ARRAY, Vector2);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_VECTOR3_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_NEQ(math, OP_NOT_EQUAL, POOL_COLOR_ARRAY, Color);
		}

		SWITCH_OP(math, OP_LESS, p_a.type) {
			CASE_TYPE(math, OP_LESS, BOOL) {
				if (p_b.type != BOOL)
					_RETURN_FAIL;

				if (p_a._data._bool == p_b._data._bool)
					_RETURN(false);

				if (p_a._data._bool && !p_b._data._bool)
					_RETURN(false);

				_RETURN(true);
			}

			CASE_TYPE(math, OP_LESS, OBJECT) {
				if (p_b.type != OBJECT)
					_RETURN_FAIL;
				_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) < _UNSAFE_OBJ_PROXY_PTR(p_b));
			}

			CASE_TYPE(math, OP_LESS, ARRAY) {
				if (p_b.type != ARRAY)
					_RETURN_FAIL;

				const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
				const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

				int l = arr_a->size();
				if (arr_b->size() < l)
					_RETURN(false);
				for (int i = 0; i < l; i++) {
					if (!((*arr_a)[i] < (*arr_b)[i])) {
						_RETURN(true);
					}
				}

				_RETURN(false);
			}

			DEFAULT_OP_NUM(math, OP_LESS, INT, <, _int);
			DEFAULT_OP_NUM(math, OP_LESS, REAL, <, _real);
			DEFAULT_OP_STR(math, OP_LESS, STRING, <, String);
			DEFAULT_OP_LOCALMEM(math, OP_LESS, VECTOR2, <, Vector2);
			DEFAULT_OP_LOCALMEM(math, OP_LESS, VECTOR3, <, Vector3);
			DEFAULT_OP_LOCALMEM(math, OP_LESS, _RID, <, RID);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_BYTE_ARRAY, uint8_t);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_INT_ARRAY, int);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_REAL_ARRAY, real_t);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_STRING_ARRAY, String);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_VECTOR2_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_VECTOR3_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_COLOR_ARRAY, Color);

			CASE_TYPE(math, OP_LESS, NIL)
			CASE_TYPE(math, OP_LESS, RECT2)
			CASE_TYPE(math, OP_LESS, TRANSFORM2D)
			CASE_TYPE(math, OP_LESS, PLANE)
			CASE_TYPE(math, OP_LESS, QUAT)
			CASE_TYPE(math, OP_LESS, AABB)
			CASE_TYPE(math, OP_LESS, BASIS)
			CASE_TYPE(math, OP_LESS, TRANSFORM)
			CASE_TYPE(math, OP_LESS, COLOR)
			CASE_TYPE(math, OP_LESS, NODE_PATH)
			CASE_TYPE(math, OP_LESS, DICTIONARY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_LESS_EQUAL, p_a.type) {
			CASE_TYPE(math, OP_LESS_EQUAL, OBJECT) {
				if (p_b.type != OBJECT)
					_RETURN_FAIL;
				_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) <= _UNSAFE_OBJ_PROXY_PTR(p_b));
			}

			DEFAULT_OP_NUM(math, OP_LESS_EQUAL, INT, <=, _int);
			DEFAULT_OP_NUM(math, OP_LESS_EQUAL, REAL, <=, _real);
			DEFAULT_OP_STR(math, OP_LESS_EQUAL, STRING, <=, String);
			DEFAULT_OP_LOCALMEM(math, OP_LESS_EQUAL, VECTOR2, <=, Vector2);
			DEFAULT_OP_LOCALMEM(math, OP_LESS_EQUAL, VECTOR3, <=, Vector3);
			DEFAULT_OP_LOCALMEM(math, OP_LESS_EQUAL, _RID, <=, RID);

			CASE_TYPE(math, OP_LESS_EQUAL, NIL)
			CASE_TYPE(math, OP_LESS_EQUAL, BOOL)
			CASE_TYPE(math, OP_LESS_EQUAL, RECT2)
			CASE_TYPE(math, OP_LESS_EQUAL, TRANSFORM2D)
			CASE_TYPE(math, OP_LESS_EQUAL, PLANE)
			CASE_TYPE(math, OP_LESS_EQUAL, QUAT)
			CASE_TYPE(math, OP_LESS_EQUAL, AABB)
			CASE_TYPE(math, OP_LESS_EQUAL, BASIS)
			CASE_TYPE(math, OP_LESS_EQUAL, TRANSFORM)
			CASE_TYPE(math, OP_LESS_EQUAL, COLOR)
			CASE_TYPE(math, OP_LESS_EQUAL, NODE_PATH)
			CASE_TYPE(math, OP_LESS_EQUAL, DICTIONARY)
			CASE_TYPE(math, OP_LESS_EQUAL, ARRAY)
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_BYTE_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_INT_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_REAL_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_STRING_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_VECTOR2_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_VECTOR3_ARRAY);
			CASE_TYPE(math, OP_LESS_EQUAL, POOL_COLOR_ARRAY);
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_GREATER, p_a.type) {
			CASE_TYPE(math, OP_GREATER, BOOL) {
				if (p_b.type != BOOL)
					_RETURN_FAIL;

				if (p_a._data._bool == p_b._data._bool)
					_RETURN(false);

				if (!p_a._data._bool && p_b._data._bool)
					_RETURN(false);

				_RETURN(true);
			}

			CASE_TYPE(math, OP_GREATER, OBJECT) {
				if (p_b.type != OBJECT)
					_RETURN_FAIL;
				_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) > _UNSAFE_OBJ_PROXY_PTR(p_b));
			}

			CASE_TYPE(math, OP_GREATER, ARRAY) {
				if (p_b.type != ARRAY)
					_RETURN_FAIL;

				const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
				const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

				int l = arr_a->size();
				if (arr_b->size() > l)
					_RETURN(false);
				for (int i = 0; i < l; i++) {
					if (((*arr_a)[i] < (*arr_b)[i])) {
						_RETURN(false);
					}
				}

				_RETURN(true);
			}

			DEFAULT_OP_NUM(math, OP_GREATER, INT, >, _int);
			DEFAULT_OP_NUM(math, OP_GREATER, REAL, >, _real);
			DEFAULT_OP_STR_REV(math, OP_GREATER, STRING, <, String);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER, VECTOR2, <, Vector2);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER, VECTOR3, <, Vector3);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER, _RID, <, RID);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_BYTE_ARRAY, uint8_t);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_INT_ARRAY, int);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_REAL_ARRAY, real_t);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_STRING_ARRAY, String);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_VECTOR2_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_VECTOR3_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_GT(math, OP_GREATER, POOL_COLOR_ARRAY, Color);

			CASE_TYPE(math, OP_GREATER, NIL)
			CASE_TYPE(math, OP_GREATER, RECT2)
			CASE_TYPE(math, OP_GREATER, TRANSFORM2D)
			CASE_TYPE(math, OP_GREATER, PLANE)
			CASE_TYPE(math, OP_GREATER, QUAT)
			CASE_TYPE(math, OP_GREATER, AABB)
			CASE_TYPE(math, OP_GREATER, BASIS)
			CASE_TYPE(math, OP_GREATER, TRANSFORM)
			CASE_TYPE(math, OP_GREATER, COLOR)
			CASE_TYPE(math, OP_GREATER, NODE_PATH)
			CASE_TYPE(math, OP_GREATER, DICTIONARY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_GREATER_EQUAL, p_a.type) {
			CASE_TYPE(math, OP_GREATER_EQUAL, OBJECT) {
				if (p_b.type != OBJECT)
					_RETURN_FAIL;
				_RETURN(_UNSAFE_OBJ_PROXY_PTR(p_a) >= _UNSAFE_OBJ_PROXY_PTR(p_b));
			}

			DEFAULT_OP_NUM(math, OP_GREATER_EQUAL, INT, >=, _int);
			DEFAULT_OP_NUM(math, OP_GREATER_EQUAL, REAL, >=, _real);
			DEFAULT_OP_STR_REV(math, OP_GREATER_EQUAL, STRING, <=, String);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER_EQUAL, VECTOR2, <=, Vector2);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER_EQUAL, VECTOR3, <=, Vector3);
			DEFAULT_OP_LOCALMEM_REV(math, OP_GREATER_EQUAL, _RID, <=, RID);

			CASE_TYPE(math, OP_GREATER_EQUAL, NIL)
			CASE_TYPE(math, OP_GREATER_EQUAL, BOOL)
			CASE_TYPE(math, OP_GREATER_EQUAL, RECT2)
			CASE_TYPE(math, OP_GREATER_EQUAL, TRANSFORM2D)
			CASE_TYPE(math, OP_GREATER_EQUAL, PLANE)
			CASE_TYPE(math, OP_GREATER_EQUAL, QUAT)
			CASE_TYPE(math, OP_GREATER_EQUAL, AABB)
			CASE_TYPE(math, OP_GREATER_EQUAL, BASIS)
			CASE_TYPE(math, OP_GREATER_EQUAL, TRANSFORM)
			CASE_TYPE(math, OP_GREATER_EQUAL, COLOR)
			CASE_TYPE(math, OP_GREATER_EQUAL, NODE_PATH)
			CASE_TYPE(math, OP_GREATER_EQUAL, DICTIONARY)
			CASE_TYPE(math, OP_GREATER_EQUAL, ARRAY)
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_BYTE_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_INT_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_REAL_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_STRING_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_VECTOR2_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_VECTOR3_ARRAY);
			CASE_TYPE(math, OP_GREATER_EQUAL, POOL_COLOR_ARRAY);
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_ADD, p_a.type) {
			CASE_TYPE(math, OP_ADD, ARRAY) {
				if (p_a.type != p_b.type)
					_RETURN_FAIL;

				const Array &array_a = *reinterpret_cast<const Array *>(p_a._data._mem);
				const Array &array_b = *reinterpret_cast<const Array *>(p_b._data._mem);
				Array sum;
				int asize = array_a.size();
				int bsize = array_b.size();
				sum.resize(asize + bsize);
				for (int i = 0; i < asize; i++) {
					sum[i] = array_a[i];
				}
				for (int i = 0; i < bsize; i++) {
					sum[i + asize] = array_b[i];
				}
				_RETURN(sum);
			}

			DEFAULT_OP_NUM(math, OP_ADD, INT, +, _int);
			DEFAULT_OP_NUM(math, OP_ADD, REAL, +, _real);
			DEFAULT_OP_STR(math, OP_ADD, STRING, +, String);
			DEFAULT_OP_LOCALMEM(math, OP_ADD, VECTOR2, +, Vector2);
			DEFAULT_OP_LOCALMEM(math, OP_ADD, VECTOR3, +, Vector3);
			DEFAULT_OP_LOCALMEM(math, OP_ADD, QUAT, +, Quat);
			DEFAULT_OP_LOCALMEM(math, OP_ADD, COLOR, +, Color);

			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_BYTE_ARRAY, uint8_t);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_INT_ARRAY, int);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_REAL_ARRAY, real_t);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_STRING_ARRAY, String);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_VECTOR2_ARRAY, Vector2);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_VECTOR3_ARRAY, Vector3);
			DEFAULT_OP_ARRAY_ADD(math, OP_ADD, POOL_COLOR_ARRAY, Color);

			CASE_TYPE(math, OP_ADD, NIL)
			CASE_TYPE(math, OP_ADD, BOOL)
			CASE_TYPE(math, OP_ADD, RECT2)
			CASE_TYPE(math, OP_ADD, TRANSFORM2D)
			CASE_TYPE(math, OP_ADD, PLANE)
			CASE_TYPE(math, OP_ADD, AABB)
			CASE_TYPE(math, OP_ADD, BASIS)
			CASE_TYPE(math, OP_ADD, TRANSFORM)
			CASE_TYPE(math, OP_ADD, NODE_PATH)
			CASE_TYPE(math, OP_ADD, _RID)
			CASE_TYPE(math, OP_ADD, OBJECT)
			CASE_TYPE(math, OP_ADD, DICTIONARY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_SUBTRACT, p_a.type) {
			DEFAULT_OP_NUM(math, OP_SUBTRACT, INT, -, _int);
			DEFAULT_OP_NUM(math, OP_SUBTRACT, REAL, -, _real);
			DEFAULT_OP_LOCALMEM(math, OP_SUBTRACT, VECTOR2, -, Vector2);
			DEFAULT_OP_LOCALMEM(math, OP_SUBTRACT, VECTOR3, -, Vector3);
			DEFAULT_OP_LOCALMEM(math, OP_SUBTRACT, QUAT, -, Quat);
			DEFAULT_OP_LOCALMEM(math, OP_SUBTRACT, COLOR, -, Color);

			CASE_TYPE(math, OP_SUBTRACT, NIL)
			CASE_TYPE(math, OP_SUBTRACT, BOOL)
			CASE_TYPE(math, OP_SUBTRACT, STRING)
			CASE_TYPE(math, OP_SUBTRACT, RECT2)
			CASE_TYPE(math, OP_SUBTRACT, TRANSFORM2D)
			CASE_TYPE(math, OP_SUBTRACT, PLANE)
			CASE_TYPE(math, OP_SUBTRACT, AABB)
			CASE_TYPE(math, OP_SUBTRACT, BASIS)
			CASE_TYPE(math, OP_SUBTRACT, TRANSFORM)
			CASE_TYPE(math, OP_SUBTRACT, NODE_PATH)
			CASE_TYPE(math, OP_SUBTRACT, _RID)
			CASE_TYPE(math, OP_SUBTRACT, OBJECT)
			CASE_TYPE(math, OP_SUBTRACT, DICTIONARY)
			CASE_TYPE(math, OP_SUBTRACT, ARRAY)
			CASE_TYPE(math, OP_SUBTRACT, POOL_BYTE_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_INT_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_REAL_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_STRING_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_VECTOR2_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_VECTOR3_ARRAY);
			CASE_TYPE(math, OP_SUBTRACT, POOL_COLOR_ARRAY);
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_MULTIPLY, p_a.type) {
			CASE_TYPE(math, OP_MULTIPLY, TRANSFORM2D) {
				switch (p_b.type) {
					case TRANSFORM2D: {
						_RETURN(*p_a._data._transform2d * *p_b._data._transform2d);
					}
					case VECTOR2: {
						_RETURN(p_a._data._transform2d->xform(*(const Vector2 *)p_b._data._mem));
					}
					default:
						_RETURN_FAIL;
				}
			}

			CASE_TYPE(math, OP_MULTIPLY, QUAT) {
				switch (p_b.type) {
					case VECTOR3: {
						_RETURN(reinterpret_cast<const Quat *>(p_a._data._mem)->xform(*(const Vector3 *)p_b._data._mem));
					}
					case QUAT: {
						_RETURN(*reinterpret_cast<const Quat *>(p_a._data._mem) * *reinterpret_cast<const Quat *>(p_b._data._mem));
					}
					case REAL: {
						_RETURN(*reinterpret_cast<const Quat *>(p_a._data._mem) * p_b._data._real);
					}
					default:
						_RETURN_FAIL;
				}
			}

			CASE_TYPE(math, OP_MULTIPLY, BASIS) {
				switch (p_b.type) {
					case VECTOR3: {
						_RETURN(p_a._data._basis->xform(*(const Vector3 *)p_b._data._mem));
					}
					case BASIS: {
						_RETURN(*p_a._data._basis * *p_b._data._basis);
					}
					default:
						_RETURN_FAIL;
				}
			}

			CASE_TYPE(math, OP_MULTIPLY, TRANSFORM) {
				switch (p_b.type) {
					case VECTOR3: {
						_RETURN(p_a._data._transform->xform(*(const Vector3 *)p_b._data._mem));
					}
					case TRANSFORM: {
						_RETURN(*p_a._data._transform * *p_b._data._transform);
					}
					default:
						_RETURN_FAIL;
				}
			}

			DEFAULT_OP_NUM_VEC(math, OP_MULTIPLY, INT, *, _int);
			DEFAULT_OP_NUM_VEC(math, OP_MULTIPLY, REAL, *, _real);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_MULTIPLY, VECTOR2, *, Vector2);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_MULTIPLY, VECTOR3, *, Vector3);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_MULTIPLY, COLOR, *, Color);

			CASE_TYPE(math, OP_MULTIPLY, NIL)
			CASE_TYPE(math, OP_MULTIPLY, BOOL)
			CASE_TYPE(math, OP_MULTIPLY, STRING)
			CASE_TYPE(math, OP_MULTIPLY, RECT2)
			CASE_TYPE(math, OP_MULTIPLY, PLANE)
			CASE_TYPE(math, OP_MULTIPLY, AABB)
			CASE_TYPE(math, OP_MULTIPLY, NODE_PATH)
			CASE_TYPE(math, OP_MULTIPLY, _RID)
			CASE_TYPE(math, OP_MULTIPLY, OBJECT)
			CASE_TYPE(math, OP_MULTIPLY, DICTIONARY)
			CASE_TYPE(math, OP_MULTIPLY, ARRAY)
			CASE_TYPE(math, OP_MULTIPLY, POOL_BYTE_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_INT_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_REAL_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_STRING_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_VECTOR2_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_VECTOR3_ARRAY);
			CASE_TYPE(math, OP_MULTIPLY, POOL_COLOR_ARRAY);
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_DIVIDE, p_a.type) {
			CASE_TYPE(math, OP_DIVIDE, QUAT) {
				if (p_b.type != REAL)
					_RETURN_FAIL;
#ifdef DEBUG_ENABLED
				if (p_b._data._real == 0) {
					r_valid = false;
					_RETURN("Division By Zero");
				}
#endif
				_RETURN(*reinterpret_cast<const Quat *>(p_a._data._mem) / p_b._data._real);
			}

			DEFAULT_OP_NUM_DIV(math, OP_DIVIDE, INT, _int);
			DEFAULT_OP_NUM_DIV(math, OP_DIVIDE, REAL, _real);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_DIVIDE, VECTOR2, /, Vector2);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_DIVIDE, VECTOR3, /, Vector3);
			DEFAULT_OP_LOCALMEM_NUM(math, OP_DIVIDE, COLOR, /, Color);

			CASE_TYPE(math, OP_DIVIDE, NIL)
			CASE_TYPE(math, OP_DIVIDE, BOOL)
			CASE_TYPE(math, OP_DIVIDE, STRING)
			CASE_TYPE(math, OP_DIVIDE, RECT2)
			CASE_TYPE(math, OP_DIVIDE, TRANSFORM2D)
			CASE_TYPE(math, OP_DIVIDE, PLANE)
			CASE_TYPE(math, OP_DIVIDE, AABB)
			CASE_TYPE(math, OP_DIVIDE, BASIS)
			CASE_TYPE(math, OP_DIVIDE, TRANSFORM)
			CASE_TYPE(math, OP_DIVIDE, NODE_PATH)
			CASE_TYPE(math, OP_DIVIDE, _RID)
			CASE_TYPE(math, OP_DIVIDE, OBJECT)
			CASE_TYPE(math, OP_DIVIDE, DICTIONARY)
			CASE_TYPE(math, OP_DIVIDE, ARRAY)
			CASE_TYPE(math, OP_DIVIDE, POOL_BYTE_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_INT_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_REAL_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_STRING_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_VECTOR2_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_VECTOR3_ARRAY);
			CASE_TYPE(math, OP_DIVIDE, POOL_COLOR_ARRAY);
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_POSITIVE, p_a.type) {
			DEFAULT_OP_NUM_POS(math, OP_POSITIVE, INT, _int);
			DEFAULT_OP_NUM_POS(math, OP_POSITIVE, REAL, _real);
			DEFAULT_OP_LOCALMEM_POS(math, OP_POSITIVE, VECTOR3, Vector3);
			DEFAULT_OP_LOCALMEM_POS(math, OP_POSITIVE, PLANE, Plane);
			DEFAULT_OP_LOCALMEM_POS(math, OP_POSITIVE, QUAT, Quat);
			DEFAULT_OP_LOCALMEM_POS(math, OP_POSITIVE, VECTOR2, Vector2);

			CASE_TYPE(math, OP_POSITIVE, NIL)
			CASE_TYPE(math, OP_POSITIVE, BOOL)
			CASE_TYPE(math, OP_POSITIVE, STRING)
			CASE_TYPE(math, OP_POSITIVE, RECT2)
			CASE_TYPE(math, OP_POSITIVE, TRANSFORM2D)
			CASE_TYPE(math, OP_POSITIVE, AABB)
			CASE_TYPE(math, OP_POSITIVE, BASIS)
			CASE_TYPE(math, OP_POSITIVE, TRANSFORM)
			CASE_TYPE(math, OP_POSITIVE, COLOR)
			CASE_TYPE(math, OP_POSITIVE, NODE_PATH)
			CASE_TYPE(math, OP_POSITIVE, _RID)
			CASE_TYPE(math, OP_POSITIVE, OBJECT)
			CASE_TYPE(math, OP_POSITIVE, DICTIONARY)
			CASE_TYPE(math, OP_POSITIVE, ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_BYTE_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_INT_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_REAL_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_STRING_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_VECTOR2_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_VECTOR3_ARRAY)
			CASE_TYPE(math, OP_POSITIVE, POOL_COLOR_ARRAY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_NEGATE, p_a.type) {
			DEFAULT_OP_NUM_NEG(math, OP_NEGATE, INT, _int);
			DEFAULT_OP_NUM_NEG(math, OP_NEGATE, REAL, _real);

			DEFAULT_OP_LOCALMEM_NEG(math, OP_NEGATE, VECTOR2, Vector2);
			DEFAULT_OP_LOCALMEM_NEG(math, OP_NEGATE, VECTOR3, Vector3);
			DEFAULT_OP_LOCALMEM_NEG(math, OP_NEGATE, PLANE, Plane);
			DEFAULT_OP_LOCALMEM_NEG(math, OP_NEGATE, QUAT, Quat);
			DEFAULT_OP_LOCALMEM_NEG(math, OP_NEGATE, COLOR, Color);

			CASE_TYPE(math, OP_NEGATE, NIL)
			CASE_TYPE(math, OP_NEGATE, BOOL)
			CASE_TYPE(math, OP_NEGATE, STRING)
			CASE_TYPE(math, OP_NEGATE, RECT2)
			CASE_TYPE(math, OP_NEGATE, TRANSFORM2D)
			CASE_TYPE(math, OP_NEGATE, AABB)
			CASE_TYPE(math, OP_NEGATE, BASIS)
			CASE_TYPE(math, OP_NEGATE, TRANSFORM)
			CASE_TYPE(math, OP_NEGATE, NODE_PATH)
			CASE_TYPE(math, OP_NEGATE, _RID)
			CASE_TYPE(math, OP_NEGATE, OBJECT)
			CASE_TYPE(math, OP_NEGATE, DICTIONARY)
			CASE_TYPE(math, OP_NEGATE, ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_BYTE_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_INT_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_REAL_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_STRING_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_VECTOR2_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_VECTOR3_ARRAY)
			CASE_TYPE(math, OP_NEGATE, POOL_COLOR_ARRAY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_MODULE, p_a.type) {
			CASE_TYPE(math, OP_MODULE, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
#ifdef DEBUG_ENABLED
				if (p_b._data._int == 0) {
					r_valid = false;
					_RETURN("Division By Zero");
				}
#endif
				_RETURN(p_a._data._int % p_b._data._int);
			}

			CASE_TYPE(math, OP_MODULE, STRING) {
				const String *format = reinterpret_cast<const String *>(p_a._data._mem);

				String result;
				bool error;
				if (p_b.type == ARRAY) {
					// e.g. "frog %s %d" % ["fish", 12]
					const Array *args = reinterpret_cast<const Array *>(p_b._data._mem);
					result = format->sprintf(*args, &error);
				} else {
					// e.g. "frog %d" % 12
					Array args;
					args.push_back(p_b);
					result = format->sprintf(args, &error);
				}
				r_valid = !error;
				_RETURN(result);
			}

			CASE_TYPE(math, OP_MODULE, NIL)
			CASE_TYPE(math, OP_MODULE, BOOL)
			CASE_TYPE(math, OP_MODULE, REAL)
			CASE_TYPE(math, OP_MODULE, VECTOR2)
			CASE_TYPE(math, OP_MODULE, RECT2)
			CASE_TYPE(math, OP_MODULE, VECTOR3)
			CASE_TYPE(math, OP_MODULE, TRANSFORM2D)
			CASE_TYPE(math, OP_MODULE, PLANE)
			CASE_TYPE(math, OP_MODULE, QUAT)
			CASE_TYPE(math, OP_MODULE, AABB)
			CASE_TYPE(math, OP_MODULE, BASIS)
			CASE_TYPE(math, OP_MODULE, TRANSFORM)
			CASE_TYPE(math, OP_MODULE, COLOR)
			CASE_TYPE(math, OP_MODULE, NODE_PATH)
			CASE_TYPE(math, OP_MODULE, _RID)
			CASE_TYPE(math, OP_MODULE, OBJECT)
			CASE_TYPE(math, OP_MODULE, DICTIONARY)
			CASE_TYPE(math, OP_MODULE, ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_BYTE_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_INT_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_REAL_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_STRING_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_VECTOR2_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_VECTOR3_ARRAY)
			CASE_TYPE(math, OP_MODULE, POOL_COLOR_ARRAY)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_STRING_CONCAT, p_a.type) {
			CASE_TYPE_ALL(math, OP_STRING_CONCAT)

			_RETURN(p_a.operator String() + p_b.operator String());
		}

		SWITCH_OP(math, OP_SHIFT_LEFT, p_a.type) {
			CASE_TYPE(math, OP_SHIFT_LEFT, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
				if (p_b._data._int < 0 || p_b._data._int >= 64)
					_RETURN_FAIL;
				_RETURN(p_a._data._int << p_b._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_SHIFT_LEFT)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_SHIFT_RIGHT, p_a.type) {
			CASE_TYPE(math, OP_SHIFT_RIGHT, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
				if (p_b._data._int < 0 || p_b._data._int >= 64)
					_RETURN_FAIL;
				_RETURN(p_a._data._int >> p_b._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_SHIFT_RIGHT)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_BIT_AND, p_a.type) {
			CASE_TYPE(math, OP_BIT_AND, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
				_RETURN(p_a._data._int & p_b._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_BIT_AND)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_BIT_OR, p_a.type) {
			CASE_TYPE(math, OP_BIT_OR, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
				_RETURN(p_a._data._int | p_b._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_BIT_OR)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_BIT_XOR, p_a.type) {
			CASE_TYPE(math, OP_BIT_XOR, INT) {
				if (p_b.type != INT)
					_RETURN_FAIL;
				_RETURN(p_a._data._int ^ p_b._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_BIT_XOR)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_BIT_NEGATE, p_a.type) {
			CASE_TYPE(math, OP_BIT_NEGATE, INT) {
				_RETURN(~p_a._data._int);
			}

			CASE_TYPE_ALL_BUT_INT(math, OP_BIT_NEGATE)
			_RETURN_FAIL;
		}

		SWITCH_OP(math, OP_AND, p_a.type) {
			CASE_TYPE_ALL(math, OP_AND) {
				bool l = p_a.booleanize();
				bool r = p_b.booleanize();

				_RETURN(l && r);
			}
		}

		SWITCH_OP(math, OP_OR, p_a.type) {
			CASE_TYPE_ALL(math, OP_OR) {
				bool l = p_a.booleanize();
				bool r = p_b.booleanize();

				_RETURN(l || r);
			}
		}

		SWITCH_OP(math, OP_XOR, p_a.type) {
			CASE_TYPE_ALL(math, OP_XOR) {
				bool l = p_a.booleanize();
				bool r = p_b.booleanize();

				_RETURN((l || r) && !(l && r));
			}
		}

		SWITCH_OP(math, OP_NOT, p_a.type) {
			CASE_TYPE_ALL(math, OP_NOT) {
				bool l = p_a.booleanize();
				_RETURN(!l);
			}
		}

		SWITCH_OP(math, OP_IN, p_a.type) {
			CASE_TYPE_ALL(math, OP_IN)
			_RETURN(p_b.in(p_a, &r_valid));
		}
	}
}

void Variant::set_named(const StringName &p_index, const Variant &p_value, bool *r_valid) {
	bool valid = false;
	switch (type) {
		case VECTOR2: {
			if (p_value.type == Variant::INT) {
				Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._int;
					valid = true;
				}
			} else if (p_value.type == Variant::REAL) {
				Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._real;
					valid = true;
				}
			}

		} break;
		case RECT2: {
			if (p_value.type == Variant::VECTOR2) {
				Rect2 *v = reinterpret_cast<Rect2 *>(_data._mem);
				//scalar name
				if (p_index == CoreStringNames::singleton->position) {
					v->position = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->size) {
					v->size = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->end) {
					v->size = *reinterpret_cast<const Vector2 *>(p_value._data._mem) - v->position;
					valid = true;
				}
			}
		} break;
		case TRANSFORM2D: {
			if (p_value.type == Variant::VECTOR2) {
				Transform2D *v = _data._transform2d;
				if (p_index == CoreStringNames::singleton->x) {
					v->elements[0] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->elements[1] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->origin) {
					v->elements[2] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
					valid = true;
				}
			}

		} break;
		case VECTOR3: {
			if (p_value.type == Variant::INT) {
				Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->z = p_value._data._int;
					valid = true;
				}
			} else if (p_value.type == Variant::REAL) {
				Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->z = p_value._data._real;
					valid = true;
				}
			} else if (p_value.type == Variant::VECTOR2) {
				// 2 component vector swizzling.
				bool swizzle_valid;
				SwizzleAxes2 axes = get_swizzle2(p_index, swizzle_valid);
				if (!swizzle_valid) {
					break;
				}

				valid = true;
				Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
				const Vector2 vec2 = p_value;
				v->set_axis(axes.x, vec2.x);
				v->set_axis(axes.y, vec2.y);
			}

		} break;
		case PLANE: {
			if (p_value.type == Variant::INT) {
				Plane *v = reinterpret_cast<Plane *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->normal.x = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->normal.y = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->normal.z = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->d) {
					v->d = p_value._data._int;
					valid = true;
				}
			} else if (p_value.type == Variant::REAL) {
				Plane *v = reinterpret_cast<Plane *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->normal.x = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->normal.y = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->normal.z = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->d) {
					v->d = p_value._data._real;
					valid = true;
				}

			} else if (p_value.type == Variant::VECTOR3) {
				Plane *v = reinterpret_cast<Plane *>(_data._mem);
				if (p_index == CoreStringNames::singleton->normal) {
					v->normal = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
					valid = true;
				}
			}

		} break;
		case QUAT: {
			if (p_value.type == Variant::INT) {
				Quat *v = reinterpret_cast<Quat *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->z = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->w) {
					v->w = p_value._data._int;
					valid = true;
				}
			} else if (p_value.type == Variant::REAL) {
				Quat *v = reinterpret_cast<Quat *>(_data._mem);
				if (p_index == CoreStringNames::singleton->x) {
					v->x = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->y = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->z = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->w) {
					v->w = p_value._data._real;
					valid = true;
				}
			}

		} break; // 10
		case AABB: {
			if (p_value.type == Variant::VECTOR3) {
				::AABB *v = _data._aabb;
				//scalar name
				if (p_index == CoreStringNames::singleton->position) {
					v->position = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->size) {
					v->size = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->end) {
					v->size = *reinterpret_cast<const Vector3 *>(p_value._data._mem) - v->position;
					valid = true;
				}
			}
		} break;
		case BASIS: {
			if (p_value.type == Variant::VECTOR3) {
				Basis *v = _data._basis;
				//scalar name
				if (p_index == CoreStringNames::singleton->x) {
					v->set_axis(0, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
					valid = true;
				} else if (p_index == CoreStringNames::singleton->y) {
					v->set_axis(1, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
					valid = true;
				} else if (p_index == CoreStringNames::singleton->z) {
					v->set_axis(2, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
					valid = true;
				}
			}
		} break;
		case TRANSFORM: {
			if (p_value.type == Variant::BASIS && p_index == CoreStringNames::singleton->basis) {
				_data._transform->basis = *p_value._data._basis;
				valid = true;
			} else if (p_value.type == Variant::VECTOR3 && p_index == CoreStringNames::singleton->origin) {
				_data._transform->origin = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
				valid = true;
			}

		} break;
		case COLOR: {
			if (p_value.type == Variant::INT) {
				Color *v = reinterpret_cast<Color *>(_data._mem);
				if (p_index == CoreStringNames::singleton->r) {
					v->r = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->g) {
					v->g = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->b) {
					v->b = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->a) {
					v->a = p_value._data._int;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->r8) {
					v->r = p_value._data._int / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->g8) {
					v->g = p_value._data._int / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->b8) {
					v->b = p_value._data._int / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->a8) {
					v->a = p_value._data._int / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->h) {
					v->set_hsv(p_value._data._int, v->get_s(), v->get_v(), v->a);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->s) {
					v->set_hsv(v->get_h(), p_value._data._int, v->get_v(), v->a);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->v) {
					v->set_hsv(v->get_h(), v->get_s(), p_value._data._int, v->a);
					valid = true;
				}
			} else if (p_value.type == Variant::REAL) {
				Color *v = reinterpret_cast<Color *>(_data._mem);
				if (p_index == CoreStringNames::singleton->r) {
					v->r = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->g) {
					v->g = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->b) {
					v->b = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->a) {
					v->a = p_value._data._real;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->r8) {
					v->r = p_value._data._real / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->g8) {
					v->g = p_value._data._real / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->b8) {
					v->b = p_value._data._real / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->a8) {
					v->a = p_value._data._real / 255.0;
					valid = true;
				} else if (p_index == CoreStringNames::singleton->h) {
					v->set_hsv(p_value._data._real, v->get_s(), v->get_v(), v->a);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->s) {
					v->set_hsv(v->get_h(), p_value._data._real, v->get_v(), v->a);
					valid = true;
				} else if (p_index == CoreStringNames::singleton->v) {
					v->set_hsv(v->get_h(), v->get_s(), p_value._data._real, v->a);
					valid = true;
				}
			}
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted set on a deleted object.");
				}
#endif
				break;
			}
			obj->set(p_index, p_value, &valid);

		} break;
		default: {
			set(p_index.operator String(), p_value, &valid);
		} break;
	}

	if (r_valid) {
		*r_valid = valid;
	}
}

Variant Variant::get_named(const StringName &p_index, bool *r_valid) const {
	if (r_valid) {
		*r_valid = true;
	}
	switch (type) {
		case VECTOR2: {
			const Vector2 *v = reinterpret_cast<const Vector2 *>(_data._mem);
			if (p_index == CoreStringNames::singleton->x) {
				return v->x;
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->y;
			}

		} break;
		case RECT2: {
			const Rect2 *v = reinterpret_cast<const Rect2 *>(_data._mem);
			//scalar name
			if (p_index == CoreStringNames::singleton->position) {
				return v->position;
			} else if (p_index == CoreStringNames::singleton->size) {
				return v->size;
			} else if (p_index == CoreStringNames::singleton->end) {
				return v->size + v->position;
			}
		} break;
		case TRANSFORM2D: {
			const Transform2D *v = _data._transform2d;
			if (p_index == CoreStringNames::singleton->x) {
				return v->elements[0];
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->elements[1];
			} else if (p_index == CoreStringNames::singleton->origin) {
				return v->elements[2];
			}

		} break;
		case VECTOR3: {
			const Vector3 *v = reinterpret_cast<const Vector3 *>(_data._mem);
			if (p_index == CoreStringNames::singleton->x) {
				return v->x;
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->y;
			} else if (p_index == CoreStringNames::singleton->z) {
				return v->z;
			} else {
				// 2 component vector swizzling.
				bool swizzle_valid;
				SwizzleAxes2 axes = get_swizzle2(p_index, swizzle_valid);
				if (!swizzle_valid) {
					break;
				}

				return Vector2(v->get_axis(axes.x), v->get_axis(axes.y));
			}

		} break;
		case PLANE: {
			const Plane *v = reinterpret_cast<const Plane *>(_data._mem);
			if (p_index == CoreStringNames::singleton->x) {
				return v->normal.x;
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->normal.y;
			} else if (p_index == CoreStringNames::singleton->z) {
				return v->normal.z;
			} else if (p_index == CoreStringNames::singleton->d) {
				return v->d;
			} else if (p_index == CoreStringNames::singleton->normal) {
				return v->normal;
			}

		} break;
		case QUAT: {
			const Quat *v = reinterpret_cast<const Quat *>(_data._mem);
			if (p_index == CoreStringNames::singleton->x) {
				return v->x;
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->y;
			} else if (p_index == CoreStringNames::singleton->z) {
				return v->z;
			} else if (p_index == CoreStringNames::singleton->w) {
				return v->w;
			}

		} break; // 10
		case AABB: {
			const ::AABB *v = _data._aabb;
			//scalar name
			if (p_index == CoreStringNames::singleton->position) {
				return v->position;
			} else if (p_index == CoreStringNames::singleton->size) {
				return v->size;
			} else if (p_index == CoreStringNames::singleton->end) {
				return v->size + v->position;
			}
		} break;
		case BASIS: {
			const Basis *v = _data._basis;
			//scalar name
			if (p_index == CoreStringNames::singleton->x) {
				return v->get_axis(0);
			} else if (p_index == CoreStringNames::singleton->y) {
				return v->get_axis(1);
			} else if (p_index == CoreStringNames::singleton->z) {
				return v->get_axis(2);
			}

		} break;
		case TRANSFORM: {
			if (p_index == CoreStringNames::singleton->basis) {
				return _data._transform->basis;
			} else if (p_index == CoreStringNames::singleton->origin) {
				return _data._transform->origin;
			}

		} break;
		case COLOR: {
			const Color *v = reinterpret_cast<const Color *>(_data._mem);
			if (p_index == CoreStringNames::singleton->r) {
				return v->r;
			} else if (p_index == CoreStringNames::singleton->g) {
				return v->g;
			} else if (p_index == CoreStringNames::singleton->b) {
				return v->b;
			} else if (p_index == CoreStringNames::singleton->a) {
				return v->a;
			} else if (p_index == CoreStringNames::singleton->r8) {
				return int(Math::round(v->r * 255.0));
			} else if (p_index == CoreStringNames::singleton->g8) {
				return int(Math::round(v->g * 255.0));
			} else if (p_index == CoreStringNames::singleton->b8) {
				return int(Math::round(v->b * 255.0));
			} else if (p_index == CoreStringNames::singleton->a8) {
				return int(Math::round(v->a * 255.0));
			} else if (p_index == CoreStringNames::singleton->h) {
				return v->get_h();
			} else if (p_index == CoreStringNames::singleton->s) {
				return v->get_s();
			} else if (p_index == CoreStringNames::singleton->v) {
				return v->get_v();
			}
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				if (r_valid) {
					*r_valid = false;
				}
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted get on a deleted object.");
				}
#endif
				return Variant();
			}

			return obj->get(p_index, r_valid);

		} break;
		default: {
			return get(p_index.operator String(), r_valid);
		}
	}

	if (r_valid) {
		*r_valid = false;
	}
	return Variant();
}

#define DEFAULT_OP_ARRAY_CMD(m_name, m_type, skip_test, cmd)                             \
	case m_name: {                                                                       \
		skip_test;                                                                       \
                                                                                         \
		if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) { \
			int index = p_index;                                                         \
			m_type *arr = reinterpret_cast<m_type *>(_data._mem);                        \
                                                                                         \
			if (index < 0)                                                               \
				index += arr->size();                                                    \
			if (index >= 0 && index < arr->size()) {                                     \
				valid = true;                                                            \
				cmd;                                                                     \
			}                                                                            \
		}                                                                                \
	} break;

// clang-format 14 wants to add a space after the last return,
// and clang-format 15 removes it...
/* clang-format off */
#define DEFAULT_OP_DVECTOR_SET(m_name, dv_type, skip_cond) \
	DEFAULT_OP_ARRAY_CMD(m_name, PoolVector<dv_type>, if (skip_cond) return;, arr->set(index, p_value); return)
/* clang-format on */

#define DEFAULT_OP_DVECTOR_GET(m_name, dv_type) \
	DEFAULT_OP_ARRAY_CMD(m_name, const PoolVector<dv_type>, ;, return arr->get(index))

void Variant::set(const Variant &p_index, const Variant &p_value, bool *r_valid) {
	static bool _dummy = false;

	bool &valid = r_valid ? *r_valid : _dummy;
	valid = false;

	switch (type) {
		case NIL: {
			return;
		} break;
		case BOOL: {
			return;
		} break;
		case INT: {
			return;
		} break;
		case REAL: {
			return;
		} break;
		case STRING: {
			if (p_index.type != Variant::INT && p_index.type != Variant::REAL) {
				return;
			}

			int idx = p_index;
			String *str = reinterpret_cast<String *>(_data._mem);
			int len = str->length();
			if (idx < 0) {
				idx += len;
			}
			if (idx < 0 || idx >= len) {
				return;
			}

			String chr;
			if (p_value.type == Variant::INT || p_value.type == Variant::REAL) {
				chr = String::chr(p_value);
			} else if (p_value.type == Variant::STRING) {
				chr = p_value;
			} else {
				return;
			}

			*str = str->substr(0, idx) + chr + str->substr(idx + 1, len);
			valid = true;
			return;

		} break;
		case VECTOR2: {
			if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
				return;
			}

			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				// scalar index
				int idx = p_index;

				if (idx < 0) {
					idx += 2;
				}
				if (idx >= 0 && idx < 2) {
					Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
					valid = true;
					(*v)[idx] = p_value;
					return;
				}
			} else if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
				if (*str == "x") {
					valid = true;
					v->x = p_value;
					return;
				} else if (*str == "y") {
					valid = true;
					v->y = p_value;
					return;
				}
			}

		} break; // 5
		case RECT2: {
			if (p_value.type != Variant::VECTOR2) {
				return;
			}

			if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Rect2 *v = reinterpret_cast<Rect2 *>(_data._mem);
				if (*str == "position") {
					valid = true;
					v->position = p_value;
					return;
				} else if (*str == "size") {
					valid = true;
					v->size = p_value;
					return;
				} else if (*str == "end") {
					valid = true;
					v->size = Vector2(p_value) - v->position;
					return;
				}
			}
		} break;
		case TRANSFORM2D: {
			if (p_value.type != Variant::VECTOR2) {
				return;
			}

			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;

				if (index < 0) {
					index += 3;
				}
				if (index >= 0 && index < 3) {
					Transform2D *v = _data._transform2d;

					valid = true;
					v->elements[index] = p_value;
					return;
				}
			} else if (p_index.get_type() == Variant::STRING && p_value.get_type() == Variant::VECTOR2) {
				//scalar name
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Transform2D *v = _data._transform2d;
				if (*str == "x") {
					valid = true;
					v->elements[0] = p_value;
					return;
				} else if (*str == "y") {
					valid = true;
					v->elements[1] = p_value;
					return;
				} else if (*str == "origin") {
					valid = true;
					v->elements[2] = p_value;
					return;
				}
			}

		} break;
		case VECTOR3: {
			if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
				return;
			}

			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				//scalar index
				int idx = p_index;
				if (idx < 0) {
					idx += 3;
				}
				if (idx >= 0 && idx < 3) {
					Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
					valid = true;
					(*v)[idx] = p_value;
					return;
				}
			} else if (p_index.get_type() == Variant::STRING) {
				//scalar name
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
				if (*str == "x") {
					valid = true;
					v->x = p_value;
					return;
				} else if (*str == "y") {
					valid = true;
					v->y = p_value;
					return;
				} else if (*str == "z") {
					valid = true;
					v->z = p_value;
					return;
				} else {
					// 2 component vector swizzling.
					bool swizzle_valid;
					SwizzleAxes2 axes = get_swizzle2(*str, swizzle_valid);
					if (!swizzle_valid) {
						break;
					}

					valid = true;
					const Vector2 vec2 = p_value;
					v->set_axis(axes.x, vec2.x);
					v->set_axis(axes.y, vec2.y);
					return;
				}
			}

		} break;
		case PLANE: {
			if (p_index.get_type() == Variant::STRING) {
				//scalar name
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Plane *v = reinterpret_cast<Plane *>(_data._mem);
				if (*str == "x") {
					if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
						return;
					}

					valid = true;
					v->normal.x = p_value;
					return;
				} else if (*str == "y") {
					if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
						return;
					}

					valid = true;
					v->normal.y = p_value;
					return;
				} else if (*str == "z") {
					if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
						return;
					}

					valid = true;
					v->normal.z = p_value;
					return;
				} else if (*str == "normal") {
					if (p_value.type != Variant::VECTOR3) {
						return;
					}

					valid = true;
					v->normal = p_value;
					return;
				} else if (*str == "d") {
					valid = true;
					v->d = p_value;
					return;
				}
			}

		} break;
		case QUAT: {
			if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
				return;
			}

			if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Quat *v = reinterpret_cast<Quat *>(_data._mem);
				if (*str == "x") {
					valid = true;
					v->x = p_value;
					return;
				} else if (*str == "y") {
					valid = true;
					v->y = p_value;
					return;
				} else if (*str == "z") {
					valid = true;
					v->z = p_value;
					return;
				} else if (*str == "w") {
					valid = true;
					v->w = p_value;
					return;
				}
			}

		} break; // 10
		case AABB: {
			if (p_value.type != Variant::VECTOR3) {
				return;
			}

			if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				::AABB *v = _data._aabb;
				if (*str == "position") {
					valid = true;
					v->position = p_value;
					return;
				} else if (*str == "size") {
					valid = true;
					v->size = p_value;
					return;
				} else if (*str == "end") {
					valid = true;
					v->size = Vector3(p_value) - v->position;
					return;
				}
			}
		} break;
		case BASIS: {
			if (p_value.type != Variant::VECTOR3) {
				return;
			}

			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;

				if (index < 0) {
					index += 3;
				}
				if (index >= 0 && index < 3) {
					Basis *v = _data._basis;

					valid = true;
					v->set_axis(index, p_value);
					return;
				}
			} else if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Basis *v = _data._basis;

				if (*str == "x") {
					valid = true;
					v->set_axis(0, p_value);
					return;
				} else if (*str == "y") {
					valid = true;
					v->set_axis(1, p_value);
					return;
				} else if (*str == "z") {
					valid = true;
					v->set_axis(2, p_value);
					return;
				}
			}

		} break;
		case TRANSFORM: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				if (p_value.type != Variant::VECTOR3) {
					return;
				}

				int index = p_index;

				if (index < 0) {
					index += 4;
				}
				if (index >= 0 && index < 4) {
					Transform *v = _data._transform;
					valid = true;
					if (index == 3) {
						v->origin = p_value;
					} else {
						v->basis.set_axis(index, p_value);
					}
					return;
				}
			} else if (p_index.get_type() == Variant::STRING) {
				Transform *v = _data._transform;
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);

				if (*str == "basis") {
					if (p_value.type != Variant::BASIS) {
						return;
					}
					valid = true;
					v->basis = p_value;
					return;
				}
				if (*str == "origin") {
					if (p_value.type != Variant::VECTOR3) {
						return;
					}
					valid = true;
					v->origin = p_value;
					return;
				}
			}

		} break;
		case COLOR: {
			if (p_value.type != Variant::INT && p_value.type != Variant::REAL) {
				return;
			}

			if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				Color *v = reinterpret_cast<Color *>(_data._mem);
				if (*str == "r") {
					valid = true;
					v->r = p_value;
					return;
				} else if (*str == "g") {
					valid = true;
					v->g = p_value;
					return;
				} else if (*str == "b") {
					valid = true;
					v->b = p_value;
					return;
				} else if (*str == "a") {
					valid = true;
					v->a = p_value;
					return;
				} else if (*str == "h") {
					valid = true;
					v->set_hsv(p_value, v->get_s(), v->get_v(), v->a);
					return;
				} else if (*str == "s") {
					valid = true;
					v->set_hsv(v->get_h(), p_value, v->get_v(), v->a);
					return;
				} else if (*str == "v") {
					valid = true;
					v->set_hsv(v->get_h(), v->get_s(), p_value, v->a);
					return;
				} else if (*str == "r8") {
					valid = true;
					v->r = float(p_value) / 255.0;
					return;
				} else if (*str == "g8") {
					valid = true;
					v->g = float(p_value) / 255.0;
					return;
				} else if (*str == "b8") {
					valid = true;
					v->b = float(p_value) / 255.0;
					return;
				} else if (*str == "a8") {
					valid = true;
					v->a = float(p_value) / 255.0;
					return;
				}
			} else if (p_index.get_type() == Variant::INT) {
				int idx = p_index;
				if (idx < 0) {
					idx += 4;
				}
				if (idx >= 0 && idx < 4) {
					Color *v = reinterpret_cast<Color *>(_data._mem);
					(*v)[idx] = p_value;
					valid = true;
				}
			}

		} break;
		case NODE_PATH: {
		} break; // 15
		case _RID: {
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				valid = false;
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted set on a deleted object.");
				}
#endif
				return;
			}

			if (p_index.get_type() != Variant::STRING) {
				obj->setvar(p_index, p_value, r_valid);
				return;
			}

			obj->set(p_index, p_value, r_valid);
			return;
		} break;
		case DICTIONARY: {
			Dictionary *dic = reinterpret_cast<Dictionary *>(_data._mem);
			dic->operator[](p_index) = p_value;
			valid = true; //always valid, i guess? should this really be ok?
			return;
		} break;
			// clang-format 14 wants to add a space after the last return,
			// and clang-format 15 removes it...
			/* clang-format off */
			DEFAULT_OP_ARRAY_CMD(ARRAY, Array, ;, (*arr)[index] = p_value; return) // 20
			/* clang-format on */
			DEFAULT_OP_DVECTOR_SET(POOL_BYTE_ARRAY, uint8_t, p_value.type != Variant::REAL && p_value.type != Variant::INT)
			DEFAULT_OP_DVECTOR_SET(POOL_INT_ARRAY, int, p_value.type != Variant::REAL && p_value.type != Variant::INT)
			DEFAULT_OP_DVECTOR_SET(POOL_REAL_ARRAY, real_t, p_value.type != Variant::REAL && p_value.type != Variant::INT)
			DEFAULT_OP_DVECTOR_SET(POOL_STRING_ARRAY, String, p_value.type != Variant::STRING)
			DEFAULT_OP_DVECTOR_SET(POOL_VECTOR2_ARRAY, Vector2, p_value.type != Variant::VECTOR2) // 25
			DEFAULT_OP_DVECTOR_SET(POOL_VECTOR3_ARRAY, Vector3, p_value.type != Variant::VECTOR3)
			DEFAULT_OP_DVECTOR_SET(POOL_COLOR_ARRAY, Color, p_value.type != Variant::COLOR)
		default:
			return;
	}
}

Variant Variant::get(const Variant &p_index, bool *r_valid) const {
	static bool _dummy = false;

	bool &valid = r_valid ? *r_valid : _dummy;

	valid = false;

	switch (type) {
		case NIL: {
			return Variant();
		} break;
		case BOOL: {
			return Variant();
		} break;
		case INT: {
			return Variant();
		} break;
		case REAL: {
			return Variant();
		} break;
		case STRING: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				//string index

				int idx = p_index;
				const String *str = reinterpret_cast<const String *>(_data._mem);
				if (idx < 0) {
					idx += str->length();
				}
				if (idx >= 0 && idx < str->length()) {
					valid = true;
					return str->substr(idx, 1);
				}
			}

		} break;
		case VECTOR2: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				// scalar index
				int idx = p_index;
				if (idx < 0) {
					idx += 2;
				}
				if (idx >= 0 && idx < 2) {
					const Vector2 *v = reinterpret_cast<const Vector2 *>(_data._mem);
					valid = true;
					return (*v)[idx];
				}
			} else if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Vector2 *v = reinterpret_cast<const Vector2 *>(_data._mem);
				if (*str == "x") {
					valid = true;
					return v->x;
				} else if (*str == "y") {
					valid = true;
					return v->y;
				}
			}

		} break; // 5
		case RECT2: {
			if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Rect2 *v = reinterpret_cast<const Rect2 *>(_data._mem);
				if (*str == "position") {
					valid = true;
					return v->position;
				} else if (*str == "size") {
					valid = true;
					return v->size;
				} else if (*str == "end") {
					valid = true;
					return v->size + v->position;
				}
			}
		} break;
		case VECTOR3: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				//scalar index
				int idx = p_index;
				if (idx < 0) {
					idx += 3;
				}
				if (idx >= 0 && idx < 3) {
					const Vector3 *v = reinterpret_cast<const Vector3 *>(_data._mem);
					valid = true;
					return (*v)[idx];
				}
			} else if (p_index.get_type() == Variant::STRING) {
				// Scalar/Vector2 name.
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Vector3 *v = reinterpret_cast<const Vector3 *>(_data._mem);
				if (*str == "x") {
					valid = true;
					return v->x;
				} else if (*str == "y") {
					valid = true;
					return v->y;
				} else if (*str == "z") {
					valid = true;
					return v->z;
				} else {
					// 2 component vector swizzling.
					bool swizzle_valid;
					SwizzleAxes2 axes = get_swizzle2(*str, swizzle_valid);
					if (!swizzle_valid) {
						break;
					}

					valid = true;
					return Vector2(v->get_axis(axes.x), v->get_axis(axes.y));
				}
			}

		} break;
		case TRANSFORM2D: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;

				if (index < 0) {
					index += 3;
				}
				if (index >= 0 && index < 3) {
					const Transform2D *v = _data._transform2d;

					valid = true;
					return v->elements[index];
				}
			} else if (p_index.get_type() == Variant::STRING) {
				//scalar name
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Transform2D *v = _data._transform2d;
				if (*str == "x") {
					valid = true;
					return v->elements[0];
				} else if (*str == "y") {
					valid = true;
					return v->elements[1];
				} else if (*str == "origin") {
					valid = true;
					return v->elements[2];
				}
			}

		} break;
		case PLANE: {
			if (p_index.get_type() == Variant::STRING) {
				//scalar name
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Plane *v = reinterpret_cast<const Plane *>(_data._mem);
				if (*str == "x") {
					valid = true;
					return v->normal.x;
				} else if (*str == "y") {
					valid = true;
					return v->normal.y;
				} else if (*str == "z") {
					valid = true;
					return v->normal.z;
				} else if (*str == "normal") {
					valid = true;
					return v->normal;
				} else if (*str == "d") {
					valid = true;
					return v->d;
				}
			}

		} break;
		case QUAT: {
			if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Quat *v = reinterpret_cast<const Quat *>(_data._mem);
				if (*str == "x") {
					valid = true;
					return v->x;
				} else if (*str == "y") {
					valid = true;
					return v->y;
				} else if (*str == "z") {
					valid = true;
					return v->z;
				} else if (*str == "w") {
					valid = true;
					return v->w;
				}
			}

		} break; // 10
		case AABB: {
			if (p_index.get_type() == Variant::STRING) {
				//scalar name

				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const ::AABB *v = _data._aabb;
				if (*str == "position") {
					valid = true;
					return v->position;
				} else if (*str == "size") {
					valid = true;
					return v->size;
				} else if (*str == "end") {
					valid = true;
					return v->size + v->position;
				}
			}
		} break;
		case BASIS: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;
				if (index < 0) {
					index += 3;
				}
				if (index >= 0 && index < 3) {
					const Basis *v = _data._basis;

					valid = true;
					return v->get_axis(index);
				}
			} else if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Basis *v = _data._basis;

				if (*str == "x") {
					valid = true;
					return v->get_axis(0);
				} else if (*str == "y") {
					valid = true;
					return v->get_axis(1);
				} else if (*str == "z") {
					valid = true;
					return v->get_axis(2);
				}
			}

		} break;
		case TRANSFORM: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;
				if (index < 0) {
					index += 4;
				}
				if (index >= 0 && index < 4) {
					const Transform *v = _data._transform;
					valid = true;
					return index == 3 ? v->origin : v->basis.get_axis(index);
				}
			} else if (p_index.get_type() == Variant::STRING) {
				const Transform *v = _data._transform;
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);

				if (*str == "basis") {
					valid = true;
					return v->basis;
				}
				if (*str == "origin") {
					valid = true;
					return v->origin;
				}
			}

		} break;
		case COLOR: {
			if (p_index.get_type() == Variant::STRING) {
				const String *str = reinterpret_cast<const String *>(p_index._data._mem);
				const Color *v = reinterpret_cast<const Color *>(_data._mem);
				if (*str == "r") {
					valid = true;
					return v->r;
				} else if (*str == "g") {
					valid = true;
					return v->g;
				} else if (*str == "b") {
					valid = true;
					return v->b;
				} else if (*str == "a") {
					valid = true;
					return v->a;
				} else if (*str == "h") {
					valid = true;
					return v->get_h();
				} else if (*str == "s") {
					valid = true;
					return v->get_s();
				} else if (*str == "v") {
					valid = true;
					return v->get_v();
				} else if (*str == "r8") {
					valid = true;
					return (int)Math::round(v->r * 255.0);
				} else if (*str == "g8") {
					valid = true;
					return (int)Math::round(v->g * 255.0);
				} else if (*str == "b8") {
					valid = true;
					return (int)Math::round(v->b * 255.0);
				} else if (*str == "a8") {
					valid = true;
					return (int)Math::round(v->a * 255.0);
				}
			} else if (p_index.get_type() == Variant::INT) {
				int idx = p_index;
				if (idx < 0) {
					idx += 4;
				}
				if (idx >= 0 && idx < 4) {
					const Color *v = reinterpret_cast<const Color *>(_data._mem);
					valid = true;
					return (*v)[idx];
				}
			}

		} break;
		case NODE_PATH: {
		} break; // 15
		case _RID: {
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				valid = false;
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted get on a deleted object.");
				}
#endif
				return Variant();
			}

			if (p_index.get_type() != Variant::STRING) {
				return obj->getvar(p_index, r_valid);
			} else {
				return obj->get(p_index, r_valid);
			}

		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			const Variant *res = dic->getptr(p_index);
			if (res) {
				valid = true;
				return *res;
			}
		} break;
			DEFAULT_OP_ARRAY_CMD(ARRAY, const Array, ;, return (*arr)[index]) // 20
			DEFAULT_OP_DVECTOR_GET(POOL_BYTE_ARRAY, uint8_t)
			DEFAULT_OP_DVECTOR_GET(POOL_INT_ARRAY, int)
			DEFAULT_OP_DVECTOR_GET(POOL_REAL_ARRAY, real_t)
			DEFAULT_OP_DVECTOR_GET(POOL_STRING_ARRAY, String)
			DEFAULT_OP_DVECTOR_GET(POOL_VECTOR2_ARRAY, Vector2) // 25
			DEFAULT_OP_DVECTOR_GET(POOL_VECTOR3_ARRAY, Vector3)
			DEFAULT_OP_DVECTOR_GET(POOL_COLOR_ARRAY, Color)
		default:
			return Variant();
	}

	return Variant();
}

bool Variant::in(const Variant &p_index, bool *r_valid) const {
	if (r_valid) {
		*r_valid = true;
	}

	switch (type) {
		case STRING: {
			if (p_index.get_type() == Variant::STRING) {
				//string index
				String idx = p_index;
				const String *str = reinterpret_cast<const String *>(_data._mem);

				return str->find(idx) != -1;
			}

		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				if (r_valid) {
					*r_valid = false;
				}
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted 'in' on a deleted object.");
				}
#endif
				return false;
			}

			bool result;
			if (p_index.get_type() != Variant::STRING) {
				obj->getvar(p_index, &result);
			} else {
				obj->get(p_index, &result);
			}
			return result;
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			return dic->has(p_index);

		} break; // 20
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			int l = arr->size();
			if (l) {
				for (int i = 0; i < l; i++) {
					if (evaluate(OP_EQUAL, (*arr)[i], p_index)) {
						return true;
					}
				}
			}

			return false;

		} break;
		case POOL_BYTE_ARRAY: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;
				const PoolVector<uint8_t> *arr = reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
				int l = arr->size();
				if (l) {
					PoolVector<uint8_t>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}

		} break;
		case POOL_INT_ARRAY: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				int index = p_index;
				const PoolVector<int> *arr = reinterpret_cast<const PoolVector<int> *>(_data._mem);
				int l = arr->size();
				if (l) {
					PoolVector<int>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}
		} break;
		case POOL_REAL_ARRAY: {
			if (p_index.get_type() == Variant::INT || p_index.get_type() == Variant::REAL) {
				real_t index = p_index;
				const PoolVector<real_t> *arr = reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
				int l = arr->size();
				if (l) {
					PoolVector<real_t>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}

		} break;
		case POOL_STRING_ARRAY: {
			if (p_index.get_type() == Variant::STRING) {
				String index = p_index;
				const PoolVector<String> *arr = reinterpret_cast<const PoolVector<String> *>(_data._mem);

				int l = arr->size();
				if (l) {
					PoolVector<String>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}

		} break; //25
		case POOL_VECTOR2_ARRAY: {
			if (p_index.get_type() == Variant::VECTOR2) {
				Vector2 index = p_index;
				const PoolVector<Vector2> *arr = reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);

				int l = arr->size();
				if (l) {
					PoolVector<Vector2>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}

		} break;
		case POOL_VECTOR3_ARRAY: {
			if (p_index.get_type() == Variant::VECTOR3) {
				Vector3 index = p_index;
				const PoolVector<Vector3> *arr = reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);

				int l = arr->size();
				if (l) {
					PoolVector<Vector3>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}

		} break;
		case POOL_COLOR_ARRAY: {
			if (p_index.get_type() == Variant::COLOR) {
				Color index = p_index;
				const PoolVector<Color> *arr = reinterpret_cast<const PoolVector<Color> *>(_data._mem);

				int l = arr->size();
				if (l) {
					PoolVector<Color>::Read r = arr->read();
					for (int i = 0; i < l; i++) {
						if (r[i] == index) {
							return true;
						}
					}
				}

				return false;
			}
		} break;
		default: {
		}
	}

	if (r_valid) {
		*r_valid = false;
	}
	return false;
}

void Variant::get_property_list(List<PropertyInfo> *p_list) const {
	switch (type) {
		case VECTOR2: {
			p_list->push_back(PropertyInfo(Variant::REAL, "x"));
			p_list->push_back(PropertyInfo(Variant::REAL, "y"));

		} break; // 5
		case RECT2: {
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "position"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "size"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "end"));

		} break;
		case VECTOR3: {
			p_list->push_back(PropertyInfo(Variant::REAL, "x"));
			p_list->push_back(PropertyInfo(Variant::REAL, "y"));
			p_list->push_back(PropertyInfo(Variant::REAL, "z"));
			// Swizzling.
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "xy"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "xz"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "yx"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "yz"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "zx"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "zy"));

		} break;
		case TRANSFORM2D: {
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "x"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "y"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, "origin"));

		} break;
		case PLANE: {
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "normal"));
			p_list->push_back(PropertyInfo(Variant::REAL, "x"));
			p_list->push_back(PropertyInfo(Variant::REAL, "y"));
			p_list->push_back(PropertyInfo(Variant::REAL, "z"));
			p_list->push_back(PropertyInfo(Variant::REAL, "d"));

		} break;
		case QUAT: {
			p_list->push_back(PropertyInfo(Variant::REAL, "x"));
			p_list->push_back(PropertyInfo(Variant::REAL, "y"));
			p_list->push_back(PropertyInfo(Variant::REAL, "z"));
			p_list->push_back(PropertyInfo(Variant::REAL, "w"));

		} break; // 10
		case AABB: {
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "position"));
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "size"));
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "end"));
		} break;
		case BASIS: {
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "x"));
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "y"));
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "z"));

		} break;
		case TRANSFORM: {
			p_list->push_back(PropertyInfo(Variant::BASIS, "basis"));
			p_list->push_back(PropertyInfo(Variant::VECTOR3, "origin"));

		} break;
		case COLOR: {
			p_list->push_back(PropertyInfo(Variant::REAL, "r"));
			p_list->push_back(PropertyInfo(Variant::REAL, "g"));
			p_list->push_back(PropertyInfo(Variant::REAL, "b"));
			p_list->push_back(PropertyInfo(Variant::REAL, "a"));
			p_list->push_back(PropertyInfo(Variant::REAL, "h"));
			p_list->push_back(PropertyInfo(Variant::REAL, "s"));
			p_list->push_back(PropertyInfo(Variant::REAL, "v"));
			p_list->push_back(PropertyInfo(Variant::INT, "r8"));
			p_list->push_back(PropertyInfo(Variant::INT, "g8"));
			p_list->push_back(PropertyInfo(Variant::INT, "b8"));
			p_list->push_back(PropertyInfo(Variant::INT, "a8"));

		} break;
		case NODE_PATH: {
		} break; // 15
		case _RID: {
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted get property list on a deleted object.");
				}
#endif
				return;
			}

			obj->get_property_list(p_list);
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			List<Variant> keys;
			dic->get_key_list(&keys);
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				if (E->get().get_type() == Variant::STRING) {
					p_list->push_back(PropertyInfo(Variant::STRING, E->get()));
				}
			}
		} break;
		case ARRAY: // 20
		case POOL_BYTE_ARRAY:
		case POOL_INT_ARRAY:
		case POOL_REAL_ARRAY:
		case POOL_STRING_ARRAY:
		case POOL_VECTOR2_ARRAY: // 25
		case POOL_VECTOR3_ARRAY:
		case POOL_COLOR_ARRAY: {
			//nothing
		} break;
		default: {
		}
	}
}

bool Variant::iter_init(Variant &r_iter, bool &valid) const {
	valid = true;
	switch (type) {
		case INT: {
			r_iter = 0;
			return _data._int > 0;
		} break;
		case REAL: {
			r_iter = 0;
			return _data._real > 0.0;
		} break;
		case VECTOR2: {
			int64_t from = reinterpret_cast<const Vector2 *>(_data._mem)->x;
			int64_t to = reinterpret_cast<const Vector2 *>(_data._mem)->y;

			r_iter = from;

			return from < to;
		} break;
		case VECTOR3: {
			int64_t from = reinterpret_cast<const Vector3 *>(_data._mem)->x;
			int64_t to = reinterpret_cast<const Vector3 *>(_data._mem)->y;
			int64_t step = reinterpret_cast<const Vector3 *>(_data._mem)->z;

			r_iter = from;

			if (from == to) {
				return false;
			} else if (from < to) {
				return step > 0;
			} else {
				return step < 0;
			}
			//return true;
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				valid = false;
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted iteration start on a deleted object.");
				}
#endif
				return false;
			}
			Variant::CallError ce;
			ce.error = Variant::CallError::CALL_OK;
			Array ref;
			ref.push_back(r_iter);
			Variant vref = ref;
			const Variant *refp[] = { &vref };
			Variant ret = obj->call(CoreStringNames::get_singleton()->_iter_init, refp, 1, ce);

			if (ref.size() != 1 || ce.error != Variant::CallError::CALL_OK) {
				valid = false;
				return false;
			}

			r_iter = ref[0];
			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			if (str->empty()) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			if (dic->empty()) {
				return false;
			}

			const Variant *next = dic->next(nullptr);
			r_iter = *next;
			return true;

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			if (arr->empty()) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case POOL_BYTE_ARRAY: {
			const PoolVector<uint8_t> *arr = reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case POOL_INT_ARRAY: {
			const PoolVector<int> *arr = reinterpret_cast<const PoolVector<int> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case POOL_REAL_ARRAY: {
			const PoolVector<real_t> *arr = reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case POOL_STRING_ARRAY: {
			const PoolVector<String> *arr = reinterpret_cast<const PoolVector<String> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case POOL_VECTOR2_ARRAY: {
			const PoolVector<Vector2> *arr = reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case POOL_VECTOR3_ARRAY: {
			const PoolVector<Vector3> *arr = reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case POOL_COLOR_ARRAY: {
			const PoolVector<Color> *arr = reinterpret_cast<const PoolVector<Color> *>(_data._mem);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		default: {
		}
	}

	valid = false;
	return false;
}
bool Variant::iter_next(Variant &r_iter, bool &valid) const {
	valid = true;
	switch (type) {
		case INT: {
			int64_t idx = r_iter;
			idx++;
			if (idx >= _data._int) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case REAL: {
			int64_t idx = r_iter;
			idx++;
			if (idx >= _data._real) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case VECTOR2: {
			int64_t to = reinterpret_cast<const Vector2 *>(_data._mem)->y;

			int64_t idx = r_iter;
			idx++;

			if (idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case VECTOR3: {
			int64_t to = reinterpret_cast<const Vector3 *>(_data._mem)->y;
			int64_t step = reinterpret_cast<const Vector3 *>(_data._mem)->z;

			int64_t idx = r_iter;
			idx += step;

			if (step < 0 && idx <= to) {
				return false;
			}

			if (step > 0 && idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				valid = false;
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted iteration check next on a deleted object.");
				}
#endif
				return false;
			}
			Variant::CallError ce;
			ce.error = Variant::CallError::CALL_OK;
			Array ref;
			ref.push_back(r_iter);
			Variant vref = ref;
			const Variant *refp[] = { &vref };
			Variant ret = obj->call(CoreStringNames::get_singleton()->_iter_next, refp, 1, ce);

			if (ref.size() != 1 || ce.error != Variant::CallError::CALL_OK) {
				valid = false;
				return false;
			}

			r_iter = ref[0];

			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= str->length()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			const Variant *next = dic->next(&r_iter);
			if (!next) {
				return false;
			}

			r_iter = *next;
			return true;

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case POOL_BYTE_ARRAY: {
			const PoolVector<uint8_t> *arr = reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case POOL_INT_ARRAY: {
			const PoolVector<int> *arr = reinterpret_cast<const PoolVector<int> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case POOL_REAL_ARRAY: {
			const PoolVector<real_t> *arr = reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case POOL_STRING_ARRAY: {
			const PoolVector<String> *arr = reinterpret_cast<const PoolVector<String> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case POOL_VECTOR2_ARRAY: {
			const PoolVector<Vector2> *arr = reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case POOL_VECTOR3_ARRAY: {
			const PoolVector<Vector3> *arr = reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case POOL_COLOR_ARRAY: {
			const PoolVector<Color> *arr = reinterpret_cast<const PoolVector<Color> *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		default: {
		}
	}

	valid = false;
	return false;
}

Variant Variant::iter_get(const Variant &r_iter, bool &r_valid) const {
	r_valid = true;
	switch (type) {
		case INT: {
			return r_iter;
		} break;
		case REAL: {
			return r_iter;
		} break;
		case VECTOR2: {
			return r_iter;
		} break;
		case VECTOR3: {
			return r_iter;
		} break;
		case OBJECT: {
			Object *obj = _OBJ_PTR(*this);
			if (unlikely(!obj)) {
				r_valid = false;
#ifdef DEBUG_ENABLED
				if (_get_obj().rc) {
					ERR_PRINT("Attempted iteration get next on a deleted object.");
				}
#endif
				return Variant();
			}
			Variant::CallError ce;
			ce.error = Variant::CallError::CALL_OK;
			const Variant *refp[] = { &r_iter };
			Variant ret = obj->call(CoreStringNames::get_singleton()->_iter_get, refp, 1, ce);

			if (ce.error != Variant::CallError::CALL_OK) {
				r_valid = false;
				return Variant();
			}

			//r_iter=ref[0];

			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			return str->substr(r_iter, 1);
		} break;
		case DICTIONARY: {
			return r_iter; //iterator is the same as the key

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_BYTE_ARRAY: {
			const PoolVector<uint8_t> *arr = reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_INT_ARRAY: {
			const PoolVector<int> *arr = reinterpret_cast<const PoolVector<int> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_REAL_ARRAY: {
			const PoolVector<real_t> *arr = reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_STRING_ARRAY: {
			const PoolVector<String> *arr = reinterpret_cast<const PoolVector<String> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_VECTOR2_ARRAY: {
			const PoolVector<Vector2> *arr = reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_VECTOR3_ARRAY: {
			const PoolVector<Vector3> *arr = reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case POOL_COLOR_ARRAY: {
			const PoolVector<Color> *arr = reinterpret_cast<const PoolVector<Color> *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		default: {
		}
	}

	r_valid = false;
	return Variant();
}

Variant Variant::duplicate(bool deep) const {
	switch (type) {
		case OBJECT: {
			/*  breaks stuff :(
			if (deep && !_get_obj().ref.is_null()) {
				Ref<Resource> resource = _get_obj().ref;
				if (resource.is_valid()) {
					return resource->duplicate(true);
				}
			}
			*/
			return *this;
		} break;
		case DICTIONARY:
			return operator Dictionary().duplicate(deep);
		case ARRAY:
			return operator Array().duplicate(deep);
		default:
			return *this;
	}
}

void Variant::blend(const Variant &a, const Variant &b, float c, Variant &r_dst) {
	if (a.type != b.type) {
		if (a.is_num() && b.is_num()) {
			real_t va = a;
			real_t vb = b;
			r_dst = va + vb * c;
		} else {
			r_dst = a;
		}
		return;
	}

	switch (a.type) {
		case NIL: {
			r_dst = Variant();
		}
			return;
		case INT: {
			int64_t va = a._data._int;
			int64_t vb = b._data._int;
			r_dst = int(va + vb * c + 0.5);
		}
			return;
		case REAL: {
			double ra = a._data._real;
			double rb = b._data._real;
			r_dst = ra + rb * c;
		}
			return;
		case VECTOR2: {
			r_dst = *reinterpret_cast<const Vector2 *>(a._data._mem) + *reinterpret_cast<const Vector2 *>(b._data._mem) * c;
		}
			return;
		case RECT2: {
			const Rect2 *ra = reinterpret_cast<const Rect2 *>(a._data._mem);
			const Rect2 *rb = reinterpret_cast<const Rect2 *>(b._data._mem);
			r_dst = Rect2(ra->position + rb->position * c, ra->size + rb->size * c);
		}
			return;
		case VECTOR3: {
			r_dst = *reinterpret_cast<const Vector3 *>(a._data._mem) + *reinterpret_cast<const Vector3 *>(b._data._mem) * c;
		}
			return;
		case AABB: {
			const ::AABB *ra = reinterpret_cast<const ::AABB *>(a._data._mem);
			const ::AABB *rb = reinterpret_cast<const ::AABB *>(b._data._mem);
			r_dst = ::AABB(ra->position + rb->position * c, ra->size + rb->size * c);
		}
			return;
		case QUAT: {
			Quat empty_rot;
			const Quat *qa = reinterpret_cast<const Quat *>(a._data._mem);
			const Quat *qb = reinterpret_cast<const Quat *>(b._data._mem);
			r_dst = *qa * empty_rot.slerp(*qb, c);
		}
			return;
		case COLOR: {
			const Color *ca = reinterpret_cast<const Color *>(a._data._mem);
			const Color *cb = reinterpret_cast<const Color *>(b._data._mem);
			float new_r = ca->r + cb->r * c;
			float new_g = ca->g + cb->g * c;
			float new_b = ca->b + cb->b * c;
			float new_a = ca->a + cb->a * c;
			new_r = new_r > 1.0 ? 1.0 : new_r;
			new_g = new_g > 1.0 ? 1.0 : new_g;
			new_b = new_b > 1.0 ? 1.0 : new_b;
			new_a = new_a > 1.0 ? 1.0 : new_a;
			r_dst = Color(new_r, new_g, new_b, new_a);
		}
			return;
		default: {
			r_dst = c < 0.5 ? a : b;
		}
			return;
	}
}

void Variant::interpolate(const Variant &a, const Variant &b, float c, Variant &r_dst) {
	if (a.type != b.type) {
		if (a.is_num() && b.is_num()) {
			//not as efficient but..
			real_t va = a;
			real_t vb = b;
			r_dst = va + (vb - va) * c;

		} else {
			r_dst = a;
		}
		return;
	}

	switch (a.type) {
		case NIL: {
			r_dst = Variant();
		}
			return;
		case BOOL: {
			r_dst = a;
		}
			return;
		case INT: {
			int64_t va = a._data._int;
			int64_t vb = b._data._int;
			r_dst = int(va + (vb - va) * c);
		}
			return;
		case REAL: {
			real_t va = a._data._real;
			real_t vb = b._data._real;
			r_dst = va + (vb - va) * c;
		}
			return;
		case STRING: {
			//this is pretty funny and bizarre, but artists like to use it for typewritter effects
			String sa = *reinterpret_cast<const String *>(a._data._mem);
			String sb = *reinterpret_cast<const String *>(b._data._mem);
			String dst;
			int sa_len = sa.length();
			int sb_len = sb.length();
			int csize = sa_len + (sb_len - sa_len) * c;
			if (csize == 0) {
				r_dst = "";
				return;
			}
			dst.resize(csize + 1);
			dst[csize] = 0;
			int split = csize / 2;

			for (int i = 0; i < csize; i++) {
				CharType chr = ' ';

				if (i < split) {
					if (i < sa.length()) {
						chr = sa[i];
					} else if (i < sb.length()) {
						chr = sb[i];
					}

				} else {
					if (i < sb.length()) {
						chr = sb[i];
					} else if (i < sa.length()) {
						chr = sa[i];
					}
				}

				dst[i] = chr;
			}

			r_dst = dst;
		}
			return;
		case VECTOR2: {
			r_dst = reinterpret_cast<const Vector2 *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Vector2 *>(b._data._mem), c);
		}
			return;
		case RECT2: {
			r_dst = Rect2(reinterpret_cast<const Rect2 *>(a._data._mem)->position.linear_interpolate(reinterpret_cast<const Rect2 *>(b._data._mem)->position, c), reinterpret_cast<const Rect2 *>(a._data._mem)->size.linear_interpolate(reinterpret_cast<const Rect2 *>(b._data._mem)->size, c));
		}
			return;
		case VECTOR3: {
			r_dst = reinterpret_cast<const Vector3 *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Vector3 *>(b._data._mem), c);
		}
			return;
		case TRANSFORM2D: {
			r_dst = a._data._transform2d->interpolate_with(*b._data._transform2d, c);
		}
			return;
		case PLANE: {
			r_dst = a;
		}
			return;
		case QUAT: {
			r_dst = reinterpret_cast<const Quat *>(a._data._mem)->slerp(*reinterpret_cast<const Quat *>(b._data._mem), c);
		}
			return;
		case AABB: {
			r_dst = ::AABB(a._data._aabb->position.linear_interpolate(b._data._aabb->position, c), a._data._aabb->size.linear_interpolate(b._data._aabb->size, c));
		}
			return;
		case BASIS: {
			r_dst = Transform(*a._data._basis).interpolate_with(Transform(*b._data._basis), c).basis;
		}
			return;
		case TRANSFORM: {
			r_dst = a._data._transform->interpolate_with(*b._data._transform, c);
		}
			return;
		case COLOR: {
			r_dst = reinterpret_cast<const Color *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Color *>(b._data._mem), c);
		}
			return;
		case NODE_PATH: {
			r_dst = a;
		}
			return;
		case _RID: {
			r_dst = a;
		}
			return;
		case OBJECT: {
			r_dst = a;
		}
			return;
		case DICTIONARY: {
		}
			return;
		case ARRAY: {
			r_dst = a;
		}
			return;
		case POOL_BYTE_ARRAY: {
			r_dst = a;
		}
			return;
		case POOL_INT_ARRAY: {
			const PoolVector<int> *arr_a = reinterpret_cast<const PoolVector<int> *>(a._data._mem);
			const PoolVector<int> *arr_b = reinterpret_cast<const PoolVector<int> *>(b._data._mem);
			int sz = arr_a->size();
			if (sz == 0 || arr_b->size() != sz) {
				r_dst = a;
			} else {
				PoolVector<int> v;
				v.resize(sz);
				{
					PoolVector<int>::Write vw = v.write();
					PoolVector<int>::Read ar = arr_a->read();
					PoolVector<int>::Read br = arr_b->read();

					Variant va;
					for (int i = 0; i < sz; i++) {
						Variant::interpolate(ar[i], br[i], c, va);
						vw[i] = va;
					}
				}
				r_dst = v;
			}
		}
			return;
		case POOL_REAL_ARRAY: {
			const PoolVector<real_t> *arr_a = reinterpret_cast<const PoolVector<real_t> *>(a._data._mem);
			const PoolVector<real_t> *arr_b = reinterpret_cast<const PoolVector<real_t> *>(b._data._mem);
			int sz = arr_a->size();
			if (sz == 0 || arr_b->size() != sz) {
				r_dst = a;
			} else {
				PoolVector<real_t> v;
				v.resize(sz);
				{
					PoolVector<real_t>::Write vw = v.write();
					PoolVector<real_t>::Read ar = arr_a->read();
					PoolVector<real_t>::Read br = arr_b->read();

					Variant va;
					for (int i = 0; i < sz; i++) {
						Variant::interpolate(ar[i], br[i], c, va);
						vw[i] = va;
					}
				}
				r_dst = v;
			}
		}
			return;
		case POOL_STRING_ARRAY: {
			r_dst = a;
		}
			return;
		case POOL_VECTOR2_ARRAY: {
			const PoolVector<Vector2> *arr_a = reinterpret_cast<const PoolVector<Vector2> *>(a._data._mem);
			const PoolVector<Vector2> *arr_b = reinterpret_cast<const PoolVector<Vector2> *>(b._data._mem);
			int sz = arr_a->size();
			if (sz == 0 || arr_b->size() != sz) {
				r_dst = a;
			} else {
				PoolVector<Vector2> v;
				v.resize(sz);
				{
					PoolVector<Vector2>::Write vw = v.write();
					PoolVector<Vector2>::Read ar = arr_a->read();
					PoolVector<Vector2>::Read br = arr_b->read();

					for (int i = 0; i < sz; i++) {
						vw[i] = ar[i].linear_interpolate(br[i], c);
					}
				}
				r_dst = v;
			}
		}
			return;
		case POOL_VECTOR3_ARRAY: {
			const PoolVector<Vector3> *arr_a = reinterpret_cast<const PoolVector<Vector3> *>(a._data._mem);
			const PoolVector<Vector3> *arr_b = reinterpret_cast<const PoolVector<Vector3> *>(b._data._mem);
			int sz = arr_a->size();
			if (sz == 0 || arr_b->size() != sz) {
				r_dst = a;
			} else {
				PoolVector<Vector3> v;
				v.resize(sz);
				{
					PoolVector<Vector3>::Write vw = v.write();
					PoolVector<Vector3>::Read ar = arr_a->read();
					PoolVector<Vector3>::Read br = arr_b->read();

					for (int i = 0; i < sz; i++) {
						vw[i] = ar[i].linear_interpolate(br[i], c);
					}
				}
				r_dst = v;
			}
		}
			return;
		case POOL_COLOR_ARRAY: {
			const PoolVector<Color> *arr_a = reinterpret_cast<const PoolVector<Color> *>(a._data._mem);
			const PoolVector<Color> *arr_b = reinterpret_cast<const PoolVector<Color> *>(b._data._mem);
			int sz = arr_a->size();
			if (sz == 0 || arr_b->size() != sz) {
				r_dst = a;
			} else {
				PoolVector<Color> v;
				v.resize(sz);
				{
					PoolVector<Color>::Write vw = v.write();
					PoolVector<Color>::Read ar = arr_a->read();
					PoolVector<Color>::Read br = arr_b->read();

					for (int i = 0; i < sz; i++) {
						vw[i] = ar[i].linear_interpolate(br[i], c);
					}
				}
				r_dst = v;
			}
		}
			return;
		default: {
			r_dst = a;
		}
	}
}

static const char *_op_names[Variant::OP_MAX] = {
	"==",
	"!=",
	"<",
	"<=",
	">",
	">=",
	"+",
	"-",
	"*",
	"/",
	"- (negation)",
	"+ (positive)",
	"%",
	"+ (concatenation)",
	"<<",
	">>",
	"&",
	"|",
	"^",
	"~",
	"and",
	"or",
	"xor",
	"not",
	"in"

};

String Variant::get_operator_name(Operator p_op) {
	ERR_FAIL_INDEX_V(p_op, OP_MAX, "");
	return _op_names[p_op];
}
