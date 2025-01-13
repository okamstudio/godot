func test():
	print(true, false)
	print(-1, 0, 1)
	print(-1.25, 0.25, 1.25)
	print("hello world")

	print(Vector2(0.25, 1))
	print(Vector2i(0, 0))

	print(Rect2(0.25, 0.25, 0.5, 1))
	print(Rect2i(0, 0, 0, 0))

	print(Vector3(0.25, 0.25, 1))
	print(Vector3i(0, 0, 0))

	print(Vector4(0.25, 0.25, 0.25, 1))
	print(Vector4i(0, 0, 0, 0))

	print(Transform2D.IDENTITY)
	print(Plane(1, 2, 3, 4))
	print(Quaternion(1, 2, 3, 4))
	print(AABB(Vector3.ZERO, Vector3.ONE))
	print(Basis.from_euler(Vector3(0, 0, 0)))
	print(Transform3D.IDENTITY)
	print(Projection.IDENTITY)

	print(Color(1, 2, 3, 4))
	print(StringName("hello"))
	print(NodePath("hello/world"))
	var node := Node.new()
	print(RID(node)) # TODO: Why is the constructor (or implicit cast) not documented?
	print(node.get_name)
	print(node.property_list_changed)
	node.free()
	print({"hello":123})
	print(["hello", 123])

	print(PackedByteArray([-1, 0, 1]))
	print(PackedInt32Array([-1, 0, 1]))
	print(PackedInt64Array([-1, 0, 1]))
	print(PackedFloat32Array([-1, 0, 1]))
	print(PackedFloat64Array([-1, 0, 1]))
	print(PackedStringArray(["hello", "world"]))
	print(PackedVector2Array([Vector2.ONE, Vector2.ZERO]))
	print(PackedVector3Array([Vector3.ONE, Vector3.ZERO]))
	print(PackedColorArray([Color.RED, Color.BLUE, Color.GREEN]))
	print(PackedVector4Array([Vector4.ONE, Vector4.ZERO]))
