class_name ImmutableTestObject

let immutable_member: float = 0.0

class InnerObject:
	let immutable_inner_member: float = 1.0

func test():
	print(immutable_member);
	print(InnerObject.new().immutable_inner_member);
