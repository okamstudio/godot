class A:
	func f1(x: int, ...args: Array) -> void:
		prints(x, args)

	func f2(x: int, ...args: Array) -> void:
		prints(x, args)

class B extends A:
	func f1(x: int, y: int, ...args: Array) -> void:
		prints(x, y, args)

	func f2(x: int) -> void:
		print(x)

func g(...args: int):
	pass

func h(...args: Array[int]):
	pass

func test():
	pass
