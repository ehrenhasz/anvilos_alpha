try:
    from io import StringIO
    import json
except ImportError:
    print("SKIP")
    raise SystemExit
s = StringIO()
json.dump(False, s)
print(s.getvalue())
s = StringIO()
json.dump({"a": (2, [3, None])}, s)
print(s.getvalue())
try:
    json.dump(123, 1)
except (AttributeError, OSError):  
    print("Exception")
try:
    json.dump(123, {})
except (AttributeError, OSError):  
    print("Exception")
