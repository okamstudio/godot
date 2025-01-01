trait SomeTrait:
	func some_func(param: Node2D)

class SomeClass:
	uses SomeTrait

	func some_func(_param: Node):
		print("overridden some func")

func test():
	var obj := Node.new()
	SomeClass.new().some_func(obj)
	obj.free()
	print("ok")
