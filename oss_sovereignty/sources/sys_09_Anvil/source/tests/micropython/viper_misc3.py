import micropython
@micropython.viper
def viper_for(a: int, b: int) -> int:
    total = 0
    for x in range(a, b):
        total += x
    return total
print(viper_for(10, 10000))
@micropython.viper
def viper_access_global():
    global gl
    gl = 1
    return gl
print(viper_access_global(), gl)
@micropython.viper
def viper_print(x, y: int):
    print(x, y + 1)
viper_print(1, 2)
@micropython.viper
def viper_tuple_consts(x):
    return (x, 1, False, True)
print(viper_tuple_consts(0))
@micropython.viper
def viper_tuple(x, y: int):
    return (x, y + 1)
print(viper_tuple(1, 2))
@micropython.viper
def viper_list(x, y: int):
    return [x, y + 1]
print(viper_list(1, 2))
@micropython.viper
def viper_set(x, y: int):
    return {x, y + 1}
print(sorted(list(viper_set(1, 2))))
@micropython.viper
def viper_raise(x: int):
    raise OSError(x)
try:
    viper_raise(1)
except OSError as e:
    print(repr(e))
@micropython.viper
def viper_gc() -> int:
    return 1
print(viper_gc())
import gc
gc.collect()
print(viper_gc())
