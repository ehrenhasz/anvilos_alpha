try:
    import hashlib
except ImportError:
    print("SKIP")
    raise SystemExit
for algo_name in ("md5", "sha1", "sha256"):
    algo = getattr(hashlib, algo_name, None)
    if not algo:
        continue
    h = algo(b"123")
    h.digest()
    try:
        h.digest()
        print("fail")
    except ValueError:
        pass
    h = algo(b"123")
    h.digest()
    try:
        h.update(b"456")
        print("fail")
    except ValueError:
        pass
print("done")
