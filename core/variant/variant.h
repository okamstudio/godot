/**************************************************************************/
/*  variant.h                                                             */
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

#include "core/core_string_names.h"
#include "core/input/input_enums.h"
#include "core/io/ip_address.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/math/vector4.h"
#include "core/math/vector4i.h"
#include "core/object/object_id.h"
#include "core/os/keyboard.h"
#include "core/string/node_path.h"
#include "core/string/ustring.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/rid.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class Object;
class RefCounted;

template <typename T>
class Ref;

struct PropertyInfo;
struct MethodInfo;

typedef Vector<uint8_t> PackedByteArray;
typedef Vector<int32_t> PackedInt32Array;
typedef Vector<int64_t> PackedInt64Array;
typedef Vector<float> PackedFloat32Array;
typedef Vector<double> PackedFloat64Array;
typedef Vector<real_t> PackedRealArray;
typedef Vector<String> PackedStringArray;
typedef Vector<Vector2> PackedVector2Array;
typedef Vector<Vector3> PackedVector3Array;
typedef Vector<Color> PackedColorArray;
typedef Vector<Vector4> PackedVector4Array;

class Variant {
public:
	// If this changes the table in variant_op must be updated
	enum Type {
		NIL,

		// atomic types
		BOOL,
		INT,
		FLOAT,
		STRING,

		// math types
		VECTOR2,
		VECTOR2I,
		RECT2,
		RECT2I,
		VECTOR3,
		VECTOR3I,
		TRANSFORM2D,
		VECTOR4,
		VECTOR4I,
		PLANE,
		QUATERNION,
		AABB,
		BASIS,
		TRANSFORM3D,
		PROJECTION,

		// misc types
		COLOR,
		STRING_NAME,
		NODE_PATH,
		RID,
		OBJECT,
		CALLABLE,
		SIGNAL,
		DICTIONARY,
		ARRAY,

		// typed arrays
		PACKED_BYTE_ARRAY,
		PACKED_INT32_ARRAY,
		PACKED_INT64_ARRAY,
		PACKED_FLOAT32_ARRAY,
		PACKED_FLOAT64_ARRAY,
		PACKED_STRING_ARRAY,
		PACKED_VECTOR2_ARRAY,
		PACKED_VECTOR3_ARRAY,
		PACKED_COLOR_ARRAY,
		PACKED_VECTOR4_ARRAY,

		VARIANT_MAX
	};

	enum {
		// Maximum recursion depth allowed when serializing variants.
		MAX_RECURSION_DEPTH = 1024,
	};

private:
	struct Pools {
		union BucketSmall {
			BucketSmall() {}
			~BucketSmall() {}
			Transform2D _transform2d;
			::AABB _aabb;
		};
		union BucketMedium {
			BucketMedium() {}
			~BucketMedium() {}
			Basis _basis;
			Transform3D _transform3d;
		};
		union BucketLarge {
			BucketLarge() {}
			~BucketLarge() {}
			Projection _projection;
		};

		static PagedAllocator<BucketSmall, true> _bucket_small;
		static PagedAllocator<BucketMedium, true> _bucket_medium;
		static PagedAllocator<BucketLarge, true> _bucket_large;
	};

	friend struct _VariantCall;
	friend class VariantInternal;
	// Variant takes 24 bytes when real_t is float, and 40 bytes if double.
	// It only allocates extra memory for AABB/Transform2D (24, 48 if double),
	// Basis/Transform3D (48, 96 if double), Projection (64, 128 if double),
	// and PackedArray/Array/Dictionary (platform-dependent).

	Type type = NIL;

	struct ObjData {
		ObjectID id;
		Object *obj = nullptr;

		void ref(const ObjData &p_from);
		void ref_pointer(Object *p_object);
		void ref_pointer(RefCounted *p_object);
		void unref();

		template <typename T>
		_ALWAYS_INLINE_ void ref(const Ref<T> &p_from) {
			if (p_from.is_valid()) {
				ref(ObjData{ p_from->get_instance_id(), p_from.ptr() });
			} else {
				unref();
			}
		}
	};

