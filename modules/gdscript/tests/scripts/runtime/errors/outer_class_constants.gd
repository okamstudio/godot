class Outer:
	const OUTER_CONST := 0
	class Inner:
		pass

func subtest_type_hard():
	var type := Outer.Inner
	print(type.OUTER_CONST)

func subtest_type_weak():
	var type := Outer.Inner
	var type_v: Variant = type
	print(type_v.OUTER_CONST)

func subtest_instance_hard():
	var instance := Outer.Inner.new()
	print(instance.OUTER_CONST)

func subtest_instance_weak():
	var instance := Outer.Inner.new()
	var instance_v: Variant = instance
	print(instance_v.OUTER_CONST)

func test():
	subtest_type_hard()
	subtest_type_weak()
	subtest_instance_hard()
	subtest_instance_weak()
