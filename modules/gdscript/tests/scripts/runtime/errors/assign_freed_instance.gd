class A extends Node:
	pass

func subtest_native():
	var x = Node.new()
	x.free()
	var _ok = x
	var _bad: Node = x
	print("end subtest_native")

func subtest_script():
	var x = A.new()
	x.free()
	var _ok = x
	var _bad: A = x
	print("end subtest_script")

func test():
	subtest_native()
	subtest_script()
