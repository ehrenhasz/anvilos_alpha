__import__("builtins")
try:
    __import__(1)
except TypeError:
    print("TypeError")
try:
    __import__("")
except ValueError:
    print("ValueError")
try:
    __import__("xyz", None, None, None, -1)
except ValueError:
    print("ValueError")
