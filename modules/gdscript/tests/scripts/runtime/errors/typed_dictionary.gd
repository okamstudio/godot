class Foo: pass
class Bar extends Foo: pass
class Baz extends Foo: pass

func get_key() -> Variant:
	return "key"

func get_value() -> Variant:
	return "value"

func expect_typed(typed: Dictionary[int, int]):
	print(typed.size())

func subtest_assign_basic_to_typed():
	var basic := { 1: 1 }
	var _typed: Dictionary[int, int] = basic
	print("end subtest_assign_basic_to_typed")

func subtest_assign_basic_to_differently_typed_key():
	var typed: Dictionary[int, int]
	typed[get_key()] = 0
	print("end subtest_assign_basic_to_differently_typed_key")

func subtest_assign_basic_to_differently_typed_value():
	var typed: Dictionary[int, int]
	typed[0] = get_value()
	print("end subtest_assign_basic_to_differently_typed_value")

func subtest_assign_differently_typed():
	var differently: Variant = { 1.0: 0.0 } as Dictionary[float, float]
	var _typed: Dictionary[int, int] = differently
	print("end subtest_assign_differently_typed")

func subtest_assign_wrong_to_typed():
	var _typed: Dictionary[Bar, Bar] = { Baz.new() as Foo: Baz.new() as Foo }
	print("end subtest_assign_wrong_to_typed")

func subtest_pass_basic_to_typed():
	var basic := { 1: 1 }
	expect_typed(basic)
	print("end subtest_pass_basic_to_typed")

func subtest_pass_basic_to_differently_typed():
	var differently: Variant = { 1.0: 0.0 } as Dictionary[float, float]
	expect_typed(differently)
	print("end subtest_pass_basic_to_differently_typed")

func test():
	subtest_assign_basic_to_typed()
	subtest_assign_basic_to_differently_typed_key()
	subtest_assign_basic_to_differently_typed_value()
	subtest_assign_differently_typed()
	subtest_assign_wrong_to_typed()
	subtest_pass_basic_to_typed()
	subtest_pass_basic_to_differently_typed()
