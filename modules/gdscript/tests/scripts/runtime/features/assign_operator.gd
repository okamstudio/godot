# https://github.com/godotengine/godot/issues/75832

@warning_ignore_start("narrowing_conversion")
func test():
	var hf := 2.0
	var sf = 2.0

	var i := 2
	i *= hf
	i *= sf
	i *= 2.0
	print(i)
	var v2 := Vector2i(1, 2)
	v2 *= hf
	v2 *= sf
	v2 *= 2.0
	print(v2)
	var v3 := Vector3i(1, 2, 3)
	v3 *= hf
	v3 *= sf
	v3 *= 2.0
	print(v3)
	var v4 := Vector4i(1, 2, 3, 4)
	v4 *= hf
	v4 *= sf
	v4 *= 2.0
	print(v4)

	var arr := [1, 2, 3]
	arr += [4, 5]
	print(arr)
