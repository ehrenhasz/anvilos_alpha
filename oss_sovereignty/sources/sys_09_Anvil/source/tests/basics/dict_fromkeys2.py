try:
    reversed
except:
    print("SKIP")
    raise SystemExit
d = dict.fromkeys(reversed(range(1)))
print(d)
