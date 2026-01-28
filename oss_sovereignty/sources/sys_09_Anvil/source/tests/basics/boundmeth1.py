print(type(repr([].append)))
class A:
    def f(self):
        return 0
    def g(self, a):
        return a
    def h(self, a, b, c, d, e, f):
        return a + b + c + d + e + f
m = A().f
print(m())
m = A().g
print(m(1))
m = A().h
print(m(1, 2, 3, 4, 5, 6))
try:
    A().f.x = 1
except AttributeError:
    print("AttributeError")
print("--------")
a = A()
m1 = a.f
m2 = a.f
print(m1 == a.f)  # should result in True
print(m2 == a.f)  # should result in True
print(m1 == m2)  # should result in True
print(m1 != a.f)  # should result in False
a1 = A()
a2 = A()
m1 = a1.f
m2 = a2.f
print(m1 == a2.f)  # should result in False
print(m2 == a1.f)  # should result in False
print(m1 != a2.f)  # should result in True
print(A().f == None)  # should result in False
print(A().f != None)  # should result in True
print(None == A().f)  # should result in False
print(None != A().f)  # should result in True
print("--------")
a = A()
m1 = a.f
m2 = a.f
print(hash(m1) == hash(a.f))  # should result in True
print(hash(m2) == hash(a.f))  # should result in True
print(hash(m1) == hash(m2))  # should result in True
print(hash(m1) != hash(a.g))  # should result in True
a2 = A()
m2 = a2.f
print(hash(m1) == hash(a2.f))  # should result in False
