class A extends CanvasItem:
	func _init():
		pass

class B extends A:
	pass

class C extends CanvasItem:
	pass

abstract
class X:
	pass

class Y extends X:
	func test() -> String:
		return "ok"

func test():
	print(Y.new().test())
