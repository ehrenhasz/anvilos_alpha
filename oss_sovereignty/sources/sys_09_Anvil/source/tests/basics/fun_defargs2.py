def foo(a, b=3):
    print(a, b)
foo(1, 333)
foo(1, b=333)
foo(a=2, b=333)
def foo2(a=1, b=2):
    print(a, b)
foo2(b='two')
