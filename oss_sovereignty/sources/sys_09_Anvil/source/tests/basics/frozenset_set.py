try:
    frozenset
except NameError:
    print("SKIP")
    raise SystemExit
print(set('abc') == frozenset('abc'))
