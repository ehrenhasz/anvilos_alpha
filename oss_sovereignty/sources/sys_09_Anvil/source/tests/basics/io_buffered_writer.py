import io
try:
    io.BytesIO
    io.BufferedWriter
except AttributeError:
    print('SKIP')
    raise SystemExit
bts = io.BytesIO()
buf = io.BufferedWriter(bts, 8)
buf.write(b"foobar")
print(bts.getvalue())
buf.write(b"foobar")
print(bts.getvalue())
buf.flush()
print(bts.getvalue())
buf.flush()
print(bts.getvalue())
bts = io.BytesIO()
buf = io.BufferedWriter(bts, 1)
buf.write(b"foo")
print(bts.getvalue())
print(type(hash(buf)))
