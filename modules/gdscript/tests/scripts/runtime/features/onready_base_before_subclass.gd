#GH-63329
class A extends Node:
	@onready var a := get_value("a")

	func get_value(var_name: String) -> String:
		print(var_name)
		return var_name

class B extends A:
	@onready var b := get_value("b")

	func _ready():
		pass

func test():
	var node := B.new()
	@warning_ignore("calling_protected_method")
	node._ready()
	node.free()
