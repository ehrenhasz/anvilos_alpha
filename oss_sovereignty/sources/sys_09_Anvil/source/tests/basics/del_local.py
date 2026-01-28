def f():
    x = 1
    y = 2
    print(x, y)
    del x
    print(y)
    try:
        print(x)
    except NameError:
        print("NameError");
f()
def g():
    x = 3
    y = 4
    print(x, y)
    del x
    print(y)
    try:
        del x
    except NameError:
        print("NameError");
g()
