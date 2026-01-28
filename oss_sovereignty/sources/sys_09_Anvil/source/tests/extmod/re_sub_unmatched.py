try:
    import re
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    re.sub
except AttributeError:
    print("SKIP")
    raise SystemExit
print(re.sub(r"(a)(b)?", r"\2-\1", "1a2"))
