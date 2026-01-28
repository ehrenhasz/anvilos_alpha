print(','.join(()))
print(','.join(('a',)))
print(','.join(('a', 'b')))
print(','.join([]))
print(','.join(['a']))
print(','.join(['a', 'b']))
print(''.join(''))
print(''.join('abc'))
print(','.join('abc'))
print(','.join('abc' for i in range(5)))
print(b','.join([b'abc', b'123']))
try:
    ''.join(None)
except TypeError:
    print("TypeError")
try:
    print(b','.join(['abc', b'123']))
except TypeError:
    print("TypeError")
try:
    print(','.join([b'abc', b'123']))
except TypeError:
    print("TypeError")
print("a" "b")
print("a" '''b''')
print("a" 
    "b")
print("a" \
    "b")
x = 'a'
'b'
print(x)
