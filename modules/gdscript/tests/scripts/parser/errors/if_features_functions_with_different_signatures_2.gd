func _test():
	pass

@if_features()
func _func(a: int) -> int:
	return 1

@if_features()
func _func(a: int) -> void:
	pass
