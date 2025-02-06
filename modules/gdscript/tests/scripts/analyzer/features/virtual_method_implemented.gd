class BaseClass:
	func _get_property_list():
		return {"property" : "definition"}

class SuperClassMethodsRecognized extends BaseClass:
	func _init():
		# Recognizes super class methods.
		@warning_ignore("calling_protected_method")
		var _x = _get_property_list()

class SuperMethodsRecognized extends BaseClass:
	func _get_property_list():
		# Recognizes super method.
		var result = super()
		result["new"] = "new"
		return result

func test():
	var test1 = SuperClassMethodsRecognized.new()
	@warning_ignore("calling_protected_method")
	print(test1._get_property_list()) # Calls base class's method.
	var test2 = SuperMethodsRecognized.new()
	@warning_ignore("calling_protected_method")
	print(test2._get_property_list())
