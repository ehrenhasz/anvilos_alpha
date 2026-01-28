try:
    enumerate
except:
    print("SKIP")
    raise SystemExit
try:
    enumerate()
except TypeError:
    print('TypeError')
