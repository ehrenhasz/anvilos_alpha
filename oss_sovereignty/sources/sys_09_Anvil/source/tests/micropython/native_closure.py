@micropython.native
def f():
    x = 1
    @micropython.native
    def g():
        nonlocal x
        return x
    return g
print(f()())
@micropython.native
def f(x):
    @micropython.native
    def g():
        nonlocal x
        return x
    return g
print(f(2)())
@micropython.native
def f(x):
    y = 2 * x
    @micropython.native
    def g(z):
        return x + y + z
    return g
print(f(2)(3))
