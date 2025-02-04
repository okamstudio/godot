trait SomeTrait:
	func some_func(param: Node):
		print("some func")

class SomeClass:
	uses SomeTrait

	func some_func(param: Node2D):
		print("overridden some func")

func test():
	SomeClass.new().some_func(Node.new())
	print("ok")
