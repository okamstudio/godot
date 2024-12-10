extends Node

@export var test_type_1 := TYPE_BOOL
@export var test_type_2 := Variant.Type.TYPE_BOOL
@export var test_type_3: Variant.Type

@export var test_side_1 := SIDE_RIGHT
@export var test_side_2 := Side.SIDE_RIGHT
@export var test_side_3: Side

@export var test_axis_1 := Vector3.AXIS_Y
@export var test_axis_2 := Vector3.Axis.AXIS_Y
@export var test_axis_3: Vector3.Axis

@export var test_mode_1 := Node.PROCESS_MODE_ALWAYS
@export var test_mode_2 := Node.ProcessMode.PROCESS_MODE_ALWAYS
@export var test_mode_3: Node.ProcessMode

func test():
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_extended_info(property, self))

func test_no_exec():
	# GH-99309
	var sprite: Sprite3D = $Sprite3D
	sprite.axis = Vector3.AXIS_Y # No warning.
	sprite.set_axis(Vector3.AXIS_Y) # No warning.
