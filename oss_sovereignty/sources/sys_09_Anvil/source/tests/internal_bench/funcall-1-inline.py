import bench
def test(num):
    for i in iter(range(num)):
        a = i + 1
bench.run(test)
