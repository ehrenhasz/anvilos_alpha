def f():
    try:
        raise TypeError
    finally:
        print(1)
        try:
            raise ValueError
        finally:
            return 42
print(f())
def f():
    try:
        raise ValueError
    finally:
        print(1)
        try:
            raise TypeError
        finally:
            print(2)
            try:
                pass
            finally:
                return 42
print(f())
def f():
    try:
        raise ValueError
    finally:
        print(1)
        try:
            raise TypeError
        finally:
            print(2)
            try:
                raise Exception
            finally:
                print(3)
                return 42
print(f())
def f():
    try:
        try:
            pass
        finally:
            print(2)
            return 42
    finally:
        print(1)
print(f())
def f():
    try:
        try:
            raise ValueError
        finally:
            print(2)
            return 42
    finally:
        print(1)
print(f())
def f():
    try:
        raise Exception
    finally:
        print(1)
        try:
            try:
                pass
            finally:
                print(3)
                return 42
        finally:
            print(2)
print(f())
def f():
    try:
        raise Exception
    finally:
        print(1)
        try:
            try:
                raise Exception
            finally:
                print(3)
                return 42
        finally:
            print(2)
print(f())
