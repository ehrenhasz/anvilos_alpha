print('0' * 5)
for i in (-4, -2, 0, 2, 4):
    print(i * '12')
    print('12' * i)
a = '123'
c = a * 3
print(a, c)
a = '456'
a *= 3
print(a)
a = 3
a *= '789'
print(a)
