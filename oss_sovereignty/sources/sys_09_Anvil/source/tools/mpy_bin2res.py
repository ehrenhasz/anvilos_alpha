import sys
print("R = {")
for fname in sys.argv[1:]:
    with open(fname, "rb") as f:
        b = f.read()
        print("%r: %r," % (fname, b))
print("}")
