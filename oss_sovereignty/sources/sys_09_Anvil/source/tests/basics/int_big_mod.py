delta = 100000000000000000000000000000012345
for i in range(11):
    for j in range(11):
        x = delta * (i - 5)
        y = delta * (j - 5)
        if y != 0:
            print(x % y)
x = 0x8000000000000000
print((x + 1) % x)
x = 0x86c60128feff5330
print((x + 1) % x)
