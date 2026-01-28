def f1(a, b):
    pass
def test1():
    val = 1
    try:
        raise ValueError()
    finally:
        f1(2, 2) # use some stack
        print(val) # check that the local variable is the same
try:
    test1()
except ValueError:
    pass
def f2(a, b, c):
    pass
def test2():
    val = 1
    try:
        raise ValueError()
    finally:
        f2(2, 2, 2) # use some stack
        print(val) # check that the local variable is the same
try:
    test2()
except ValueError:
    pass
