print([0] * 5)
for i in (-4, -2, 0, 2, 4):
    print(i * [1, 2])
    print([1, 2] * i)
a = [1, 2, 3]
c = a * 3
print(a, c)
a = [4, 5, 6]
a *= 3
print(a)
a = 3
a *= [7, 8, 9]
print(a)
try:
    [] * None
except TypeError:
    print('TypeError')
