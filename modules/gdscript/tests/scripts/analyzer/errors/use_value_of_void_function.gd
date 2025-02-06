func foo() -> void:
	pass

func test():
	var array := []
	var ref_counted: Variant = RefCounted.new()
	var node := Node.new()

	print(print()) # Built-in utility function.
	print(print_debug()) # GDScript utility function.
	print(array.reverse()) # Built-in type method.
	print(ref_counted.notify_property_list_changed()) # Native type method. # TODO
	print(node.free()) # `Object.free()` method.
	print(foo()) # Custom method.
