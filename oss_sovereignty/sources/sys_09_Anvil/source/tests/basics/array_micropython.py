try:
    import array
except ImportError:
    print("SKIP")
    raise SystemExit
a = array.array('O')
a.append(1)
print(a[0])
a = array.array('P')
a.append(1)
print(a[0])
typecodes = ["b", "h", "i", "l", "q", "P", "O", "S", "f", "d"]
for a in typecodes:
    for b in typecodes:
        if a == b and a not in ["f", "d"]:
            continue
        try:
            array.array(a) == array.array(b)
            print('FAIL')
        except NotImplementedError:
            pass
