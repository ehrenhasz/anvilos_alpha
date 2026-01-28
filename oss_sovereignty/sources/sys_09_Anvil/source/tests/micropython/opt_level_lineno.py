import micropython as micropython
micropython.opt_level(3)
exec("try:\n xyz\nexcept NameError as er:\n import sys\n sys.print_exception(er)")
