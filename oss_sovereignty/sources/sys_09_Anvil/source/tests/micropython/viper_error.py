def test(code):
    try:
        exec(code)
    except (SyntaxError, ViperTypeError, NotImplementedError) as e:
        print(repr(e))
test("@micropython.viper\ndef f(a:1): pass")
test("@micropython.viper\ndef f() -> 1: pass")
test("@micropython.viper\ndef f(x:unknown_type): pass")
test(
    """
@micropython.viper
def f():
    print(x)
    x = 1
"""
)
test(
    """
@micropython.viper
def f():
    x = 1
    y = []
    x = y
"""
)
test(
    """
@micropython.viper
def f():
    x = ptr(0)
    if x:
        pass
"""
)
test("@micropython.viper\ndef f() -> int: return []")
test("@micropython.viper\ndef f(x:ptr): -x")
test("@micropython.viper\ndef f(): 1 + []")
test("@micropython.viper\ndef f(x:int, y:uint): x < y")
test("@micropython.viper\ndef f(): 1[0]")
test("@micropython.viper\ndef f(): 1[x]")
test("@micropython.viper\ndef f(): 1[0] = 1")
test("@micropython.viper\ndef f(): 1[x] = 1")
test("@micropython.viper\ndef f(x:int): x[0] = x")
test("@micropython.viper\ndef f(x:ptr32): x[0] = None")
test("@micropython.viper\ndef f(x:ptr32): x[x] = None")
test("@micropython.viper\ndef f(): raise 1")
test("@micropython.viper\ndef f(x:int): not x")
test("@micropython.viper\ndef f(x:uint, y:uint): res = x // y")
test("@micropython.viper\ndef f(x:uint, y:uint): res = x % y")
test("@micropython.viper\ndef f(x:int): res = x in x")
test("@micropython.viper\ndef f(): yield")
test("@micropython.viper\ndef f(): yield from f")
test("@micropython.viper\ndef f(): print(ptr(1))")
test("@micropython.viper\ndef f(): int(int)")
