class A:
    def __init__(self):
        self.val = 4
    def foo(self):
        return list(range(self.val))
class B(A):
    def foo(self):
        return [self.bar(i) for i in super().foo()]
    def bar(self, x):
        return 2 * x
print(A().foo())
print(B().foo())
