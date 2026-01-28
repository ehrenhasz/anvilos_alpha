if not hasattr(int, "__dict__"):
    print("SKIP")
    raise SystemExit
print("from_bytes" in int.__dict__)
class Foo:
    a = 1
    b = "bar"
d = Foo.__dict__
print(d["a"], d["b"])
d = type(type('')).__dict__
print(d is not None)
