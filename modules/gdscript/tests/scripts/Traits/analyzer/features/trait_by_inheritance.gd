extends OtherClass

class OtherClass extends SomeClass:
	uses SomeTrait

class SomeClass:
	uses OtherTrait

trait SomeTrait:
	pass

trait OtherTrait:
	pass

func test():
	print(self is SomeTrait)
	print(self is OtherTrait)
	print("ok")
