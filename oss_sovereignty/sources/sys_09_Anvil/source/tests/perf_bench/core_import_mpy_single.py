import sys, io, vfs
if not hasattr(io, "IOBase"):
    print("SKIP")
    raise SystemExit
"""
class A0:
    def a0(self): pass
    def a1(self): pass
    def a2(self): pass
    def a3(self): pass
class A1:
    def a0(self): pass
    def a1(self): pass
    def a2(self): pass
    def a3(self): pass
def f0():
    __call__, __class__, __delitem__, __enter__, __exit__, __getattr__, __getitem__,
    __hash__, __init__, __int__, __iter__, __len__, __main__, __module__, __name__,
    __new__, __next__, __qualname__, __repr__, __setitem__, __str__,
    ArithmeticError, AssertionError, AttributeError, BaseException, EOFError, Ellipsis,
    Exception, GeneratorExit, ImportError, IndentationError, IndexError, KeyError,
    KeyboardInterrupt, LookupError, MemoryError, NameError, NoneType,
    NotImplementedError, OSError, OverflowError, RuntimeError, StopIteration,
    SyntaxError, SystemExit, TypeError, ValueError, ZeroDivisionError,
    abs, all, any, append, args, bool, builtins, bytearray, bytecode, bytes, callable,
    chr, classmethod, clear, close, const, copy, count, dict, dir, divmod, end,
    endswith, eval, exec, extend, find, format, from_bytes, get, getattr, globals,
    hasattr, hash, id, index, insert, int, isalpha, isdigit, isinstance, islower,
    isspace, issubclass, isupper, items, iter, join, key, keys, len, list, little,
    locals, lower, lstrip, main, map, micropython, next, object, open, ord, pop,
    popitem, pow, print, range, read, readinto, readline, remove, replace, repr,
    reverse, rfind, rindex, round, rsplit, rstrip, self, send, sep, set, setattr,
    setdefault, sort, sorted, split, start, startswith, staticmethod, step, stop, str,
    strip, sum, super, throw, to_bytes, tuple, type, update, upper, value, values,
    write, zip,
    name0, name1, name2, name3, name4, name5, name6, name7, name8, name9,
    quite_a_long_name0, quite_a_long_name1, quite_a_long_name2, quite_a_long_name3,
    quite_a_long_name4, quite_a_long_name5, quite_a_long_name6, quite_a_long_name7,
    quite_a_long_name8, quite_a_long_name9, quite_a_long_name10, quite_a_long_name11,
def f1():
    x = "this will be a string object 0"
    x = "this will be a string object 1"
    x = "this will be a string object 2"
    x = "this will be a string object 3"
    x = "this will be a string object 4"
    x = "this will be a string object 5"
    x = "this will be a string object 6"
    x = "this will be a string object 7"
    x = "this will be a string object 8"
    x = "this will be a string object 9"
    x = b"this will be a bytes object 0"
    x = b"this will be a bytes object 1"
    x = b"this will be a bytes object 2"
    x = b"this will be a bytes object 3"
    x = b"this will be a bytes object 4"
    x = b"this will be a bytes object 5"
    x = b"this will be a bytes object 6"
    x = b"this will be a bytes object 7"
    x = b"this will be a bytes object 8"
    x = b"this will be a bytes object 9"
    x = ("const tuple 0", None, False, True, 1, 2, 3)
    x = ("const tuple 1", None, False, True, 1, 2, 3)
    x = ("const tuple 2", None, False, True, 1, 2, 3)
    x = ("const tuple 3", None, False, True, 1, 2, 3)
    x = ("const tuple 4", None, False, True, 1, 2, 3)
    x = ("const tuple 5", None, False, True, 1, 2, 3)
    x = ("const tuple 6", None, False, True, 1, 2, 3)
    x = ("const tuple 7", None, False, True, 1, 2, 3)
    x = ("const tuple 8", None, False, True, 1, 2, 3)
    x = ("const tuple 9", None, False, True, 1, 2, 3)
result = 123
"""
file_data = b"M\x06\x00\x1f\x81=\x1e\x0etest.py\x00\x0f\x04A0\x00\x04A1\x00\x04f0\x00\x04f1\x00\x0cresult\x00/-5\x04a0\x00\x04a1\x00\x04a2\x00\x04a3\x00\x13\x15\x17\x19\x1b\x1d\x1f!
class File(io.IOBase):
    def __init__(self):
        self.off = 0
    def ioctl(self, request, arg):
        return 0
    def readinto(self, buf):
        buf[:] = memoryview(file_data)[self.off : self.off + len(buf)]
        self.off += len(buf)
        return len(buf)
class FS:
    def mount(self, readonly, mkfs):
        pass
    def chdir(self, path):
        pass
    def stat(self, path):
        if path == "/__injected.mpy":
            return tuple(0 for _ in range(10))
        else:
            raise OSError(-2)  
    def open(self, path, mode):
        return File()
def mount():
    vfs.mount(FS(), "/__remote")
    sys.path.insert(0, "/__remote")
def test():
    global result
    module = __import__("__injected")
    result = module.result
bm_params = {
    (1, 1): (),
}
def bm_setup(params):
    mount()
    return lambda: test(), lambda: (1, result)
