func subtest_builtin():
	var array: Variant = []
	print(array.reverse())

func subtest_native():
	var ref_counted: Variant = RefCounted.new()
	print(ref_counted.notify_property_list_changed())

func subtest_free():
	var node: Variant = Node.new()
	print(node.free())

func test():
	subtest_builtin()
	subtest_native()
	subtest_free()
