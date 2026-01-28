s = {1}
print(s.pop())
try:
    print(s.pop(), "!!!")
except KeyError:
    pass
else:
    print("Failed to raise KeyError")
N = 11
s = set(range(N))
while s:
    print(s.pop()) # last pop() should trigger the optimisation
for i in range(N):
    s.add(i) # check that we can add the numbers back to the set
print(sorted(s))
