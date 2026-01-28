class L(list):
    def __init__(self, a, b):
        super().__init__([a, b])
print(L(2, 3))
class A:
    def __init__(self):
        print("A.__init__")
        super().__init__()
A()
class B(object):
    def __init__(self):
        print("B.__init__")
        super().__init__()
B()
class C:
    pass
class D(C, object):
    def __init__(self):
        print('D.__init__')
        super().__init__()
    def reinit(self):
        print('D.foo')
        super().__init__()
a = D()
a.__init__()
a.reinit()
class L(list):
    def reinit(self):
        super().__init__(range(2))
a = L(range(5))
print(a)
a.reinit()
print(a)
