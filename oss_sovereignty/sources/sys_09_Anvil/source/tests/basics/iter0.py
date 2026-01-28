try:
    for i in 1:
        pass
except TypeError:
    print('TypeError')
print(iter(range(4)).__next__())
