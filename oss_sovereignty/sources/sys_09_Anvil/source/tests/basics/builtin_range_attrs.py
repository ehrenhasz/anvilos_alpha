try:
    range(0).start
except AttributeError:
    print("SKIP")
    raise SystemExit
print(range(1, 2, 3).start)
print(range(1, 2, 3).stop)
print(range(1, 2, 3).step)
try:
    range(4).start = 0
except AttributeError:
    print('AttributeError')
