try:
    memoryview
except:
    print("SKIP")
    raise SystemExit
b = bytearray(10)
m = memoryview(b)[1:]
for i in range(len(m)):
    m[i] = i
b = None
import gc
gc.collect()
for i in range(100000):
    [42, 42, 42, 42]
print(list(m))
m = None
gc.collect()  # cleanup from previous test
m = memoryview(memoryview(bytearray(i for i in range(50)))[5:-5])
print(sum(m), list(m[:10]))
gc.collect()
for i in range(10):
    list(range(10))  # allocate memory to overwrite any reclaimed heap
print(sum(m), list(m[:10]))
