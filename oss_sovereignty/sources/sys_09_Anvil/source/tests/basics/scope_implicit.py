def f():
    def g():
        return x # implicit nonlocal
    x = 3 # variable defined after function that closes over it
    return g
print(f()())
def f():
    def g():
        def h():
            return x # implicit nonlocal
        return h
    x = 4 # variable defined after function that closes over it
    return g
print(f()()())
def f():
    x = 0
    def g():
        x # local because next statement assigns to it
        x = 1
    g()
try:
    f()
except NameError:
    print('NameError')
