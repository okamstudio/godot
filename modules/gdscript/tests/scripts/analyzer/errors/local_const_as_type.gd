enum MyEnum { A }

func test():
	var use_before_declared: EnumType
	const EnumType = MyEnum

	var enum_variable = MyEnum
	var use_non_constant: enum_variable

	const ENUM_VALUE = MyEnum.A
	var use_not_type: ENUM_VALUE
