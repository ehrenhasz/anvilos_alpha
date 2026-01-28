if range(1) != range(1):
    print("SKIP")
    raise SystemExit
print(range(1) == range(1))
print(range(1) != range(1))
print(range(1) != range(2))
print(range(0) == range(0))
print(range(1, 0) == range(0))
print(range(1, 4, -1) == range(6, 3))
print(range(1, 4, 10) == range(1, 4, 10))
print(range(1, 4, 10) == range(1, 4, 20))
print(range(1, 4, 10) == range(1, 8, 20))
print(range(0, 3, 2) == range(0, 3, 2))
print(range(0, 3, 2) == range(0, 4, 2))
print(range(0, 3, 2) == range(0, 5, 2))
try:
    range(1) + 10
except TypeError:
    print('TypeError')
