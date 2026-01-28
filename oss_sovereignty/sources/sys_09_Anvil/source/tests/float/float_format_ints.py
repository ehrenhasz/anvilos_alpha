for b in [13, 123, 457, 23456]:
    for r in range(1, 10):
        e_fmt = "{:." + str(r) + "e}"
        f_fmt = "{:." + str(r) + "f}"
        g_fmt = "{:." + str(r) + "g}"
        for e in range(0, 5):
            f = b * (10**e)
            title = str(b) + " x 10^" + str(e)
            print(title, "with format", e_fmt, "gives", e_fmt.format(f))
            print(title, "with format", f_fmt, "gives", f_fmt.format(f))
            print(title, "with format", g_fmt, "gives", g_fmt.format(f))
print("{:f}".format(16777215))
print("{:f}".format(2147483520))
print("{:.6e}".format(float("9" * 30 + "e8")))
