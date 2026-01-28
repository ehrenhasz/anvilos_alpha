import micropython
try:
    name()
except NameError as e:
    print(type(e).__name__, e)
try:
    raise 0
except TypeError as e:
    print(type(e).__name__, e)
try:
    name()
except NameError as e:
    print(e.args[0])
try:
    raise 0
except TypeError as e:
    print(e.args[0])
micropython.heap_lock()
try:
    name()
except NameError as e:
    print(type(e).__name__)
try:
    raise 0
except TypeError as e:
    print(type(e).__name__)
micropython.heap_unlock()
