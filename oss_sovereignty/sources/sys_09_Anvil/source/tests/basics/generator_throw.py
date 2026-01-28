def gen():
    yield 123
    yield 456
g = gen()
print(next(g))
try:
    g.throw(KeyError)
except KeyError:
    print('got KeyError from downstream!')
def gen():
    try:
        yield 1
        yield 2
    except:
        pass
g = gen()
print(next(g))
try:
    g.throw(ValueError)
except StopIteration:
    print('got StopIteration')
def gen():
    try:
        yield 123
    except GeneratorExit as e:
        print('GeneratorExit', repr(e.args))
    yield 456
g = gen()
print(next(g))
print(g.throw(GeneratorExit))
g = gen()
print(next(g))
print(g.throw(GeneratorExit()))
