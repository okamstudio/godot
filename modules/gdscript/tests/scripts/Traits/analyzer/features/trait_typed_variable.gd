trait SomeTrait:
	pass

class SomeClass:
	uses SomeTrait

func test():
	var trait_class: SomeTrait = SomeClass.new()
	print(trait_class != null)
	print("ok")
