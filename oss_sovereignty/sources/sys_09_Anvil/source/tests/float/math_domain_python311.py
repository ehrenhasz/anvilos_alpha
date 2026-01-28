try:
    import math
except ImportError:
    print("SKIP")
    raise SystemExit
inf = float("inf")
for name, f in (
    ("pow", math.pow),
    ("log", math.log),
    ("fmod", math.fmod),
    ("atan2", math.atan2),
    ("copysign", math.copysign),
):
    for x in ((0, -inf),):
        try:
            ans = "%.4f" % f(*x)
        except ValueError:
            ans = "ValueError"
        print("%s(%.4f, %.4f) = %s" % (name, x[0], x[1], ans))
