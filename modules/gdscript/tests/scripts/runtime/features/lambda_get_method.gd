# https://github.com/godotengine/godot/issues/94074

func foo():
	pass

func test():
	var lambda_self := func test() -> void:
		foo()
	var anon_lambda_self := func() -> void:
		foo()

	print(lambda_self.get_method())  # Should print "test".
	print(anon_lambda_self.get_method())  # Should print "<anonymous lambda(anon_lambda_self)>".

	var lambda_non_self := func test() -> void:
		pass
	var anon_lambda_non_self := func() -> void:
		pass

	print(lambda_non_self.get_method())  # Should print "test".
	print(anon_lambda_non_self.get_method())  # Should print "<anonymous lambda(anon_lambda_non_self)>".
