class A:
    @warning_ignore("unused_protected_class_variable")
    static var _static_a = null
    @warning_ignore("unused_private_class_variable")
    static var __static_b = null

    @warning_ignore("unused_protected_class_variable")
    var _a = null
    @warning_ignore("unused_private_class_variable")
    var __b = null

    func _call_a():
        pass

    func __call_b():
        pass

    func _call_a_ret():
        return null

    func __call_b_ret():
        return null

    static func _static_call_a():
        pass

    static func __static_call_b():
        pass

    static func _static_call_a_ret():
        return null

    static func __static_call_b_ret():
        return null

class B:
    func _init():
        var cls_a = A.new()
        A._static_a = null
        A.__static_b = null
        cls_a._a = null
        cls_a.__b = null
        cls_a._call_a()
        cls_a.__call_b()

        @warning_ignore("unused_variable")
        var t1 = cls_a._call_a_ret()
        @warning_ignore("unused_variable")
        var t2 = cls_a.__call_b_ret()

        A._static_call_a()
        A.__static_call_b()
        @warning_ignore("unused_variable")
        var t3 = A._static_call_a_ret()
        @warning_ignore("unused_variable")
        var t4 = A.__static_call_b_ret()

class C extends A:
    func _init():
        A._static_a = null
        A.__static_b = null
        _a = null
        __b = null
        _call_a()
        __call_b()

        @warning_ignore("unused_variable")
        var t1 = _call_a_ret()
        @warning_ignore("unused_variable")
        var t2 = __call_b_ret()

        _static_call_a()
        __static_call_b()
        @warning_ignore("unused_variable")
        var t3 = _static_call_a_ret()
        @warning_ignore("unused_variable")
        var t4 = __static_call_b_ret()

func test():
    pass
