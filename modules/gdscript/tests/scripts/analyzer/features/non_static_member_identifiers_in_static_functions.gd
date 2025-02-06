class_name NonStaticMemberIdentifiersInStaticFunctionsOuterClass

var member1 = 1
var member2 = 2
var member3 = 3
var member4 = 4
var member5 = 5


static func f(member1):
	print(member1)

	var member2 = 2
	print(member2)

	const member3 = 3
	print(member3)

	for member4 in 1:
		print(member4)

	match 1:
		var member5:
			print(member5)


class InnerClass extends NonStaticMemberIdentifiersInStaticFunctionsOuterClass:
	static func g(member1):
		print(member1)

		var member2 = 2
		print(member2)

		const member3 = 3
		print(member3)

		for member4 in 1:
			print(member4)

		match 1:
			var member5:
				print(member5)


func test():
	pass
