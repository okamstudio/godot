var _prop: int

# Inline setter assigns `String` to `int`.
# Inline getter returns `int` instead of `String`.
var prop_1: String:
	set(value):
		_prop = value
	get:
		return _prop

# Setter function has wrong argument type.
# Getter function has wrong return type.
var prop_2: String:
	set = set_prop_2, get = get_prop_2

# Inline setter parameter uses property type.
var prop_3 := 0:
	set(value):
		var x: String = value
		prop_3 = value

func set_prop_2(value: int):
	_prop = value

func get_prop_2():
	return _prop

func test():
	pass
