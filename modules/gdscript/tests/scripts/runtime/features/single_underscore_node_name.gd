extends Node

func test() -> void:
	var node1 := Node.new()
	node1.name = "_"
	var node2 := Node.new()
	node2.name = "Child"
	var node3 := Node.new()
	node3.name = "Child"

	add_child(node1)
	node1.add_child(node2)
	add_child(node3)

	Utils.check(get_node("_/Child") == $_/Child)
