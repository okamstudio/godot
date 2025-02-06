# https://github.com/godotengine/godot/issues/75658

class MyObj:
	var callable: Callable

	func run():
		callable.call()

	var prop:
		set(value):
			callable.call()
		get:
			callable.call()
			return 0

	func _on_some_signal():
		callable.call()

	func _init(p_callable: Callable):
		self.callable = p_callable

signal some_signal

var obj: MyObj

func test():
	# Call.
	obj = MyObj.new(nullify_obj)
	obj.run()
	print(obj)

	# Get.
	obj = MyObj.new(nullify_obj)
	var _aux = obj.prop
	print(obj)

	# Set.
	obj = MyObj.new(nullify_obj)
	obj.prop = 1
	print(obj)

	# Signal handling.
	obj = MyObj.new(nullify_obj)
	@warning_ignore("return_value_discarded", "accessing_protected_member")
	some_signal.connect(obj._on_some_signal)
	some_signal.emit()
	print(obj)

func nullify_obj():
	obj = null
