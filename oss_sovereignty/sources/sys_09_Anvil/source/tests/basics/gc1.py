try:
    import gc
except ImportError:
    print("SKIP")
    raise SystemExit
print(gc.isenabled())
gc.disable()
print(gc.isenabled())
gc.enable()
print(gc.isenabled())
gc.collect()
if hasattr(gc, 'mem_free'):
    assert type(gc.mem_free()) is int
    assert type(gc.mem_alloc()) is int
if hasattr(gc, 'threshold'):
    assert(gc.threshold(1) is None)
    assert(gc.threshold() == 0)
    assert(gc.threshold(-1) is None)
    assert(gc.threshold() == -1)
    gc.threshold(1)
    [[], []]
    gc.threshold(-1)
