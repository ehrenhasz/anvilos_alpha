def dec(f):
    print('decorator')
    return f
@dec
async def foo():
    print('foo')
coro = foo()
try:
    coro.send(None)
except StopIteration:
    print('StopIteration')
