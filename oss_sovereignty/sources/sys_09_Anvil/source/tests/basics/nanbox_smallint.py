try:
    import micropython
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    micropython.heap_lock()
    float(123)
    micropython.heap_unlock()
except:
    micropython.heap_unlock()
    print("SKIP")
    raise SystemExit
if float("1e100") == float("inf"):
    print("SKIP")
    raise SystemExit
micropython.heap_lock()
print(int("0x80000000"))
micropython.heap_unlock()
micropython.heap_lock()
print(int("0x3fffffffffff"))
micropython.heap_unlock()
micropython.heap_lock()
print(int("-0x3fffffffffff") - 1)
micropython.heap_unlock()
x = 1
micropython.heap_lock()
print((x << 31) + 1)
micropython.heap_unlock()
x = 1
micropython.heap_lock()
print((x << 45) + 1)
micropython.heap_unlock()
