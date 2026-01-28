def test_syntax(code):
    try:
        exec(code)
    except SyntaxError:
        print("SyntaxError")
test_syntax("@micropython.a\ndef f(): pass")
test_syntax("@micropython.a.b\ndef f(): pass")
