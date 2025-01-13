class_name TestMemberInfo

class MyClass:
	pass

enum MyEnum {}

static var test_static_var_untyped
static var test_static_var_weak_null = null
static var test_static_var_weak_int = 1
static var test_static_var_hard_int: int

var test_var_untyped
var test_var_weak_null = null
var test_var_weak_int = 1
@export var test_var_weak_int_exported = 1
var test_var_weak_variant_type = TYPE_NIL
@export var test_var_weak_variant_type_exported = TYPE_NIL
var test_var_hard_variant: Variant
var test_var_hard_int: int
var test_var_hard_variant_type: Variant.Type
@export var test_var_hard_variant_type_exported: Variant.Type
var test_var_hard_node_process_mode: Node.ProcessMode
@warning_ignore("enum_variable_without_default")
var test_var_hard_my_enum: MyEnum
var test_var_hard_array: Array
var test_var_hard_array_int: Array[int]
var test_var_hard_array_variant_type: Array[Variant.Type]
var test_var_hard_array_node_process_mode: Array[Node.ProcessMode]
var test_var_hard_array_my_enum: Array[MyEnum]
var test_var_hard_array_resource: Array[Resource]
var test_var_hard_array_this: Array[TestMemberInfo]
var test_var_hard_array_my_class: Array[MyClass]
var test_var_hard_dictionary: Dictionary
var test_var_hard_dictionary_int_variant: Dictionary[int, Variant]
var test_var_hard_dictionary_variant_int: Dictionary[Variant, int]
var test_var_hard_dictionary_int_int: Dictionary[int, int]
var test_var_hard_dictionary_variant_type: Dictionary[Variant.Type, Variant.Type]
var test_var_hard_dictionary_node_process_mode: Dictionary[Node.ProcessMode, Node.ProcessMode]
var test_var_hard_dictionary_my_enum: Dictionary[MyEnum, MyEnum]
var test_var_hard_dictionary_resource: Dictionary[Resource, Resource]
var test_var_hard_dictionary_this: Dictionary[TestMemberInfo, TestMemberInfo]
var test_var_hard_dictionary_my_class: Dictionary[MyClass, MyClass]
var test_var_hard_resource: Resource
var test_var_hard_this: TestMemberInfo
var test_var_hard_my_class: MyClass

static func test_static_func(): pass

func test_func_implicit_void(): pass
func test_func_explicit_void() -> void: pass
func test_func_weak_null(): return null
func test_func_weak_int(): return 1
func test_func_hard_variant() -> Variant: return null
func test_func_hard_int() -> int: return 1
func test_func_args_1(_a: int, _b: Array[int], _c: Dictionary[int, int], _d: int = 1, _e = 2): pass
func test_func_args_2(_a = 1, _b = _a, _c = [2], _d = 3): pass

@warning_ignore_start("unused_signal")
signal test_signal_1()
signal test_signal_2(a: Variant, b)
signal test_signal_3(a: int, b: Array[int], c: Dictionary[int, int])
signal test_signal_4(a: Variant.Type, b: Array[Variant.Type], c: Dictionary[Variant.Type, Variant.Type])
signal test_signal_5(a: MyEnum, b: Array[MyEnum], c: Dictionary[MyEnum, MyEnum])
signal test_signal_6(a: Resource, b: Array[Resource], c: Dictionary[Resource, Resource])
signal test_signal_7(a: TestMemberInfo, b: Array[TestMemberInfo], c: Dictionary[TestMemberInfo, TestMemberInfo])
signal test_signal_8(a: MyClass, b: Array[MyClass], c: Dictionary[MyClass, MyClass])
@warning_ignore_restore("unused_signal")

func test():
	var script: Script = get_script()
	for property in script.get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_signature(property, null, true))
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_signature(property))
	for method in get_method_list():
		if str(method.name).begins_with("test_"):
			print(Utils.get_method_signature(method))
	for method in get_signal_list():
		if str(method.name).begins_with("test_"):
			print(Utils.get_method_signature(method, true))
