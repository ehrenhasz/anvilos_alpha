import bench
def test(num):
    for i in iter(range(num // 10000)):
        arr = bytearray(b"\0" * 1000)
        arr2 = bytearray(map(lambda x: x + 1, arr))
bench.run(test)
