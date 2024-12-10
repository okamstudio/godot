extends Node

@export_enum("A", "B", "C") var test_1
@export_enum("A", "B", "C",) var test_2

@export_enum(
	"A",
	"B",
	"C"
) var test_3

@export_enum(
	"A",
	"B",
	"C",
) var test_4

@export
var test_5: int

@export()
var test_6: int

@export() var test_7: int = 42
@warning_ignore("onready_with_export") @onready @export var test_8: int = 42
@warning_ignore("onready_with_export") @onready() @export() var test_9: int = 42

@warning_ignore("onready_with_export")
@onready
@export
var test_10: int = 42

@warning_ignore("onready_with_export")
@onready()
@export()
var test_11: int = 42

@warning_ignore("onready_with_export")
@onready()
@export()

var test_12: int = 42

func test():
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_extended_info(property, self))
