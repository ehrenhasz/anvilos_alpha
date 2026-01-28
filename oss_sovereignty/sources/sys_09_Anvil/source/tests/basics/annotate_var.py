x: int
print("x" in globals())
x: int = 1
print(x)
t: tuple = 1, 2
print(t)
def f():
    x: int
    try:
        print(x)
    except NameError:
        print("NameError")
f()
def f():
    x.y: int
    print(x)
f()
