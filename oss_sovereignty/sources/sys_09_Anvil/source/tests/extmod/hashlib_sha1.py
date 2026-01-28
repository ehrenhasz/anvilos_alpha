try:
    import hashlib
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    hashlib.sha1
except AttributeError:
    print("SKIP")
    raise SystemExit
sha1 = hashlib.sha1(b"hello")
sha1.update(b"world")
print(sha1.digest())
