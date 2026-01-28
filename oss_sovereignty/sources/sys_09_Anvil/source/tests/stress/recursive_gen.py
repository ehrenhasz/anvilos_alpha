def gen():
    yield from gen()
try:
    list(gen())
except RuntimeError:
    print("RuntimeError")
def gen2():
    for x in gen2():
        yield x
try:
    next(gen2())
except RuntimeError:
    print("RuntimeError")
