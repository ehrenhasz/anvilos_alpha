import os
if not hasattr(os, "remove"):
    print("SKIP")
    raise SystemExit
try:
    os.remove("testfile")
except OSError:
    pass
try:
    f = open("testfile", "r+b")
    print("Unexpectedly opened non-existing file")
except OSError:
    print("Expected OSError")
    pass
f = open("testfile", "w+b")
f.write(b"1234567890")
f.seek(0)
print(f.read())
f.close()
f = open("testfile", "w+b")
f.write(b"abcdefg")
f.seek(0)
print(f.read())
f.close()
f = open("testfile", "r+b")
f.write(b"1234")
f.seek(0)
print(f.read())
f.close()
try:
    os.remove("testfile")
except OSError:
    pass
