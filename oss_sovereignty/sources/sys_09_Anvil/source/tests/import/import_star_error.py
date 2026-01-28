try:
    exec("def foo(): from x import *")
except SyntaxError as er:
    print("function", "SyntaxError")
try:
    exec("class C: from x import *")
except SyntaxError as er:
    print("class", "SyntaxError")
