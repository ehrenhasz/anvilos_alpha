d = {}
print(d)
d[2] = 123
print(d)
d = {1:2}
d[3] = 3
print(len(d), d[1], d[3])
d[1] = 0
print(len(d), d[1], d[3])
print(str(d) == '{1: 0, 3: 3}' or str(d) == '{3: 3, 1: 0}')
x = 1
while x < 100:
    d[x] = x
    x += 1
print(d[50])
print({} == {1:1})
print({1:1} == {2:1})
d = {}
d[False] = 'false'
d[0] = 'zero'
print(d)
d = {}
d[0] = 'zero'
d[False] = 'false'
print(d)
d = {}
d[True] = 'true'
d[1] = 'one'
print(d)
d = {}
d[1] = 'one'
d[True] = 'true'
print(d)
d = {False:10, True:11, 2:12}
print(d[0], d[1], d[2])
try:
    {}[0]
except KeyError as er:
    print('KeyError', er, er.args)
try:
    +{}
except TypeError:
    print('TypeError')
try:
    {} + {}
except TypeError:
    print('TypeError')
