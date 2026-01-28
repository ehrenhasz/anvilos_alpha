class my_bytes(bytes):
    pass
b1 = my_bytes([0, 1])
b2 = my_bytes([2, 3])
b3 = bytes([4, 5])
print(b1 + b2)
print(b1 + b3)
print(b3 + b1)
print(bytes(b1))
