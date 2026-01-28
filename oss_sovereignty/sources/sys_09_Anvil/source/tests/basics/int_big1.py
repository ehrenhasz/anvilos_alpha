x = 1000000000000000000000000000000
xn = -1000000000000000000000000000000
y = 2000000000000000000000000000000
print(x)
print(y)
print('%
print('{:
print(int(x))
print(x + 1)
print(x + y)
print(x + xn == 0)
print(bool(x + xn))
print(x - 1)
print(x - y)
print(y - x)
print(x - x == 0)
print(bool(x - x))
print(x * 2)
print(x * y)
print(x // 2)
print(y // x)
print(~x)
print(~(-x))
x = 0x10000000000000000000000
for i in range(32):
    x = x << 1
    print(x)
x = 0x10000000000000000000000
for i in range(32):
    x = x >> 1
    print(x)
for i in range(8):
    print(-10000000000000000000000000 << i)
    print(-10000000000000000000000001 << i)
    print(-10000000000000000000000002 << i)
    print(-10000000000000000000000003 << i)
    print(-10000000000000000000000004 << i)
for i in range(8):
    print(-10000000000000000000000000 >> i)
    print(-10000000000000000000000001 >> i)
    print(-10000000000000000000000002 >> i)
    print(-10000000000000000000000003 >> i)
    print(-10000000000000000000000004 >> i)
print(int("123456789012345678901234567890"))
print(int("-123456789012345678901234567890"))
print(int("123456789012345678901234567890abcdef", 16))
print(int("123456789012345678901234567890ABCDEF", 16))
print(int("1234567890abcdefghijklmnopqrstuvwxyz", 36))
try:
    print(int("123456789012345678901234567890abcdef"))
except ValueError:
    print('ValueError');
try:
    print(int("123456789012345678901234567890\x01"))
except ValueError:
    print('ValueError');
x = 0x84ce72aa8699df436059f052ac51b6398d2511e49631bcb7e71f89c499b9ee425dfbc13a5f6d408471b054f2655617cbbaf7937b7c80cd8865cf02c8487d30d2b0fbd8b2c4e102e16d828374bbc47b93852f212d5043c3ea720f086178ff798cc4f63f787b9c2e419efa033e7644ea7936f54462dc21a6c4580725f7f0e7d1aaaaaaa
print(x)
x = 1073741823 
x = -1073741823 
x = 1073741824 
x = -1073741824 
print(int('0x3fffffffffff', 16)) 
print(int('-0x3fffffffffff', 16)) 
print(int('0x400000000000', 16)) 
print(int('-0x400000000000', 16)) 
x = 4611686018427387903 
x = -4611686018427387903 
x = 4611686018427387904 
x = -4611686018427387904 
import sys
print(sys.maxsize + 1 - 1 == sys.maxsize)
x = 1 << 70
print('a' * (x + 4 - x))
