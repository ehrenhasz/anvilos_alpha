f = open("data/file1")
with f as f2:
    print(f2.read())
try:
    f.read()
except:
    print("can't read file after with")
try:
    with open("__non_existent", "r"):
        pass
except OSError:
    print("OSError")
