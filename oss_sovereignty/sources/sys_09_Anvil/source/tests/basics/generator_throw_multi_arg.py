def gen():
    try:
        yield 123
    except GeneratorExit as e:
        print("GeneratorExit", repr(e.args))
    yield 456
g = gen()
print(next(g))
print(g.throw(GeneratorExit(), None))
g = gen()
print(next(g))
print(g.throw(GeneratorExit, GeneratorExit(123)))
