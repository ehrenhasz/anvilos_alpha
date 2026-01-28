s = {1}
print(s)
s = {3, 4, 3, 1}
print(sorted(s))
s = {1 + len(s)}
print(s)
s = {False, True, 0, 1, 2}
print(len(s))
try:
    {s: 1}
except TypeError:
    print("TypeError")
