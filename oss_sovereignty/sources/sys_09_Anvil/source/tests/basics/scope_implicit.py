def f():
    def g():
        return x 
    x = 3 
    return g
print(f()())
def f():
    def g():
        def h():
            return x 
        return h
    x = 4 
    return g
print(f()()())
def f():
    x = 0
    def g():
        x 
        x = 1
    g()
try:
    f()
except NameError:
    print('NameError')
