if not hasattr(object, '__bases__'):
    print("SKIP")
    raise SystemExit
class A:
    pass
class B(object):
    pass
class C(B):
    pass
class D(C, A):
    pass
print(hasattr(A, '__bases__'))
print(hasattr(B, '__bases__'))
print(hasattr(C, '__bases__'))
print(hasattr(D, '__bases__'))
print(type(A.__bases__) == tuple)
print(type(B.__bases__) == tuple)
print(type(C.__bases__) == tuple)
print(type(D.__bases__) == tuple)
print(len(A.__bases__) == 1)
print(len(B.__bases__) == 1)
print(len(C.__bases__) == 1)
print(len(D.__bases__) == 2)
print(A.__bases__[0] == object)
print(B.__bases__[0] == object)
print(C.__bases__[0] == B)
print(D.__bases__[0] == C)
print(D.__bases__[1] == A)
print(object.__bases__ == tuple())
