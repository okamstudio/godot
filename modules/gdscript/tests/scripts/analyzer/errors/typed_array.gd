func expect_typed(typed: Array[int]):
	print(typed.size())

func test():
	var float_array: Array[float] = [1.0]
	var integer := 1

	var array_1: Array[int] = ["Hello", "World"]
	var array_2: Array[int] = float_array
	var array_3: Array[Object] = [integer]
	expect_typed(float_array)
