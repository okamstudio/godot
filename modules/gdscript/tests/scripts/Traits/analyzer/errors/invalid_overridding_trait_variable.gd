trait SomeTrait:
	var some_variable: Node

class SomeClass:
	uses SomeTrait
	var some_variable: Node2D

func test():
	print("ok")
