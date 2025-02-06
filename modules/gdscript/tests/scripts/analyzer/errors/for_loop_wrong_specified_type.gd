func test():
	var a: Array[Resource] = []
	for node: Node in a:
		print(node)

	# GH-82021
	for x: String in [1, 2, 3]:
		print(x)

	for key: int in { "a": 1 }:
		print(key)
