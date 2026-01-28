try:
    ~bytearray()
except TypeError:
    print('TypeError')
try:
    bytearray() // 2
except TypeError:
    print('TypeError')
try:
    bytearray(1) + 1
except TypeError:
    print('TypeError')
