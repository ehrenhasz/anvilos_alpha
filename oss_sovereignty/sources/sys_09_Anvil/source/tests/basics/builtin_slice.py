class A:
    def __getitem__(self, idx):
        print(idx)
        return idx
s = A()[1:2:3]
print(type(s) is slice)
