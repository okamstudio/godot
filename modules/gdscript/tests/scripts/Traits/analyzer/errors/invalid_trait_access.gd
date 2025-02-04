trait SomeTrait:
	enum some_enum {
		A,
		B
	}
	static var some_static_var = 0
	static func some_static_func():
		print("some static func")

func test():
	SomeTrait.new()
	SomeTrait.some_static_func()
	SomeTrait.some_static_var
	SomeTrait.some_enum
