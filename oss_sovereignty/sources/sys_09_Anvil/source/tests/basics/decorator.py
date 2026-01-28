def dec(f):
    print('dec')
    return f
def dec_arg(x):
    print(x)
    return lambda f:f
@dec
def f():
    pass
@dec_arg('dec_arg')
def g():
    pass
@dec
class A:
    pass
