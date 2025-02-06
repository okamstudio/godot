extends Node
class_name B

class InnerB:
	class InnerInnerB:
		enum EnumOfInnerInnerB {
		ENUM_VALUE_1,
		ENUM_VALUE_2,
	}

	enum EnumOfInnerB {
		ENUM_VALUE_1,
		ENUM_VALUE_2,
	}

	signal signal_of_inner_b
	var property_of_inner_b
	func func_of_inner_b():
		pass

enum EnumOfB {
	ENUM_VALUE_1,
	ENUM_VALUE_2,
}

signal signal_of_b

var property_of_b

func func_of_b():
	pass
