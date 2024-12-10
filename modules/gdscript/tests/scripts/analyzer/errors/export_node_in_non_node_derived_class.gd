# GH-82809

extends Node

class MyResource extends Resource:
	@export var node: Node
	@export var node_array: Array[Node]

func test():
	pass
