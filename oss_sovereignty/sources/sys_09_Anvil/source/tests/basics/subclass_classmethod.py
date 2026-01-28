class Base:
    @classmethod
    def foo(cls):
        print(cls.__name__)
try:
    Base.__name__
except AttributeError:
    print("SKIP")
    raise SystemExit
class Sub(Base):
    pass
Sub.foo()
class A(object):
    foo = 0
    @classmethod
    def bar(cls):
        print(cls.foo)
    def baz(self):
        print(self.foo)
class B(A):
    foo = 1
B.bar() 
B().bar() 
B().baz() 
