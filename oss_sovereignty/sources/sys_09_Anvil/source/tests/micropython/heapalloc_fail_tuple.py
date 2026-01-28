import micropython
x = 1
micropython.heap_lock()
try:
    (x,)
except MemoryError:
    print("MemoryError: tuple create")
micropython.heap_unlock()
