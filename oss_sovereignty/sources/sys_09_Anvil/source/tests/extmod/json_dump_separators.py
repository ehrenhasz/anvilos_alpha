try:
    from io import StringIO
    import json
except ImportError:
    print("SKIP")
    raise SystemExit
for sep in [
    None,
    (", ", ": "),
    (",", ": "),
    (",", ":"),
    [", ", ": "],
    [",", ": "],
    [",", ":"],
]:
    s = StringIO()
    json.dump(False, s, separators=sep)
    print(s.getvalue())
    s = StringIO()
    json.dump({"a": (2, [3, None])}, s, separators=sep)
    print(s.getvalue())
    try:
        json.dump(123, 1, separators=sep)
    except (AttributeError, OSError):  
        print("Exception")
    try:
        json.dump(123, {}, separators=sep)
    except (AttributeError, OSError):  
        print("Exception")
try:
    s = StringIO()
    json.dump(False, s, separators={"a": 1})
except (TypeError, ValueError):  
    print("Exception")
for sep in [1, object()]:
    try:
        s = StringIO()
        json.dump(False, s, separators=sep)
    except TypeError:
        print("Exception")
for sep in [(), (",", ":", "?"), (",",), []]:
    try:
        s = StringIO()
        json.dump(False, s, separators=sep)
    except ValueError:
        print("Exception")
