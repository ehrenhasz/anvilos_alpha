try:
    from collections import OrderedDict
except ImportError:
    print("SKIP")
    raise SystemExit
d = OrderedDict([(10, 20), ("b", 100), (1, 2)])
print(len(d))
print(list(d.keys()))
print(list(d.values()))
del d["b"]
print(len(d))
print(list(d.keys()))
print(list(d.values()))
print(d[10], d[1])
d["abc"] = 123
print(len(d))
print(list(d.keys()))
print(list(d.values()))
print(d.popitem())
print(len(d))
print(list(d.keys()))
print(list(d.values()))
d["xyz"] = 321
print(len(d))
print(list(d.keys()))
print(list(d.values()))
print(d.popitem())
print(d.popitem())
try:
    d.popitem()
except:
    print('empty')
