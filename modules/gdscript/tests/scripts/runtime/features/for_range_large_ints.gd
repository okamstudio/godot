# GH-83293

func test():
	for x in range(1 << 31, (1 << 31) + 3):
		print(x)
	for x in range(1 << 62, (1 << 62) + 3):
		print(x)
