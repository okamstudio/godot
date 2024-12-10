# GH-73843
@export_group("Resource")

# GH-78252
@export var test_1: int
@export_category("test_1")
@export var test_2: int

func test():
	var resource := Resource.new()
	prints("Not shadowed:", resource.get_class())

	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_extended_info(property, self))
