class SomeClass:
	const CONSTANT = 1

const CONSTANT = 1

var variable = 1

# GH-75870

class Class1 extends CONSTANT:
	pass

class Class2 extends variable:
	pass

# GH-82081. `Time` is an engine singleton.
class Class3 extends Time:
	pass

class Class4 extends RefCounted.Nested:
	pass

class Class5 extends SomeClass.UnknownClass:
	pass

class Class6 extends SomeClass.CONSTANT:
	pass

class ClassA extends ClassB:
	pass
class ClassB extends ClassA:
	pass

func test():
	pass
