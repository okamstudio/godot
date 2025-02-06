extends Node

class InnerA:
	class InnerInnerA:
		enum EnumOfInnerInnerA {
		ENUM_VALUE_1,
		ENUM_VALUE_2,
	}

	enum EnumOfInnerA {
		ENUM_VALUE_1,
		ENUM_VALUE_2,
	}

	signal signal_of_inner_a
	var property_of_inner_a
	func func_of_inner_a():
		pass

enum EnumOfA {
	ENUM_VALUE_1,
	ENUM_VALUE_2,
}

signal signal_of_a

var property_of_a

func func_of_a():
	pass
