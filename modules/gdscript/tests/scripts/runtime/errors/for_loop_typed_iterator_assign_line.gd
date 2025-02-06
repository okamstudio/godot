# GH-92681

func test():
	var a = [1, 2, "3"]
	for x: int in a: # The error should be here.
		print(x) # Not here.
