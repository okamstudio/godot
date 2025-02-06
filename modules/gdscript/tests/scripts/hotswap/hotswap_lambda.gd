var member1 := func():
	return "Member 1"

var local1: Callable

var param1: Callable

var anon1: Callable
var anon2: Callable

func test():
	var _v1 = func(): return "Local 1"
	local1 = _v1

	print(local1.call())

	print(member1.call())

	test_parameters()

	print(param1.call())

	anon1 = func(): return "Anonymous 1"
	anon2 = func(): return "Anonymous 2"

	print(anon1.call())
	print(anon2.call())

func test_parameters(_v1 = func(): return "Param 1"):
	param1 = _v1
