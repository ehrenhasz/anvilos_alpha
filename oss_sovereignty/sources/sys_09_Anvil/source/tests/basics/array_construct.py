try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(array('b', (1, 2)))
print(array('h', [1, 2]))
print(array('h', b'22'))  
print(array('h', bytearray(2)))
print(array('i', bytearray(4)))
print(array('H', array('b', [1, 2])))
print(array('b', array('I', [1, 2])))
