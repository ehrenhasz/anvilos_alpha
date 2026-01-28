print(0)
print(1)
print(-1)
print(63)
print(64)
print(65)
print(-63)
print(-64)
print(-65)
print(1073741823)
print(-1073741823)
print(1 + 3)
print(3 - 2)
print(2 * 3)
print(1 & 3)
print(1 | 2)
print(1 ^ 3)
print(+3)
print(-3)
print(~3)
a = 0x3fffff
print(a)
a *= 0x10
print(a)
a *= 0x10
print(a)
a += 0xff
print(a)
a = -0x3fffff
print(a)
a *= 0x10
print(a)
a *= 0x10
print(a)
a -= 0xff
print(a)
a -= 1
print(a)
try:
    a << -1
except ValueError:
    print("ValueError")
try:
    a >> -1
except ValueError:
    print("ValueError")
print(1 >> 32)
print(1 >> 64)
print(1 >> 128)
a = 1
print(a >> 32)
print(a >> 64)
print(a >> 128)
