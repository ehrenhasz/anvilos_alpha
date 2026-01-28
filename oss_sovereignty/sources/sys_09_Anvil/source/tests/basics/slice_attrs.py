class A:
    def __getitem__(self, idx):
        print(idx.start, idx.stop, idx.step)
try:
    t = A()[1:2]
except:
    print("SKIP")
    raise SystemExit
A()[1:2:3]
class B:
    def __getitem__(self, idx):
        try:
            idx.start = 0
        except AttributeError:
            print('AttributeError')
B()[:]
