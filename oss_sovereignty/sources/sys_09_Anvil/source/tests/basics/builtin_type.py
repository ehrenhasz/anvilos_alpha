print(type(int))
try:
    type()
except TypeError:
    print('TypeError')
try:
    type(1, 2)
except TypeError:
    print('TypeError')
try:
    type('abc', None, None)
except TypeError:
    print('TypeError')
try:
    type('abc', (), None)
except TypeError:
    print('TypeError')
try:
    type('abc', (1,), {})
except TypeError:
    print('TypeError')
