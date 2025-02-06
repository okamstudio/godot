const ARRAY := [{}]
const DICTIONARY := { 0: [0] }

func subtest_constant_array():
	var dictionary := ARRAY[0]
	var key := 0
	dictionary[key] = 0
	print(ARRAY)

func subtest_constant_dictionary():
	var array := DICTIONARY[0]
	var key := 0
	array[key] = 0
	print(DICTIONARY)

func subtest_readonly_array():
	var array := [0]
	array.make_read_only()
	array[0] = 1
	print(array)

func subtest_readonly_dictionary():
	var dictionary := { "a": 0 }
	dictionary.make_read_only()
	dictionary.a = 1
	print(dictionary)

func test():
	subtest_constant_array()
	subtest_constant_dictionary()
	subtest_readonly_array()
	subtest_readonly_dictionary()
