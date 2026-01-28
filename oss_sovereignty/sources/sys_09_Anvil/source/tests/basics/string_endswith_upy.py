try:
    "foobar".endswith(("bar", "sth"))
except TypeError:
    print("TypeError")
