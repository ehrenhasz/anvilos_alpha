import io
try:
    io.IOBase
except AttributeError:
    print('SKIP')
    raise SystemExit
class MyIO(io.IOBase):
    def write(self, buf):
        print('write', len(buf))
        return len(buf)
print('test', file=MyIO())
