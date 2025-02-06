enum NamedEnum0 { VALUE = NamedEnum0.VALUE }
enum NamedEnum1 { VALUE = NamedEnum2.VALUE }
enum NamedEnum2 { VALUE = NamedEnum1.VALUE }

enum { ENUM_VALUE_0 = ENUM_VALUE_0 }
enum { ENUM_VALUE_1 = ENUM_VALUE_2 }
enum { ENUM_VALUE_2 = ENUM_VALUE_1 }

const CONST_0 = CONST_0
const CONST_1 = CONST_2
const CONST_2 = CONST_1

var var_0 = var_0
var var_1 := var_2
var var_2 := var_1

static func func_0(p := func_0()) -> int:
	return 0
static func func_1(p := func_2()) -> int:
	return 1
static func func_2(p := func_1()) -> int:
	return 2

var lambda_body = (func (_p): return 0).call(lambda_body_ref)
var lambda_body_ref = lambda_body

var lambda_param = func (_p = lambda_param_ref): return 0
var lambda_param_ref = lambda_param

const External = preload("cyclic_reference.notest.gd")
var member = External.member

class InnerA:
	func f(p := InnerB.new().f()) -> int:
		return 1
class InnerB extends InnerA:
	func f(p := 1) -> int:
		return super.f()

func test():
	pass
