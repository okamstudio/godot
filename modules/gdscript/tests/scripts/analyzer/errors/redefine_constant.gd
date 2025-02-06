# See also `parser-warnings/shadowed-constant.gd`.

const CLASS_CONSTANT = 25

func test():
	const LOCAL_CONSTANT = 25

	CLASS_CONSTANT = 50
	LOCAL_CONSTANT = 50
