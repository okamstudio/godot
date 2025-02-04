trait SomeTrait:
	var some_variable: Node

class SomeClass:
	uses SomeTrait
	var some_variable: Object

func test():
	print("ok")
