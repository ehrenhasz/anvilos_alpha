for val in (116, 1111, 1234, 5010, 11111):
    print("%.0f" % val)
    print("%.1f" % val)
    print("%.3f" % val)
for prec in range(8):
    print(("%%.%df" % prec) % 6e-5)
print("%.2e" % float("9" * 51 + "e-39"))
print("%.2e" % float("9" * 40 + "e-21"))
float("%.23e" % 1e-80)
for r in range(38):
    s = "%.12e" % float("1e-" + str(r))
    if s[0] == "0":
        print("FAIL:", s)
