try:
    import hashlib
except ImportError:
    print("SKIP")
    raise SystemExit
h = hashlib.sha256()
print(h.digest())
h = hashlib.sha256()
h.update(b"123")
print(h.digest())
h = hashlib.sha256()
h.update(b"abcd" * 1000)
print(h.digest())
print(hashlib.sha256(b"\xff" * 64).digest())
print(hashlib.sha256(b"\xff" * 56).digest())
