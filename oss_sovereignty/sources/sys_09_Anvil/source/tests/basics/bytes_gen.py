def gen():
    for i in range(4):
        yield i
print(bytes(gen()))
