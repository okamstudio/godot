# https://github.com/godotengine/godot/issues/88885

extends Node

var member: int

signal s(int)

func _first(arg: int) -> void:
	print("In first: ", arg)
	member = 1

func _second(arg: int) -> void:
	print("In second: ", arg)

@warning_ignore_start("return_value_discarded")
func test():
	member = 0
	s.connect(_first)
	s.connect(_second)
	s.emit(member)
	member = 0
	emit_signal(&"s", member)
