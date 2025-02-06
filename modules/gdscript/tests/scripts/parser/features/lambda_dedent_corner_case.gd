# https://github.com/godotengine/godot/issues/97204

func test():
	var string_array: Array[String] = ["aaa"]
	var result := (
		string_array
		. map(func(a): return a)
		. filter(
			func(string):
				return string == "aaa"
		)
	)
	assert(result is Array)
