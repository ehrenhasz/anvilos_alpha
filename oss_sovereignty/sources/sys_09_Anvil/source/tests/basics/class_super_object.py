try:
    object.__init__
except AttributeError:
    print("SKIP")
    raise SystemExit
class Test(object):
    def __init__(self):
        super().__init__()
        print("Test.__init__")
t = Test()
class Test2:
    def __init__(self):
        super().__init__()
        print("Test2.__init__")
t = Test2()
