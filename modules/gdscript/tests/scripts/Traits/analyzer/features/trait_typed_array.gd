trait SomeTrait:
	pass

class SomeClass:
	uses SomeTrait

class OtherClass:
	pass

func test():
	var trait_classes: Array[SomeTrait] = [SomeClass.new()]
	trait_classes.append(SomeClass.new())
	trait_classes.append(OtherClass.new())
	var count = 1
	for trait_class in trait_classes:
		print(count)
		count += 1
	print("ok")
