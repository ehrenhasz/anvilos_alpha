class A:
    def __init__(self, v):
        self.v = v
    def __add__(self, o):
        return A(self.v + o.v)
    def __repr__(self):
        return "A({})".format(self.v)
a = A(5)
b = a
a += A(3)
print(a)
print(b)
class L:
    def __init__(self, v):
        self.v = v
    def __add__(self, o):
        print("L.__add__")
        return L(self.v + o.v)
    def __iadd__(self, o):
        self.v += o.v
        return self
    def __repr__(self):
        return "L({})".format(self.v)
c = L([1, 2])
d = c
c += L([3, 4])
print(c)
print(d)
