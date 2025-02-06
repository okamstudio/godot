# https://github.com/godotengine/godot/issues/90086

class MyObj:
	var obj: WeakRef

func test():
	var obj_1 = MyObj.new()
	var obj_2 = MyObj.new()
	obj_1.obj = obj_2
	prints(obj_1, obj_2)
