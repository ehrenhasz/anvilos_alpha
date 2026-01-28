l = list(range(1000))
try:
    1000000000 * l
except MemoryError:
    print('MemoryError')
print(len(l), l[0], l[-1])
try:
    [].extend(range(1000000000))
except MemoryError:
    print('MemoryError')