	/* array helpers */
	struct PackedArrayRefBase {
		SafeRefCount refcount;
		_FORCE_INLINE_ PackedArrayRefBase *reference() {
			if (refcount.ref()) {
				return this;
			} else {
				return nullptr;
			}
		}
		static _FORCE_INLINE_ PackedArrayRefBase *reference_from(PackedArrayRefBase *p_base, PackedArrayRefBase *p_from) {
			if (p_base == p_from) {
				return p_base; //same thing, do nothing
			}

			if (p_from->reference()) {
				if (p_base->refcount.unref()) {
					memdelete(p_base);
				}
				return p_from;
			} else {
				return p_base; //keep, could not reference new
			}
		}
		static _FORCE_INLINE_ void destroy(PackedArrayRefBase *p_array) {
			if (p_array->refcount.unref()) {
				memdelete(p_array);
			}
		}
		_FORCE_INLINE_ virtual ~PackedArrayRefBase() {} //needs virtual destructor, but make inline
	};

	template <typename T>
	struct PackedArrayRef : public PackedArrayRefBase {
		Vector<T> array;
		static _FORCE_INLINE_ PackedArrayRef<T> *create() {
			return memnew(PackedArrayRef<T>);
		}
		static _FORCE_INLINE_ PackedArrayRef<T> *create(const Vector<T> &p_from) {
			return memnew(PackedArrayRef<T>(p_from));
		}

		static _FORCE_INLINE_ const Vector<T> &get_array(PackedArrayRefBase *p_base) {
			return static_cast<PackedArrayRef<T> *>(p_base)->array;
		}
		static _FORCE_INLINE_ Vector<T> *get_array_ptr(const PackedArrayRefBase *p_base) {
			return &const_cast<PackedArrayRef<T> *>(static_cast<const PackedArrayRef<T> *>(p_base))->array;
		}

		_FORCE_INLINE_ PackedArrayRef(const Vector<T> &p_from) {
			array = p_from;
			refcount.init();
		}
		_FORCE_INLINE_ PackedArrayRef() {
			refcount.init();
		}
	};

	/* end of array helpers */
	_ALWAYS_INLINE_ ObjData &_get_obj();
	_ALWAYS_INLINE_ const ObjData &_get_obj() const;

	union {
		bool _bool;
		int64_t _int;
		double _float;
		Transform2D *_transform2d;
		::AABB *_aabb;
		Basis *_basis;
		Transform3D *_transform3d;
		Projection *_projection;
		PackedArrayRefBase *packed_array;
		void *_ptr; //generic pointer
		uint8_t _mem[sizeof(ObjData) > (sizeof(real_t) * 4) ? sizeof(ObjData) : (sizeof(real_t) * 4)]{ 0 };
	} _data alignas(8);

	void reference(const Variant &p_variant);

	void _clear_internal();

	static constexpr bool needs_deinit[Variant::VARIANT_MAX] = {
		false, //NIL,
		false, //BOOL,
		false, //INT,
		false, //FLOAT,
		true, //STRING,
		false, //VECTOR2,
		false, //VECTOR2I,
		false, //RECT2,
		false, //RECT2I,
		false, //VECTOR3,
		false, //VECTOR3I,
		true, //TRANSFORM2D,
		false, //VECTOR4,
		false, //VECTOR4I,
		false, //PLANE,
		false, //QUATERNION,
		true, //AABB,
		true, //BASIS,
		true, //TRANSFORM,
		true, //PROJECTION,

		// misc types
		false, //COLOR,
		true, //STRING_NAME,
		true, //NODE_PATH,
		false, //RID,
		true, //OBJECT,
		true, //CALLABLE,
		true, //SIGNAL,
		true, //DICTIONARY,
		true, //ARRAY,

		// typed arrays
		true, //PACKED_BYTE_ARRAY,
		true, //PACKED_INT32_ARRAY,
		true, //PACKED_INT64_ARRAY,
		true, //PACKED_FLOAT32_ARRAY,
		true, //PACKED_FLOAT64_ARRAY,
		true, //PACKED_STRING_ARRAY,
		true, //PACKED_VECTOR2_ARRAY,
		true, //PACKED_VECTOR3_ARRAY,
		true, //PACKED_COLOR_ARRAY,
		true, //PACKED_VECTOR4_ARRAY,
	};

	_FORCE_INLINE_ void clear() {
		if (unlikely(needs_deinit[type])) { // Make it fast for types that don't need deinit.
			_clear_internal();
		}
		type = NIL;
	}

