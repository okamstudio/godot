# GH-75645
class MyNode extends Node:
	static func static_func():
		var node = $Node

class MyRefCounted extends RefCounted:
	func non_static_func():
		var node = $Node

func test():
	pass
