try:
    import re
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    m = re.match(".", "a")
    m.groups
except AttributeError:
    print("SKIP")
    raise SystemExit
m = re.match(r"(([0-9]*)([a-z]*)[0-9]*)", "1234hello567")
print(m.groups())
m = re.match(r"([0-9]*)(([a-z]*)([0-9]*))", "1234hello567")
print(m.groups())
print(re.match(r"(a)?b(c)", "abc").groups())
print(re.match(r"(a)?b(c)", "bc").groups())
print(re.match(r"abc", "abc").groups())
