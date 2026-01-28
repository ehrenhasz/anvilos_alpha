b = bytearray(b"123456789")
for _ in range(4):
    c = bytearray(b)  
    b += b
print(b)
