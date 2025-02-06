func test():
	var array: Array[int]
	var packed_array: PackedVector2Array

	# these types should not be compatible
	packed_array = array;
