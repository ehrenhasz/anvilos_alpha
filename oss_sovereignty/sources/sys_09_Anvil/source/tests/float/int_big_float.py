i = 1 << 65
print("%.5g" % (2.0 * i))
print("%.5g" % float(-i))
print("%.5g" % (i / 5))
print("%.5g" % (i * 1.2))
print("%.5g" % (i / 1.2))
print("%.5g" % (i * 1.2j).imag)
print("%.5g" % (i**-1))
print("%.5g" % ((2 + i - i) ** -3))
try:
    i / 0
except ZeroDivisionError:
    print("ZeroDivisionError")
