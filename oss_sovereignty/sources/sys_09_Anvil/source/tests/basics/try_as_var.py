try:
    raise ValueError(534)
except ValueError as e:
    print(type(e), e.args)
try:
    e
except NameError:
    print("NameError")
