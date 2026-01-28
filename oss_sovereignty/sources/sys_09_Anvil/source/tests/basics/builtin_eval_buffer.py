try:
    eval
    bytearray
    memoryview
except:
    print("SKIP")
    raise SystemExit
print(eval(bytearray(b'1 + 1')))
print(eval(memoryview(b'2 + 2')))
