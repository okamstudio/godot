const A = preload("res://completion/class_a.notest.gd")

class LocalInnerClass:
    const AInner = preload("res://completion/class_a.notest.gd")
    class InnerInnerClass:
        const AInnerInner = preload("res://completion/class_a.notest.gd")
        enum InnerInnerInnerEnum {}
        class InnerInnerInnerClass:
            pass
    enum InnerInnerEnum {}

enum TestEnum {}

var test_var: LocalInnerClass.InnerInnerClass.➡
