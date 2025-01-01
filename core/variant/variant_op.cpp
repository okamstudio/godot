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

#include "variant_op.h"

typedef void (*VariantEvaluatorFunction)(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid);

static Variant::Type operator_return_type_table[Variant::OP_MAX][Variant::VARIANT_MAX][Variant::VARIANT_MAX];
static VariantEvaluatorFunction operator_evaluator_table[Variant::OP_MAX][Variant::VARIANT_MAX][Variant::VARIANT_MAX];
static Variant::ValidatedOperatorEvaluator validated_operator_evaluator_table[Variant::OP_MAX][Variant::VARIANT_MAX][Variant::VARIANT_MAX];
static Variant::PTROperatorEvaluator ptr_operator_evaluator_table[Variant::OP_MAX][Variant::VARIANT_MAX][Variant::VARIANT_MAX];

template <typename T>
void register_op(Variant::Operator p_op, Variant::Type p_type_a, Variant::Type p_type_b) {
	operator_return_type_table[p_op][p_type_a][p_type_b] = T::get_return_type();
	operator_evaluator_table[p_op][p_type_a][p_type_b] = T::evaluate;
	validated_operator_evaluator_table[p_op][p_type_a][p_type_b] = T::validated_evaluate;
	ptr_operator_evaluator_table[p_op][p_type_a][p_type_b] = T::ptr_evaluate;
}

// Special cases that can't be done otherwise because of the forced casting to float.

template <>
class OperatorEvaluatorMul<Vector2, Vector2i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector2i &a = *VariantGetInternalPtr<Vector2i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		*r_ret = Vector2(a.x, a.y) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector2>::get_ptr(r_ret) = Vector2(VariantGetInternalPtr<Vector2i>::get_ptr(left)->x, VariantGetInternalPtr<Vector2i>::get_ptr(left)->y) * *VariantGetInternalPtr<double>::get_ptr(right);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector2>::encode(Vector2(PtrToArg<Vector2i>::convert(left).x, PtrToArg<Vector2i>::convert(left).y) * PtrToArg<double>::convert(right), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector2>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorMul<Vector2, double, Vector2i> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector2i &a = *VariantGetInternalPtr<Vector2i>::get_ptr(&p_right);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_left);
		*r_ret = Vector2(a.x, a.y) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector2>::get_ptr(r_ret) = Vector2(VariantGetInternalPtr<Vector2i>::get_ptr(right)->x, VariantGetInternalPtr<Vector2i>::get_ptr(right)->y) * *VariantGetInternalPtr<double>::get_ptr(left);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector2>::encode(Vector2(PtrToArg<Vector2i>::convert(right).x, PtrToArg<Vector2i>::convert(right).y) * PtrToArg<double>::convert(left), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector2>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorDivNZ<Vector2, Vector2i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector2i &a = *VariantGetInternalPtr<Vector2i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		if (unlikely(b == 0)) {
			r_valid = false;
			*r_ret = "Division by zero error";
			return;
		}
		*r_ret = Vector2(a.x, a.y) / b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector2>::get_ptr(r_ret) = Vector2(VariantGetInternalPtr<Vector2i>::get_ptr(left)->x, VariantGetInternalPtr<Vector2i>::get_ptr(left)->y) / *VariantGetInternalPtr<double>::get_ptr(right);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector2>::encode(Vector2(PtrToArg<Vector2i>::convert(left).x, PtrToArg<Vector2i>::convert(left).y) / PtrToArg<double>::convert(right), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector2>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorMul<Vector3, Vector3i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector3i &a = *VariantGetInternalPtr<Vector3i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		*r_ret = Vector3(a.x, a.y, a.z) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector3>::get_ptr(r_ret) = Vector3(VariantGetInternalPtr<Vector3i>::get_ptr(left)->x, VariantGetInternalPtr<Vector3i>::get_ptr(left)->y, VariantGetInternalPtr<Vector3i>::get_ptr(left)->z) * *VariantGetInternalPtr<double>::get_ptr(right);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector3>::encode(Vector3(PtrToArg<Vector3i>::convert(left).x, PtrToArg<Vector3i>::convert(left).y, PtrToArg<Vector3i>::convert(left).z) * PtrToArg<double>::convert(right), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector3>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorMul<Vector3, double, Vector3i> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector3i &a = *VariantGetInternalPtr<Vector3i>::get_ptr(&p_right);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_left);
		*r_ret = Vector3(a.x, a.y, a.z) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector3>::get_ptr(r_ret) = Vector3(VariantGetInternalPtr<Vector3i>::get_ptr(right)->x, VariantGetInternalPtr<Vector3i>::get_ptr(right)->y, VariantGetInternalPtr<Vector3i>::get_ptr(right)->z) * *VariantGetInternalPtr<double>::get_ptr(left);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector3>::encode(Vector3(PtrToArg<Vector3i>::convert(right).x, PtrToArg<Vector3i>::convert(right).y, PtrToArg<Vector3i>::convert(right).z) * PtrToArg<double>::convert(left), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector3>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorDivNZ<Vector3, Vector3i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector3i &a = *VariantGetInternalPtr<Vector3i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		if (unlikely(b == 0)) {
			r_valid = false;
			*r_ret = "Division by zero error";
			return;
		}
		*r_ret = Vector3(a.x, a.y, a.z) / b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector3>::get_ptr(r_ret) = Vector3(VariantGetInternalPtr<Vector3i>::get_ptr(left)->x, VariantGetInternalPtr<Vector3i>::get_ptr(left)->y, VariantGetInternalPtr<Vector3i>::get_ptr(left)->z) / *VariantGetInternalPtr<double>::get_ptr(right);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector3>::encode(Vector3(PtrToArg<Vector3i>::convert(left).x, PtrToArg<Vector3i>::convert(left).y, PtrToArg<Vector3i>::convert(left).z) / PtrToArg<double>::convert(right), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector3>::VARIANT_TYPE; }
};

//

template <>
class OperatorEvaluatorMul<Vector4, Vector4i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector4i &a = *VariantGetInternalPtr<Vector4i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		*r_ret = Vector4(a.x, a.y, a.z, a.w) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector4>::get_ptr(r_ret) = Vector4(VariantGetInternalPtr<Vector4i>::get_ptr(left)->x, VariantGetInternalPtr<Vector4i>::get_ptr(left)->y, VariantGetInternalPtr<Vector4i>::get_ptr(left)->z, VariantGetInternalPtr<Vector4i>::get_ptr(left)->w) * *VariantGetInternalPtr<double>::get_ptr(right);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector4>::encode(Vector4(PtrToArg<Vector4i>::convert(left).x, PtrToArg<Vector4i>::convert(left).y, PtrToArg<Vector4i>::convert(left).z, PtrToArg<Vector4i>::convert(left).w) * PtrToArg<double>::convert(right), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector4>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorMul<Vector4, double, Vector4i> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector4i &a = *VariantGetInternalPtr<Vector4i>::get_ptr(&p_right);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_left);
		*r_ret = Vector4(a.x, a.y, a.z, a.w) * b;
		r_valid = true;
	}
	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector4>::get_ptr(r_ret) = Vector4(VariantGetInternalPtr<Vector4i>::get_ptr(right)->x, VariantGetInternalPtr<Vector4i>::get_ptr(right)->y, VariantGetInternalPtr<Vector4i>::get_ptr(right)->z, VariantGetInternalPtr<Vector4i>::get_ptr(right)->w) * *VariantGetInternalPtr<double>::get_ptr(left);
	}
	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector4>::encode(Vector4(PtrToArg<Vector4i>::convert(right).x, PtrToArg<Vector4i>::convert(right).y, PtrToArg<Vector4i>::convert(right).z, PtrToArg<Vector4i>::convert(right).w) * PtrToArg<double>::convert(left), r_ret);
	}
	static Variant::Type get_return_type() { return GetTypeInfo<Vector4>::VARIANT_TYPE; }
};

template <>
class OperatorEvaluatorDivNZ<Vector4, Vector4i, double> {
public:
	static void evaluate(const Variant &p_left, const Variant &p_right, Variant *r_ret, bool &r_valid) {
		const Vector4i &a = *VariantGetInternalPtr<Vector4i>::get_ptr(&p_left);
		const double &b = *VariantGetInternalPtr<double>::get_ptr(&p_right);
		if (unlikely(b == 0)) {
			r_valid = false;
			*r_ret = "Division by zero error";
			return;
		}
		*r_ret = Vector4(a.x, a.y, a.z, a.w) / b;
		r_valid = true;
	}

	static inline void validated_evaluate(const Variant *left, const Variant *right, Variant *r_ret) {
		*VariantGetInternalPtr<Vector4>::get_ptr(r_ret) = Vector4(VariantGetInternalPtr<Vector4i>::get_ptr(left)->x, VariantGetInternalPtr<Vector4i>::get_ptr(left)->y, VariantGetInternalPtr<Vector4i>::get_ptr(left)->z, VariantGetInternalPtr<Vector4i>::get_ptr(left)->w) / *VariantGetInternalPtr<double>::get_ptr(right);
	}

	static void ptr_evaluate(const void *left, const void *right, void *r_ret) {
		PtrToArg<Vector4>::encode(Vector4(PtrToArg<Vector4i>::convert(left).x, PtrToArg<Vector4i>::convert(left).y, PtrToArg<Vector4i>::convert(left).z, PtrToArg<Vector4i>::convert(left).w) / PtrToArg<double>::convert(right), r_ret);
	}

	static Variant::Type get_return_type() { return GetTypeInfo<Vector4>::VARIANT_TYPE; }
};

