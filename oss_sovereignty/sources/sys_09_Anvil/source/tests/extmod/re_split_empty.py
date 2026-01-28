try:
    import re
except ImportError:
    print("SKIP")
    raise SystemExit
r = re.compile(" *")
s = r.split("a b    c   foobar")
print(s)
r = re.compile("x*")
s = r.split("foo")
print(s)
r = re.compile("x*")
s = r.split("axbc")
print(s)
