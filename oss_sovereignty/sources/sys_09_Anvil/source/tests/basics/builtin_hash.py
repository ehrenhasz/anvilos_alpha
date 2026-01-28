print(hash(False))
print(hash(True))
print({():1}) # hash tuple
print({(1,):1}) # hash non-empty tuple
print(hash in {hash:1}) # hash function
print(type(hash(list.pop))) # hash checked function (mp_type_checked_fun)
print(type(hash([].pop))) # hash bound method
print(type(hash(object()))) # hash object instance
print(type(hash(super(object, object)))) # hash super
print(type(hash(classmethod(hash)))) # hash classmethod
print(type(hash(staticmethod(hash)))) # hash staticmethod
print(type(hash(iter("")))) # hash string iterator
print(type(hash(iter(b"")))) # hash bytes iterator
print(type(hash(iter(range(0))))) # hash range iterator
print(type(hash(map(None, [])))) # hash map object
print(type(hash(zip([])))) # hash zip object
def f(x):
    def g():
        return x
    return g
print(type(hash(f(1)))) # hash closure
try:
    hash([])
except TypeError:
    print("TypeError")
class A:
    def __hash__(self):
        return 123
    def __repr__(self):
        return "a instance"
print(hash(A()))
print({A():1})
class B:
    pass
hash(B())
class C:
    def __eq__(self, another):
        return True
try:
    hash(C())
except TypeError:
    print("TypeError")
class D:
    def __hash__(self):
        return None
try:
    hash(D())
except TypeError:
    print("TypeError")
class E:
    def __hash__(self):
        return True
print(hash(E()))
