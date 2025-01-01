trait SomeTrait:
	pass

class SomeClass:
	uses SomeTrait

func return_typed() -> SomeTrait:
	return SomeClass.new()

func test():
	print(return_typed() != null)
	print("ok")
