class Foo: pass
class Bar extends Foo: pass
class Baz extends Foo: pass

func expect_typed(typed: Array[int]):
	print(typed.size())

func subtest_assign_basic_to_typed():
	var basic := [1]
	var _typed: Array[int] = basic
	print("end subtest_assign_basic_to_typed")

func subtest_assign_basic_to_differently_typed():
	var differently: Variant = [1.0] as Array[float]
	var _typed: Array[int] = differently
	print("end subtest_assign_basic_to_differently_typed")

func subtest_assign_wrong_to_typed():
	var _typed: Array[Bar] = [Baz.new() as Foo]
	print("end subtest_assign_wrong_to_typed")

func subtest_pass_basic_to_typed():
	var basic := [1]
	expect_typed(basic)
	print("end subtest_pass_basic_to_typed")

func subtest_pass_basic_to_differently_typed():
	var differently: Variant = [1.0] as Array[float]
	expect_typed(differently)
	print("end subtest_pass_basic_to_differently_typed")

func test():
	subtest_assign_basic_to_typed()
	subtest_assign_basic_to_differently_typed()
	subtest_assign_wrong_to_typed()
	subtest_pass_basic_to_typed()
	subtest_pass_basic_to_differently_typed()
