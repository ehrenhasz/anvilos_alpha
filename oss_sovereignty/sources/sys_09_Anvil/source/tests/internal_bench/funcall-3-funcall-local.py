import bench
def f(x):
    return x + 1
def test(num):
    f_ = f
    for i in iter(range(num)):
        a = f_(i)
bench.run(test)
