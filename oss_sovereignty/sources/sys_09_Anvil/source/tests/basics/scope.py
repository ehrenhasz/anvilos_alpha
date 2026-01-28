a = 1
def f():
    global a
    global a, a # should be able to redefine as global
    a = 2
f()
print(a)
def f():
    a = 1
    def g():
        nonlocal a
        nonlocal a, a # should be able to redefine as nonlocal
        a = 2
    g()
    return a
print(f())
def f():
    x = 1
    def g():
        def h():
            nonlocal x
            return x
        return h
    return g
print(f()()())
def f():
    x = 1
    def g():
        nonlocal x
        def h():
            return x
        return h
    return g
print(f()()())
