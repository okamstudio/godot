class A: pass
class B extends A: pass

enum E { E0 = 391, E1 = 193 }

func floats_identity(floats: Dictionary[float, float]): return floats

class Members:
	var one: Dictionary[int, int] = { 104: 401 }
	var two: Dictionary[int, int] = one

	func check_passing() -> bool:
		Utils.check(str(one) == '{ 104: 401 }')
		Utils.check(str(two) == '{ 104: 401 }')
		two[582] = 285
		Utils.check(str(one) == '{ 104: 401, 582: 285 }')
		Utils.check(str(two) == '{ 104: 401, 582: 285 }')
		two = { 486: 684 }
		Utils.check(str(one) == '{ 104: 401, 582: 285 }')
		Utils.check(str(two) == '{ 486: 684 }')
		return true


@warning_ignore_start("unsafe_method_access", "return_value_discarded")
func test():
	var untyped_basic = { 459: 954 }
	Utils.check(str(untyped_basic) == '{ 459: 954 }')
	Utils.check(untyped_basic.get_typed_key_builtin() == TYPE_NIL)
	Utils.check(untyped_basic.get_typed_value_builtin() == TYPE_NIL)

	var inferred_basic := { 366: 663 }
	Utils.check(str(inferred_basic) == '{ 366: 663 }')
	Utils.check(inferred_basic.get_typed_key_builtin() == TYPE_NIL)
	Utils.check(inferred_basic.get_typed_value_builtin() == TYPE_NIL)

	var typed_basic: Dictionary = { 521: 125 }
	Utils.check(str(typed_basic) == '{ 521: 125 }')
	Utils.check(typed_basic.get_typed_key_builtin() == TYPE_NIL)
	Utils.check(typed_basic.get_typed_value_builtin() == TYPE_NIL)


	var empty_floats: Dictionary[float, float] = {}
	Utils.check(str(empty_floats) == '{  }')
	Utils.check(empty_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(empty_floats.get_typed_value_builtin() == TYPE_FLOAT)

	untyped_basic = empty_floats
	Utils.check(untyped_basic.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(untyped_basic.get_typed_value_builtin() == TYPE_FLOAT)

	inferred_basic = empty_floats
	Utils.check(inferred_basic.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(inferred_basic.get_typed_value_builtin() == TYPE_FLOAT)

	typed_basic = empty_floats
	Utils.check(typed_basic.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(typed_basic.get_typed_value_builtin() == TYPE_FLOAT)

	empty_floats[705.0] = 507.0
	untyped_basic[430.0] = 34.0
	inferred_basic[263.0] = 362.0
	typed_basic[518.0] = 815.0
	Utils.check(str(empty_floats) == '{ 705.0: 507.0, 430.0: 34.0, 263.0: 362.0, 518.0: 815.0 }')
	Utils.check(str(untyped_basic) == '{ 705.0: 507.0, 430.0: 34.0, 263.0: 362.0, 518.0: 815.0 }')
	Utils.check(str(inferred_basic) == '{ 705.0: 507.0, 430.0: 34.0, 263.0: 362.0, 518.0: 815.0 }')
	Utils.check(str(typed_basic) == '{ 705.0: 507.0, 430.0: 34.0, 263.0: 362.0, 518.0: 815.0 }')


	const constant_float := 950.0
	const constant_int := 170
	var typed_float := 954.0
	var filled_floats: Dictionary[float, float] = { constant_float: constant_int, typed_float: empty_floats[430.0] + empty_floats[263.0] }
	Utils.check(str(filled_floats) == '{ 950.0: 170.0, 954.0: 396.0 }')
	Utils.check(filled_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(filled_floats.get_typed_value_builtin() == TYPE_FLOAT)

	var casted_floats := { empty_floats[263.0] * 2: empty_floats[263.0] / 2 } as Dictionary[float, float]
	Utils.check(str(casted_floats) == '{ 724.0: 181.0 }')
	Utils.check(casted_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(casted_floats.get_typed_value_builtin() == TYPE_FLOAT)

	var returned_floats = (func () -> Dictionary[float, float]: return { 554: 455 }).call()
	Utils.check(str(returned_floats) == '{ 554.0: 455.0 }')
	Utils.check(returned_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(returned_floats.get_typed_value_builtin() == TYPE_FLOAT)

	var passed_floats = floats_identity({ 663.0 if randf() > 0.5 else 663.0: 366.0 if randf() <= 0.5 else 366.0 })
	Utils.check(str(passed_floats) == '{ 663.0: 366.0 }')
	Utils.check(passed_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(passed_floats.get_typed_value_builtin() == TYPE_FLOAT)

	var default_floats = (func (floats: Dictionary[float, float] = { 364.0: 463.0 }): return floats).call()
	Utils.check(str(default_floats) == '{ 364.0: 463.0 }')
	Utils.check(default_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(default_floats.get_typed_value_builtin() == TYPE_FLOAT)

	var typed_int := 556
	var converted_floats: Dictionary[float, float] = { typed_int: typed_int }
	converted_floats[498.0] = 894
	Utils.check(str(converted_floats) == '{ 556.0: 556.0, 498.0: 894.0 }')
	Utils.check(converted_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(converted_floats.get_typed_value_builtin() == TYPE_FLOAT)


	const constant_basic = { 228: 822 }
	Utils.check(str(constant_basic) == '{ 228: 822 }')
	Utils.check(constant_basic.get_typed_key_builtin() == TYPE_NIL)
	Utils.check(constant_basic.get_typed_value_builtin() == TYPE_NIL)

	const constant_floats: Dictionary[float, float] = { constant_float - constant_basic[228] - constant_int: constant_float + constant_basic[228] + constant_int }
	Utils.check(str(constant_floats) == '{ -42.0: 1942.0 }')
	Utils.check(constant_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(constant_floats.get_typed_value_builtin() == TYPE_FLOAT)


	var source_floats: Dictionary[float, float] = { 999.74: 47.999 }
	untyped_basic = source_floats
	var destination_floats: Dictionary[float, float] = untyped_basic
	destination_floats[999.74] -= 0.999
	Utils.check(str(source_floats) == '{ 999.74: 47.0 }')
	Utils.check(str(untyped_basic) == '{ 999.74: 47.0 }')
	Utils.check(str(destination_floats) == '{ 999.74: 47.0 }')
	Utils.check(destination_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(destination_floats.get_typed_value_builtin() == TYPE_FLOAT)


	var duplicated_floats := empty_floats.duplicate()
	duplicated_floats.erase(705.0)
	duplicated_floats.erase(430.0)
	duplicated_floats.erase(518.0)
	duplicated_floats[263.0] *= 3
	Utils.check(str(duplicated_floats) == '{ 263.0: 1086.0 }')
	Utils.check(duplicated_floats.get_typed_key_builtin() == TYPE_FLOAT)
	Utils.check(duplicated_floats.get_typed_value_builtin() == TYPE_FLOAT)


	var b_objects: Dictionary[int, B] = { 0: B.new(), 1: B.new() as A, 2: null }
	Utils.check(b_objects.size() == 3)
	Utils.check(b_objects.get_typed_value_builtin() == TYPE_OBJECT)
	Utils.check(b_objects.get_typed_value_script() == B)

	var a_objects: Dictionary[int, A] = { 0: A.new(), 1: B.new(), 2: null, 3: b_objects[0] }
	Utils.check(a_objects.size() == 4)
	Utils.check(a_objects.get_typed_value_builtin() == TYPE_OBJECT)
	Utils.check(a_objects.get_typed_value_script() == A)

	var a_passed = (func check_a_passing(p_objects: Dictionary[int, A]): return p_objects.size()).call(a_objects)
	Utils.check(a_passed == 4)

	var b_passed = (func check_b_passing(basic: Dictionary): return basic[0] != null).call(b_objects)
	Utils.check(b_passed == true)


	var empty_strings: Dictionary[String, String] = {}
	var empty_bools: Dictionary[bool, bool] = {}
	var empty_basic_one := {}
	var empty_basic_two := {}
	Utils.check(empty_strings == empty_bools)
	Utils.check(empty_basic_one == empty_basic_two)
	Utils.check(empty_strings.hash() == empty_bools.hash())
	Utils.check(empty_basic_one.hash() == empty_basic_two.hash())


	var assign_source: Dictionary[int, int] = { 527: 725 }
	var assign_target: Dictionary[int, int] = {}
	assign_target.assign(assign_source)
	Utils.check(str(assign_source) == '{ 527: 725 }')
	Utils.check(str(assign_target) == '{ 527: 725 }')
	assign_source[657] = 756
	Utils.check(str(assign_source) == '{ 527: 725, 657: 756 }')
	Utils.check(str(assign_target) == '{ 527: 725 }')


	var defaults_passed = (func check_defaults_passing(one: Dictionary[int, int] = {}, two := one):
		one[887] = 788
		two[198] = 891
		Utils.check(str(one) == '{ 887: 788, 198: 891 }')
		Utils.check(str(two) == '{ 887: 788, 198: 891 }')
		two = {130: 31}
		Utils.check(str(one) == '{ 887: 788, 198: 891 }')
		Utils.check(str(two) == '{ 130: 31 }')
		return true
	).call()
	Utils.check(defaults_passed == true)


	var members := Members.new()
	var members_passed := members.check_passing()
	Utils.check(members_passed == true)


	var typed_enums: Dictionary[E, E] = {}
	typed_enums[E.E0] = E.E1
	Utils.check(str(typed_enums) == '{ 391: 193 }')
	Utils.check(typed_enums.get_typed_key_builtin() == TYPE_INT)
	Utils.check(typed_enums.get_typed_value_builtin() == TYPE_INT)

	const const_enums: Dictionary[E, E] = {}
	Utils.check(const_enums.get_typed_key_builtin() == TYPE_INT)
	Utils.check(const_enums.get_typed_key_class_name() == &'')
	Utils.check(const_enums.get_typed_value_builtin() == TYPE_INT)
	Utils.check(const_enums.get_typed_value_class_name() == &'')


	var a := A.new()
	var b := B.new()
	var typed_natives: Dictionary[RefCounted, RefCounted] = { a: b }
	var typed_scripts = Dictionary(typed_natives, TYPE_OBJECT, "RefCounted", A, TYPE_OBJECT, "RefCounted", B)
	Utils.check(typed_scripts[a] == b)


	print('ok')
