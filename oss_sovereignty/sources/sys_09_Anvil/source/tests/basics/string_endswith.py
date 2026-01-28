print("foobar".endswith("bar"))
print("foobar".endswith("baR"))
print("foobar".endswith("bar1"))
print("foobar".endswith("foobar"))
print("foobar".endswith(""))
print("foobar".endswith("foobarbaz"))
try:
    "foobar".endswith(1)
except TypeError:
    print("TypeError")
