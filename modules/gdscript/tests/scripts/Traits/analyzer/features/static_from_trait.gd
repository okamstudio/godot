trait SomeTrait:
	static var some_static_var = 0
	static var other_static_var = 0
	static var third_static_var = 0

	static func some_static_func():
		print("some static func")

	static func other_static_func():
		print("other static func")

	static func third_static_func():
		print("third static func")

class SomeClass:
	uses SomeTrait

	# Overridden static variable
	var other_static_var = 1 # using 'static' keyword is optional
	static var third_static_var = 1

	# Overridden static function
	func other_static_func(): # using 'static' keyword is optional
		print("overridden other static func")

	static func third_static_func():
		print("overridden third static func")

class ThirdClass:
	uses SomeTrait

func test():
	print(SomeClass.some_static_var)
	print(SomeClass.other_static_var)
	print(SomeClass.third_static_var)
	print(ThirdClass.some_static_var)
	SomeClass.some_static_func()
	SomeClass.other_static_func()
	SomeClass.third_static_func()
	ThirdClass.some_static_func()
	print("ok")
