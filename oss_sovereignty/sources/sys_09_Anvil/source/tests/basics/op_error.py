try:
    ~None
except TypeError:
    print('TypeError')
try:
    ~''
except TypeError:
    print('TypeError')
try:
    ~[]
except TypeError:
    print('TypeError')
try:
    False in True
except TypeError:
    print('TypeError')
try:
    1 * {}
except TypeError:
    print('TypeError')
try:
    1 in 1
except TypeError:
    print('TypeError')
try:
    1[0] = 1
except TypeError:
    print('TypeError')
try:
    'a'[0] = 1
except TypeError:
    print('TypeError')
try:
    del 1[0]
except TypeError:
    print('TypeError')
try:
    next(1)
except TypeError:
    print('TypeError')
try:
    raise 1
except TypeError:
    print('TypeError')
try:
    from sys import youcannotimportmebecauseidontexist
except ImportError:
    print('ImportError')
