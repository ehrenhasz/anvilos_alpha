import micropython
try:
    def stackless():
        pass
    micropython.heap_lock()
    stackless()
    micropython.heap_unlock()
except RuntimeError:
    print("SKIP")
    raise SystemExit
def f1(a):
    print(a)
def f2(a, b=2):
    print(a, b)
def f3(a, b, c, d):
    x1 = x2 = a
    x3 = x4 = b
    x5 = x6 = c
    x7 = x8 = d
    print(x1, x3, x5, x7, x2 + x4 + x6 + x8)
def f4():
    return True, b"bytes", ()
global_var = 1
def test():
    global global_var, global_exc
    global_var = 2  
    for i in range(2):  
        f1(i)  
        f1(i * 2 + 1)  
        f1(a=i)  
        f2(i)  
        f2(i, i)  
    f1((1, "two", (b"three",)))  
    f3(1, 2, 3, 4)  
    for i in 1, "two":  
        print(i)
    print(f4())  
micropython.heap_lock()
test()
micropython.heap_unlock()
