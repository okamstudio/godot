func subtest_attribute(state):
	state.center_of_mass.x -= 1.0
	print("end subtest_attribute")

func subtest_variable_index(state, prop):
	state[prop].x = 1.0
	print("end subtest_variable_index")

func test():
	var state = PhysicsDirectBodyState3DExtension.new()
	subtest_attribute(state)
	subtest_variable_index(state, &"center_of_mass")
	state.free()
