def f(*args):
    return len(args)
def test(n):
    pos_args = ",".join(str(i) for i in range(n))
    s = "f({}, *(100, 101), 102, 103)".format(pos_args)
    try:
        return eval(s)
    except SyntaxError:
        return "SyntaxError"
print(test(29))
print(test(70))
reached_limit = False
for i in range(30, 70):
    result = test(i)
    if reached_limit:
        if result != "SyntaxError":
            print("FAIL")
    else:
        if result == "SyntaxError":
            reached_limit = True
        else:
            if result != i + 4:
                print("FAIL")
