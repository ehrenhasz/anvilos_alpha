def f():
    return 1
print(__name__, f())
def g():
    yield 2
print(__name__, next(g()))
