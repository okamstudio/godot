var member_untyped = 1
var member_inferred := member_untyped

func check(param_untyped = 1, param_inferred := param_untyped):
	pass

func test():
	var local_untyped = 1
	var local_inferred := local_untyped
