try:
    t = [][:]
except:
    print("SKIP")
    raise SystemExit
try:
    {}[:] = {}
except TypeError:
    print('TypeError')
