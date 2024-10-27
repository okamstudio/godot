func get_parse_string(t: Variant):
	return t.parse_string

func test():
	var a: Callable = JSON.parse_string
	var b: Callable = get_parse_string(JSON)
	prints(a.call("{\"test\": \"a\"}"), a.is_valid())
	prints(b.call("{\"test\": \"b\"}"), b.is_valid())
