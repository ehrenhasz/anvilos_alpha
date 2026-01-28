def gen():
    return 1
    yield
f = gen()
def run():
    print((yield from f))
    print((yield from f))
    print((yield from f))
try:
    next(run())
except StopIteration:
    print("StopIteration")
def run():
    print((yield from f))
f = zip()
try:
    next(run())
except StopIteration:
    print("StopIteration")
