import micropython
l = []
l2 = list(range(100))
micropython.heap_lock()
micropython.heap_lock()
try:
    print([])
except MemoryError:
    print("MemoryError")
try:
    l.extend(l2)
except MemoryError:
    print("MemoryError")
print(micropython.heap_unlock())
try:
    print([])
except MemoryError:
    print("MemoryError")
micropython.heap_unlock()
print([])
