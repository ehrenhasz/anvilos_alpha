for i in range(2):
    print(i)
else:
    print('else')
for i in range(2):
    print(i)
    break
else:
    print('else')
for i in range(4):
    print(i)
    for j in range(4):
        pass
    else:
        continue
    break
N = 2
for i in range(N):
    print(i)
else:
    print('else')
for i in [0, 1]:
    print(i)
else:
    print('else')
for i in [0, 1]:
    print(i)
    break
else:
    print('else')
