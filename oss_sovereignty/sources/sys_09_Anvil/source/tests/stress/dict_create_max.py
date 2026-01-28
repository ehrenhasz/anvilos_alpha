d = {}
try:
    for i in range(54908):
        d[i] = i
except MemoryError:
    pass
print(d[0])