	static void _register_variant_operators();
	static void _unregister_variant_operators();
	static void _register_variant_methods();
	static void _unregister_variant_methods();
	static void _register_variant_setters_getters();
	static void _unregister_variant_setters_getters();
	static void _register_variant_constructors();
	static void _unregister_variant_destructors();
	static void _register_variant_destructors();
	static void _unregister_variant_constructors();
	static void _register_variant_utility_functions();
	static void _unregister_variant_utility_functions();

	void _variant_call_error(const String &p_method, Callable::CallError &error);

	template <typename T>
	_ALWAYS_INLINE_ T _to_int() const {
		switch (get_type()) {
			case NIL:
				return 0;
			case BOOL:
				return _data._bool ? 1 : 0;
			case INT:
				return T(_data._int);
			case FLOAT:
				return T(_data._float);
			case STRING:
				return reinterpret_cast<const String *>(_data._mem)->to_int();
			default: {
				return 0;
			}
		}
	}

	template <typename T>
	_ALWAYS_INLINE_ T _to_float() const {
		switch (type) {
			case NIL:
				return 0;
			case BOOL:
				return _data._bool ? 1 : 0;
			case INT:
				return T(_data._int);
			case FLOAT:
				return T(_data._float);
			case STRING:
				return reinterpret_cast<const String *>(_data._mem)->to_float();
			default: {
				return 0;
			}
		}
	}

	// Avoid accidental conversion. If you reached this point, it's because you most likely forgot to dereference
	// a Variant pointer (so add * like this: *variant_pointer).

	Variant(const Variant *) {}
	Variant(const Variant **) {}

public:
	_FORCE_INLINE_ Type get_type() const {
		return type;
	}
	static String get_type_name(Variant::Type p_type);
	static Variant::Type get_type_by_name(const String &p_type_name);
	static bool can_convert(Type p_type_from, Type p_type_to);
	static bool can_convert_strict(Type p_type_from, Type p_type_to);
	static bool is_type_shared(Variant::Type p_type);

	bool is_ref_counted() const;
	_FORCE_INLINE_ bool is_num() const {
		return type == INT || type == FLOAT;
	}
	_FORCE_INLINE_ bool is_string() const {
		return type == STRING || type == STRING_NAME;
	}
	_FORCE_INLINE_ bool is_array() const {
		return type >= ARRAY;
	}
	bool is_shared() const;
	bool is_zero() const;
	bool is_one() const;
	bool is_null() const;
	bool is_read_only() const;

	// Make sure Variant is not implicitly cast when accessing it with bracket notation (GH-49469).
	Variant &operator[](const Variant &p_key) = delete;
	const Variant &operator[](const Variant &p_key) const = delete;

	operator bool() const;
	operator int64_t() const;
	operator int32_t() const;
	operator int16_t() const;
	operator int8_t() const;
	operator uint64_t() const;
	operator uint32_t() const;
	operator uint16_t() const;
	operator uint8_t() const;

	operator ObjectID() const;

	operator char32_t() const;
	operator float() const;
	operator double() const;
	operator String() const;
	operator StringName() const;
	operator Vector2() const;
	operator Vector2i() const;
	operator Rect2() const;
	operator Rect2i() const;
	operator Vector3() const;
	operator Vector3i() const;
	operator Vector4() const;
	operator Vector4i() const;
	operator Plane() const;
	operator ::AABB() const;
	operator Quaternion() const;
	operator Basis() const;
	operator Transform2D() const;
	operator Transform3D() const;
	operator Projection() const;

	operator Color() const;
	operator NodePath() const;
	operator ::RID() const;

	operator Object *() const;

	operator Callable() const;
	operator Signal() const;

	operator Dictionary() const;
	operator Array() const;

	operator PackedByteArray() const;
	operator PackedInt32Array() const;
	operator PackedInt64Array() const;
	operator PackedFloat32Array() const;
	operator PackedFloat64Array() const;
	operator PackedStringArray() const;
	operator PackedVector3Array() const;
	operator PackedVector2Array() const;
	operator PackedColorArray() const;
	operator PackedVector4Array() const;

	operator Vector<::RID>() const;
	operator Vector<Plane>() const;
	operator Vector<Face3>() const;
	operator Vector<Variant>() const;
	operator Vector<StringName>() const;

	// some core type enums to convert to
	operator Side() const;
	operator Orientation() const;

	operator IPAddress() const;

