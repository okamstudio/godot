class MyClass:
	const TEST = 10

enum MyEnum {A, B, C}

const Other = preload("./metatypes.notest.gd")

var test_native := JSON
var test_script := Other
var test_class := MyClass
var test_enum := MyEnum

func check_gdscript_native_class(value: Variant) -> void:
	print(var_to_str(value).get_slice(",", 0).trim_prefix("Object(\n"))

func check_gdscript(value: GDScript) -> void:
	print(value.get_class())

func check_enum(value: Dictionary) -> void:
	print(value)

func test():
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_signature(property))

	print("---")
	check_gdscript_native_class(test_native)
	check_gdscript(test_script)
	check_gdscript(test_class)
	check_enum(test_enum)

	print("---")
	print(test_native.stringify([]))
	print(test_script.TEST)
	print(test_class.TEST)
	print(test_enum.keys())

	print("---")
	# Some users add unnecessary type hints to `const`-`preload`, which removes metatypes.
	# For **constant** `GDScript` we still check the class members, despite the wider type.
	const ScriptNoMeta: GDScript = Other
	const ClassNoMeta: GDScript = MyClass
	var a := ScriptNoMeta.TEST
	var b := ClassNoMeta.TEST
	print(a)
	print(b)
