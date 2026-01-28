class C:
    pass
c = C()
try:
    d = bytes(c)
except TypeError:
    print('TypeError')
