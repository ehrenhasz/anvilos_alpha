try:
    from collections import deque
except ImportError:
    print("SKIP")
    raise SystemExit
d = deque((), 10)
d.append(1)
d.append(2)
d.append(3)
try:
    d[0:1]
except TypeError:
    print("TypeError")
try:
    d[0:1] = (-1, -2)
except TypeError:
    print("TypeError")
try:
    del d[0:1]
except TypeError:
    print("TypeError")
