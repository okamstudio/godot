func test():
	var dict = { a = 1 }
	var a = 1
	match 2:
		dict["a"]: # TODO: Fix positional information (parser bug).
			print("not allowed")
		a + 2:
			print("not allowed")
		_ when b == 0:
			print("b does not exist")
