def test1():
    def method():
        pass
    class A:
        def method():
            pass
    print(hasattr(A, "method"))
    print(hasattr(A(), "method"))
test1()
def test2():
    def method():
        return "outer"
    class A:
        nonlocal method
        def method():
            return "inner"
    print(hasattr(A, "method"))
    print(hasattr(A(), "method"))
    return method()  # this is actually A.method
print(test2())
def test3(x):
    class A:
        local = x
    x += 1
    return x, A.local
print(test3(42))
def test4(global_):
    class A:
        local = global_  # fetches outer global_
        global_ = "global2"  # creates class attribute
    global_ += 1  # updates local variable
    return global_, A.local, A.global_
global_ = "global"
print(test4(42), global_)
def test5(x):
    def closure():
        return x
    class A:
        def method():
            return x, closure()
    closure = lambda: x + 1  # change it after A has been created
    return A
print(test5(42).method())
