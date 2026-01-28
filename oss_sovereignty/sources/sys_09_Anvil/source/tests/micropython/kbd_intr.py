import micropython
try:
    micropython.kbd_intr
except AttributeError:
    print("SKIP")
    raise SystemExit
micropython.kbd_intr(3)
