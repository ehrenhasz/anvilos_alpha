b = bytearray(b"123456789")
for _ in range(4):
    c = bytearray(b)  # extra allocation increases chance 'b' has to relocate
    b += b
print(b)
