try:
    from time import ticks_diff, ticks_add
except ImportError:
    print("SKIP")
    raise SystemExit
MAX = ticks_add(0, -1)
MODULO_HALF = MAX // 2 + 1
assert ticks_diff(1, 0) == 1, ticks_diff(1, 0)
assert ticks_diff(0, 1) == -1
assert ticks_diff(0, MAX) == 1
assert ticks_diff(MAX, 0) == -1
assert ticks_diff(0, MAX - 1) == 2
assert ticks_diff(MODULO_HALF, 1) == MODULO_HALF - 1, ticks_diff(MODULO_HALF, 1)
assert ticks_diff(MODULO_HALF, 0) == -MODULO_HALF
off = 100
assert ticks_diff(MODULO_HALF + off, 1 + off) == MODULO_HALF - 1
assert ticks_diff(MODULO_HALF + off, 0 + off) == -MODULO_HALF
assert ticks_diff(MODULO_HALF - off, ticks_add(1, -off)) == MODULO_HALF - 1
assert ticks_diff(MODULO_HALF - off, ticks_add(0, -off)) == -MODULO_HALF
print("OK")
