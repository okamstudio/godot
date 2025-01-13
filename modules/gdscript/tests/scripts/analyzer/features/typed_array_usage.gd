class A: pass
class B extends A: pass

enum E { E0 = 391 }

func floats_identity(floats: Array[float]): return floats

class Members:
	var one: Array[int] = [104]
	var two: Array[int] = one

	func check_passing() -> bool:
		Utils.check(str(one) == '[104]')
		Utils.check(str(two) == '[104]')
		two.push_back(582)
		Utils.check(str(one) == '[104, 582]')
		Utils.check(str(two) == '[104, 582]')
		two = [486]
		Utils.check(str(one) == '[104, 582]')
		Utils.check(str(two) == '[486]')
		return true

@warning_ignore_start('unsafe_method_access', 'return_value_discarded')
func test():
	var untyped_basic = [459]
	Utils.check(str(untyped_basic) == '[459]')
	Utils.check(untyped_basic.get_typed_builtin() == TYPE_NIL)

	var inferred_basic := [366]
	Utils.check(str(inferred_basic) == '[366]')
	Utils.check(inferred_basic.get_typed_builtin() == TYPE_NIL)

	var typed_basic: Array = [521]
	Utils.check(str(typed_basic) == '[521]')
	Utils.check(typed_basic.get_typed_builtin() == TYPE_NIL)


	var empty_floats: Array[float] = []
	Utils.check(str(empty_floats) == '[]')
	Utils.check(empty_floats.get_typed_builtin() == TYPE_FLOAT)

	untyped_basic = empty_floats
	Utils.check(untyped_basic.get_typed_builtin() == TYPE_FLOAT)

	inferred_basic = empty_floats
	Utils.check(inferred_basic.get_typed_builtin() == TYPE_FLOAT)

	typed_basic = empty_floats
	Utils.check(typed_basic.get_typed_builtin() == TYPE_FLOAT)

	empty_floats.push_back(705.0)
	untyped_basic.push_back(430.0)
	inferred_basic.push_back(263.0)
	typed_basic.push_back(518.0)
	Utils.check(str(empty_floats) == '[705.0, 430.0, 263.0, 518.0]')
	Utils.check(str(untyped_basic) == '[705.0, 430.0, 263.0, 518.0]')
	Utils.check(str(inferred_basic) == '[705.0, 430.0, 263.0, 518.0]')
	Utils.check(str(typed_basic) == '[705.0, 430.0, 263.0, 518.0]')


	const constant_float := 950.0
	const constant_int := 170
	var typed_float := 954.0
	var filled_floats: Array[float] = [constant_float, constant_int, typed_float, empty_floats[1] + empty_floats[2]]
	Utils.check(str(filled_floats) == '[950.0, 170.0, 954.0, 693.0]')
	Utils.check(filled_floats.get_typed_builtin() == TYPE_FLOAT)

	var casted_floats := [empty_floats[2] * 2] as Array[float]
	Utils.check(str(casted_floats) == '[526.0]')
	Utils.check(casted_floats.get_typed_builtin() == TYPE_FLOAT)

	var returned_floats = (func () -> Array[float]: return [554]).call()
	Utils.check(str(returned_floats) == '[554.0]')
	Utils.check(returned_floats.get_typed_builtin() == TYPE_FLOAT)

	var passed_floats = floats_identity([663.0 if randf() > 0.5 else 663.0])
	Utils.check(str(passed_floats) == '[663.0]')
	Utils.check(passed_floats.get_typed_builtin() == TYPE_FLOAT)

	var default_floats = (func (floats: Array[float] = [364.0]): return floats).call()
	Utils.check(str(default_floats) == '[364.0]')
	Utils.check(default_floats.get_typed_builtin() == TYPE_FLOAT)

	var typed_int := 556
	var converted_floats: Array[float] = [typed_int]
	converted_floats.push_back(498)
	Utils.check(str(converted_floats) == '[556.0, 498.0]')
	Utils.check(converted_floats.get_typed_builtin() == TYPE_FLOAT)


	const constant_basic = [228]
	Utils.check(str(constant_basic) == '[228]')
	Utils.check(constant_basic.get_typed_builtin() == TYPE_NIL)

	const constant_floats: Array[float] = [constant_float - constant_basic[0] - constant_int]
	Utils.check(str(constant_floats) == '[552.0]')
	Utils.check(constant_floats.get_typed_builtin() == TYPE_FLOAT)


	var source_floats: Array[float] = [999.74]
	untyped_basic = source_floats
	var destination_floats: Array[float] = untyped_basic
	destination_floats[0] -= 0.74
	Utils.check(str(source_floats) == '[999.0]')
	Utils.check(str(untyped_basic) == '[999.0]')
	Utils.check(str(destination_floats) == '[999.0]')
	Utils.check(destination_floats.get_typed_builtin() == TYPE_FLOAT)


	var duplicated_floats := empty_floats.duplicate().slice(2, 3)
	duplicated_floats[0] *= 3
	Utils.check(str(duplicated_floats) == '[789.0]')
	Utils.check(duplicated_floats.get_typed_builtin() == TYPE_FLOAT)


	var b_objects: Array[B] = [B.new(), B.new() as A, null]
	Utils.check(b_objects.size() == 3)
	Utils.check(b_objects.get_typed_builtin() == TYPE_OBJECT)
	Utils.check(b_objects.get_typed_script() == B)

	var a_objects: Array[A] = [A.new(), B.new(), null, b_objects[0]]
	Utils.check(a_objects.size() == 4)
	Utils.check(a_objects.get_typed_builtin() == TYPE_OBJECT)
	Utils.check(a_objects.get_typed_script() == A)

	var a_passed = (func check_a_passing(p_objects: Array[A]): return p_objects.size()).call(a_objects)
	Utils.check(a_passed == 4)

	var b_passed = (func check_b_passing(basic: Array): return basic[0] != null).call(b_objects)
	Utils.check(b_passed == true)


	var empty_strings: Array[String] = []
	var empty_bools: Array[bool] = []
	var empty_basic_one := []
	var empty_basic_two := []
	Utils.check(empty_strings == empty_bools)
	Utils.check(empty_basic_one == empty_basic_two)
	Utils.check(empty_strings.hash() == empty_bools.hash())
	Utils.check(empty_basic_one.hash() == empty_basic_two.hash())


	var assign_source: Array[int] = [527]
	var assign_target: Array[int] = []
	assign_target.assign(assign_source)
	Utils.check(str(assign_source) == '[527]')
	Utils.check(str(assign_target) == '[527]')
	assign_source.push_back(657)
	Utils.check(str(assign_source) == '[527, 657]')
	Utils.check(str(assign_target) == '[527]')


	var defaults_passed = (func check_defaults_passing(one: Array[int] = [], two := one):
		one.push_back(887)
		two.push_back(198)
		Utils.check(str(one) == '[887, 198]')
		Utils.check(str(two) == '[887, 198]')
		two = [130]
		Utils.check(str(one) == '[887, 198]')
		Utils.check(str(two) == '[130]')
		return true
	).call()
	Utils.check(defaults_passed == true)


	var members := Members.new()
	var members_passed := members.check_passing()
	Utils.check(members_passed == true)


	var resized_basic: Array = []
	resized_basic.resize(1)
	Utils.check(typeof(resized_basic[0]) == TYPE_NIL)
	Utils.check(resized_basic[0] == null)

	var resized_ints: Array[int] = []
	resized_ints.resize(1)
	Utils.check(typeof(resized_ints[0]) == TYPE_INT)
	Utils.check(resized_ints[0] == 0)

	var resized_arrays: Array[Array] = []
	resized_arrays.resize(1)
	Utils.check(typeof(resized_arrays[0]) == TYPE_ARRAY)
	resized_arrays[0].resize(1)
	resized_arrays[0][0] = 523
	Utils.check(str(resized_arrays) == '[[523]]')

	var resized_objects: Array[Object] = []
	resized_objects.resize(1)
	Utils.check(typeof(resized_objects[0]) == TYPE_NIL)
	Utils.check(resized_objects[0] == null)


	var typed_enums: Array[E] = []
	typed_enums.resize(1)
	Utils.check(str(typed_enums) == '[0]')
	typed_enums[0] = E.E0
	Utils.check(str(typed_enums) == '[391]')
	Utils.check(typed_enums.get_typed_builtin() == TYPE_INT)

	const const_enums: Array[E] = []
	Utils.check(const_enums.get_typed_builtin() == TYPE_INT)
	Utils.check(const_enums.get_typed_class_name() == &'')


	var a := A.new()
	var typed_natives: Array[RefCounted] = [a]
	var typed_scripts = Array(typed_natives, TYPE_OBJECT, 'RefCounted', A)
	Utils.check(typed_scripts[0] == a)


	print('ok')
