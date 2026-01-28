try:
    reversed
except:
    print("SKIP")
    raise SystemExit
print(list(reversed([])))
print(list(reversed([1])))
print(list(reversed([1, 2, 3])))
print(list(reversed(())))
print(list(reversed((1, 2, 3))))
for c in reversed('ab'):
    print(c)
for b in reversed(b'1234'):
    print(b)
for i in reversed(range(3)):
    print(i)
class A:
    def __init__(self):
        pass
    def __len__(self):
        return 3
    def __getitem__(self, pos):
        return pos + 1
for a in reversed(A()):
    print(a)
class B:
    def __reversed__(self):
        return [1, 2, 3]
print(reversed(B()))
