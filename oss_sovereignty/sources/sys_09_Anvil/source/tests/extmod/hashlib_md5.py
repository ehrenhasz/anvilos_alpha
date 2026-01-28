try:
    import hashlib
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    hashlib.md5
except AttributeError:
    print("SKIP")
    raise SystemExit
md5 = hashlib.md5(b"hello")
md5.update(b"world")
print(md5.digest())
