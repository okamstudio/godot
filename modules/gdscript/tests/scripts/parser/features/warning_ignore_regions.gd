@warning_ignore_start("unreachable_code", "narrowing_conversion")

var _a = 1
@warning_ignore_start("unused_protected_class_variable")
var _b = 2
var _c = 3
@warning_ignore_restore("unused_protected_class_variable")
var _d = 4

var __e = 5
@warning_ignore_start("unused_private_class_variable")
var __f = 6
var __g = 7
@warning_ignore_restore("unused_private_class_variable")
var __h = 8

func test():
	return

	var a = 1
	@warning_ignore_start("unused_variable")
	var b = 2
	var c = 3
	@warning_ignore_restore("unused_variable")
	var d = 4

	var _x: int = 1.0
	@warning_ignore_restore("narrowing_conversion")
	var _y: int = 1.0

func test_2():
	return
	print(42)
