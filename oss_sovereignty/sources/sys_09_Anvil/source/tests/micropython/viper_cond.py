@micropython.viper
def f():
    x = False
    if x:
        pass
    else:
        print("not x", x)
f()
@micropython.viper
def f():
    x = True
    if x:
        print("x", x)
f()
@micropython.viper
def g():
    y = 1
    if y:
        print("y", y)
g()
@micropython.viper
def h():
    z = 0x10000
    if z:
        print("z", z)
h()
