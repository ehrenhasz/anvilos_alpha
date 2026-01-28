b = bytearray(2)
b[0] = 1
b[1] = 2
print(b + bytearray(2))
b += bytearray(3)
print(b)
b.extend(bytearray(4))
print(b)
b = bytearray()
b += b""
b = bytearray(b"abcdefgh")
for _ in range(4):
    c = bytearray(b)  
    b.extend(b)
print(b)
