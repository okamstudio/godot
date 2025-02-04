trait SomeTrait:
	signal some_signal(to_emit:Node)

class SomeClass:
	uses SomeTrait
	signal some_signal(to_emit: Node2D)

func test():
	print("ok")
