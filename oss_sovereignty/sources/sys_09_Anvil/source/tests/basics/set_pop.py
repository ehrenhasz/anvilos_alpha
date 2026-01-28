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
    print(s.pop()) 
for i in range(N):
    s.add(i) 
print(sorted(s))
