for x in range(*(1, 3)):
    print(x)
for x in range(1, *(6, 2)):
    print(x)
try:
    for x in range(1, 2, 0):
        pass
except ValueError:
    print('ValueError')
try:
    for x in range(**{'end':1}):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, **{'end':1}):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, 1, **{'step':1}):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(end=1):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, end=1):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(start=0, end=1):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, 1, step=1):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0 for i in []):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, (0 for i in [])):
        print(x)
except TypeError:
    print('TypeError')
try:
    for x in range(0, 1, (0 for i in [])):
        print(x)
except TypeError:
    print('TypeError')
