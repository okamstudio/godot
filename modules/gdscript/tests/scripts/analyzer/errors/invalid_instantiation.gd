class A extends InstancePlaceholder:
	func _init():
		pass

class B extends A:
	pass

func test():
	Time.new()
	InstancePlaceholder.new()
	B.new()
