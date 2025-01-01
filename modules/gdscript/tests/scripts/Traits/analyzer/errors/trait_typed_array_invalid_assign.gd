trait SomeTrait:
	pass

class OtherClass:
	pass

func test():
	var trait_classes: Array[SomeTrait] = [OtherClass.new()]
