d = {1: 2}
for m in d.items, d.values, d.keys:
    print(m())
    print(list(m()))
print({1:1, 2:1}.values())
try:
    {1:1}.values() + 1
except TypeError:
    print('TypeError')
try:
    {1:1}.keys() + 1
except TypeError:
    print('TypeError')
try:
    hash({}.keys())
except TypeError:
    print('TypeError')
print(type(hash({}.values())))
try:
    hash({}.items())
except TypeError:
    print('TypeError')
