# https://github.com/godotengine/godot/issues/50285

@warning_ignore_start("unused_local_constant")
func test():
	const CONST_INNER_DICTIONARY = { "key": true }
	const CONST_NESTED_DICTIONARY_OLD_WORKAROUND = {
		"key1": "value1",
		"key2": CONST_INNER_DICTIONARY
	}
	# All of these should be valid
	const CONST_NESTED_DICTIONARY = {
		"key1": "value1",
		"key2": { "key": true }
	}


	const CONST_DICTIONARY_WITH_ARRAY = {
		"key1": [1,2,3,4]
	}

	const CONST_NESTED_ARRAY = [[],[2],[1,2,3]]
	const CONST_ARRAY_WITH_DICT = [{"key1": 3}, {"key2": 5}]

	const THREE_DIMENSIONAL_ARRAY = [[[],[],[]],[[],[],[]],[[],[],[]]]
	const MANY_NESTED_DICT = {
		"key1": {
			"key11": {
				"key111": {},
				"key112": {},
			},
			"key12": {
				"key121": {},
				"key122": {},
			},
		},
		"key2": {
			"key21": {
				"key211": {},
				"key212": {},
			},
			"key22": {
				"key221": {},
				"key222": {},
			},
		}
	}


	const CONST_ARRAY_ACCESS = [1,2,3][0]
	const CONST_DICT_ACCESS = {"key1": 5}["key1"]

	const CONST_ARRAY_NESTED_ACCESS = [[1,2,3],[4,5,6],[8,9,10]][0][1]
	const CONST_DICT_NESTED_ACCESS = {"key1": {"key2": 1}}["key1"]["key2"]

	print(CONST_ARRAY_ACCESS)
	print(CONST_DICT_ACCESS)
	print(CONST_ARRAY_NESTED_ACCESS)
	print(CONST_DICT_NESTED_ACCESS)
