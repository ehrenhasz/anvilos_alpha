import re
def repl(m): return "replacement"
try:
    print(re.sub(r'foo', repl, "foo bar"))
except Exception as e:
    print(e)
