import sys
try:
    sys.x = 1
except AttributeError:
    print("AttributeError")
