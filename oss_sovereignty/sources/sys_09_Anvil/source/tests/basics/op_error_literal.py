try:
    1[0]
except TypeError:
    print("TypeError")
try:
    ""[""]
except TypeError:
    print("TypeError")
try:
    1()
except TypeError:
    print("TypeError")
