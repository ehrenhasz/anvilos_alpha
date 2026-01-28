try:
    import random
except ImportError:
    print("SKIP")
    raise SystemExit
for b in (1, 2, 3, 4, 16, 32):
    for i in range(50):
        assert random.getrandbits(b) < (1 << b)
random.seed(0)
print(random.getrandbits(16) != 0)
random.seed(1)
r = random.getrandbits(16)
random.seed(1)
print(random.getrandbits(16) == r)
print(random.getrandbits(0))
try:
    random.getrandbits(-1)
except ValueError:
    print("ValueError")
