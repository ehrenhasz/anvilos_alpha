import __main__
print(repr(__main__).startswith("<module '__main__'"))
__main__.x = 1
del __main__.x
