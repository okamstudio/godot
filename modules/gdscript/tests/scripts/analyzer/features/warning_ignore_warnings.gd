@warning_ignore("redundant_static_unload")
@static_unload
extends Node

class A extends Node:
	static func static_called_on_instance():
		pass

	@warning_ignore("get_node_default_without_onready")
	var get_node_default_without_onready = $Node

@warning_ignore("unused_private_class_variable")
var __unused_private_class_variable
@warning_ignore("unused_protected_class_variable")
var _unused_protected_class_variable

@warning_ignore("onready_with_export")
@onready @export var onready_with_export = 1

var shadowed_variable
var confusable_local_usage

@warning_ignore("unused_signal")
signal unused_signal()

func variant_func() -> Variant:
	return null

func int_func() -> int:
	return 1

@warning_ignore("unused_parameter")
func test_warnings(unused_private_class_variable):
	var t = 1

	var unassigned_variable
	@warning_ignore("unassigned_variable")
	print(unassigned_variable)

	var _unassigned_variable_op_assign
	@warning_ignore("unassigned_variable_op_assign", "accessing_protected_member")
	_unassigned_variable_op_assign += t

	@warning_ignore("unused_variable")
	var unused_variable

	@warning_ignore("unused_local_constant")
	const unused_local_constant = 1

	@warning_ignore("shadowed_variable")
	var shadowed_variable = 1
	print(shadowed_variable)

	@warning_ignore("shadowed_variable_base_class")
	var name = "test"
	print(name)

	@warning_ignore("shadowed_global_identifier")
	var var_to_str = 1
	print(var_to_str)

	@warning_ignore("standalone_expression")
	1 + 2

	@warning_ignore("standalone_ternary")
	1 if 2 else 3

	@warning_ignore("incompatible_ternary")
	t = 1 if 2 else false

	@warning_ignore("unsafe_property_access")
	self.unsafe_property_access = 1

	var node: Node = null
	@warning_ignore("unsafe_method_access")
	node.unsafe_method_access()

	@warning_ignore("unsafe_cast")
	print(variant_func().x as int)

	var key: Variant = "key"
	@warning_ignore("unsafe_call_argument")
	set(key, 1)

	variant_func() # No warning (intended?).
	@warning_ignore("return_value_discarded")
	int_func()

	var a: A = null
	@warning_ignore("static_called_on_instance")
	a.static_called_on_instance()

	@warning_ignore("redundant_await")
	await 1

	@warning_ignore("assert_always_true")
	assert(true)

	assert(false) # No warning (intended).
	@warning_ignore("assert_always_false")
	assert(false and false)

	@warning_ignore("integer_division")
	var _integer_division = 5 / 2

	@warning_ignore("narrowing_conversion")
	var _narrowing_conversion: int = floorf(2.5)

	@warning_ignore("int_as_enum_without_cast")
	var _int_as_enum_without_cast: Variant.Type = 1

	@warning_ignore("int_as_enum_without_cast", "int_as_enum_without_match")
	var _int_as_enum_without_match: Variant.Type = 255

	@warning_ignore("confusable_identifier")
	var _cОnfusable_identifier = 1

	if true:
		@warning_ignore("confusable_local_declaration")
		var _confusable_local_declaration = 1
	var _confusable_local_declaration = 2

	@warning_ignore("confusable_local_usage")
	print(confusable_local_usage)
	@warning_ignore("shadowed_variable")
	var confusable_local_usage = 2
	print(confusable_local_usage)

	@warning_ignore("inference_on_variant")
	var _inference_on_variant := variant_func()

func test_unreachable_code():
	return
	@warning_ignore("unreachable_code")
	print(1)

func test_unreachable_pattern():
	match 1:
		_:
			print(0)
		@warning_ignore("unreachable_pattern")
		1:
			print(1)

func test_unsafe_void_return_variant() -> void:
	return variant_func() # No warning (intended?).

func test_unsafe_void_return() -> void:
	@warning_ignore("unsafe_method_access", "unsafe_void_return")
	return variant_func().f()

@warning_ignore("native_method_override")
func get_class():
	pass

class AccessTestA:
	@warning_ignore("unused_protected_class_variable")
	static var _static_a = null
	@warning_ignore("unused_private_class_variable")
	static var __static_b = null

	@warning_ignore("unused_protected_class_variable")
	var _a = null
	@warning_ignore("unused_private_class_variable")
	var __b = null

	func _call_a():
		pass

	func __call_b():
		pass

	func _call_a_ret():
		return null

	func __call_b_ret():
		return null

	static func _static_call_a():
		pass

	static func __static_call_b():
		pass

	static func _static_call_a_ret():
		return null

	static func __static_call_b_ret():
		return null

class AccessTestB:
	func _init():
		var cls_a = AccessTestA.new()
		@warning_ignore("accessing_protected_member")
		AccessTestA._static_a = null
		@warning_ignore("accessing_private_member")
		AccessTestA.__static_b = null
		@warning_ignore("accessing_protected_member")
		cls_a._a = null
		@warning_ignore("accessing_private_member")
		cls_a.__b = null
		@warning_ignore("calling_protected_method")
		cls_a._call_a()
		@warning_ignore("calling_private_method")
		cls_a.__call_b()

		@warning_ignore("unused_variable", "calling_protected_method")
		var t1 = cls_a._call_a_ret()
		@warning_ignore("unused_variable", "calling_private_method")
		var t2 = cls_a.__call_b_ret()
		@warning_ignore("calling_protected_method")
		AccessTestA._static_call_a()
		@warning_ignore("calling_private_method")
		AccessTestA.__static_call_b()
		@warning_ignore("unused_variable", "calling_protected_method")
		var t3 = AccessTestA._static_call_a_ret()
		@warning_ignore("unused_variable", "calling_private_method")
		var t4 = AccessTestA.__static_call_b_ret()

class AccessTestC extends AccessTestA:
	func _init():
		@warning_ignore("accessing_private_member")
		AccessTestA.__static_b = null
		@warning_ignore("accessing_private_member")
		__b = null
		@warning_ignore("calling_private_method")
		__call_b()

		@warning_ignore("unused_variable", "calling_private_method")
		var t1 = __call_b_ret()
		@warning_ignore("calling_private_method")
		AccessTestA.__static_call_b()
		@warning_ignore("unused_variable", "calling_private_method")
		var t2 = AccessTestA.__static_call_b_ret()

# We don't want to execute it because of errors, just analyze.
func test():
	pass
