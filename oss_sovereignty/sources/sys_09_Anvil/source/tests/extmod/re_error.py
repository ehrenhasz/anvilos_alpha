try:
    import re
except ImportError:
    print("SKIP")
    raise SystemExit
def test_re(r):
    try:
        re.compile(r)
        print("OK")
    except:  
        print("Error")
test_re(r"?")
test_re(r"*")
test_re(r"+")
test_re(r")")
test_re(r"[")
test_re(r"([")
test_re(r"([)")
test_re(r"[a\]")
