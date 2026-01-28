def f():
    for _ in range(2):
        print(1)
        try:
            pass
        finally:
            print(2)
            break
            print(3)
        print(4)
    print(5)
f()
def f():
    lst = [1, 2, 3]
    for x in lst:
        print('a', x)
        try:
            raise Exception
        finally:
            print(1)
            break
        print('b', x)
f()
def f():
    for i in range(2):
        print('iter', i)
        try:
            raise TypeError
        finally:
            print(1)
            try:
                raise ValueError
            finally:
                break
print(f())
def f():
    for i in range(2):
        try:
            raise ValueError
        finally:
            print(1)
            try:
                raise TypeError
            finally:
                print(2)
                try:
                    pass
                finally:
                    break
print(f())
def f():
    for i in range(2):
        try:
            raise ValueError
        finally:
            print(1)
            try:
                raise TypeError
            finally:
                print(2)
                try:
                    raise Exception
                finally:
                    break
print(f())
def f(arg):
    for _ in range(2):
        print(1)
        try:
            if arg == 1:
                raise ValueError
            elif arg == 2:
                raise TypeError
        except ValueError:
            print(2)
        else:
            print(3)
        finally:
            print(4)
            break
            print(5)
        print(6)
    print(7)
f(0) 
f(1) 
f(2) 
