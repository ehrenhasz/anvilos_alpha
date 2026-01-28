def test(r):
    for _ in r:
        str("a string that shouldn't be interned")
bm_params = {
    (32, 10): (400,),
    (1000, 10): (4000,),
    (5000, 10): (40000,),
}
def bm_setup(params):
    (nloop,) = params
    return lambda: test(range(nloop)), lambda: (nloop // 100, None)
