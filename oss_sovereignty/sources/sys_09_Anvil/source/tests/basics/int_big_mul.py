for rhs in range(2, 11):
    lhs = 1
    for k in range(100):
        res = lhs * rhs
        print(lhs, '*', rhs, '=', res)
        lhs = res
i = 1 << 20
print(i * i)
print(i * -i)
print(-i * i)
print(-i * -i)
i = 1 << 40
print(i * i)
print(i * -i)
print(-i * i)
print(-i * -i)
