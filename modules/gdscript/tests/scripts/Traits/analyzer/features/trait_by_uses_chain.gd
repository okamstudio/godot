uses OtherTrait

trait SomeTrait:
	pass

trait OtherTrait:
	uses SomeTrait

func test() -> void:
	print(self is OtherTrait)
	print(self is SomeTrait)
	print("ok")
