for n in range(20):
    print('testing dict with {} items'.format(n))
    for i in range(n):
        d = dict()
        for j in range(n):
            d[str(j)] = j
        print(len(d))
        del d[str(i)]
        print(len(d))
        for j in range(n):
            if str(j) in d:
                if j == i:
                    print(j, 'in d, but it should not be')
            else:
                if j != i:
                    print(j, 'not in d, but it should be')
