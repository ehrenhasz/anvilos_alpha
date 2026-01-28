print('abc')
print(r'abc')
print(u'abc')
print(repr('\a\b\t\n\v\f\r'))
print(str())
print(str('abc'))
x = 'abc'
print(x)
x += 'def'
print(x)
print('123' + "456")
print('123' * 5)
try:
    '123' * '1'
except TypeError:
    print('TypeError')
try:
    '123' + 1
except TypeError:
    print('TypeError')
print('abc'[1])
print('abc'[-1])
try:
    'abc'[100]
except IndexError:
    print('IndexError')
try:
    'abc'[-4]
except IndexError:
    print('IndexError')
print(list('str'))
print('123' + '789' == '123789')
print('a' + 'b' != 'a' + 'b ')
print('1' + '2' > '2')
print('1' + '2' < '2')
print(repr('\'\"'))
