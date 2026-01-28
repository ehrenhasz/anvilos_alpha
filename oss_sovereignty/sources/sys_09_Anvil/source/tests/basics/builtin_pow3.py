try:
    print(pow(3, 4, 7))
except NotImplementedError:
    print("SKIP")
    raise SystemExit
print(pow(1, 1, 1))
print(pow(0, 1, 1))
print(pow(1, 0, 1))
print(pow(1, 0, 2))
try:
    print(pow("x", 5, 6))
except TypeError:
    print("TypeError expected")
try:
    print(pow(4, "y", 6))
except TypeError:
    print("TypeError expected")
try:
    print(pow(4, 5, "z"))
except TypeError:
    print("TypeError expected")
