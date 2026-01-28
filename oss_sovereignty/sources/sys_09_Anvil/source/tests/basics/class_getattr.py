class C:
    def __init__(self):
        self.__add__ = lambda: print('member __add__')
    def __add__(self, x):
        print('__add__')
    def __getattr__(self, attr):
        print('__getattr__', attr)
        return None
c = C()
c.add 
c.__add__() 
c + 1 
