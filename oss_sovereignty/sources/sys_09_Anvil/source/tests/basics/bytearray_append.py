a = bytearray(4)
print(a)
a.append(2)
print(a)
try:
    a.append(None)
except TypeError:
    print('TypeError')
print(a)
