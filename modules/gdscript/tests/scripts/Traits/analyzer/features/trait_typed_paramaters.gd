trait SomeTrait:
	pass

class SomeClass:
	uses SomeTrait

signal emits_trait_type(param: SomeTrait)

func receive_trait_type(param: SomeTrait):
	print(param != null)

func test():
	emits_trait_type.connect(receive_trait_type)
	emits_trait_type.emit(SomeClass.new())
	receive_trait_type(SomeClass.new())
	print("ok")
