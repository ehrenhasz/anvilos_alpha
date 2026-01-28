import sys
t = sys.implementation
try:
    t.name
except AttributeError:
    print("SKIP")
    raise SystemExit
print(str(t).find("version=") > 0)
print(isinstance(t.name, str))
