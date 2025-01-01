trait SomeTrait extends Node:
	pass

class SomeClass extends Node2D:
	uses SomeTrait

func test() -> void:
	print("ok")
