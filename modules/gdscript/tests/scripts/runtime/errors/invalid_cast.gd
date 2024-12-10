func subtest_wrong_builtin():
	var integer: Variant = 1
	print(integer as Array)

func subtest_builtin_as_object():
	var integer: Variant = 1
	print(integer as Node)

func subtest_object_as_builtin():
	var object: Variant = RefCounted.new()
	print(object as int)

func subtest_freed_object():
	var node := Node.new()
	node.free()
	print(node as Node2D)

func test():
	subtest_wrong_builtin()
	subtest_builtin_as_object()
	subtest_object_as_builtin()
	subtest_freed_object()
