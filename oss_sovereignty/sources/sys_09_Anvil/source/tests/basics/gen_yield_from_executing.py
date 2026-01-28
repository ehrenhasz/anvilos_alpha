def f():
    yield 1
    yield from g
g = f()
print(next(g))
try:
    next(g)
except ValueError:
    print('ValueError')