	Object *get_validated_object() const;
	Object *get_validated_object_with_check(bool &r_previously_freed) const;

	Variant(bool p_bool);
	Variant(int64_t p_int64);
	Variant(int32_t p_int32);
	Variant(int16_t p_int16);
	Variant(int8_t p_int8);
	Variant(uint64_t p_uint64);
	Variant(uint32_t p_uint32);
	Variant(uint16_t p_uint16);
	Variant(uint8_t p_uint8);
	Variant(float p_float);
	Variant(double p_double);
	Variant(const ObjectID &p_id);
	Variant(const String &p_string);
	Variant(const StringName &p_string);
	Variant(const char *const p_cstring);
	Variant(const char32_t *p_wstring);
	Variant(const Vector2 &p_vector2);
	Variant(const Vector2i &p_vector2i);
	Variant(const Rect2 &p_rect2);
	Variant(const Rect2i &p_rect2i);
	Variant(const Vector3 &p_vector3);
	Variant(const Vector3i &p_vector3i);
	Variant(const Vector4 &p_vector4);
	Variant(const Vector4i &p_vector4i);
	Variant(const Plane &p_plane);
	Variant(const ::AABB &p_aabb);
	Variant(const Quaternion &p_quat);
	Variant(const Basis &p_matrix);
	Variant(const Transform2D &p_transform);
	Variant(const Transform3D &p_transform);
	Variant(const Projection &p_projection);
	Variant(const Color &p_color);
	Variant(const NodePath &p_node_path);
	Variant(const ::RID &p_rid);
	Variant(const Object *p_object);
	Variant(const Callable &p_callable);
	Variant(const Signal &p_signal);
	Variant(const Dictionary &p_dictionary);

	Variant(const Array &p_array);
	Variant(const PackedByteArray &p_byte_array);
	Variant(const PackedInt32Array &p_int32_array);
	Variant(const PackedInt64Array &p_int64_array);
	Variant(const PackedFloat32Array &p_float32_array);
	Variant(const PackedFloat64Array &p_float64_array);
	Variant(const PackedStringArray &p_string_array);
	Variant(const PackedVector2Array &p_vector2_array);
	Variant(const PackedVector3Array &p_vector3_array);
	Variant(const PackedColorArray &p_color_array);
	Variant(const PackedVector4Array &p_vector4_array);

	Variant(const Vector<::RID> &p_array); // helper
	Variant(const Vector<Plane> &p_array); // helper
	Variant(const Vector<Face3> &p_face_array);
	Variant(const Vector<Variant> &p_array);
	Variant(const Vector<StringName> &p_array);

	Variant(const IPAddress &p_address);

#define VARIANT_ENUM_CLASS_CONSTRUCTOR(m_enum) \
	Variant(m_enum p_value) :                  \
			type(INT) {                        \
		_data._int = (int64_t)p_value;         \
	}

	// Only enum classes that need to be bound need this to be defined.
	VARIANT_ENUM_CLASS_CONSTRUCTOR(EulerOrder)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(JoyAxis)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(JoyButton)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(Key)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(KeyLocation)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(MIDIMessage)
	VARIANT_ENUM_CLASS_CONSTRUCTOR(MouseButton)

#undef VARIANT_ENUM_CLASS_CONSTRUCTOR

	// If this changes the table in variant_op must be updated
	enum Operator {
		//comparison
		OP_EQUAL,
		OP_NOT_EQUAL,
		OP_LESS,
		OP_LESS_EQUAL,
		OP_GREATER,
		OP_GREATER_EQUAL,
		//mathematic
		OP_ADD,
		OP_SUBTRACT,
		OP_MULTIPLY,
		OP_DIVIDE,
		OP_NEGATE,
		OP_POSITIVE,
		OP_MODULE,
		OP_POWER,
		//bitwise
		OP_SHIFT_LEFT,
		OP_SHIFT_RIGHT,
		OP_BIT_AND,
		OP_BIT_OR,
		OP_BIT_XOR,
		OP_BIT_NEGATE,
		//logic
		OP_AND,
		OP_OR,
		OP_XOR,
		OP_NOT,
		//containment
		OP_IN,
		OP_MAX

	};

