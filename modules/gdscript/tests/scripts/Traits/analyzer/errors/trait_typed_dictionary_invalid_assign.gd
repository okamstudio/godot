trait SomeTrait:
	pass

class OtherClass:
	pass

func test():
	var classes: Dictionary[SomeTrait, String] = {OtherClass.new() : "A"}
