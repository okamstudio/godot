class_name EnumFunctionTypecheckOuterClass

enum MyEnum { V0, V1, V2 }

class InnerClass:
	enum MyEnum { V0, V2, V1 }

	func inner_inner_no_class(e: MyEnum) -> MyEnum:
		print(e)
		return e

	func inner_inner_class(e: InnerClass.MyEnum) -> InnerClass.MyEnum:
		print(e)
		return e

	func inner_inner_class_class(e: EnumFunctionTypecheckOuterClass.InnerClass.MyEnum) -> EnumFunctionTypecheckOuterClass.InnerClass.MyEnum:
		print(e)
		return e

	func inner_outer(e: EnumFunctionTypecheckOuterClass.MyEnum) -> EnumFunctionTypecheckOuterClass.MyEnum:
		print(e)
		return e

	func test():
		var _d
		print("Inner")

		var o := EnumFunctionTypecheckOuterClass.new()
		_d = o.outer_outer_no_class(EnumFunctionTypecheckOuterClass.MyEnum.V1)
		print()
		_d = o.outer_outer_class(EnumFunctionTypecheckOuterClass.MyEnum.V1)
		print()
		_d = o.outer_inner_class(MyEnum.V1)
		_d = o.outer_inner_class(InnerClass.MyEnum.V1)
		_d = o.outer_inner_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
		print()
		_d = o.outer_inner_class_class(MyEnum.V1)
		_d = o.outer_inner_class_class(InnerClass.MyEnum.V1)
		_d = o.outer_inner_class_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
		print()
		print()


		_d = inner_inner_no_class(MyEnum.V1)
		_d = inner_inner_no_class(InnerClass.MyEnum.V1)
		_d = inner_inner_no_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
		print()
		_d = inner_inner_class(MyEnum.V1)
		_d = inner_inner_class(InnerClass.MyEnum.V1)
		_d = inner_inner_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
		print()
		_d = inner_inner_class_class(MyEnum.V1)
		_d = inner_inner_class_class(InnerClass.MyEnum.V1)
		_d = inner_inner_class_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
		print()
		_d = inner_outer(EnumFunctionTypecheckOuterClass.MyEnum.V1)
		print()
		print()


func outer_outer_no_class(e: MyEnum) -> MyEnum:
	print(e)
	return e

func outer_outer_class(e: EnumFunctionTypecheckOuterClass.MyEnum) -> EnumFunctionTypecheckOuterClass.MyEnum:
	print(e)
	return e

func outer_inner_class(e: InnerClass.MyEnum) -> InnerClass.MyEnum:
	print(e)
	return e

func outer_inner_class_class(e: EnumFunctionTypecheckOuterClass.InnerClass.MyEnum) -> EnumFunctionTypecheckOuterClass.InnerClass.MyEnum:
	print(e)
	return e

func test():
	var _d
	print("Outer")

	_d = outer_outer_no_class(MyEnum.V1)
	_d = outer_outer_no_class(EnumFunctionTypecheckOuterClass.MyEnum.V1)
	print()
	_d = outer_outer_class(MyEnum.V1)
	_d = outer_outer_class(EnumFunctionTypecheckOuterClass.MyEnum.V1)
	print()
	_d = outer_inner_class(InnerClass.MyEnum.V1)
	_d = outer_inner_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
	print()
	_d = outer_inner_class_class(InnerClass.MyEnum.V1)
	_d = outer_inner_class_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
	print()
	print()

	var i := EnumFunctionTypecheckOuterClass.InnerClass.new()

	_d = i.inner_inner_no_class(InnerClass.MyEnum.V1)
	_d = i.inner_inner_no_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
	print()
	_d = i.inner_inner_class(InnerClass.MyEnum.V1)
	_d = i.inner_inner_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
	print()
	_d = i.inner_inner_class_class(InnerClass.MyEnum.V1)
	_d = i.inner_inner_class_class(EnumFunctionTypecheckOuterClass.InnerClass.MyEnum.V1)
	print()
	_d = i.inner_outer(MyEnum.V1)
	_d = i.inner_outer(EnumFunctionTypecheckOuterClass.MyEnum.V1)
	print()
	print()

	i.test()
