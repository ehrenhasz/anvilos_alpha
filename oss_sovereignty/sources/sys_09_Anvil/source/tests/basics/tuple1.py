x = (1, 2, 3 * 4)
print(x)
try:
    x[0] = 4
except TypeError:
    print("TypeError")
print(x)
try:
    x.append(5)
except AttributeError:
    print("AttributeError")
print(x + (10, 100, 10000))
x += (10, 11, 12)
print(x)
print(tuple(range(20)))
try:
    +()
except TypeError:
    print('TypeError')
try:
    () + None
except TypeError:
    print('TypeError')
