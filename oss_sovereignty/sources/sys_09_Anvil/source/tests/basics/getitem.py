class A:
    def __getitem__(self, index):
        print('getitem', index)
        if index > 10:
            raise StopIteration
A()[0]
A()[1]
for i in A():
    pass
it = iter(A())
try:
    while True:
        next(it)
except StopIteration:
    pass
class A:
    def __getitem__(self, i):
        raise IndexError
print(list(A()))
class A:
    def __getitem__(self, i):
        raise TypeError
try:
    for i in A():
        pass
except TypeError:
    print("TypeError")
