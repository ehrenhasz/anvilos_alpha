class Iter:
    def __iter__(self):
        return self
    def __next__(self):
        return 1
    def throw(self, x):
        print('throw', x)
        return 456
def gen():
    yield from Iter()
g = gen()
print(next(g))
g.close()
g = gen()
print(next(g))
print(g.throw(123))
g = gen()
print(next(g))
print(g.throw(ZeroDivisionError))
class Iter2:
    def __iter__(self):
        return self
    def __next__(self):
        return 1
def gen2():
    yield from Iter2()
g = gen2()
print(next(g))
try:
    g.throw(ValueError)
except:
    print('ValueError')
g = gen2()
print(next(g))
try:
    g.throw(123)
except TypeError:
    print('TypeError')
