try:
    import time
    time.sleep_us
    time.time_ns
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
t0 = time.time_ns()
time.sleep_us(5000)
t1 = time.time_ns()
print(t0 < t1)
if 2000000 < t1 - t0 < 50000000:
    print(True)
else:
    print(t0, t1, t1 - t0)
