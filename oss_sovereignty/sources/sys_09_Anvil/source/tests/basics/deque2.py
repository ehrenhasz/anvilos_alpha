try:
    from collections import deque
except ImportError:
    print("SKIP")
    raise SystemExit
d = deque([1, 2, 3], 10)
for x in d:
    print(x)
d = deque([1, 2, 3, 4, 5], 3)
print(list(d))
deque([], 10)
d.extend([6, 7])
print(list(d))
d = deque((0, 1, 2, 3), 5)
print(d[0], d[1], d[-1])
d[3] = 5
print(d[3])
try:
    d[4]
except IndexError:
    print("IndexError")
try:
    d[4] = 0
except IndexError:
    print("IndexError")
