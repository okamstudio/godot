const A = Array()
const A1 = Array(Array())
const A2 = Array([])
const A3 = [Array()]
const A4 = [[]]

const IA = Array([1, 2, 3], TYPE_INT, &"", null)

const D = Dictionary()
const D1 = Dictionary(Dictionary())
const D2 = Dictionary({})
const D3 = {d = Dictionary()}
const D4 = {d = {}}

var a = Array()
var a1 = Array(Array())
var a2 = Array([])
var a3 = [Array()]
var a4 = [[]]

var ia = Array([1, 2, 3], TYPE_INT, &"", null)

var d = Dictionary()
var d1 = Dictionary(Dictionary())
var d2 = Dictionary({})
var d3 = {d = Dictionary()}
var d4 = {d = {}}

func test_value(value):
	@warning_ignore("unsafe_method_access")
	prints(
		value.is_read_only(),
		var_to_str(value) if value is Array else value,
	)

func test():
	test_value(A)
	test_value(A1)
	test_value(A2)
	test_value(A3)
	test_value(A4)

	test_value(IA)

	test_value(D)
	test_value(D1)
	test_value(D2)
	test_value(D3)
	test_value(D4)

	test_value(a)
	test_value(a1)
	test_value(a2)
	test_value(a3)
	test_value(a4)

	test_value(ia)

	test_value(d)
	test_value(d1)
	test_value(d2)
	test_value(d3)
	test_value(d4)
