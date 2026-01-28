try:
    '' % ()
except TypeError:
    print("SKIP")
    raise SystemExit
print("%d" % 10)
print("%+d" % 10)
print("% d" % 10)
print("%d" % -10)
print("%d" % True)
print("%i" % -10)
print("%i" % True)
print("%u" % -10)
print("%u" % True)
print("%x" % 18)
print("%o" % 18)
print("%X" % 18)
print("%#x" % 18)
print("%#X" % 18)
print("%#6o" % 18)
print("%#6x" % 18)
print("%#06x" % 18)
print("%*d" % (5, 10))
print("%*.*d" % (2, 2, 20))
print("%*.*d" % (5, 8, 20))
for val in (-12, 12):
    print(">%8.4d<" % val)
    print(">% 8.4d<" % val)
    print(">%+8.4d<" % val)
    print(">%08.4d<" % val)
    print(">%-8.4d<" % val)
    print(">%-08.4d<" % val)
    print(">%-+08.4d<" % val)
for pad in ('', ' ', '0'):
    for n in (1, 2, 3):
        for val in (-1, 0, 1):
            print(('%+' + pad + str(n) + 'd') % val)
