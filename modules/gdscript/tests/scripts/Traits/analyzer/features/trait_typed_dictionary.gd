trait SomeTrait:
	pass

class SomeClass:
	uses SomeTrait

func test():
	var key_classes: Dictionary[SomeTrait, String] = {SomeClass.new() : "A"}
	key_classes[SomeClass.new()] = "B"
	var count = 1
	for trait_class in key_classes:
		print(count)
		count += 1
	print("key ok")
	var value_classes: Dictionary[String, SomeTrait] = {"A" : SomeClass.new()}
	value_classes["B"] = SomeClass.new()
	count = 1
	for trait_class in value_classes:
		print(count)
		count += 1
	print("value ok")
