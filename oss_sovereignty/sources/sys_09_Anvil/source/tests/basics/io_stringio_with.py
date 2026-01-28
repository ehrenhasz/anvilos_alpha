import io
with io.StringIO() as b:
    b.write("foo")
    print(b.getvalue())
