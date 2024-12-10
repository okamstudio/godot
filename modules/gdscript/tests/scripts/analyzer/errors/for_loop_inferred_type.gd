class Iterator:
	func _iter_init(_count):
		return true
	func _iter_next(_count):
		return false
	func _iter_get(_count) -> StringName:
		return &"custom"

enum { ENUM_VALUE = 1 }

func test():
	# Literals.

	for x in true:
		pass

	for x in 1:
		if x is String:
			pass

	# Constants.

	const CONSTANT_INT = 1
	for x in CONSTANT_INT:
		if x is String:
			pass

	const CONSTANT_FLOAT = 1.0
	for x in CONSTANT_FLOAT:
		if x is String:
			pass

	# Hard types.

	var hard_int := 1
	for x in hard_int:
		if x is String:
			pass

	var hard_float := 1.0
	for x in hard_float:
		if x is String:
			pass

	var hard_string := "a"
	for x in hard_string:
		if x is int:
			pass

	# Other.

	for x in ENUM_VALUE:
		if x is String:
			pass

	var hard_iterator := Iterator.new()
	for x in hard_iterator:
		if x is int:
			pass
