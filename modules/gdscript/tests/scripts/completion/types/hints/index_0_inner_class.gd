const A = preload("res://completion/class_a.notest.gd")

class LocalInnerClass:
    const AInner = preload("res://completion/class_a.notest.gd")
    enum LocalInnerInnerEnum {}
    class LocalInnerInnerClass:
        pass
    var test_var: A➡

enum LocalInnerEnum {}
