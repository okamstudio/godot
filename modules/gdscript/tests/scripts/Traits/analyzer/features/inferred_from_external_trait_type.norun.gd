extends Node

@onready var member_var: GlobalTraitB = owner

func test():
	var _inferred_var_1 := member_var.some_var
	var _inferred_var_2 := member_var.some_func()
