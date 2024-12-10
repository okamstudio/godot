class InnerClass:
	enum MyEnum { ENUM_VALUE_1, ENUM_VALUE_2 }

enum MyEnum { ENUM_VALUE_1, ENUM_VALUE_2 }
enum MyOtherEnum { OTHER_ENUM_VALUE_1, OTHER_ENUM_VALUE_2 }

enum { ENUM_VALUE_1, ENUM_VALUE_2 }

var class_var_1: MyEnum = MyOtherEnum.OTHER_ENUM_VALUE_1
var class_var_2: MyEnum

func enum_func_1(e: MyEnum) -> void:
	print(e)

func enum_func_2() -> MyEnum:
	return MyOtherEnum.OTHER_ENUM_VALUE_1

func test():
	class_var_2 = MyOtherEnum.OTHER_ENUM_VALUE_2

	var local_var_1: MyEnum = MyOtherEnum.OTHER_ENUM_VALUE_1

	var local_var_2: MyEnum
	local_var_2 = MyOtherEnum.OTHER_ENUM_VALUE_2

	var local_var_3: MyEnum
	local_var_3 = InnerClass.MyEnum.ENUM_VALUE_2

	var local_var_4: MyEnum = ENUM_VALUE_1

	const P1 = preload("../features/enum_from_outer.gd")
	var local_var_5: MyEnum
	local_var_5 = P1.Named.VALUE_A

	const P2 = preload("../features/enum_value_from_parent.gd")
	var local_var_6: MyEnum
	local_var_6 = P2.VALUE_B

	enum_func_1(MyOtherEnum.OTHER_ENUM_VALUE_1)
