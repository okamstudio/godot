class A:
	func f(x: int) -> void:
		print(x)

class B extends A:
	func f(x: int, ...args: Array) -> void:
		prints(x, args)

class C extends B:
	func f(x: int, y: int = 0, ...args: Array) -> void:
		prints(x, y, args)

class D extends C:
	func f(...args: Array) -> void:
		print(args)

func test_func(x: int, y: int = 0, ...args: Array) -> void:
	prints(x, y, args)

var test_lambda := func (x: int, y: int = 0, ...args: Array) -> void:
	prints(x, y, args)

func test():
	for method in get_method_list():
		if str(method.name).begins_with("test_"):
			print(Utils.get_method_signature(method))

	test_func(1)
	test_func(1, 2)
	test_func(1, 2, 3)
	test_func(1, 2, 3, 4)
	test_func(1, 2, 3, 4, 5)

	test_lambda.call(1)
	test_lambda.call(1, 2)
	test_lambda.call(1, 2, 3)
	test_lambda.call(1, 2, 3, 4)
	test_lambda.call(1, 2, 3, 4, 5)
