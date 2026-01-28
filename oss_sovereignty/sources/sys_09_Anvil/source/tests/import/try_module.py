import import1b
def func1():
    print("func1")
def func2():
    try:
        import1b.throw()
    except ValueError:
        pass
    func1()
func2()
