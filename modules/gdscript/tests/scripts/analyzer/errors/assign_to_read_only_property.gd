func test():
	# Directly.
	var tree := SceneTree.new()
	tree.root = Window.new()
	tree.free()

	# Indirectly.
	var state := PhysicsDirectBodyState3DExtension.new()
	state.center_of_mass.x += 1.0
	state.free()
