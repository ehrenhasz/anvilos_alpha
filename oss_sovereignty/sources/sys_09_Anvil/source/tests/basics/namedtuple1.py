try:
    from collections import namedtuple
except ImportError:
    print("SKIP")
    raise SystemExit
T = namedtuple("Tup", ["foo", "bar"])
for t in T(1, 2), T(bar=1, foo=2):
    print(t)
    print(t[0], t[1])
    print(t.foo, t.bar)
    print(len(t))
    print(bool(t))
    print(t + t)
    print(t * 3)
    print([f for f in t])
    print(isinstance(t, tuple))
    print(t == (t[0], t[1]), (t[0], t[1]) == t)
print(T(3, bar=4))
try:
    t[0] = 200
except TypeError:
    print("TypeError")
try:
    t.bar = 200
except AttributeError:
    print("AttributeError")
try:
    t = T(1)
except TypeError:
    print("TypeError")
try:
    t = T(1, 2, 3)
except TypeError:
    print("TypeError")
try:
    t = T(foo=1)
except TypeError:
    print("TypeError")
try:
    t = T(1, foo=1)
except TypeError:
    print("TypeError")
try:
    t = T(1, baz=3)
except TypeError:
    print("TypeError")
try:
    namedtuple('T', 1)
except TypeError:
    print("TypeError")
T3 = namedtuple("TupComma", "foo bar")
t = T3(1, 2)
print(t.foo, t.bar)
T4 = namedtuple("TupTuple", ("foo", "bar"))
t = T4(1, 2)
print(t.foo, t.bar)
T5 = namedtuple("TupEmpty", [])
t = T5()
print(t)
