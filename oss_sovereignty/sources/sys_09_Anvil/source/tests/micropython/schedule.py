import micropython
try:
    micropython.schedule
except AttributeError:
    print("SKIP")
    raise SystemExit
def callback(arg):
    global done
    print(arg)
    done = True
done = False
micropython.schedule(callback, 1)
while not done:
    pass
def callback_inner(arg):
    global done
    print("inner")
    done += 1
def callback_outer(arg):
    global done
    micropython.schedule(callback_inner, 0)
    for i in range(2):
        pass
    print("outer")
    done += 1
done = 0
micropython.schedule(callback_outer, 0)
while done != 2:
    pass
def callback(arg):
    global done
    try:
        for i in range(100):
            micropython.schedule(lambda x: x, None)
    except RuntimeError:
        print("RuntimeError")
    done = True
done = False
micropython.schedule(callback, None)
while not done:
    pass
