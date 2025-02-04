trait SomeTrait:
	enum {
		enum_value
	}
	enum named_enum {
		named_enum_value
	}

class SomeClass:
	uses SomeTrait
	enum {
		enum_value_2
	}
	enum named_enum {
		named_enum_value_2
	}

func test():
	var using_class = SomeClass.new()
	print(using_class.enum_value)
	print(using_class.enum_value_2)
	print(using_class.named_enum.named_enum_value)
	print(using_class.named_enum.named_enum_value_2)
	print("ok")
