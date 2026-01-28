from micropython import const
import sys
try:
    sys.settrace
    print("SKIP")
    raise SystemExit
except AttributeError:
    pass
_STR = const("foo")
_EMPTY_TUPLE = const(())
_TRUE = const(True)
_FALSE = const(False)
_SMALLINT = const(33)
_ZERO = const(0)
if _STR or _EMPTY_TUPLE:
    print("Kept")
if _STR and _EMPTY_TUPLE:
    print("Eliminated")
if _TRUE:
    print("Kept")
if _SMALLINT:
    print("Kept")
if _ZERO and _SMALLINT:
    print("Eliminated")
if _FALSE:
    print("Eliminated")
while _SMALLINT:
    print("Kept")
    break
while _ZERO:
    print("Eliminated")
while _FALSE:
    print("Eliminated")
a = _EMPTY_TUPLE or _STR
if a == _STR:
    print("Kept")
b = _SMALLINT and _STR
if b == _STR:
    print("Kept")
if (_EMPTY_TUPLE or _STR) == _STR:
    print("Kept")
if (_EMPTY_TUPLE and _STR) == _STR:
    print("Not Eliminated")
if (not _STR) == _FALSE:
    print("Kept")
assert True
