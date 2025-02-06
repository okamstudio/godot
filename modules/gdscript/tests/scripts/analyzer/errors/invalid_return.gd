static func _static_init() -> int:
	pass

func _init() -> int:
	pass

func f() -> void:
	return null

func g() -> void:
	var a
	a = 1
	return a

func test():
	var lambda_1 := func() -> int:
		print("no return")
	lambda_1.call()

	var lambda_2 := func() -> int:
		return "string"
	lambda_2.call()
