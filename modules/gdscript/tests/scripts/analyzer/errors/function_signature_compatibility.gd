class A:
	func f1(_p1: int) -> int: return 0
	func f2(_p1: int) -> int: return 0
	func f3(_p1: int = 0) -> int: return 0
	func f4(_p1: int) -> int: return 0
	func f5() -> int: return 0

	func g1(_p: Object): pass
	func g2(_p: Variant): pass # No `is_type_compatible()` misuse.
	func g3(_p: int): pass # No implicit conversion.

	func h1() -> Node: return null
	func h2() -> Node: return null # No `is_type_compatible()` misuse.
	func h3() -> Node: return null # No `is_type_compatible()` misuse.
	func h4() -> float: return 0.0 # No implicit conversion.

class B extends A:
	func f1() -> int: return 0
	func f2(_p1: int, _p2: int) -> int: return 0
	func f3(_p1: int) -> int: return 0
	func f4(_p1: Vector2) -> int: return 0
	func f5() -> Vector2: return Vector2()

	func g1(_p: Node): pass
	func g2(_p: Node): pass
	func g3(_p: float): pass

	func h1() -> Object: return null
	func h2() -> Variant: return null
	func h3() -> void: return
	func h4() -> int: return 0

func test():
	pass
