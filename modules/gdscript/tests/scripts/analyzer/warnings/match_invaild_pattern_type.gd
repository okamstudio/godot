func test():
	# Test Array
	match [1, 2.0, "str", false]:
		[2, 4.0, "str2", true]:
			print("should not match")
		[2.0, 4.0, "str2", true]:
			print("should not match")

	# Test Dictionary
	match {1:2, 1.0:2.0, "str":1, true:2.0}:
		{1:2, 1.0:2.0, "str":1, true:2.0}:
			print("should not match")
		{1:2.0, 1.0:2.0, "str":1, true:2.0, 1:3}:
			print("should not match")
		{1:2.0, 1.0:2.0, "str":1, true:2.0}:
			print("should not match")
		[2.0, 4.0, "str2", true]:
			print("should not match")

	# Test GODOT types
	var bool_type: bool = false
	var int_type: int = 1
	var float_type: float = 1.0

	var vector2_type: Vector2 = Vector2(1.0, 1.0)
	var vector2i_type: Vector2i = Vector2i(1, 1)
	var rect2_type: Rect2 = Rect2(1.0, 1.0, 10.0, 10.0)
	var rect2i_type: Rect2i = Rect2(1, 1, 10, 10)
	var vector3_type: Vector3 = Vector3(1.0, 0.0, 0.0)
	var vector3i_type: Vector3i = Vector3i(1, 1, 1)
	var transform2d_type: Transform2D = Transform2D(1.0, vector2_type)
	var vector4_type: Vector4 = Vector4(1.0, 1.0, 1.0, 1.0)
	var vector4i_type: Vector4i = Vector4i(1, 1, 1, 1)
	var plane_type: Plane = Plane(0.0, 1.0, 2.0, 3.0)
	var quaternion_type: Quaternion = Quaternion(0.0, 1.0, 2.0, 3.0)
	var aabb_type: AABB = AABB(vector3_type, vector3_type)
	var basis_type: Basis = Basis(vector3_type, 1.0)
	var transform3d_type: Transform3D = Transform3D(basis_type, vector3_type)
	var projection_type: Projection = Projection(transform3d_type)
	var color_type: Color = Color(0.0, 0.0, 0.0, 0.0)
	var node_path_type: NodePath = NodePath("/test/string")
	var rid_type: RID = RID()
	var callable_type: Callable = Callable()
	var signal_type: Signal = Signal()
	var object_type: Object = Object.new()

	var packed_byte_array_type: PackedByteArray = PackedByteArray()
	var packed_int32_array_type: PackedInt32Array = PackedInt32Array()
	var packed_int64_array_type: PackedInt64Array = PackedInt64Array()
	var packed_float32_array_type: PackedFloat32Array = PackedFloat32Array()
	var packed_float64_array_type: PackedFloat64Array = PackedFloat64Array()
	var packed_string_array_type: PackedStringArray = PackedStringArray()
	var packed_vector2_array_type: PackedVector2Array = PackedVector2Array()
	var packed_vector3_array_type: PackedVector3Array = PackedVector3Array()
	var packed_color_array_type: PackedColorArray = PackedColorArray()
	var packed_vector4_array_type: PackedVector4Array = PackedVector4Array()

	# Test untyped
	var untyped = 0
	match untyped:
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match bool_type:
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match int_type:
		bool_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match float_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector2_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector2i_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match rect2_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match rect2i_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector3_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector3i_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match transform2d_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector4_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match vector4i_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match plane_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match quaternion_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match aabb_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match basis_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match transform3d_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match projection_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match color_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match node_path_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match rid_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match callable_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match signal_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match object_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_byte_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_int32_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_int64_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_float32_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_float64_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_string_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_vector2_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_vector3_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_color_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_vector4_array_type:
			print("should not match")

	match packed_vector4_array_type:
		bool_type:
			print("should not match")
		int_type:
			print("should not match")
		float_type:
			print("should not match")
		vector2_type:
			print("should not match")
		vector2i_type:
			print("should not match")
		rect2_type:
			print("should not match")
		rect2i_type:
			print("should not match")
		vector3_type:
			print("should not match")
		vector3i_type:
			print("should not match")
		transform2d_type:
			print("should not match")
		vector4_type:
			print("should not match")
		vector4i_type:
			print("should not match")
		plane_type:
			print("should not match")
		quaternion_type:
			print("should not match")
		aabb_type:
			print("should not match")
		basis_type:
			print("should not match")
		transform3d_type:
			print("should not match")
		projection_type:
			print("should not match")
		color_type:
			print("should not match")
		node_path_type:
			print("should not match")
		rid_type:
			print("should not match")
		callable_type:
			print("should not match")
		signal_type:
			print("should not match")
		object_type:
			print("should not match")
		packed_byte_array_type:
			print("should not match")
		packed_int32_array_type:
			print("should not match")
		packed_int64_array_type:
			print("should not match")
		packed_float32_array_type:
			print("should not match")
		packed_float64_array_type:
			print("should not match")
		packed_string_array_type:
			print("should not match")
		packed_vector2_array_type:
			print("should not match")
		packed_vector3_array_type:
			print("should not match")
		packed_color_array_type:
			print("should not match")

	object_type.free()