	static String get_operator_name(Operator p_op);
	static void evaluate(const Operator &p_op, const Variant &p_a, const Variant &p_b, Variant &r_ret, bool &r_valid);
	static _FORCE_INLINE_ Variant evaluate(const Operator &p_op, const Variant &p_a, const Variant &p_b) {
		bool valid = true;
		Variant res;
		evaluate(p_op, p_a, p_b, res, valid);
		return res;
	}

	static Variant::Type get_operator_return_type(Operator p_operator, Type p_type_a, Type p_type_b);
	typedef void (*ValidatedOperatorEvaluator)(const Variant *left, const Variant *right, Variant *r_ret);
	static ValidatedOperatorEvaluator get_validated_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b);
	typedef void (*PTROperatorEvaluator)(const void *left, const void *right, void *r_ret);
	static PTROperatorEvaluator get_ptr_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b);

	void zero();
	Variant duplicate(bool p_deep = false) const;
	Variant recursive_duplicate(bool p_deep, int recursion_count) const;

	/* Built-In Methods */

	typedef void (*ValidatedBuiltInMethod)(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret);
	typedef void (*PTRBuiltInMethod)(void *p_base, const void **p_args, void *r_ret, int p_argcount);

	static bool has_builtin_method(Variant::Type p_type, const StringName &p_method);

	static ValidatedBuiltInMethod get_validated_builtin_method(Variant::Type p_type, const StringName &p_method);
	static PTRBuiltInMethod get_ptr_builtin_method(Variant::Type p_type, const StringName &p_method);

	static MethodInfo get_builtin_method_info(Variant::Type p_type, const StringName &p_method);
	static int get_builtin_method_argument_count(Variant::Type p_type, const StringName &p_method);
	static Variant::Type get_builtin_method_argument_type(Variant::Type p_type, const StringName &p_method, int p_argument);
	static String get_builtin_method_argument_name(Variant::Type p_type, const StringName &p_method, int p_argument);
	static Vector<Variant> get_builtin_method_default_arguments(Variant::Type p_type, const StringName &p_method);
	static bool has_builtin_method_return_value(Variant::Type p_type, const StringName &p_method);
	static Variant::Type get_builtin_method_return_type(Variant::Type p_type, const StringName &p_method);
	static bool is_builtin_method_const(Variant::Type p_type, const StringName &p_method);
	static bool is_builtin_method_static(Variant::Type p_type, const StringName &p_method);
	static bool is_builtin_method_vararg(Variant::Type p_type, const StringName &p_method);
	static void get_builtin_method_list(Variant::Type p_type, List<StringName> *p_list);
	static int get_builtin_method_count(Variant::Type p_type);
	static uint32_t get_builtin_method_hash(Variant::Type p_type, const StringName &p_method);

	void callp(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error);

	template <typename... VarArgs>
	Variant call(const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		Callable::CallError cerr;
		Variant ret;
		callp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args), ret, cerr);
		if (cerr.error != Callable::CallError::CALL_OK) {
			_variant_call_error(p_method, cerr);
		}
		return ret;
	}

	void call_const(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error);
	static void call_static(Variant::Type p_type, const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error);

	static String get_call_error_text(const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);
	static String get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);
	static String get_callable_error_text(const Callable &p_callable, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);

	//dynamic (includes Object)
	void get_method_list(List<MethodInfo> *p_list) const;
	bool has_method(const StringName &p_method) const;

	/* Constructors */

	typedef void (*ValidatedConstructor)(Variant *r_base, const Variant **p_args);
	typedef void (*PTRConstructor)(void *base, const void **p_args);

	static int get_constructor_count(Variant::Type p_type);
	static ValidatedConstructor get_validated_constructor(Variant::Type p_type, int p_constructor);
	static PTRConstructor get_ptr_constructor(Variant::Type p_type, int p_constructor);
	static int get_constructor_argument_count(Variant::Type p_type, int p_constructor);
	static Variant::Type get_constructor_argument_type(Variant::Type p_type, int p_constructor, int p_argument);
	static String get_constructor_argument_name(Variant::Type p_type, int p_constructor, int p_argument);
	static void construct(Variant::Type, Variant &base, const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	static void get_constructor_list(Type p_type, List<MethodInfo> *r_list); //convenience

	/* Destructors */

	// Only ptrcall is available.
	typedef void (*PTRDestructor)(void *base);

	static PTRDestructor get_ptr_destructor(Variant::Type p_type);
	static bool has_destructor(Variant::Type p_type);

	/* Properties */

	void set_named(const StringName &p_member, const Variant &p_value, bool &r_valid);
	Variant get_named(const StringName &p_member, bool &r_valid) const;

	typedef void (*ValidatedSetter)(Variant *base, const Variant *value);
	typedef void (*ValidatedGetter)(const Variant *base, Variant *value);

	static bool has_member(Variant::Type p_type, const StringName &p_member);
	static Variant::Type get_member_type(Variant::Type p_type, const StringName &p_member);
	static void get_member_list(Type p_type, List<StringName> *r_members);
	static int get_member_count(Type p_type);

	static ValidatedSetter get_member_validated_setter(Variant::Type p_type, const StringName &p_member);
	static ValidatedGetter get_member_validated_getter(Variant::Type p_type, const StringName &p_member);

	typedef void (*PTRSetter)(void *base, const void *value);
	typedef void (*PTRGetter)(const void *base, void *value);

	static PTRSetter get_member_ptr_setter(Variant::Type p_type, const StringName &p_member);
	static PTRGetter get_member_ptr_getter(Variant::Type p_type, const StringName &p_member);

	/* Indexing */

	static bool has_indexing(Variant::Type p_type);
	static Variant::Type get_indexed_element_type(Variant::Type p_type);
	static uint32_t get_indexed_element_usage(Variant::Type p_type);

	typedef void (*ValidatedIndexedSetter)(Variant *base, int64_t index, const Variant *value, bool *oob);
	typedef void (*ValidatedIndexedGetter)(const Variant *base, int64_t index, Variant *value, bool *oob);

	static ValidatedIndexedSetter get_member_validated_indexed_setter(Variant::Type p_type);
	static ValidatedIndexedGetter get_member_validated_indexed_getter(Variant::Type p_type);

	typedef void (*PTRIndexedSetter)(void *base, int64_t index, const void *value);
	typedef void (*PTRIndexedGetter)(const void *base, int64_t index, void *value);

	static PTRIndexedSetter get_member_ptr_indexed_setter(Variant::Type p_type);
	static PTRIndexedGetter get_member_ptr_indexed_getter(Variant::Type p_type);

	void set_indexed(int64_t p_index, const Variant &p_value, bool &r_valid, bool &r_oob);
	Variant get_indexed(int64_t p_index, bool &r_valid, bool &r_oob) const;

	uint64_t get_indexed_size() const;

	/* Keying */

	static bool is_keyed(Variant::Type p_type);

	typedef void (*ValidatedKeyedSetter)(Variant *base, const Variant *key, const Variant *value, bool *valid);
	typedef void (*ValidatedKeyedGetter)(const Variant *base, const Variant *key, Variant *value, bool *valid);
	typedef bool (*ValidatedKeyedChecker)(const Variant *base, const Variant *key, bool *valid);

	static ValidatedKeyedSetter get_member_validated_keyed_setter(Variant::Type p_type);
	static ValidatedKeyedGetter get_member_validated_keyed_getter(Variant::Type p_type);
	static ValidatedKeyedChecker get_member_validated_keyed_checker(Variant::Type p_type);

	typedef void (*PTRKeyedSetter)(void *base, const void *key, const void *value);
	typedef void (*PTRKeyedGetter)(const void *base, const void *key, void *value);
	typedef uint32_t (*PTRKeyedChecker)(const void *base, const void *key);

	static PTRKeyedSetter get_member_ptr_keyed_setter(Variant::Type p_type);
	static PTRKeyedGetter get_member_ptr_keyed_getter(Variant::Type p_type);
	static PTRKeyedChecker get_member_ptr_keyed_checker(Variant::Type p_type);

	void set_keyed(const Variant &p_key, const Variant &p_value, bool &r_valid);
	Variant get_keyed(const Variant &p_key, bool &r_valid) const;
	bool has_key(const Variant &p_key, bool &r_valid) const;

	/* Generic */
	enum VariantSetError {
		SET_OK,
		SET_KEYED_ERR,
		SET_NAMED_ERR,
		SET_INDEXED_ERR
	};
	enum VariantGetError {
		GET_OK,
		GET_KEYED_ERR,
		GET_NAMED_ERR,
		GET_INDEXED_ERR
	};
	void set(const Variant &p_index, const Variant &p_value, bool *r_valid = nullptr, VariantSetError *err_code = nullptr);
	Variant get(const Variant &p_index, bool *r_valid = nullptr, VariantGetError *err_code = nullptr) const;
	bool in(const Variant &p_index, bool *r_valid = nullptr) const;

	bool iter_init(Variant &r_iter, bool &r_valid) const;
	bool iter_next(Variant &r_iter, bool &r_valid) const;
	Variant iter_get(const Variant &r_iter, bool &r_valid) const;

	void get_property_list(List<PropertyInfo> *p_list) const;

	static void call_utility_function(const StringName &p_name, Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	static bool has_utility_function(const StringName &p_name);

	typedef void (*ValidatedUtilityFunction)(Variant *r_ret, const Variant **p_args, int p_argcount);
	typedef void (*PTRUtilityFunction)(void *r_ret, const void **p_args, int p_argcount);

	static ValidatedUtilityFunction get_validated_utility_function(const StringName &p_name);
	static PTRUtilityFunction get_ptr_utility_function(const StringName &p_name);

	enum UtilityFunctionType {
		UTILITY_FUNC_TYPE_MATH,
		UTILITY_FUNC_TYPE_RANDOM,
		UTILITY_FUNC_TYPE_GENERAL,
	};

	static UtilityFunctionType get_utility_function_type(const StringName &p_name);

	static MethodInfo get_utility_function_info(const StringName &p_name);
	static int get_utility_function_argument_count(const StringName &p_name);
	static Variant::Type get_utility_function_argument_type(const StringName &p_name, int p_arg);
	static String get_utility_function_argument_name(const StringName &p_name, int p_arg);
	static bool has_utility_function_return_value(const StringName &p_name);
	static Variant::Type get_utility_function_return_type(const StringName &p_name);
	static bool is_utility_function_vararg(const StringName &p_name);
	static uint32_t get_utility_function_hash(const StringName &p_name);

	static void get_utility_function_list(List<StringName> *r_functions);
	static int get_utility_function_count();

	//argsVariant call()

	bool operator==(const Variant &p_variant) const;
	bool operator!=(const Variant &p_variant) const;
	bool operator<(const Variant &p_variant) const;
	uint32_t hash() const;
	uint32_t recursive_hash(int recursion_count) const;

	// By default, performs a semantic comparison. Otherwise, numeric/binary comparison (if appropriate).
	bool hash_compare(const Variant &p_variant, int recursion_count = 0, bool semantic_comparison = true) const;
	bool identity_compare(const Variant &p_variant) const;
	bool booleanize() const;
	String stringify(int recursion_count = 0) const;
	String to_json_string() const;

	static void get_constants_for_type(Variant::Type p_type, List<StringName> *p_constants);
	static int get_constants_count_for_type(Variant::Type p_type);
	static bool has_constant(Variant::Type p_type, const StringName &p_value);
	static Variant get_constant_value(Variant::Type p_type, const StringName &p_value, bool *r_valid = nullptr);

	static void get_enums_for_type(Variant::Type p_type, List<StringName> *p_enums);
	static void get_enumerations_for_enum(Variant::Type p_type, const StringName &p_enum_name, List<StringName> *p_enumerations);
	static int get_enum_value(Variant::Type p_type, const StringName &p_enum_name, const StringName &p_enumeration, bool *r_valid = nullptr);
	static bool has_enum(Variant::Type p_type, const StringName &p_enum_name);
	static StringName get_enum_for_enumeration(Variant::Type p_type, const StringName &p_enumeration);

	typedef String (*ObjectDeConstruct)(const Variant &p_object, void *ud);
	typedef void (*ObjectConstruct)(const String &p_text, void *ud, Variant &r_value);

	String get_construct_string() const;
	static void construct_from_string(const String &p_string, Variant &r_value, ObjectConstruct p_obj_construct = nullptr, void *p_construct_ud = nullptr);

	void operator=(const Variant &p_variant); // only this is enough for all the other types
	void operator=(Variant &&p_variant) {
		if (unlikely(this == &p_variant)) {
			return;
		}
		clear();
		type = p_variant.type;
		_data = p_variant._data;
		p_variant.type = NIL;
	}

	static void register_types();
	static void unregister_types();

	Variant(const Variant &p_variant);
	Variant(Variant &&p_variant) {
		type = p_variant.type;
		_data = p_variant._data;
		p_variant.type = NIL;
	}
	_FORCE_INLINE_ Variant() {}
	_FORCE_INLINE_ ~Variant() {
		if (unlikely(needs_deinit[type])) { // Make it fast for types that don't need deinit.
			_clear_internal();
		}
	}
};

//typedef Dictionary Dictionary; no
//typedef Array Array;

template <typename... VarArgs>
Vector<Variant> varray(VarArgs... p_args) {
	Vector<Variant> v;

	Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
	uint32_t argc = sizeof...(p_args);

	if (argc > 0) {
		v.resize(argc);
		Variant *vw = v.ptrw();

		for (uint32_t i = 0; i < argc; i++) {
			vw[i] = args[i];
		}
	}
	return v;
}

struct VariantHasher {
	static _FORCE_INLINE_ uint32_t hash(const Variant &p_variant) { return p_variant.hash(); }
};

struct VariantComparator {
	static _FORCE_INLINE_ bool compare(const Variant &p_lhs, const Variant &p_rhs) { return p_lhs.hash_compare(p_rhs); }
};

struct StringLikeVariantComparator {
	static bool compare(const Variant &p_lhs, const Variant &p_rhs);
};

struct StringLikeVariantOrder {
	static _ALWAYS_INLINE_ bool compare(const Variant &p_lhs, const Variant &p_rhs) {
		if (p_lhs.is_string() && p_rhs.is_string()) {
			return p_lhs.operator String() < p_rhs.operator String();
		}
		return p_lhs < p_rhs;
	}

	_ALWAYS_INLINE_ bool operator()(const Variant &p_lhs, const Variant &p_rhs) const {
		return compare(p_lhs, p_rhs);
	}
};

Variant::ObjData &Variant::_get_obj() {
	return *reinterpret_cast<ObjData *>(&_data._mem[0]);
}

const Variant::ObjData &Variant::_get_obj() const {
	return *reinterpret_cast<const ObjData *>(&_data._mem[0]);
}

template <typename... VarArgs>
String vformat(const String &p_text, const VarArgs... p_args) {
	Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
	Array args_array;
	args_array.resize(sizeof...(p_args));
	for (uint32_t i = 0; i < sizeof...(p_args); i++) {
		args_array[i] = args[i];
	}

	bool error = false;
	String fmt = p_text.sprintf(args_array, &error);

	ERR_FAIL_COND_V_MSG(error, String(), String("Formatting error in string \"") + p_text + "\": " + fmt + ".");

	return fmt;
}

template <typename... VarArgs>
Variant Callable::call(VarArgs... p_args) const {
	Variant args[sizeof...(p_args) + 1] = { p_args..., 0 }; // +1 makes sure zero sized arrays are also supported.
	const Variant *argptrs[sizeof...(p_args) + 1];
	for (uint32_t i = 0; i < sizeof...(p_args); i++) {
		argptrs[i] = &args[i];
	}

	Variant ret;
	CallError ce;
	callp(sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args), ret, ce);
	return ret;
}

