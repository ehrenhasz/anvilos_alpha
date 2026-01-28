print({1 << 66:1}) # hash big int
print({-(1 << 66):2}) # hash negative big int
class F:
    def __hash__(self):
        return 1 << 70 | 1
print(hash(F()) != 0)
print(hash(6699999999999999999999999999999999999999999999999999999999999999999999) != 0)
