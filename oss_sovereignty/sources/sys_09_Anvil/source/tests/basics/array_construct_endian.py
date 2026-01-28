try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(array('h', b'12'))
