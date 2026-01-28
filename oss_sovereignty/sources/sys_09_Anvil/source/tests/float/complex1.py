print(complex(1))
print(complex(1.2))
print(complex(1.2j))
print(complex("j"))
print(complex("J"))
print(complex("1"))
print(complex("1.2"))
print(complex("1.2j"))
print(complex("1+j"))
print(complex("1+2j"))
print(complex("-1-2j"))
print(complex("+1-2j"))
print(complex(" -1-2j "))
print(complex(" +1-2j "))
print(complex("nanj"))
print(complex("nan-infj"))
print(complex(1, 2))
print(complex(1j, 2j))
print(bool(1j))
print(+(1j))
print(-(1 + 2j))
print(1j + False)
print(1j + True)
print(1j + 2)
print(1j + 2j)
print(1j - 2)
print(1j - 2j)
print(1j * 2)
print(1j * 2j)
print(1j / 2)
print((1j / 2j).real)
print(1j / (1 + 2j))
ans = 0j**0
print("%.5g %.5g" % (ans.real, ans.imag))
ans = 0j**1
print("%.5g %.5g" % (ans.real, ans.imag))
ans = 0j**0j
print("%.5g %.5g" % (ans.real, ans.imag))
ans = 1j**2.5
print("%.5g %.5g" % (ans.real, ans.imag))
ans = 1j**2.5j
print("%.5g %.5g" % (ans.real, ans.imag))
print(1j == 1)
print(1j == 1j)
print(0 + 0j == False, 1 + 0j == True)
print(False == 0 + 0j, True == 1 + 0j)
nan = float("nan") * 1j
print(nan == 1j)
print(nan == nan)
print(abs(1j))
print("%.5g" % abs(1j + 2))
print(hash(1 + 0j))
print(type(hash(1j)))
print(1.2 + 3j)
ans = (-1) ** 2.3
print("%.5g %.5g" % (ans.real, ans.imag))
ans = (-1.2) ** -3.4
print("%.5g %.5g" % (ans.real, ans.imag))
print(float("nan") * 1j)
print(float("-nan") * 1j)
print(float("inf") * (1 + 1j))
print(float("-inf") * (1 + 1j))
for test in ("1+2", "1j+2", "1+2j+3", "1+2+3j", "1 + 2j"):
    try:
        complex(test)
    except ValueError:
        print("ValueError", test)
try:
    (1j).imag = 0
except AttributeError:
    print("AttributeError")
try:
    1j + []
except TypeError:
    print("TypeError")
try:
    ~(1j)
except TypeError:
    print("TypeError")
try:
    1j // 2
except TypeError:
    print("TypeError")
try:
    1j < 2j
except TypeError:
    print("TypeError")
try:
    print(1 | 1j)
except TypeError:
    print("TypeError")
try:
    1j / 0
except ZeroDivisionError:
    print("ZeroDivisionError")
try:
    0j**-1
except ZeroDivisionError:
    print("ZeroDivisionError")
try:
    0j**1j
except ZeroDivisionError:
    print("ZeroDivisionError")
