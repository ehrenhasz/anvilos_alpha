try:
    memoryview(b'a').itemsize
except:
    print("SKIP")
    raise SystemExit
try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
for code in ['b', 'h', 'i', 'q', 'f', 'd']:
    print(memoryview(array(code)).itemsize)
print(memoryview(array('l')).itemsize in (4, 8))
try:
    memoryview(b'a').itemsize = 1
except AttributeError:
    print('AttributeError')
