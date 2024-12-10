# https://github.com/godotengine/godot/issues/61636

const External := preload("const_class_reference_external.notest.gd")

class Class1:
	class Class2:
		pass

const Class1Alias = Class1
const Class1Class2Alias = Class1.Class2

const ExternalAlias = External
const ExternalClassAlias = External.Class

func test():
	pass
