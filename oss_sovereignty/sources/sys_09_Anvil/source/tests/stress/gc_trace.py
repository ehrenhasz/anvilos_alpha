try:
    import gc
except ImportError:
    print("SKIP")
    raise SystemExit
lst = [[i] for i in range(200)]
gc.collect()
print(lst)
lst = [
    [[[[(i, j, k, l)] for i in range(3)] for j in range(3)] for k in range(3)] for l in range(3)
]
gc.collect()
print(lst)
