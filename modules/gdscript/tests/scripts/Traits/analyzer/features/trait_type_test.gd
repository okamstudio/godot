trait Shearable:
	func shear() -> void:
		print("Shearable")

trait Milkable:
	func milk() -> void:
		print("Milkable")

class FluffyCow:
	uses Shearable, Milkable

class FluffyBull:
	uses Shearable

class Bird:
	pass

func test_func(node: Object):
	if node is Shearable:
		print("Shearable")
	if node is Milkable:
		print("Milkable")

func test():
	print(FluffyCow is Shearable)
	print(FluffyCow is Milkable)

	var bull = FluffyBull.new()
	print(bull is Shearable)
	print(bull is Milkable)
	test_func(bull)

	var my_animals : Array = []
	my_animals.append(FluffyCow.new())
	my_animals.append(FluffyBull.new())
	my_animals.append(Bird.new())
	var count = 1
	for animal in my_animals:
		print("Animal ", count)
		if animal is Shearable:
			animal.shear()
		if animal is Milkable:
			animal.milk()
		count += 1

	print("ok")
