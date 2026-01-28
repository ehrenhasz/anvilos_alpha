@micropython.viper
def f():
    try:
        fail
    finally:
        print("finally")
try:
    f()
except NameError:
    print("NameError")
@micropython.viper
def f():
    try:
        try:
            fail
        finally:
            print("finally")
    except NameError:
        print("NameError")
f()
@micropython.viper
def f():
    a = 100
    try:
        print(a)
        a = 200
        fail
    except NameError:
        print(a)
        a = 300
    print(a)
f()
