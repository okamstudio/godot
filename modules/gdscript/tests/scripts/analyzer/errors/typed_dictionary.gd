func expect_typed(typed: Dictionary[int, int]):
	print(typed.size())

func test():
	var float_dict: Dictionary[float, float] = { 1.0: 0.0 }
	var integer := 1

	var dict_1: Dictionary[int, int] = { "Hello": "World" }
	var dict_2: Dictionary[int, int] = float_dict
	var dict_3: Dictionary[Object, Object] = { integer: integer }
	expect_typed(float_dict)
