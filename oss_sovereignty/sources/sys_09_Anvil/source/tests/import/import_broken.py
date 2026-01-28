import sys, pkg
print("pkg" in sys.modules)
try:
    from broken.zerodiv import x
except Exception as e:
    print(e.__class__.__name__)
print("broken.zerodiv" in sys.modules)
try:
    from broken.zerodiv import x
except Exception as e:
    print(e.__class__.__name__)
try:
    import broken.pkg2_and_zerodiv
except ZeroDivisionError:
    pass
print("pkg2" in sys.modules)
print("pkg2.mod1" in sys.modules)
print("pkg2.mod2" in sys.modules)
print("broken.zerodiv" in sys.modules)
print("broken.pkg2_and_zerodiv" in sys.modules)
