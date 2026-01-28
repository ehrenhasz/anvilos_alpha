import os
if not hasattr(os, "remove"):
    print("SKIP")
    raise SystemExit
try:
    os.remove("testfile")
except OSError:
    pass
f = open("testfile", "a")
f.write("foo")
f.close()
f = open("testfile")
print(f.read())
f.close()
f = open("testfile", "a")
f.write("bar")
f.close()
f = open("testfile")
print(f.read())
f.close()
try:
    os.remove("testfile")
except OSError:
    pass
