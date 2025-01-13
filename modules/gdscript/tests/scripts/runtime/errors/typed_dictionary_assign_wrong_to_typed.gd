class Foo: pass
class Bar extends Foo: pass
class Baz extends Foo: pass

func test():
	var _typed: Dictionary[Bar, Bar] = { Baz.new() as Foo: Baz.new() as Foo }
	print('not ok')