template <typename... VarArgs>
Callable Callable::bind(VarArgs... p_args) const {
	Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
	const Variant *argptrs[sizeof...(p_args) + 1];
	for (uint32_t i = 0; i < sizeof...(p_args); i++) {
		argptrs[i] = &args[i];
	}
	return bindp(sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
}

Variant &Array::Iterator::operator*() const {
	if (unlikely(read_only)) {
		*read_only = *element_ptr;
		return *read_only;
	}
	return *element_ptr;
}

Variant *Array::Iterator::operator->() const {
	if (unlikely(read_only)) {
		*read_only = *element_ptr;
		return read_only;
	}
	return element_ptr;
}

Array::Iterator &Array::Iterator::operator++() {
	element_ptr++;
	return *this;
}

Array::Iterator &Array::Iterator::operator--() {
	element_ptr--;
	return *this;
}

const Variant &Array::ConstIterator::operator*() const {
	if (unlikely(read_only)) {
		*read_only = *element_ptr;
		return *read_only;
	}
	return *element_ptr;
}

const Variant *Array::ConstIterator::operator->() const {
	if (unlikely(read_only)) {
		*read_only = *element_ptr;
		return read_only;
	}
	return element_ptr;
}

Array::ConstIterator &Array::ConstIterator::operator++() {
	element_ptr++;
	return *this;
}

Array::ConstIterator &Array::ConstIterator::operator--() {
	element_ptr--;
	return *this;
}
