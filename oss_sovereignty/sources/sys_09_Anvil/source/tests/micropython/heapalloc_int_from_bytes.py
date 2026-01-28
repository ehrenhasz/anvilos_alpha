import micropython
micropython.heap_lock()
print(int.from_bytes(b"1", "little"))
print(int.from_bytes(b"12", "little"))
print(int.from_bytes(b"2\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", "little"))
micropython.heap_unlock()
