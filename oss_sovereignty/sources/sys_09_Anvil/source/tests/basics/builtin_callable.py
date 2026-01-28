print(callable(None))
print(callable(1))
print(callable([]))
print(callable("dfsd"))
import sys
print(callable(sys))
print(callable(callable))
print(callable(lambda:None))
def f():
    pass
print(callable(f))
class A:
    pass
print(callable(A))
print(callable(A()))
class B:
    def __call__(self):
        pass
print(callable(B()))
class C:
    def f(self):
        return "A.f"
class D:
    g = C() # g is a value and is not callable
print(callable(D().g))
print(D().g.f())
