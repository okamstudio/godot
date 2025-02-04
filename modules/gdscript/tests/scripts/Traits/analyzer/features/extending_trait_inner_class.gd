trait SomeTrait:
	class SomeInnerTrait:
		signal some_signal(param: Node)
		enum {
			enum_value
		}
		enum named_enum {
			named_enum_value
		}
		var some_var = "some var"
		func some_func():
			print("some func")

class SomeClass:
	uses SomeTrait
	class SomeInnerTrait:
		signal some_signal(param: Node2D)
		signal other_signal()
		enum {
			enum_value_2
		}
		enum named_enum {
			named_enum_value_2
		}
		var some_var = "overridden some var"
		var other_var = "some var in innerclass"
		func other_func():
			print("other func")

func test():
	var inner_class = SomeClass.SomeInnerTrait.new()
	inner_class.other_signal.connect(print.bind("some signal in innerclass"))
	inner_class.some_signal.emit()
	print(inner_class.enum_value)
	print(inner_class.enum_value_2)
	print(inner_class.named_enum.named_enum_value)
	print(inner_class.named_enum.named_enum_value_2)
	print(inner_class.some_var)
	print(inner_class.other_var)
	inner_class.some_func()
	inner_class.other_func()
	print("ok")
