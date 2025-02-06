func subtest_int():
	var x: int = 1
	x /= 0
	print(x)

func subtest_vector2i():
	var v: Vector2i = Vector2i.ONE
	v /= Vector2i.ZERO
	print(v)

func subtest_vector3i():
	var v: Vector3i = Vector3i.ONE
	v /= Vector3i.ZERO
	print(v)

func subtest_vector4i():
	var v: Vector4i = Vector4i.ONE
	v /= Vector4i.ZERO
	print(v)

func subtest_vector2i_div_int():
	var v: Vector2i = Vector2i.ONE
	v /= 0
	print(v)

func subtest_vector3i_div_int():
	var v: Vector3i = Vector3i.ONE
	v /= 0
	print(v)

func subtest_vector4i_div_int():
	var v: Vector4i = Vector4i.ONE
	v /= 0
	print(v)

func test():
	subtest_int()
	subtest_vector2i()
	subtest_vector3i()
	subtest_vector4i()
	subtest_vector2i_div_int()
	subtest_vector3i_div_int()
	subtest_vector4i_div_int()
