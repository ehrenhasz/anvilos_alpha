@micropython.native
def native_test(x):
    print(1, [], x)
native_test(2)
import gc
gc.collect()
native_test(3)
@micropython.native
def f(a, b):
    print(a + b)
f(1, 2)
@micropython.native
def f(a, b, c):
    print(a + b + c)
f(1, 2, 3)
@micropython.native
def f(a):
    print(not a)
f(False)
f(True)
@micropython.native
def f(a):
    print(1, 2, 3, 4 if a else 5)
f(False)
f(True)
