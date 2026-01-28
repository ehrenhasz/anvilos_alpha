def f():
    try:
        raise ValueError()
    finally:
        print('finally')
        return 0
    print('got here')
print(f())
def f():
    try:
        try:
            raise ValueError
        finally:
            print('finally 1')
        print('got here')
    finally:
        print('finally 2')
        return 2
    print('got here')
print(f())
def f():
    try:
        try:
            raise ValueError
        finally:
            print('finally 1')
            return 1
        print('got here')
    finally:
        print('finally 2')
    print('got here')
print(f())
def f():
    try:
        try:
            raise ValueError
        finally:
            print('finally 1')
            return 1
        print('got here')
    finally:
        print('finally 2')
        return 2
    print('got here')
print(f())
def f():
    try:
        try:
            raise ValueError
        except:
            raise
        print('got here')
    finally:
        print('finally')
        return 0
    print('got here')
print(f())
def f():
    try:
        try:
            try:
                raise ValueError
            except:
                raise
        except:
            raise
    finally:
        print('finally')
        return 0
print(f())
def f():
    try:
        raise ValueError
    except NonExistingError:
        pass
    finally:
        print('finally')
        return 0
print(f())
def f():
    try:
        raise ValueError
    finally:
        print('finally')
        return 0
print(f())
