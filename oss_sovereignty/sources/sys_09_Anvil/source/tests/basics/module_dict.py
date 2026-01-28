import sys
if not hasattr(sys, "__dict__"):
    print("SKIP")
    raise SystemExit
print(type(sys.__dict__))
print(sys.__dict__["__name__"])