#define register_string_op(m_op_type, m_op_code)                                                               \
	if constexpr (true) {                                                                                      \
		register_op<m_op_type<String, String>>(m_op_code, Variant::STRING, Variant::STRING);                   \
		register_op<m_op_type<String, StringName>>(m_op_code, Variant::STRING, Variant::STRING_NAME);          \
		register_op<m_op_type<StringName, String>>(m_op_code, Variant::STRING_NAME, Variant::STRING);          \
		register_op<m_op_type<StringName, StringName>>(m_op_code, Variant::STRING_NAME, Variant::STRING_NAME); \
	} else                                                                                                     \
		((void)0)

#define register_string_modulo_op(m_class, m_type)                                                                         \
	if constexpr (true) {                                                                                                  \
		register_op<OperatorEvaluatorStringFormat<String, m_class>>(Variant::OP_MODULE, Variant::STRING, m_type);          \
		register_op<OperatorEvaluatorStringFormat<StringName, m_class>>(Variant::OP_MODULE, Variant::STRING_NAME, m_type); \
	} else                                                                                                                 \
		((void)0)

void Variant::_register_variant_operators() {
	memset(operator_return_type_table, 0, sizeof(operator_return_type_table));
	memset(operator_evaluator_table, 0, sizeof(operator_evaluator_table));
	memset(validated_operator_evaluator_table, 0, sizeof(validated_operator_evaluator_table));
	memset(ptr_operator_evaluator_table, 0, sizeof(ptr_operator_evaluator_table));

	register_op<OperatorEvaluatorAdd<int64_t, int64_t, int64_t>>(Variant::OP_ADD, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorAdd<double, int64_t, double>>(Variant::OP_ADD, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorAdd<double, double, int64_t>>(Variant::OP_ADD, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorAdd<double, double, double>>(Variant::OP_ADD, Variant::FLOAT, Variant::FLOAT);
	register_string_op(OperatorEvaluatorStringConcat, Variant::OP_ADD);
	register_op<OperatorEvaluatorAdd<Vector2, Vector2, Vector2>>(Variant::OP_ADD, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorAdd<Vector2i, Vector2i, Vector2i>>(Variant::OP_ADD, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorAdd<Vector3, Vector3, Vector3>>(Variant::OP_ADD, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorAdd<Vector3i, Vector3i, Vector3i>>(Variant::OP_ADD, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorAdd<Vector4, Vector4, Vector4>>(Variant::OP_ADD, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorAdd<Vector4i, Vector4i, Vector4i>>(Variant::OP_ADD, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorAdd<Quaternion, Quaternion, Quaternion>>(Variant::OP_ADD, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorAdd<Color, Color, Color>>(Variant::OP_ADD, Variant::COLOR, Variant::COLOR);
	register_op<OperatorEvaluatorAddArray>(Variant::OP_ADD, Variant::ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorAppendArray<uint8_t>>(Variant::OP_ADD, Variant::PACKED_BYTE_ARRAY, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorAppendArray<int32_t>>(Variant::OP_ADD, Variant::PACKED_INT32_ARRAY, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorAppendArray<int64_t>>(Variant::OP_ADD, Variant::PACKED_INT64_ARRAY, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorAppendArray<float>>(Variant::OP_ADD, Variant::PACKED_FLOAT32_ARRAY, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorAppendArray<double>>(Variant::OP_ADD, Variant::PACKED_FLOAT64_ARRAY, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorAppendArray<String>>(Variant::OP_ADD, Variant::PACKED_STRING_ARRAY, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorAppendArray<Vector2>>(Variant::OP_ADD, Variant::PACKED_VECTOR2_ARRAY, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorAppendArray<Vector3>>(Variant::OP_ADD, Variant::PACKED_VECTOR3_ARRAY, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorAppendArray<Color>>(Variant::OP_ADD, Variant::PACKED_COLOR_ARRAY, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorAppendArray<Vector4>>(Variant::OP_ADD, Variant::PACKED_VECTOR4_ARRAY, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorSub<int64_t, int64_t, int64_t>>(Variant::OP_SUBTRACT, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorSub<double, int64_t, double>>(Variant::OP_SUBTRACT, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorSub<double, double, int64_t>>(Variant::OP_SUBTRACT, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorSub<double, double, double>>(Variant::OP_SUBTRACT, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorSub<Vector2, Vector2, Vector2>>(Variant::OP_SUBTRACT, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorSub<Vector2i, Vector2i, Vector2i>>(Variant::OP_SUBTRACT, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorSub<Vector3, Vector3, Vector3>>(Variant::OP_SUBTRACT, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorSub<Vector3i, Vector3i, Vector3i>>(Variant::OP_SUBTRACT, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorSub<Vector4, Vector4, Vector4>>(Variant::OP_SUBTRACT, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorSub<Vector4i, Vector4i, Vector4i>>(Variant::OP_SUBTRACT, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorSub<Quaternion, Quaternion, Quaternion>>(Variant::OP_SUBTRACT, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorSub<Color, Color, Color>>(Variant::OP_SUBTRACT, Variant::COLOR, Variant::COLOR);

	register_op<OperatorEvaluatorMul<int64_t, int64_t, int64_t>>(Variant::OP_MULTIPLY, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorMul<double, int64_t, double>>(Variant::OP_MULTIPLY, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorMul<Vector2, int64_t, Vector2>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR2);
	register_op<OperatorEvaluatorMul<Vector2i, int64_t, Vector2i>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR2I);
	register_op<OperatorEvaluatorMul<Vector3, int64_t, Vector3>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR3);
	register_op<OperatorEvaluatorMul<Vector3i, int64_t, Vector3i>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR3I);
	register_op<OperatorEvaluatorMul<Vector4, int64_t, Vector4>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR4);
	register_op<OperatorEvaluatorMul<Vector4i, int64_t, Vector4i>>(Variant::OP_MULTIPLY, Variant::INT, Variant::VECTOR4I);

	register_op<OperatorEvaluatorMul<double, double, double>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorMul<double, double, int64_t>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector2, double, Vector2>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR2);
	register_op<OperatorEvaluatorMul<Vector2, double, Vector2i>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR2I);
	register_op<OperatorEvaluatorMul<Vector3, double, Vector3>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR3);
	register_op<OperatorEvaluatorMul<Vector3, double, Vector3i>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR3I);
	register_op<OperatorEvaluatorMul<Vector4, double, Vector4>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR4);
	register_op<OperatorEvaluatorMul<Vector4, double, Vector4i>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::VECTOR4I);

	register_op<OperatorEvaluatorMul<Vector2, Vector2, Vector2>>(Variant::OP_MULTIPLY, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorMul<Vector2, Vector2, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR2, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector2, Vector2, double>>(Variant::OP_MULTIPLY, Variant::VECTOR2, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Vector2i, Vector2i, Vector2i>>(Variant::OP_MULTIPLY, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorMul<Vector2i, Vector2i, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR2I, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector2, Vector2i, double>>(Variant::OP_MULTIPLY, Variant::VECTOR2I, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Vector3, Vector3, Vector3>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorMul<Vector3, Vector3, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector3, Vector3, double>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Vector3i, Vector3i, Vector3i>>(Variant::OP_MULTIPLY, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorMul<Vector3i, Vector3i, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR3I, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector3, Vector3i, double>>(Variant::OP_MULTIPLY, Variant::VECTOR3I, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Vector4, Vector4, Vector4>>(Variant::OP_MULTIPLY, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorMul<Vector4, Vector4, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR4, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector4, Vector4, double>>(Variant::OP_MULTIPLY, Variant::VECTOR4, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Vector4i, Vector4i, Vector4i>>(Variant::OP_MULTIPLY, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorMul<Vector4i, Vector4i, int64_t>>(Variant::OP_MULTIPLY, Variant::VECTOR4I, Variant::INT);
	register_op<OperatorEvaluatorMul<Vector4, Vector4i, double>>(Variant::OP_MULTIPLY, Variant::VECTOR4I, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, Quaternion>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, int64_t>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::INT);
	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, double>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Color, Color, Color>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::COLOR);
	register_op<OperatorEvaluatorMul<Color, Color, int64_t>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::INT);
	register_op<OperatorEvaluatorMul<Color, Color, double>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::FLOAT);

	register_op<OperatorEvaluatorMul<Transform2D, Transform2D, Transform2D>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorMul<Transform2D, Transform2D, int64_t>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::INT);
	register_op<OperatorEvaluatorMul<Transform2D, Transform2D, double>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::FLOAT);
	register_op<OperatorEvaluatorXForm<Vector2, Transform2D, Vector2>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::VECTOR2);
	register_op<OperatorEvaluatorXFormInv<Vector2, Vector2, Transform2D>>(Variant::OP_MULTIPLY, Variant::VECTOR2, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorXForm<Rect2, Transform2D, Rect2>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::RECT2);
	register_op<OperatorEvaluatorXFormInv<Rect2, Rect2, Transform2D>>(Variant::OP_MULTIPLY, Variant::RECT2, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorXForm<Vector<Vector2>, Transform2D, Vector<Vector2>>>(Variant::OP_MULTIPLY, Variant::TRANSFORM2D, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorXFormInv<Vector<Vector2>, Vector<Vector2>, Transform2D>>(Variant::OP_MULTIPLY, Variant::PACKED_VECTOR2_ARRAY, Variant::TRANSFORM2D);

	register_op<OperatorEvaluatorMul<Transform3D, Transform3D, Transform3D>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorMul<Transform3D, Transform3D, int64_t>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::INT);
	register_op<OperatorEvaluatorMul<Transform3D, Transform3D, double>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::FLOAT);
	register_op<OperatorEvaluatorXForm<Vector3, Transform3D, Vector3>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::VECTOR3);
	register_op<OperatorEvaluatorXFormInv<Vector3, Vector3, Transform3D>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorXForm<::AABB, Transform3D, ::AABB>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::AABB);
	register_op<OperatorEvaluatorXFormInv<::AABB, ::AABB, Transform3D>>(Variant::OP_MULTIPLY, Variant::AABB, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorXForm<Plane, Transform3D, Plane>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::PLANE);
	register_op<OperatorEvaluatorXFormInv<Plane, Plane, Transform3D>>(Variant::OP_MULTIPLY, Variant::PLANE, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorXForm<Vector<Vector3>, Transform3D, Vector<Vector3>>>(Variant::OP_MULTIPLY, Variant::TRANSFORM3D, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorXFormInv<Vector<Vector3>, Vector<Vector3>, Transform3D>>(Variant::OP_MULTIPLY, Variant::PACKED_VECTOR3_ARRAY, Variant::TRANSFORM3D);

	register_op<OperatorEvaluatorXForm<Vector4, Projection, Vector4>>(Variant::OP_MULTIPLY, Variant::PROJECTION, Variant::VECTOR4);
	register_op<OperatorEvaluatorXFormInv<Vector4, Vector4, Projection>>(Variant::OP_MULTIPLY, Variant::VECTOR4, Variant::PROJECTION);

	register_op<OperatorEvaluatorMul<Projection, Projection, Projection>>(Variant::OP_MULTIPLY, Variant::PROJECTION, Variant::PROJECTION);

	register_op<OperatorEvaluatorMul<Basis, Basis, Basis>>(Variant::OP_MULTIPLY, Variant::BASIS, Variant::BASIS);
	register_op<OperatorEvaluatorMul<Basis, Basis, int64_t>>(Variant::OP_MULTIPLY, Variant::BASIS, Variant::INT);
	register_op<OperatorEvaluatorMul<Basis, Basis, double>>(Variant::OP_MULTIPLY, Variant::BASIS, Variant::FLOAT);
	register_op<OperatorEvaluatorXForm<Vector3, Basis, Vector3>>(Variant::OP_MULTIPLY, Variant::BASIS, Variant::VECTOR3);
	register_op<OperatorEvaluatorXFormInv<Vector3, Vector3, Basis>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::BASIS);

	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, Quaternion>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, int64_t>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::INT);
	register_op<OperatorEvaluatorMul<Quaternion, int64_t, Quaternion>>(Variant::OP_MULTIPLY, Variant::INT, Variant::QUATERNION);
	register_op<OperatorEvaluatorMul<Quaternion, Quaternion, double>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::FLOAT);
	register_op<OperatorEvaluatorMul<Quaternion, double, Quaternion>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::QUATERNION);
	register_op<OperatorEvaluatorXForm<Vector3, Quaternion, Vector3>>(Variant::OP_MULTIPLY, Variant::QUATERNION, Variant::VECTOR3);
	register_op<OperatorEvaluatorXFormInv<Vector3, Vector3, Quaternion>>(Variant::OP_MULTIPLY, Variant::VECTOR3, Variant::QUATERNION);

	register_op<OperatorEvaluatorMul<Color, Color, Color>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::COLOR);
	register_op<OperatorEvaluatorMul<Color, Color, int64_t>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::INT);
	register_op<OperatorEvaluatorMul<Color, int64_t, Color>>(Variant::OP_MULTIPLY, Variant::INT, Variant::COLOR);
	register_op<OperatorEvaluatorMul<Color, Color, double>>(Variant::OP_MULTIPLY, Variant::COLOR, Variant::FLOAT);
	register_op<OperatorEvaluatorMul<Color, double, Color>>(Variant::OP_MULTIPLY, Variant::FLOAT, Variant::COLOR);

	register_op<OperatorEvaluatorDivNZ<int64_t, int64_t, int64_t>>(Variant::OP_DIVIDE, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorDiv<double, double, int64_t>>(Variant::OP_DIVIDE, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorDiv<double, int64_t, double>>(Variant::OP_DIVIDE, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<double, double, double>>(Variant::OP_DIVIDE, Variant::FLOAT, Variant::FLOAT);

	register_op<OperatorEvaluatorDiv<Vector2, Vector2, Vector2>>(Variant::OP_DIVIDE, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorDiv<Vector2, Vector2, double>>(Variant::OP_DIVIDE, Variant::VECTOR2, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<Vector2, Vector2, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR2, Variant::INT);

	register_op<OperatorEvaluatorDivNZ<Vector2i, Vector2i, Vector2i>>(Variant::OP_DIVIDE, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorDivNZ<Vector2, Vector2i, double>>(Variant::OP_DIVIDE, Variant::VECTOR2I, Variant::FLOAT);
	register_op<OperatorEvaluatorDivNZ<Vector2i, Vector2i, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR2I, Variant::INT);

	register_op<OperatorEvaluatorDiv<Vector3, Vector3, Vector3>>(Variant::OP_DIVIDE, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorDiv<Vector3, Vector3, double>>(Variant::OP_DIVIDE, Variant::VECTOR3, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<Vector3, Vector3, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR3, Variant::INT);

	register_op<OperatorEvaluatorDivNZ<Vector3i, Vector3i, Vector3i>>(Variant::OP_DIVIDE, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorDivNZ<Vector3, Vector3i, double>>(Variant::OP_DIVIDE, Variant::VECTOR3I, Variant::FLOAT);
	register_op<OperatorEvaluatorDivNZ<Vector3i, Vector3i, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR3I, Variant::INT);

	register_op<OperatorEvaluatorDiv<Vector4, Vector4, Vector4>>(Variant::OP_DIVIDE, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorDiv<Vector4, Vector4, double>>(Variant::OP_DIVIDE, Variant::VECTOR4, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<Vector4, Vector4, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR4, Variant::INT);

	register_op<OperatorEvaluatorDivNZ<Vector4i, Vector4i, Vector4i>>(Variant::OP_DIVIDE, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorDivNZ<Vector4, Vector4i, double>>(Variant::OP_DIVIDE, Variant::VECTOR4I, Variant::FLOAT);
	register_op<OperatorEvaluatorDivNZ<Vector4i, Vector4i, int64_t>>(Variant::OP_DIVIDE, Variant::VECTOR4I, Variant::INT);

	register_op<OperatorEvaluatorDiv<Transform2D, Transform2D, int64_t>>(Variant::OP_DIVIDE, Variant::TRANSFORM2D, Variant::INT);
	register_op<OperatorEvaluatorDiv<Transform2D, Transform2D, double>>(Variant::OP_DIVIDE, Variant::TRANSFORM2D, Variant::FLOAT);

	register_op<OperatorEvaluatorDiv<Transform3D, Transform3D, int64_t>>(Variant::OP_DIVIDE, Variant::TRANSFORM3D, Variant::INT);
	register_op<OperatorEvaluatorDiv<Transform3D, Transform3D, double>>(Variant::OP_DIVIDE, Variant::TRANSFORM3D, Variant::FLOAT);

	register_op<OperatorEvaluatorDiv<Basis, Basis, int64_t>>(Variant::OP_DIVIDE, Variant::BASIS, Variant::INT);
	register_op<OperatorEvaluatorDiv<Basis, Basis, double>>(Variant::OP_DIVIDE, Variant::BASIS, Variant::FLOAT);

	register_op<OperatorEvaluatorDiv<Quaternion, Quaternion, double>>(Variant::OP_DIVIDE, Variant::QUATERNION, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<Quaternion, Quaternion, int64_t>>(Variant::OP_DIVIDE, Variant::QUATERNION, Variant::INT);

	register_op<OperatorEvaluatorDiv<Color, Color, Color>>(Variant::OP_DIVIDE, Variant::COLOR, Variant::COLOR);
	register_op<OperatorEvaluatorDiv<Color, Color, double>>(Variant::OP_DIVIDE, Variant::COLOR, Variant::FLOAT);
	register_op<OperatorEvaluatorDiv<Color, Color, int64_t>>(Variant::OP_DIVIDE, Variant::COLOR, Variant::INT);

	register_op<OperatorEvaluatorModNZ<int64_t, int64_t, int64_t>>(Variant::OP_MODULE, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorModNZ<Vector2i, Vector2i, Vector2i>>(Variant::OP_MODULE, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorModNZ<Vector2i, Vector2i, int64_t>>(Variant::OP_MODULE, Variant::VECTOR2I, Variant::INT);

	register_op<OperatorEvaluatorModNZ<Vector3i, Vector3i, Vector3i>>(Variant::OP_MODULE, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorModNZ<Vector3i, Vector3i, int64_t>>(Variant::OP_MODULE, Variant::VECTOR3I, Variant::INT);

	register_op<OperatorEvaluatorModNZ<Vector4i, Vector4i, Vector4i>>(Variant::OP_MODULE, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorModNZ<Vector4i, Vector4i, int64_t>>(Variant::OP_MODULE, Variant::VECTOR4I, Variant::INT);

	register_string_modulo_op(void, Variant::NIL);

	register_string_modulo_op(bool, Variant::BOOL);
	register_string_modulo_op(int64_t, Variant::INT);
	register_string_modulo_op(double, Variant::FLOAT);
	register_string_modulo_op(String, Variant::STRING);
	register_string_modulo_op(Vector2, Variant::VECTOR2);
	register_string_modulo_op(Vector2i, Variant::VECTOR2I);
	register_string_modulo_op(Rect2, Variant::RECT2);
	register_string_modulo_op(Rect2i, Variant::RECT2I);
	register_string_modulo_op(Vector3, Variant::VECTOR3);
	register_string_modulo_op(Vector3i, Variant::VECTOR3I);
	register_string_modulo_op(Vector4, Variant::VECTOR4);
	register_string_modulo_op(Vector4i, Variant::VECTOR4I);
	register_string_modulo_op(Transform2D, Variant::TRANSFORM2D);
	register_string_modulo_op(Plane, Variant::PLANE);
	register_string_modulo_op(Quaternion, Variant::QUATERNION);
	register_string_modulo_op(::AABB, Variant::AABB);
	register_string_modulo_op(Basis, Variant::BASIS);
	register_string_modulo_op(Transform3D, Variant::TRANSFORM3D);
	register_string_modulo_op(Projection, Variant::PROJECTION);

	register_string_modulo_op(Color, Variant::COLOR);
	register_string_modulo_op(StringName, Variant::STRING_NAME);
	register_string_modulo_op(NodePath, Variant::NODE_PATH);
	register_string_modulo_op(Object, Variant::OBJECT);
	register_string_modulo_op(Callable, Variant::CALLABLE);
	register_string_modulo_op(Signal, Variant::SIGNAL);
	register_string_modulo_op(Dictionary, Variant::DICTIONARY);
	register_string_modulo_op(Array, Variant::ARRAY);

	register_string_modulo_op(PackedByteArray, Variant::PACKED_BYTE_ARRAY);
	register_string_modulo_op(PackedInt32Array, Variant::PACKED_INT32_ARRAY);
	register_string_modulo_op(PackedInt64Array, Variant::PACKED_INT64_ARRAY);
	register_string_modulo_op(PackedFloat32Array, Variant::PACKED_FLOAT32_ARRAY);
	register_string_modulo_op(PackedFloat64Array, Variant::PACKED_FLOAT64_ARRAY);
	register_string_modulo_op(PackedStringArray, Variant::PACKED_STRING_ARRAY);
	register_string_modulo_op(PackedVector2Array, Variant::PACKED_VECTOR2_ARRAY);
	register_string_modulo_op(PackedVector3Array, Variant::PACKED_VECTOR3_ARRAY);
	register_string_modulo_op(PackedColorArray, Variant::PACKED_COLOR_ARRAY);
	register_string_modulo_op(PackedVector4Array, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorPow<int64_t, int64_t, int64_t>>(Variant::OP_POWER, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorPow<double, int64_t, double>>(Variant::OP_POWER, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorPow<double, double, double>>(Variant::OP_POWER, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorPow<double, double, int64_t>>(Variant::OP_POWER, Variant::FLOAT, Variant::INT);

	register_op<OperatorEvaluatorNeg<int64_t, int64_t>>(Variant::OP_NEGATE, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorNeg<double, double>>(Variant::OP_NEGATE, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector2, Vector2>>(Variant::OP_NEGATE, Variant::VECTOR2, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector2i, Vector2i>>(Variant::OP_NEGATE, Variant::VECTOR2I, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector3, Vector3>>(Variant::OP_NEGATE, Variant::VECTOR3, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector3i, Vector3i>>(Variant::OP_NEGATE, Variant::VECTOR3I, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector4, Vector4>>(Variant::OP_NEGATE, Variant::VECTOR4, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Vector4i, Vector4i>>(Variant::OP_NEGATE, Variant::VECTOR4I, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Quaternion, Quaternion>>(Variant::OP_NEGATE, Variant::QUATERNION, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Plane, Plane>>(Variant::OP_NEGATE, Variant::PLANE, Variant::NIL);
	register_op<OperatorEvaluatorNeg<Color, Color>>(Variant::OP_NEGATE, Variant::COLOR, Variant::NIL);

	register_op<OperatorEvaluatorPos<int64_t, int64_t>>(Variant::OP_POSITIVE, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorPos<double, double>>(Variant::OP_POSITIVE, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector2, Vector2>>(Variant::OP_POSITIVE, Variant::VECTOR2, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector2i, Vector2i>>(Variant::OP_POSITIVE, Variant::VECTOR2I, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector3, Vector3>>(Variant::OP_POSITIVE, Variant::VECTOR3, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector3i, Vector3i>>(Variant::OP_POSITIVE, Variant::VECTOR3I, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector4, Vector4>>(Variant::OP_POSITIVE, Variant::VECTOR4, Variant::NIL);
	register_op<OperatorEvaluatorPos<Vector4i, Vector4i>>(Variant::OP_POSITIVE, Variant::VECTOR4I, Variant::NIL);
	register_op<OperatorEvaluatorPos<Quaternion, Quaternion>>(Variant::OP_POSITIVE, Variant::QUATERNION, Variant::NIL);
	register_op<OperatorEvaluatorPos<Plane, Plane>>(Variant::OP_POSITIVE, Variant::PLANE, Variant::NIL);
	register_op<OperatorEvaluatorPos<Color, Color>>(Variant::OP_POSITIVE, Variant::COLOR, Variant::NIL);

	register_op<OperatorEvaluatorShiftLeft<int64_t, int64_t, int64_t>>(Variant::OP_SHIFT_LEFT, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorShiftRight<int64_t, int64_t, int64_t>>(Variant::OP_SHIFT_RIGHT, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorBitOr<int64_t, int64_t, int64_t>>(Variant::OP_BIT_OR, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorBitAnd<int64_t, int64_t, int64_t>>(Variant::OP_BIT_AND, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorBitXor<int64_t, int64_t, int64_t>>(Variant::OP_BIT_XOR, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorBitNeg<int64_t, int64_t>>(Variant::OP_BIT_NEGATE, Variant::INT, Variant::NIL);

	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_EQUAL, Variant::NIL, Variant::NIL>>(Variant::OP_EQUAL, Variant::NIL, Variant::NIL);
	register_op<OperatorEvaluatorEqual<bool, bool>>(Variant::OP_EQUAL, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorEqual<int64_t, int64_t>>(Variant::OP_EQUAL, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorEqual<int64_t, double>>(Variant::OP_EQUAL, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorEqual<double, int64_t>>(Variant::OP_EQUAL, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorEqual<double, double>>(Variant::OP_EQUAL, Variant::FLOAT, Variant::FLOAT);
	register_string_op(OperatorEvaluatorEqual, Variant::OP_EQUAL);
	register_op<OperatorEvaluatorEqual<Vector2, Vector2>>(Variant::OP_EQUAL, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorEqual<Vector2i, Vector2i>>(Variant::OP_EQUAL, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorEqual<Rect2, Rect2>>(Variant::OP_EQUAL, Variant::RECT2, Variant::RECT2);
	register_op<OperatorEvaluatorEqual<Rect2i, Rect2i>>(Variant::OP_EQUAL, Variant::RECT2I, Variant::RECT2I);
	register_op<OperatorEvaluatorEqual<Vector3, Vector3>>(Variant::OP_EQUAL, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorEqual<Vector3i, Vector3i>>(Variant::OP_EQUAL, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorEqual<Transform2D, Transform2D>>(Variant::OP_EQUAL, Variant::TRANSFORM2D, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorEqual<Vector4, Vector4>>(Variant::OP_EQUAL, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorEqual<Vector4i, Vector4i>>(Variant::OP_EQUAL, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorEqual<Plane, Plane>>(Variant::OP_EQUAL, Variant::PLANE, Variant::PLANE);
	register_op<OperatorEvaluatorEqual<Quaternion, Quaternion>>(Variant::OP_EQUAL, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorEqual<::AABB, ::AABB>>(Variant::OP_EQUAL, Variant::AABB, Variant::AABB);
	register_op<OperatorEvaluatorEqual<Basis, Basis>>(Variant::OP_EQUAL, Variant::BASIS, Variant::BASIS);
	register_op<OperatorEvaluatorEqual<Transform3D, Transform3D>>(Variant::OP_EQUAL, Variant::TRANSFORM3D, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorEqual<Projection, Projection>>(Variant::OP_EQUAL, Variant::PROJECTION, Variant::PROJECTION);
	register_op<OperatorEvaluatorEqual<Color, Color>>(Variant::OP_EQUAL, Variant::COLOR, Variant::COLOR);

	register_op<OperatorEvaluatorEqual<NodePath, NodePath>>(Variant::OP_EQUAL, Variant::NODE_PATH, Variant::NODE_PATH);
	register_op<OperatorEvaluatorEqual<::RID, ::RID>>(Variant::OP_EQUAL, Variant::RID, Variant::RID);

	register_op<OperatorEvaluatorEqualObject>(Variant::OP_EQUAL, Variant::OBJECT, Variant::OBJECT);
	register_op<OperatorEvaluatorEqualObjectNil>(Variant::OP_EQUAL, Variant::OBJECT, Variant::NIL);
	register_op<OperatorEvaluatorEqualNilObject>(Variant::OP_EQUAL, Variant::NIL, Variant::OBJECT);

	register_op<OperatorEvaluatorEqual<Callable, Callable>>(Variant::OP_EQUAL, Variant::CALLABLE, Variant::CALLABLE);
	register_op<OperatorEvaluatorEqual<Signal, Signal>>(Variant::OP_EQUAL, Variant::SIGNAL, Variant::SIGNAL);
	register_op<OperatorEvaluatorEqual<Dictionary, Dictionary>>(Variant::OP_EQUAL, Variant::DICTIONARY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorEqual<Array, Array>>(Variant::OP_EQUAL, Variant::ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorEqual<PackedByteArray, PackedByteArray>>(Variant::OP_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedInt32Array, PackedInt32Array>>(Variant::OP_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedInt64Array, PackedInt64Array>>(Variant::OP_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedFloat32Array, PackedFloat32Array>>(Variant::OP_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedFloat64Array, PackedFloat64Array>>(Variant::OP_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedStringArray, PackedStringArray>>(Variant::OP_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedVector2Array, PackedVector2Array>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedVector3Array, PackedVector3Array>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedColorArray, PackedColorArray>>(Variant::OP_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorEqual<PackedVector4Array, PackedVector4Array>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::BOOL, Variant::NIL>>(Variant::OP_EQUAL, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::INT, Variant::NIL>>(Variant::OP_EQUAL, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::FLOAT, Variant::NIL>>(Variant::OP_EQUAL, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::STRING, Variant::NIL>>(Variant::OP_EQUAL, Variant::STRING, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR2, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR2, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR2I, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR2I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::RECT2, Variant::NIL>>(Variant::OP_EQUAL, Variant::RECT2, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::RECT2I, Variant::NIL>>(Variant::OP_EQUAL, Variant::RECT2I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR3, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR3, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR3I, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR3I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR4, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR4, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::VECTOR4I, Variant::NIL>>(Variant::OP_EQUAL, Variant::VECTOR4I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::TRANSFORM2D, Variant::NIL>>(Variant::OP_EQUAL, Variant::TRANSFORM2D, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PLANE, Variant::NIL>>(Variant::OP_EQUAL, Variant::PLANE, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::QUATERNION, Variant::NIL>>(Variant::OP_EQUAL, Variant::QUATERNION, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::AABB, Variant::NIL>>(Variant::OP_EQUAL, Variant::AABB, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::BASIS, Variant::NIL>>(Variant::OP_EQUAL, Variant::BASIS, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::TRANSFORM3D, Variant::NIL>>(Variant::OP_EQUAL, Variant::TRANSFORM3D, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PROJECTION, Variant::NIL>>(Variant::OP_EQUAL, Variant::PROJECTION, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::COLOR, Variant::NIL>>(Variant::OP_EQUAL, Variant::COLOR, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::STRING_NAME, Variant::NIL>>(Variant::OP_EQUAL, Variant::STRING_NAME, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NODE_PATH, Variant::NIL>>(Variant::OP_EQUAL, Variant::NODE_PATH, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::RID, Variant::NIL>>(Variant::OP_EQUAL, Variant::RID, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::CALLABLE, Variant::NIL>>(Variant::OP_EQUAL, Variant::CALLABLE, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::SIGNAL, Variant::NIL>>(Variant::OP_EQUAL, Variant::SIGNAL, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::DICTIONARY, Variant::NIL>>(Variant::OP_EQUAL, Variant::DICTIONARY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::NIL>>(Variant::OP_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::NIL);

	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::BOOL>>(Variant::OP_EQUAL, Variant::NIL, Variant::BOOL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::INT>>(Variant::OP_EQUAL, Variant::NIL, Variant::INT);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::FLOAT>>(Variant::OP_EQUAL, Variant::NIL, Variant::FLOAT);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::STRING>>(Variant::OP_EQUAL, Variant::NIL, Variant::STRING);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR2>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR2);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR2I>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR2I);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::RECT2>>(Variant::OP_EQUAL, Variant::NIL, Variant::RECT2);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::RECT2I>>(Variant::OP_EQUAL, Variant::NIL, Variant::RECT2I);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR3>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR3);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR3I>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR3I);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR4>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR4);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR4I>>(Variant::OP_EQUAL, Variant::NIL, Variant::VECTOR4I);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::TRANSFORM2D>>(Variant::OP_EQUAL, Variant::NIL, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PLANE>>(Variant::OP_EQUAL, Variant::NIL, Variant::PLANE);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::QUATERNION>>(Variant::OP_EQUAL, Variant::NIL, Variant::QUATERNION);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::AABB>>(Variant::OP_EQUAL, Variant::NIL, Variant::AABB);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::BASIS>>(Variant::OP_EQUAL, Variant::NIL, Variant::BASIS);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::TRANSFORM3D>>(Variant::OP_EQUAL, Variant::NIL, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PROJECTION>>(Variant::OP_EQUAL, Variant::NIL, Variant::PROJECTION);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::COLOR>>(Variant::OP_EQUAL, Variant::NIL, Variant::COLOR);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::STRING_NAME>>(Variant::OP_EQUAL, Variant::NIL, Variant::STRING_NAME);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::NODE_PATH>>(Variant::OP_EQUAL, Variant::NIL, Variant::NODE_PATH);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::RID>>(Variant::OP_EQUAL, Variant::NIL, Variant::RID);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::CALLABLE>>(Variant::OP_EQUAL, Variant::NIL, Variant::CALLABLE);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::SIGNAL>>(Variant::OP_EQUAL, Variant::NIL, Variant::SIGNAL);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::DICTIONARY>>(Variant::OP_EQUAL, Variant::NIL, Variant::DICTIONARY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_BYTE_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_INT32_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_INT64_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_FLOAT32_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_FLOAT64_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_STRING_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR2_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR3_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_COLOR_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR4_ARRAY>>(Variant::OP_EQUAL, Variant::NIL, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::NIL);
	register_op<OperatorEvaluatorNotEqual<bool, bool>>(Variant::OP_NOT_EQUAL, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorNotEqual<int64_t, int64_t>>(Variant::OP_NOT_EQUAL, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorNotEqual<int64_t, double>>(Variant::OP_NOT_EQUAL, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorNotEqual<double, int64_t>>(Variant::OP_NOT_EQUAL, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorNotEqual<double, double>>(Variant::OP_NOT_EQUAL, Variant::FLOAT, Variant::FLOAT);
	register_string_op(OperatorEvaluatorNotEqual, Variant::OP_NOT_EQUAL);
	register_op<OperatorEvaluatorNotEqual<Vector2, Vector2>>(Variant::OP_NOT_EQUAL, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorNotEqual<Vector2i, Vector2i>>(Variant::OP_NOT_EQUAL, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorNotEqual<Rect2, Rect2>>(Variant::OP_NOT_EQUAL, Variant::RECT2, Variant::RECT2);
	register_op<OperatorEvaluatorNotEqual<Rect2i, Rect2i>>(Variant::OP_NOT_EQUAL, Variant::RECT2I, Variant::RECT2I);
	register_op<OperatorEvaluatorNotEqual<Vector3, Vector3>>(Variant::OP_NOT_EQUAL, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorNotEqual<Vector3i, Vector3i>>(Variant::OP_NOT_EQUAL, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorNotEqual<Vector4, Vector4>>(Variant::OP_NOT_EQUAL, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorNotEqual<Vector4i, Vector4i>>(Variant::OP_NOT_EQUAL, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorNotEqual<Transform2D, Transform2D>>(Variant::OP_NOT_EQUAL, Variant::TRANSFORM2D, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorNotEqual<Plane, Plane>>(Variant::OP_NOT_EQUAL, Variant::PLANE, Variant::PLANE);
	register_op<OperatorEvaluatorNotEqual<Quaternion, Quaternion>>(Variant::OP_NOT_EQUAL, Variant::QUATERNION, Variant::QUATERNION);
	register_op<OperatorEvaluatorNotEqual<::AABB, ::AABB>>(Variant::OP_NOT_EQUAL, Variant::AABB, Variant::AABB);
	register_op<OperatorEvaluatorNotEqual<Basis, Basis>>(Variant::OP_NOT_EQUAL, Variant::BASIS, Variant::BASIS);
	register_op<OperatorEvaluatorNotEqual<Transform3D, Transform3D>>(Variant::OP_NOT_EQUAL, Variant::TRANSFORM3D, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorNotEqual<Projection, Projection>>(Variant::OP_NOT_EQUAL, Variant::PROJECTION, Variant::PROJECTION);
	register_op<OperatorEvaluatorNotEqual<Color, Color>>(Variant::OP_NOT_EQUAL, Variant::COLOR, Variant::COLOR);

	register_op<OperatorEvaluatorNotEqual<NodePath, NodePath>>(Variant::OP_NOT_EQUAL, Variant::NODE_PATH, Variant::NODE_PATH);
	register_op<OperatorEvaluatorNotEqual<::RID, ::RID>>(Variant::OP_NOT_EQUAL, Variant::RID, Variant::RID);

	register_op<OperatorEvaluatorNotEqualObject>(Variant::OP_NOT_EQUAL, Variant::OBJECT, Variant::OBJECT);
	register_op<OperatorEvaluatorNotEqualObjectNil>(Variant::OP_NOT_EQUAL, Variant::OBJECT, Variant::NIL);
	register_op<OperatorEvaluatorNotEqualNilObject>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::OBJECT);

	register_op<OperatorEvaluatorNotEqual<Callable, Callable>>(Variant::OP_NOT_EQUAL, Variant::CALLABLE, Variant::CALLABLE);
	register_op<OperatorEvaluatorNotEqual<Signal, Signal>>(Variant::OP_NOT_EQUAL, Variant::SIGNAL, Variant::SIGNAL);
	register_op<OperatorEvaluatorNotEqual<Dictionary, Dictionary>>(Variant::OP_NOT_EQUAL, Variant::DICTIONARY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorNotEqual<Array, Array>>(Variant::OP_NOT_EQUAL, Variant::ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedByteArray, PackedByteArray>>(Variant::OP_NOT_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedInt32Array, PackedInt32Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedInt64Array, PackedInt64Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedFloat32Array, PackedFloat32Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedFloat64Array, PackedFloat64Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedStringArray, PackedStringArray>>(Variant::OP_NOT_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedVector2Array, PackedVector2Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedVector3Array, PackedVector3Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedColorArray, PackedColorArray>>(Variant::OP_NOT_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorNotEqual<PackedVector4Array, PackedVector4Array>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::BOOL, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::INT, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::FLOAT, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::STRING, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::STRING, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR2, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR2, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR2I, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR2I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::RECT2, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::RECT2, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::RECT2I, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::RECT2I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR3, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR3, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR3I, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR3I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::TRANSFORM2D, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::TRANSFORM2D, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR4, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR4, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::VECTOR4I, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::VECTOR4I, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PLANE, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PLANE, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::QUATERNION, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::QUATERNION, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::AABB, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::AABB, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::BASIS, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::BASIS, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::TRANSFORM3D, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::TRANSFORM3D, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PROJECTION, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PROJECTION, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::COLOR, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::COLOR, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::STRING_NAME, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::STRING_NAME, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NODE_PATH, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::NODE_PATH, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::RID, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::RID, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::CALLABLE, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::CALLABLE, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::SIGNAL, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::SIGNAL, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::DICTIONARY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::DICTIONARY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_BYTE_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_INT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_INT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_FLOAT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_STRING_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR2_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR3_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_COLOR_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::NIL>>(Variant::OP_NOT_EQUAL, Variant::PACKED_VECTOR4_ARRAY, Variant::NIL);

	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::BOOL>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::BOOL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::INT>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::INT);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::FLOAT>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::FLOAT);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::STRING>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::STRING);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR2>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR2);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR2I>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR2I);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RECT2>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RECT2);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RECT2I>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RECT2I);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR3>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR3);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR3I>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR3I);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR4>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR4);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR4I>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::VECTOR4I);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::TRANSFORM2D>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::TRANSFORM2D);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PLANE>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PLANE);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::QUATERNION>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::QUATERNION);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::AABB>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::AABB);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::BASIS>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::BASIS);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::TRANSFORM3D>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::TRANSFORM3D);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PROJECTION>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PROJECTION);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::COLOR>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::COLOR);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::STRING_NAME>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::STRING_NAME);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::NODE_PATH>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::NODE_PATH);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RID>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::RID);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::CALLABLE>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::CALLABLE);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::SIGNAL>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::SIGNAL);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::DICTIONARY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::DICTIONARY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_BYTE_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_INT32_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_INT64_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_FLOAT32_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_FLOAT64_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_STRING_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR2_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR3_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_COLOR_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR4_ARRAY>>(Variant::OP_NOT_EQUAL, Variant::NIL, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorLess<bool, bool>>(Variant::OP_LESS, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorLess<int64_t, int64_t>>(Variant::OP_LESS, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorLess<int64_t, double>>(Variant::OP_LESS, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorLess<double, int64_t>>(Variant::OP_LESS, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorLess<double, double>>(Variant::OP_LESS, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorLess<String, String>>(Variant::OP_LESS, Variant::STRING, Variant::STRING);
	register_op<OperatorEvaluatorLess<StringName, StringName>>(Variant::OP_LESS, Variant::STRING_NAME, Variant::STRING_NAME);
	register_op<OperatorEvaluatorLess<Vector2, Vector2>>(Variant::OP_LESS, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorLess<Vector2i, Vector2i>>(Variant::OP_LESS, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorLess<Vector3, Vector3>>(Variant::OP_LESS, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorLess<Vector3i, Vector3i>>(Variant::OP_LESS, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorLess<Vector4, Vector4>>(Variant::OP_LESS, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorLess<Vector4i, Vector4i>>(Variant::OP_LESS, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorLess<::RID, ::RID>>(Variant::OP_LESS, Variant::RID, Variant::RID);
	register_op<OperatorEvaluatorLess<Array, Array>>(Variant::OP_LESS, Variant::ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorLessEqual<int64_t, int64_t>>(Variant::OP_LESS_EQUAL, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorLessEqual<int64_t, double>>(Variant::OP_LESS_EQUAL, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorLessEqual<double, int64_t>>(Variant::OP_LESS_EQUAL, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorLessEqual<double, double>>(Variant::OP_LESS_EQUAL, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorLessEqual<String, String>>(Variant::OP_LESS_EQUAL, Variant::STRING, Variant::STRING);
	register_op<OperatorEvaluatorLessEqual<StringName, StringName>>(Variant::OP_LESS_EQUAL, Variant::STRING_NAME, Variant::STRING_NAME);
	register_op<OperatorEvaluatorLessEqual<Vector2, Vector2>>(Variant::OP_LESS_EQUAL, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorLessEqual<Vector2i, Vector2i>>(Variant::OP_LESS_EQUAL, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorLessEqual<Vector3, Vector3>>(Variant::OP_LESS_EQUAL, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorLessEqual<Vector3i, Vector3i>>(Variant::OP_LESS_EQUAL, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorLessEqual<Vector4, Vector4>>(Variant::OP_LESS_EQUAL, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorLessEqual<Vector4i, Vector4i>>(Variant::OP_LESS_EQUAL, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorLessEqual<::RID, ::RID>>(Variant::OP_LESS_EQUAL, Variant::RID, Variant::RID);
	register_op<OperatorEvaluatorLessEqual<Array, Array>>(Variant::OP_LESS_EQUAL, Variant::ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorGreater<bool, bool>>(Variant::OP_GREATER, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorGreater<int64_t, int64_t>>(Variant::OP_GREATER, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorGreater<int64_t, double>>(Variant::OP_GREATER, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorGreater<double, int64_t>>(Variant::OP_GREATER, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorGreater<double, double>>(Variant::OP_GREATER, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorGreater<String, String>>(Variant::OP_GREATER, Variant::STRING, Variant::STRING);
	register_op<OperatorEvaluatorGreater<StringName, StringName>>(Variant::OP_GREATER, Variant::STRING_NAME, Variant::STRING_NAME);
	register_op<OperatorEvaluatorGreater<Vector2, Vector2>>(Variant::OP_GREATER, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorGreater<Vector2i, Vector2i>>(Variant::OP_GREATER, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorGreater<Vector3, Vector3>>(Variant::OP_GREATER, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorGreater<Vector3i, Vector3i>>(Variant::OP_GREATER, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorGreater<Vector4, Vector4>>(Variant::OP_GREATER, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorGreater<Vector4i, Vector4i>>(Variant::OP_GREATER, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorGreater<::RID, ::RID>>(Variant::OP_GREATER, Variant::RID, Variant::RID);
	register_op<OperatorEvaluatorGreater<Array, Array>>(Variant::OP_GREATER, Variant::ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorGreaterEqual<int64_t, int64_t>>(Variant::OP_GREATER_EQUAL, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorGreaterEqual<int64_t, double>>(Variant::OP_GREATER_EQUAL, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorGreaterEqual<double, int64_t>>(Variant::OP_GREATER_EQUAL, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorGreaterEqual<double, double>>(Variant::OP_GREATER_EQUAL, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorGreaterEqual<String, String>>(Variant::OP_GREATER_EQUAL, Variant::STRING, Variant::STRING);
	register_op<OperatorEvaluatorGreaterEqual<StringName, StringName>>(Variant::OP_GREATER_EQUAL, Variant::STRING_NAME, Variant::STRING_NAME);
	register_op<OperatorEvaluatorGreaterEqual<Vector2, Vector2>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR2, Variant::VECTOR2);
	register_op<OperatorEvaluatorGreaterEqual<Vector2i, Vector2i>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR2I, Variant::VECTOR2I);
	register_op<OperatorEvaluatorGreaterEqual<Vector3, Vector3>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR3, Variant::VECTOR3);
	register_op<OperatorEvaluatorGreaterEqual<Vector3i, Vector3i>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR3I, Variant::VECTOR3I);
	register_op<OperatorEvaluatorGreaterEqual<Vector4, Vector4>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR4, Variant::VECTOR4);
	register_op<OperatorEvaluatorGreaterEqual<Vector4i, Vector4i>>(Variant::OP_GREATER_EQUAL, Variant::VECTOR4I, Variant::VECTOR4I);
	register_op<OperatorEvaluatorGreaterEqual<::RID, ::RID>>(Variant::OP_GREATER_EQUAL, Variant::RID, Variant::RID);
	register_op<OperatorEvaluatorGreaterEqual<Array, Array>>(Variant::OP_GREATER_EQUAL, Variant::ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorAlwaysFalse<Variant::OP_OR, Variant::NIL, Variant::NIL>>(Variant::OP_OR, Variant::NIL, Variant::NIL);

	// OR
	register_op<OperatorEvaluatorNilXBoolOr>(Variant::OP_OR, Variant::NIL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXNilOr>(Variant::OP_OR, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorNilXIntOr>(Variant::OP_OR, Variant::NIL, Variant::INT);
	register_op<OperatorEvaluatorIntXNilOr>(Variant::OP_OR, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorNilXFloatOr>(Variant::OP_OR, Variant::NIL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXNilOr>(Variant::OP_OR, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorNilXObjectOr>(Variant::OP_OR, Variant::NIL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXNilOr>(Variant::OP_OR, Variant::OBJECT, Variant::NIL);

	register_op<OperatorEvaluatorBoolXBoolOr>(Variant::OP_OR, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXIntOr>(Variant::OP_OR, Variant::BOOL, Variant::INT);
	register_op<OperatorEvaluatorIntXBoolOr>(Variant::OP_OR, Variant::INT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXFloatOr>(Variant::OP_OR, Variant::BOOL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXBoolOr>(Variant::OP_OR, Variant::FLOAT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXObjectOr>(Variant::OP_OR, Variant::BOOL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXBoolOr>(Variant::OP_OR, Variant::OBJECT, Variant::BOOL);

	register_op<OperatorEvaluatorIntXIntOr>(Variant::OP_OR, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorIntXFloatOr>(Variant::OP_OR, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXIntOr>(Variant::OP_OR, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorIntXObjectOr>(Variant::OP_OR, Variant::INT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXIntOr>(Variant::OP_OR, Variant::OBJECT, Variant::INT);

	register_op<OperatorEvaluatorFloatXFloatOr>(Variant::OP_OR, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXObjectOr>(Variant::OP_OR, Variant::FLOAT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXFloatOr>(Variant::OP_OR, Variant::OBJECT, Variant::FLOAT);
	register_op<OperatorEvaluatorObjectXObjectOr>(Variant::OP_OR, Variant::OBJECT, Variant::OBJECT);

	// AND
	register_op<OperatorEvaluatorNilXBoolAnd>(Variant::OP_AND, Variant::NIL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXNilAnd>(Variant::OP_AND, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorNilXIntAnd>(Variant::OP_AND, Variant::NIL, Variant::INT);
	register_op<OperatorEvaluatorIntXNilAnd>(Variant::OP_AND, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorNilXFloatAnd>(Variant::OP_AND, Variant::NIL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXNilAnd>(Variant::OP_AND, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorNilXObjectAnd>(Variant::OP_AND, Variant::NIL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXNilAnd>(Variant::OP_AND, Variant::OBJECT, Variant::NIL);

	register_op<OperatorEvaluatorBoolXBoolAnd>(Variant::OP_AND, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXIntAnd>(Variant::OP_AND, Variant::BOOL, Variant::INT);
	register_op<OperatorEvaluatorIntXBoolAnd>(Variant::OP_AND, Variant::INT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXFloatAnd>(Variant::OP_AND, Variant::BOOL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXBoolAnd>(Variant::OP_AND, Variant::FLOAT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXObjectAnd>(Variant::OP_AND, Variant::BOOL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXBoolAnd>(Variant::OP_AND, Variant::OBJECT, Variant::BOOL);

	register_op<OperatorEvaluatorIntXIntAnd>(Variant::OP_AND, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorIntXFloatAnd>(Variant::OP_AND, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXIntAnd>(Variant::OP_AND, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorIntXObjectAnd>(Variant::OP_AND, Variant::INT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXIntAnd>(Variant::OP_AND, Variant::OBJECT, Variant::INT);

	register_op<OperatorEvaluatorFloatXFloatAnd>(Variant::OP_AND, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXObjectAnd>(Variant::OP_AND, Variant::FLOAT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXFloatAnd>(Variant::OP_AND, Variant::OBJECT, Variant::FLOAT);
	register_op<OperatorEvaluatorObjectXObjectAnd>(Variant::OP_AND, Variant::OBJECT, Variant::OBJECT);

	// XOR
	register_op<OperatorEvaluatorNilXBoolXor>(Variant::OP_XOR, Variant::NIL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXNilXor>(Variant::OP_XOR, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorNilXIntXor>(Variant::OP_XOR, Variant::NIL, Variant::INT);
	register_op<OperatorEvaluatorIntXNilXor>(Variant::OP_XOR, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorNilXFloatXor>(Variant::OP_XOR, Variant::NIL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXNilXor>(Variant::OP_XOR, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorNilXObjectXor>(Variant::OP_XOR, Variant::NIL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXNilXor>(Variant::OP_XOR, Variant::OBJECT, Variant::NIL);

	register_op<OperatorEvaluatorBoolXBoolXor>(Variant::OP_XOR, Variant::BOOL, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXIntXor>(Variant::OP_XOR, Variant::BOOL, Variant::INT);
	register_op<OperatorEvaluatorIntXBoolXor>(Variant::OP_XOR, Variant::INT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXFloatXor>(Variant::OP_XOR, Variant::BOOL, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXBoolXor>(Variant::OP_XOR, Variant::FLOAT, Variant::BOOL);
	register_op<OperatorEvaluatorBoolXObjectXor>(Variant::OP_XOR, Variant::BOOL, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXBoolXor>(Variant::OP_XOR, Variant::OBJECT, Variant::BOOL);

	register_op<OperatorEvaluatorIntXIntXor>(Variant::OP_XOR, Variant::INT, Variant::INT);
	register_op<OperatorEvaluatorIntXFloatXor>(Variant::OP_XOR, Variant::INT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXIntXor>(Variant::OP_XOR, Variant::FLOAT, Variant::INT);
	register_op<OperatorEvaluatorIntXObjectXor>(Variant::OP_XOR, Variant::INT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXIntXor>(Variant::OP_XOR, Variant::OBJECT, Variant::INT);

	register_op<OperatorEvaluatorFloatXFloatXor>(Variant::OP_XOR, Variant::FLOAT, Variant::FLOAT);
	register_op<OperatorEvaluatorFloatXObjectXor>(Variant::OP_XOR, Variant::FLOAT, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectXFloatXor>(Variant::OP_XOR, Variant::OBJECT, Variant::FLOAT);
	register_op<OperatorEvaluatorObjectXObjectXor>(Variant::OP_XOR, Variant::OBJECT, Variant::OBJECT);

	register_op<OperatorEvaluatorAlwaysTrue<Variant::OP_NOT, Variant::NIL, Variant::NIL>>(Variant::OP_NOT, Variant::NIL, Variant::NIL);
	register_op<OperatorEvaluatorNotBool>(Variant::OP_NOT, Variant::BOOL, Variant::NIL);
	register_op<OperatorEvaluatorNotInt>(Variant::OP_NOT, Variant::INT, Variant::NIL);
	register_op<OperatorEvaluatorNotFloat>(Variant::OP_NOT, Variant::FLOAT, Variant::NIL);
	register_op<OperatorEvaluatorNotObject>(Variant::OP_NOT, Variant::OBJECT, Variant::NIL);
	register_op<OperatorEvaluatorNot<String>>(Variant::OP_NOT, Variant::STRING, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector2>>(Variant::OP_NOT, Variant::VECTOR2, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector2i>>(Variant::OP_NOT, Variant::VECTOR2I, Variant::NIL);
	register_op<OperatorEvaluatorNot<Rect2>>(Variant::OP_NOT, Variant::RECT2, Variant::NIL);
	register_op<OperatorEvaluatorNot<Rect2i>>(Variant::OP_NOT, Variant::RECT2I, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector3>>(Variant::OP_NOT, Variant::VECTOR3, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector3i>>(Variant::OP_NOT, Variant::VECTOR3I, Variant::NIL);
	register_op<OperatorEvaluatorNot<Transform2D>>(Variant::OP_NOT, Variant::TRANSFORM2D, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector4>>(Variant::OP_NOT, Variant::VECTOR4, Variant::NIL);
	register_op<OperatorEvaluatorNot<Vector4i>>(Variant::OP_NOT, Variant::VECTOR4I, Variant::NIL);
	register_op<OperatorEvaluatorNot<Plane>>(Variant::OP_NOT, Variant::PLANE, Variant::NIL);
	register_op<OperatorEvaluatorNot<Quaternion>>(Variant::OP_NOT, Variant::QUATERNION, Variant::NIL);
	register_op<OperatorEvaluatorNot<::AABB>>(Variant::OP_NOT, Variant::AABB, Variant::NIL);
	register_op<OperatorEvaluatorNot<Basis>>(Variant::OP_NOT, Variant::BASIS, Variant::NIL);
	register_op<OperatorEvaluatorNot<Transform3D>>(Variant::OP_NOT, Variant::TRANSFORM3D, Variant::NIL);
	register_op<OperatorEvaluatorNot<Projection>>(Variant::OP_NOT, Variant::PROJECTION, Variant::NIL);
	register_op<OperatorEvaluatorNot<Color>>(Variant::OP_NOT, Variant::COLOR, Variant::NIL);
	register_op<OperatorEvaluatorNot<StringName>>(Variant::OP_NOT, Variant::STRING_NAME, Variant::NIL);
	register_op<OperatorEvaluatorNot<NodePath>>(Variant::OP_NOT, Variant::NODE_PATH, Variant::NIL);
	register_op<OperatorEvaluatorNot<::RID>>(Variant::OP_NOT, Variant::RID, Variant::NIL);
	register_op<OperatorEvaluatorNot<Callable>>(Variant::OP_NOT, Variant::CALLABLE, Variant::NIL);
	register_op<OperatorEvaluatorNot<Signal>>(Variant::OP_NOT, Variant::SIGNAL, Variant::NIL);
	register_op<OperatorEvaluatorNot<Dictionary>>(Variant::OP_NOT, Variant::DICTIONARY, Variant::NIL);
	register_op<OperatorEvaluatorNot<Array>>(Variant::OP_NOT, Variant::ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedByteArray>>(Variant::OP_NOT, Variant::PACKED_BYTE_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedInt32Array>>(Variant::OP_NOT, Variant::PACKED_INT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedInt64Array>>(Variant::OP_NOT, Variant::PACKED_INT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedFloat32Array>>(Variant::OP_NOT, Variant::PACKED_FLOAT32_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedFloat64Array>>(Variant::OP_NOT, Variant::PACKED_FLOAT64_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedStringArray>>(Variant::OP_NOT, Variant::PACKED_STRING_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedVector2Array>>(Variant::OP_NOT, Variant::PACKED_VECTOR2_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedVector3Array>>(Variant::OP_NOT, Variant::PACKED_VECTOR3_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedColorArray>>(Variant::OP_NOT, Variant::PACKED_COLOR_ARRAY, Variant::NIL);
	register_op<OperatorEvaluatorNot<PackedVector4Array>>(Variant::OP_NOT, Variant::PACKED_VECTOR4_ARRAY, Variant::NIL);

	register_string_op(OperatorEvaluatorInStringFind, Variant::OP_IN);

	register_op<OperatorEvaluatorInDictionaryHasNil>(Variant::OP_IN, Variant::NIL, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<bool>>(Variant::OP_IN, Variant::BOOL, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<int64_t>>(Variant::OP_IN, Variant::INT, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<double>>(Variant::OP_IN, Variant::FLOAT, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<String>>(Variant::OP_IN, Variant::STRING, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector2>>(Variant::OP_IN, Variant::VECTOR2, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector2i>>(Variant::OP_IN, Variant::VECTOR2I, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Rect2>>(Variant::OP_IN, Variant::RECT2, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Rect2i>>(Variant::OP_IN, Variant::RECT2I, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector3>>(Variant::OP_IN, Variant::VECTOR3, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector3i>>(Variant::OP_IN, Variant::VECTOR3I, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector4>>(Variant::OP_IN, Variant::VECTOR4, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Vector4i>>(Variant::OP_IN, Variant::VECTOR4I, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Transform2D>>(Variant::OP_IN, Variant::TRANSFORM2D, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Plane>>(Variant::OP_IN, Variant::PLANE, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Quaternion>>(Variant::OP_IN, Variant::QUATERNION, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<::AABB>>(Variant::OP_IN, Variant::AABB, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Basis>>(Variant::OP_IN, Variant::BASIS, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Transform3D>>(Variant::OP_IN, Variant::TRANSFORM3D, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Projection>>(Variant::OP_IN, Variant::PROJECTION, Variant::DICTIONARY);

	register_op<OperatorEvaluatorInDictionaryHas<Color>>(Variant::OP_IN, Variant::COLOR, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<StringName>>(Variant::OP_IN, Variant::STRING_NAME, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<NodePath>>(Variant::OP_IN, Variant::NODE_PATH, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<::RID>>(Variant::OP_IN, Variant::RID, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHasObject>(Variant::OP_IN, Variant::OBJECT, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Callable>>(Variant::OP_IN, Variant::CALLABLE, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Signal>>(Variant::OP_IN, Variant::SIGNAL, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Dictionary>>(Variant::OP_IN, Variant::DICTIONARY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<Array>>(Variant::OP_IN, Variant::ARRAY, Variant::DICTIONARY);

	register_op<OperatorEvaluatorInDictionaryHas<PackedByteArray>>(Variant::OP_IN, Variant::PACKED_BYTE_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedInt32Array>>(Variant::OP_IN, Variant::PACKED_INT32_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedInt64Array>>(Variant::OP_IN, Variant::PACKED_INT64_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedFloat32Array>>(Variant::OP_IN, Variant::PACKED_FLOAT32_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedFloat64Array>>(Variant::OP_IN, Variant::PACKED_FLOAT64_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedStringArray>>(Variant::OP_IN, Variant::PACKED_STRING_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedVector2Array>>(Variant::OP_IN, Variant::PACKED_VECTOR2_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedVector3Array>>(Variant::OP_IN, Variant::PACKED_VECTOR3_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedColorArray>>(Variant::OP_IN, Variant::PACKED_COLOR_ARRAY, Variant::DICTIONARY);
	register_op<OperatorEvaluatorInDictionaryHas<PackedVector4Array>>(Variant::OP_IN, Variant::PACKED_VECTOR4_ARRAY, Variant::DICTIONARY);

	register_op<OperatorEvaluatorInArrayFindNil>(Variant::OP_IN, Variant::NIL, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<bool, Array>>(Variant::OP_IN, Variant::BOOL, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<int64_t, Array>>(Variant::OP_IN, Variant::INT, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, Array>>(Variant::OP_IN, Variant::FLOAT, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<String, Array>>(Variant::OP_IN, Variant::STRING, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector2, Array>>(Variant::OP_IN, Variant::VECTOR2, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector2i, Array>>(Variant::OP_IN, Variant::VECTOR2I, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Rect2, Array>>(Variant::OP_IN, Variant::RECT2, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Rect2i, Array>>(Variant::OP_IN, Variant::RECT2I, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector3, Array>>(Variant::OP_IN, Variant::VECTOR3, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector3i, Array>>(Variant::OP_IN, Variant::VECTOR3I, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector4, Array>>(Variant::OP_IN, Variant::VECTOR4, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector4i, Array>>(Variant::OP_IN, Variant::VECTOR4I, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Transform2D, Array>>(Variant::OP_IN, Variant::TRANSFORM2D, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Plane, Array>>(Variant::OP_IN, Variant::PLANE, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Quaternion, Array>>(Variant::OP_IN, Variant::QUATERNION, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<::AABB, Array>>(Variant::OP_IN, Variant::AABB, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Basis, Array>>(Variant::OP_IN, Variant::BASIS, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Transform3D, Array>>(Variant::OP_IN, Variant::TRANSFORM3D, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Projection, Array>>(Variant::OP_IN, Variant::PROJECTION, Variant::ARRAY);

	register_op<OperatorEvaluatorInArrayFind<Color, Array>>(Variant::OP_IN, Variant::COLOR, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<StringName, Array>>(Variant::OP_IN, Variant::STRING_NAME, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<NodePath, Array>>(Variant::OP_IN, Variant::NODE_PATH, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<::RID, Array>>(Variant::OP_IN, Variant::RID, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFindObject>(Variant::OP_IN, Variant::OBJECT, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Callable, Array>>(Variant::OP_IN, Variant::CALLABLE, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Signal, Array>>(Variant::OP_IN, Variant::SIGNAL, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Dictionary, Array>>(Variant::OP_IN, Variant::DICTIONARY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Array, Array>>(Variant::OP_IN, Variant::ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorInArrayFind<PackedByteArray, Array>>(Variant::OP_IN, Variant::PACKED_BYTE_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedInt32Array, Array>>(Variant::OP_IN, Variant::PACKED_INT32_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedInt64Array, Array>>(Variant::OP_IN, Variant::PACKED_INT64_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedFloat32Array, Array>>(Variant::OP_IN, Variant::PACKED_FLOAT32_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedFloat64Array, Array>>(Variant::OP_IN, Variant::PACKED_FLOAT64_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedStringArray, Array>>(Variant::OP_IN, Variant::PACKED_STRING_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedVector2Array, Array>>(Variant::OP_IN, Variant::PACKED_VECTOR2_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedVector3Array, Array>>(Variant::OP_IN, Variant::PACKED_VECTOR3_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedColorArray, Array>>(Variant::OP_IN, Variant::PACKED_COLOR_ARRAY, Variant::ARRAY);
	register_op<OperatorEvaluatorInArrayFind<PackedVector4Array, Array>>(Variant::OP_IN, Variant::PACKED_VECTOR4_ARRAY, Variant::ARRAY);

	register_op<OperatorEvaluatorInArrayFind<int64_t, PackedByteArray>>(Variant::OP_IN, Variant::INT, Variant::PACKED_BYTE_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, PackedByteArray>>(Variant::OP_IN, Variant::FLOAT, Variant::PACKED_BYTE_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<int64_t, PackedInt32Array>>(Variant::OP_IN, Variant::INT, Variant::PACKED_INT32_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, PackedInt32Array>>(Variant::OP_IN, Variant::FLOAT, Variant::PACKED_INT32_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<int64_t, PackedInt64Array>>(Variant::OP_IN, Variant::INT, Variant::PACKED_INT64_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, PackedInt64Array>>(Variant::OP_IN, Variant::FLOAT, Variant::PACKED_INT64_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<int64_t, PackedFloat32Array>>(Variant::OP_IN, Variant::INT, Variant::PACKED_FLOAT32_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, PackedFloat32Array>>(Variant::OP_IN, Variant::FLOAT, Variant::PACKED_FLOAT32_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<int64_t, PackedFloat64Array>>(Variant::OP_IN, Variant::INT, Variant::PACKED_FLOAT64_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<double, PackedFloat64Array>>(Variant::OP_IN, Variant::FLOAT, Variant::PACKED_FLOAT64_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<String, PackedStringArray>>(Variant::OP_IN, Variant::STRING, Variant::PACKED_STRING_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<StringName, PackedStringArray>>(Variant::OP_IN, Variant::STRING_NAME, Variant::PACKED_STRING_ARRAY);

	register_op<OperatorEvaluatorInArrayFind<Vector2, PackedVector2Array>>(Variant::OP_IN, Variant::VECTOR2, Variant::PACKED_VECTOR2_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector3, PackedVector3Array>>(Variant::OP_IN, Variant::VECTOR3, Variant::PACKED_VECTOR3_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Color, PackedColorArray>>(Variant::OP_IN, Variant::COLOR, Variant::PACKED_COLOR_ARRAY);
	register_op<OperatorEvaluatorInArrayFind<Vector4, PackedVector4Array>>(Variant::OP_IN, Variant::VECTOR4, Variant::PACKED_VECTOR4_ARRAY);

	register_op<OperatorEvaluatorObjectHasPropertyString>(Variant::OP_IN, Variant::STRING, Variant::OBJECT);
	register_op<OperatorEvaluatorObjectHasPropertyStringName>(Variant::OP_IN, Variant::STRING_NAME, Variant::OBJECT);
}

#undef register_string_op
#undef register_string_modulo_op

void Variant::_unregister_variant_operators() {
}

void Variant::evaluate(const Operator &p_op, const Variant &p_a,
		const Variant &p_b, Variant &r_ret, bool &r_valid) {
	ERR_FAIL_INDEX(p_op, Variant::OP_MAX);
	Variant::Type type_a = p_a.get_type();
	Variant::Type type_b = p_b.get_type();
	ERR_FAIL_INDEX(type_a, Variant::VARIANT_MAX);
	ERR_FAIL_INDEX(type_b, Variant::VARIANT_MAX);

	VariantEvaluatorFunction ev = operator_evaluator_table[p_op][type_a][type_b];
	if (unlikely(!ev)) {
		r_valid = false;
		r_ret = Variant();
		return;
	}

	ev(p_a, p_b, &r_ret, r_valid);
}

Variant::Type Variant::get_operator_return_type(Operator p_operator, Type p_type_a, Type p_type_b) {
	ERR_FAIL_INDEX_V(p_operator, Variant::OP_MAX, Variant::NIL);
	ERR_FAIL_INDEX_V(p_type_a, Variant::VARIANT_MAX, Variant::NIL);
	ERR_FAIL_INDEX_V(p_type_b, Variant::VARIANT_MAX, Variant::NIL);

	return operator_return_type_table[p_operator][p_type_a][p_type_b];
}

Variant::ValidatedOperatorEvaluator Variant::get_validated_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b) {
	ERR_FAIL_INDEX_V(p_operator, Variant::OP_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_type_a, Variant::VARIANT_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_type_b, Variant::VARIANT_MAX, nullptr);
	return validated_operator_evaluator_table[p_operator][p_type_a][p_type_b];
}

Variant::PTROperatorEvaluator Variant::get_ptr_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b) {
	ERR_FAIL_INDEX_V(p_operator, Variant::OP_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_type_a, Variant::VARIANT_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_type_b, Variant::VARIANT_MAX, nullptr);
	return ptr_operator_evaluator_table[p_operator][p_type_a][p_type_b];
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
	"unary-",
	"unary+",
	"%",
	"**",
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

Variant::operator bool() const {
	return booleanize();
}

// We consider all uninitialized or empty types to be false based on the type's
// zeroiness.
bool Variant::booleanize() const {
	return !is_zero();
}

bool Variant::in(const Variant &p_index, bool *r_valid) const {
	bool valid;
	Variant ret;
	evaluate(OP_IN, p_index, *this, ret, valid);
	if (r_valid) {
		*r_valid = valid;
	}
	if (valid) {
		ERR_FAIL_COND_V(ret.type != BOOL, false);
		return *VariantGetInternalPtr<bool>::get_ptr(&ret);
	} else {
		return false;
	}
}
