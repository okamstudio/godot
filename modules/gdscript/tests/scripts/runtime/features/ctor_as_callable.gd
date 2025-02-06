# https://github.com/godotengine/godot/issues/70319

class InnerClass:
	pass

func test():
	var inner_ctor : Callable = InnerClass.new
	print(inner_ctor)
	var native_ctor : Callable = Node.new
	print(native_ctor)
