import io
a = io.BytesIO(b"foobar")
try:
    a.seek(-10)
except Exception as e:
    print(type(e), e.args[0] > 0)
