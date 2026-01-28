try:
    set
except NameError:
    print("SKIP")
    raise SystemExit
print(set)
print(type(set()) == set)
print(type({None}) == set)
