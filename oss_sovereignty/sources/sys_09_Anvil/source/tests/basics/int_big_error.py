i = 1 << 65
try:
    i << -1
except ValueError:
    print("ValueError")
try:
    i @ 0
except TypeError:
    print("TypeError")
try:
    i @= 0
except TypeError:
    print("TypeError")
try:
    len(i)
except TypeError:
    print("TypeError")
try:
    1 in i
except TypeError:
    print("TypeError")
try:
    bytearray(i)
except OverflowError:
    print('OverflowError')
try:
    i << (-(i >> 40))
except ValueError:
    print('ValueError')
try:
    i // 0
except ZeroDivisionError:
    print('ZeroDivisionError')
try:
    i % 0
except ZeroDivisionError:
    print('ZeroDivisionError')
