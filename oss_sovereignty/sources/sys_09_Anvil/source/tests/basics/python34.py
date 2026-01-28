try:
    exec
except NameError:
    print("SKIP")
    raise SystemExit
def print_ret(x):
    print(x)
    return x
{print_ret(1):print_ret(2)}
def test_syntax(code):
    try:
        exec(code)
    except SyntaxError:
        print("SyntaxError")
test_syntax("f(**a, b)") # can't have positional after **
test_syntax("() = []") # can't assign to empty tuple (in 3.6 we can)
test_syntax("del ()") # can't delete empty tuple (in 3.6 we can)
import sys
print(sys.version[:3])
print(sys.version_info[0], sys.version_info[1])
print(repr(IndexError("foo")))
