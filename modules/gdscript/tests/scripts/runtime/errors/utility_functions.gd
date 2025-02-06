func subtest_pass_wrong_arg_builtin():
	var x = Color()
	print(floor(x)) # Built-in utility function.

func subtest_pass_wrong_arg_gdscript():
	var x = Color()
	print(len(x)) # GDScript utility function.

func test():
	subtest_pass_wrong_arg_builtin()
	subtest_pass_wrong_arg_gdscript()
