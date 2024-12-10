enum BadEnum {
	A = 0.0,
	B = "hello",
}

enum CustomEnum { A, B }

func test():
	print(Variant.Operator) # Global.
	print(Vector3.Axis) # Built-in.
	print(Node.ProcessMode) # Native.

	print(Side.NOT_EXIST) # Global.
	print(Vector3.Axis.NOT_EXIST) # Built-in.
	print(TileSet.TileShape.NOT_EXIST) # Native.
	print(CustomEnum.NOT_EXIST) # Custom.

	print(Side.size()) # Global.
	print(Vector3.Axis.size()) # Built-in.
	print(TileSet.TileShape.size()) # Native.

	Side.clear() # Global.
	Vector3.Axis.clear() # Built-in.
	TileSet.TileShape.clear() # Native.
	CustomEnum.clear() # Custom.

	var enum_type = CustomEnum
	enum_type.clear()
