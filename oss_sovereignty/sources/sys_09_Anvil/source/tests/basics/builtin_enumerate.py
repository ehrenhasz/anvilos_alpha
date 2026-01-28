try:
    enumerate
except:
    print("SKIP")
    raise SystemExit
print(list(enumerate([])))
print(list(enumerate([1, 2, 3])))
print(list(enumerate([1, 2, 3], 5)))
print(list(enumerate([1, 2, 3], -5)))
print(list(enumerate(range(100))))
print(list(enumerate([1, 2, 3], start=1)))
print(list(enumerate(iterable=[1, 2, 3])))
print(list(enumerate(iterable=[1, 2, 3], start=1)))
try:
    enumerate([], 1, 2)
except TypeError:
    pass
