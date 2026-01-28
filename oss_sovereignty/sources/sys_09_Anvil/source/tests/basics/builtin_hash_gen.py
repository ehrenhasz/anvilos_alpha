def gen():
    yield
print(type(hash(gen)))
print(type(hash(gen())))
