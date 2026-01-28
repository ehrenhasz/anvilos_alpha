import bench
def test(num):
    for i in iter(range(num // 10000)):
        arr = [0] * 1000
        arr2 = list(map(lambda x: x + 1, arr))
bench.run(test)
