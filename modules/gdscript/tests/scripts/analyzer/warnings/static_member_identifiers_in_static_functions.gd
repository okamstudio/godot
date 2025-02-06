class_name StaticMemberIdentifiersInStaticFunctionsOuterClass

static var static1 = 1
static var static2 = 2
static var static3 = 3
static var static4 = 4
static var static5 = 5


static func f(static1):
	print(static1)

	var static2 = 2
	print(static2)

	const static3 = 3
	print(static3)

	for static4 in 1:
		print(static4)

	match 1:
		var static5:
			print(static5)


class InnerClass extends StaticMemberIdentifiersInStaticFunctionsOuterClass:
	static func g(static1):
		print(static1)

		var static2 = 2
		print(static2)

		const static3 = 3
		print(static3)

		for static4 in 1:
			print(static4)

		match 1:
			var static5:
				print(static5)


func test():
	pass
