class A extends RefCounted:
	pass

class B extends A:
	pass

func test():
	var builtin: Variant = 3
	Utils.check((builtin is Variant) == true)
	Utils.check((builtin is int) == true)
	Utils.check(is_instance_of(builtin, TYPE_INT) == true)
	Utils.check((builtin is float) == false)
	Utils.check(is_instance_of(builtin, TYPE_FLOAT) == false)

	const const_builtin: Variant = 3
	Utils.check((const_builtin is Variant) == true)
	Utils.check((const_builtin is int) == true)
	Utils.check(is_instance_of(const_builtin, TYPE_INT) == true)
	Utils.check((const_builtin is float) == false)
	Utils.check(is_instance_of(const_builtin, TYPE_FLOAT) == false)

	var int_array: Variant = [] as Array[int]
	Utils.check((int_array is Variant) == true)
	Utils.check((int_array is Array) == true)
	Utils.check(is_instance_of(int_array, TYPE_ARRAY) == true)
	Utils.check((int_array is Array[int]) == true)
	Utils.check((int_array is Array[float]) == false)
	Utils.check((int_array is int) == false)
	Utils.check(is_instance_of(int_array, TYPE_INT) == false)

	var const_int_array: Variant = [] as Array[int]
	Utils.check((const_int_array is Variant) == true)
	Utils.check((const_int_array is Array) == true)
	Utils.check(is_instance_of(const_int_array, TYPE_ARRAY) == true)
	Utils.check((const_int_array is Array[int]) == true)
	Utils.check((const_int_array is Array[float]) == false)
	Utils.check((const_int_array is int) == false)
	Utils.check(is_instance_of(const_int_array, TYPE_INT) == false)

	var b_array: Variant = [] as Array[B]
	Utils.check((b_array is Variant) == true)
	Utils.check((b_array is Array) == true)
	Utils.check(is_instance_of(b_array, TYPE_ARRAY) == true)
	Utils.check((b_array is Array[B]) == true)
	Utils.check((b_array is Array[A]) == false)
	Utils.check((b_array is Array[int]) == false)
	Utils.check((b_array is int) == false)
	Utils.check(is_instance_of(b_array, TYPE_INT) == false)

	var const_b_array: Variant = [] as Array[B]
	Utils.check((const_b_array is Variant) == true)
	Utils.check((const_b_array is Array) == true)
	Utils.check(is_instance_of(const_b_array, TYPE_ARRAY) == true)
	Utils.check((const_b_array is Array[B]) == true)
	Utils.check((const_b_array is Array[A]) == false)
	Utils.check((const_b_array is Array[int]) == false)
	Utils.check((const_b_array is int) == false)
	Utils.check(is_instance_of(const_b_array, TYPE_INT) == false)

	var native: Variant = RefCounted.new()
	Utils.check((native is Variant) == true)
	Utils.check((native is Object) == true)
	Utils.check(is_instance_of(native, TYPE_OBJECT) == true)
	Utils.check(is_instance_of(native, Object) == true)
	Utils.check((native is RefCounted) == true)
	Utils.check(is_instance_of(native, RefCounted) == true)
	Utils.check((native is Node) == false)
	Utils.check(is_instance_of(native, Node) == false)
	Utils.check((native is int) == false)
	Utils.check(is_instance_of(native, TYPE_INT) == false)

	var a_script: Variant = A.new()
	Utils.check((a_script is Variant) == true)
	Utils.check((a_script is Object) == true)
	Utils.check(is_instance_of(a_script, TYPE_OBJECT) == true)
	Utils.check(is_instance_of(a_script, Object) == true)
	Utils.check((a_script is RefCounted) == true)
	Utils.check(is_instance_of(a_script, RefCounted) == true)
	Utils.check((a_script is A) == true)
	Utils.check(is_instance_of(a_script, A) == true)
	Utils.check((a_script is B) == false)
	Utils.check(is_instance_of(a_script, B) == false)
	Utils.check((a_script is Node) == false)
	Utils.check(is_instance_of(a_script, Node) == false)
	Utils.check((a_script is int) == false)
	Utils.check(is_instance_of(a_script, TYPE_INT) == false)

	var b_script: Variant = B.new()
	Utils.check((b_script is Variant) == true)
	Utils.check((b_script is Object) == true)
	Utils.check(is_instance_of(b_script, TYPE_OBJECT) == true)
	Utils.check(is_instance_of(b_script, Object) == true)
	Utils.check((b_script is RefCounted) == true)
	Utils.check(is_instance_of(b_script, RefCounted) == true)
	Utils.check((b_script is A) == true)
	Utils.check(is_instance_of(b_script, A) == true)
	Utils.check((b_script is B) == true)
	Utils.check(is_instance_of(b_script, B) == true)
	Utils.check((b_script is Node) == false)
	Utils.check(is_instance_of(b_script, Node) == false)
	Utils.check((b_script is int) == false)
	Utils.check(is_instance_of(b_script, TYPE_INT) == false)

	var var_null: Variant = null
	Utils.check((var_null is Variant) == true)
	Utils.check((var_null is int) == false)
	Utils.check(is_instance_of(var_null, TYPE_INT) == false)
	Utils.check((var_null is Object) == false)
	Utils.check(is_instance_of(var_null, TYPE_OBJECT) == false)
	Utils.check((var_null is RefCounted) == false)
	Utils.check(is_instance_of(var_null, RefCounted) == false)
	Utils.check((var_null is A) == false)
	Utils.check(is_instance_of(var_null, A) == false)

	const const_null: Variant = null
	Utils.check((const_null is Variant) == true)
	Utils.check((const_null is int) == false)
	Utils.check(is_instance_of(const_null, TYPE_INT) == false)
	Utils.check((const_null is Object) == false)
	Utils.check(is_instance_of(const_null, TYPE_OBJECT) == false)
	Utils.check((const_null is RefCounted) == false)
	Utils.check(is_instance_of(const_null, RefCounted) == false)
	Utils.check((const_null is A) == false)
	Utils.check(is_instance_of(const_null, A) == false)

	print("ok")
