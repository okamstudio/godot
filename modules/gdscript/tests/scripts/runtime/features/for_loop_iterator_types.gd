class Iterator:
	func _iter_init(_count):
		return true
	func _iter_next(_count):
		return false
	func _iter_get(_count) -> StringName:
		return &"custom"

func test():
	var hard_int := 1
	var hard_vector_2i := Vector2i(1, 2)
	var hard_vector_3i := Vector3i(1, 2, 1)
	var hard_float := 1.0
	var hard_vector_2 := Vector2(1.0, 2.0)
	var hard_vector_3 := Vector3(1.0, 2.0, 1.0)
	var hard_string := "a"
	var hard_array := ["elem"]
	var hard_int_array: Array[int] = [123]
	var hard_packed_color_array: PackedColorArray = [Color.RED]
	var hard_dictionary := {key = "value"}
	var hard_iterator := Iterator.new()

	var variant_int: Variant = hard_int
	var variant_vector_2i: Variant = hard_vector_2i
	var variant_vector_3i: Variant = hard_vector_3i
	var variant_float: Variant = hard_float
	var variant_vector_2: Variant = hard_vector_2
	var variant_vector_3: Variant = hard_vector_3
	var variant_string: Variant = hard_string
	var variant_array: Variant = hard_array
	var variant_int_array: Variant = hard_int_array
	var variant_packed_color_array: Variant = hard_packed_color_array
	var variant_dictionary: Variant = hard_dictionary
	var variant_iterator: Variant = hard_iterator

	for i in 1:
		var _i := i
		print(var_to_str(i))
	for i in Vector2i(1, 2):
		var _i := i
		print(var_to_str(i))
	for i in Vector3i(1, 2, 1):
		var _i := i
		print(var_to_str(i))
	for i in 1.0:
		var _i := i
		print(var_to_str(i))
	for i in Vector2(1.0, 2.0):
		var _i := i
		print(var_to_str(i))
	for i in Vector3(1.0, 2.0, 1.0):
		var _i := i
		print(var_to_str(i))
	for i in "a":
		var _i := i
		print(var_to_str(i))
	for i in ["elem"]:
		@warning_ignore("inference_on_variant")
		var _i := i
		print(var_to_str(i))
	for i in [123] as Array[int]:
		var _i := i
		print(var_to_str(i))
	for i in PackedColorArray([Color.RED]):
		var _i := i
		print(var_to_str(i))
	for i in {key = "value"}:
		@warning_ignore("inference_on_variant")
		var _i := i
		print(var_to_str(i))
	for i in Iterator.new():
		var _i := i
		print(var_to_str(i))

	print("=====")

	for i in hard_int:
		var _i := i
		print(var_to_str(i))
	for i in hard_vector_2i:
		var _i := i
		print(var_to_str(i))
	for i in hard_vector_3i:
		var _i := i
		print(var_to_str(i))
	for i in hard_float:
		var _i := i
		print(var_to_str(i))
	for i in hard_vector_2:
		var _i := i
		print(var_to_str(i))
	for i in hard_vector_3:
		var _i := i
		print(var_to_str(i))
	for i in hard_string:
		var _i := i
		print(var_to_str(i))
	for i in hard_array:
		@warning_ignore("inference_on_variant")
		var _i := i
		print(var_to_str(i))
	for i in hard_int_array:
		var _i := i
		print(var_to_str(i))
	for i in hard_packed_color_array:
		var _i := i
		print(var_to_str(i))
	for i in hard_dictionary:
		@warning_ignore("inference_on_variant")
		var _i := i
		print(var_to_str(i))
	for i in hard_iterator:
		var _i := i
		print(var_to_str(i))

	print("=====")

	for i in variant_int:
		print(var_to_str(i))
	for i in variant_vector_2i:
		print(var_to_str(i))
	for i in variant_vector_3i:
		print(var_to_str(i))
	for i in variant_float:
		print(var_to_str(i))
	for i in variant_vector_2:
		print(var_to_str(i))
	for i in variant_vector_3:
		print(var_to_str(i))
	for i in variant_string:
		print(var_to_str(i))
	for i in variant_array:
		print(var_to_str(i))
	for i in variant_int_array:
		print(var_to_str(i))
	for i in variant_packed_color_array:
		print(var_to_str(i))
	for i in variant_dictionary:
		print(var_to_str(i))
	for i in variant_iterator:
		print(var_to_str(i))
