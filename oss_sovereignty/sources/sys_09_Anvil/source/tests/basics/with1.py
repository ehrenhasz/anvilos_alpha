class CtxMgr:
    def __enter__(self):
        print("__enter__")
        return self
    def __exit__(self, a, b, c):
        print("__exit__", repr(a), repr(b))
with CtxMgr() as a:
    print(isinstance(a, CtxMgr))
try:
    with CtxMgr() as a:
        raise ValueError
except ValueError:
    print("ValueError")
class CtxMgr2:
    def __enter__(self):
        print("__enter__")
        return self
    def __exit__(self, a, b, c):
        print("__exit__", repr(a), repr(b))
        return True
try:
    with CtxMgr2() as a:
        raise ValueError
    print("No ValueError2")
except ValueError:
    print("ValueError2")
print("===")
with CtxMgr2() as a:
    try:
        try:
            raise ValueError
            print("No ValueError3")
        finally:
            print("finally1")
    finally:
        print("finally2")
print("===")
try:
    try:
        with CtxMgr2() as a:
            try:
                try:
                    raise ValueError
                    print("No ValueError3")
                finally:
                    print("finally1")
            finally:
                print("finally2")
    finally:
        print("finally3")
finally:
    print("finally4")
