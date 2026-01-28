class mymap(map):
    pass
m = mymap(lambda x: x + 10, range(4))
print(list(m))
