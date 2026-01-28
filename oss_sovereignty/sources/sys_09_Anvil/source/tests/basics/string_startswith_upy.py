try:
    "foobar".startswith(("foo", "sth"))
except TypeError:
    print("TypeError")
