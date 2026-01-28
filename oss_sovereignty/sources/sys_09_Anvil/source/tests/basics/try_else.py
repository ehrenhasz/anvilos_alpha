try:
    print(1)
except:
    print(2)
else:
    print(3)
try:
    print(1)
    raise Exception
except:
    print(2)
else:
    print(3)
try:
    try:
        print(1)
        raise ValueError
    except TypeError:
        print(2)
    else:
        print(3)
except:
    print('caught')
try:
    print(1)
    try:
        print(2)
        raise Exception
    except:
        print(3)
    else:
        print(4)
except:
    print(5)
else:
    print(6)
try:
    print(1)
    raise Exception
except:
    print(2)
    try:
        print(3)
    except:
        print(4)
    else:
        print(5)
else:
    print(6)
try:
    print(1)
    raise Exception
except:
    print(2)
    try:
        print(3)
        raise Exception
    except:
        print(4)
    else:
        print(5)
else:
    print(6)
