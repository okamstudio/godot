func test():
	var array: Array[Vector2] = [Vector2(0,0), Vector2(1,1)]
	var packed_array: PackedVector2Array

	packed_array = array

	print(packed_array)
