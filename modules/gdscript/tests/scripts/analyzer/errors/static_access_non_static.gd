# GH-83468, GH-91403

func non_static_func():
	pass

static var static_var_11 = non_static_func()

static var static_var_12:
	set(_value):
		var f := func ():
			var g := func ():
				print(non_static_func())
			g.call()
		f.call()

static var static_var_13 = func ():
	var f := func ():
		var g := func ():
			print(non_static_func())
		g.call()
	f.call()

static var static_var_21a = non_static_func
static var static_var_21b = Callable(non_static_func)

static var static_var_22:
	set(_value):
		var f := func ():
			var g := func ():
				print(non_static_func)
			g.call()
		f.call()

static var static_var_23 = func ():
	var f := func ():
		var g := func ():
			print(non_static_func)
		g.call()
	f.call()

static func static_func_11():
	non_static_func()

static func static_func_12():
	print(non_static_func)

static func static_func_21():
	var f := func ():
		var g := func ():
			non_static_func()
		g.call()
	f.call()

static func static_func_22():
	var f := func ():
		var g := func ():
			print(non_static_func)
		g.call()
	f.call()

static func static_func_31(
		f := func ():
			var g := func ():
				non_static_func()
			g.call()
):
	f.call()

static func static_func_32(
		f := func ():
			var g := func ():
				print(non_static_func)
			g.call()
):
	f.call()

func test():
	pass
