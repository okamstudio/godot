# In 3.x there are warnings `CONSTANT_USED_AS_FUNCTION`, `FUNCTION_USED_AS_PROPERTY`, and `PROPERTY_USED_AS_FUNCTION`.
# In 4.x they are deprecated because either an error is produced or a `Callable` is returned.

const CONSTANT = 25

var property = 25

func function():
	pass

func test():
	function = 25
	CONSTANT(123)
	property()
