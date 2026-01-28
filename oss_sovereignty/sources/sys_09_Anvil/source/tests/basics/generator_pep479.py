def gen():
    yield 1
    raise StopIteration
g = gen()
print(next(g))
try:
    next(g)
except RuntimeError:
    print('RuntimeError')
try:
    next(g)
except StopIteration:
    print('StopIteration')
def gen():
    yield 1
    yield 2
g = gen()
print(next(g))
try:
    g.throw(StopIteration)
except RuntimeError:
    print('RuntimeError')
def gen():
    yield from range(2)
    print('should not get here')
g = gen()
print(next(g))
try:
    g.throw(StopIteration)
except RuntimeError:
    print('RuntimeError')
