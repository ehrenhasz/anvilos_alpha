print('__name__' in dir())
import sys
print('version' in dir(sys))
print('append' in dir(list))
class Foo:
    def __init__(self):
        self.x = 1
foo = Foo()
print('__init__' in dir(foo))
print('x' in dir(foo))
class A:
    def a():
        pass
class B(A):
    def b():
        pass
d = dir(B())
print(d.count('a'), d.count('b'))
class C(A):
    def c():
        pass
class D(B, C):
    def d():
        pass
d = dir(D())
print(d.count('a'), d.count('b'), d.count('c'), d.count('d'))
