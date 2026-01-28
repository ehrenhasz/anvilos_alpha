try:
    sorted
    set
except:
    print("SKIP")
    raise SystemExit
print(sorted(set(range(100))))
print(sorted(set(range(100)), key=lambda x: x + 100*(x % 2)))
try:
    sorted([], None)
except TypeError:
    print("TypeError")
