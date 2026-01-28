s = "asdf©qwer"
for i in range(len(s)):
    print("s[%d]: %s   %X" % (i, s[i], ord(s[i])))
s = "a\xA9\xFF\u0123\u0800\uFFEE\U0001F44C"
for i in range(-len(s), len(s)):
    print("s[%d]: %s   %X" % (i, s[i], ord(s[i])))
    print("s[:%d]: %d chars, '%s'" % (i, len(s[:i]), s[:i]))
    for j in range(i, len(s)):
        print("s[%d:%d]: %d chars, '%s'" % (i, j, len(s[i:j]), s[i:j]))
    print("s[%d:]: %d chars, '%s'" % (i, len(s[i:]), s[i:]))
enc = s.encode()
print(enc, enc.decode() == s)
print(repr("a\uffff"))
print(repr("a\U0001ffff"))
try:
    eval('"\\U00110000"')
except SyntaxError:
    print("SyntaxError")
try:
    int("\u0200")
except ValueError:
    print("ValueError")
try:
    str(b"ab\xa1", "utf8")
except UnicodeError:
    print("UnicodeError")
try:
    str(b"ab\xf8", "utf8")
except UnicodeError:
    print("UnicodeError")
try:
    str(bytearray(b"ab\xc0a"), "utf8")
except UnicodeError:
    print("UnicodeError")
try:
    str(b"\xf0\xe0\xed\xe8", "utf8")
except UnicodeError:
    print("UnicodeError")
