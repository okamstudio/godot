func test():
	_implementation()
	_various()
	var inner = InnerClass.new()
	inner._implementation()
	inner._various()

# ---------------------------------------------

# - First match after default prevails.
# - One default can't override another default.
# - Non-fitting are ignored.

@if_features()
func _implementation():
	print("default")

@if_features("non_existent_feature")
func _implementation():
	print("nope")

@if_features("test_feature_1")
func _implementation():
	print("test 1")

@if_features("test_feature_2")
func _implementation():
	print("test 2")

@if_features()
func _implementation():
	print("default 2")

# ---------------------------------------------

# - Non-default match can't be overridden.
# - Non-fitting are ignored.

@if_features("non_existent_feature")
func _various():
	print("other nope")

@if_features("test_feature_1", "test_feature_2")
func _various():
	print("other 1")

@if_features("test_feature_1", "test_feature_2")
func _various():
	print("other 1 again")

@if_features()
func _various():
	print("other default")

class InnerClass:
	# - First match after default prevails.
	# - One default can't override another default.
	# - Non-fitting are ignored.

	@if_features()
	func _implementation():
		print("inner default")

	@if_features("non_existent_feature")
	func _implementation():
		print("inner nope")

	@if_features("test_feature_1")
	func _implementation():
		print("inner test 1")

	@if_features("test_feature_2")
	func _implementation():
		print("inner test 2")

	@if_features()
	func _implementation():
		print("inner default 2")

	# ---------------------------------------------

	# - Non-default match can't be overridden.
	# - Non-fitting are ignored.

	@if_features("non_existent_feature")
	func _various():
		print("inner other nope")

	@if_features("test_feature_1", "test_feature_2")
	func _various():
		print("inner other 1")

	@if_features("test_feature_1", "test_feature_2")
	func _various():
		print("inner other 1 again")

	@if_features()
	func _various():
		print("inner other default")
