trait SomeTrait:
	signal some_signal()
	enum {
		enum_value
	}
	enum named_enum {
		named_enum_value
	}
	var some_var = "some var"
	const some_const = "some const"
	func some_func():
		print("some func")

	class SomeInnerClass:
		func func_in_inner_trait():
			print("func in inner trait()")

class SomeClass:
	uses SomeTrait

func test():
	var using_class = SomeClass.new()
	using_class.some_signal.connect(print.bind("some signal"))
	using_class.some_signal.emit()
	print(using_class.enum_value)
	print(using_class.named_enum.named_enum_value)
	print(using_class.some_var)
	print(using_class.some_const)
	using_class.some_func()
	SomeClass.SomeInnerClass.new().func_in_inner_trait()
	print("ok")
