import micropython
class GetSlice:
    def __getitem__(self, idx):
        return idx
sl = GetSlice()[:]
micropython.heap_lock()
try:
    memoryview(b"")
except MemoryError:
    print("MemoryError: memoryview create")
micropython.heap_unlock()
m = memoryview(b"")
micropython.heap_lock()
try:
    m[sl]
except MemoryError:
    print("MemoryError: memoryview subscr get")
micropython.heap_unlock()
