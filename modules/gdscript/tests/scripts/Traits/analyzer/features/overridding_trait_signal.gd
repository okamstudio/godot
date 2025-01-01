trait SomeTrait:
	signal some_signal(to_emit: Node)

class SomeClass:
	uses SomeTrait
	signal some_signal(to_emit: Object)

func test():
	print("ok")
