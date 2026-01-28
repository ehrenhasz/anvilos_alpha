print(bytes((1, 2)))
print(bytes([1, 2]))
try:
    bytes([-1])
except ValueError:
    print('ValueError')
try:
    bytes([256])
except ValueError:
    print('ValueError')
try:
    a = bytes([1, 2, 3], 1)
except TypeError:
    print('TypeError')
