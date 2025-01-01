uses SomeTrait, OtherTrait
uses CTrait, BTrait, ATrait

trait SomeTrait:
	func func_one():
		print("function one")

trait OtherTrait:
	func func_two():
		print("function two")

trait ATrait:
	uses BTrait

trait BTrait:
	uses CTrait

trait CTrait:
	func func_three():
		print("function three")

func test():
	func_one()
	func_two()
	func_three()
	print("ok")
