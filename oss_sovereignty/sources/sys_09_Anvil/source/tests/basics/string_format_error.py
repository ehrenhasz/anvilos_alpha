try:
    '{0:=}'.format('zzz')
except (ValueError):
    print('ValueError')
try:
    '{1:}'.format(1)
except IndexError:
    print('IndexError')
try:
    '}'.format('zzzz')
except ValueError:
    print('ValueError')
try:
    '{!'.format('a')
except ValueError:
    print('ValueError')
try:
    'abc{!d}'.format('1')
except ValueError:
    print('ValueError')
try:
    '{abc'.format('zzzz')
except ValueError:
    print('ValueError')
try:
    '{!s :}'.format(2)
except ValueError:
    print('ValueError')
try:
    '{}{0}'.format(1, 2)
except ValueError:
    print('ValueError')
try:
    '{1:}'.format(1)
except IndexError:
    print('IndexError')
try:
    '{ 0 :*^10}'.format(12)
except KeyError:
    print('KeyError')
try:
    '{0}{}'.format(1)
except ValueError:
    print('ValueError')
try:
    '{}{}'.format(1)
except IndexError:
    print('IndexError')
try:
    '{0:+s}'.format('1')
except ValueError:
    print('ValueError')
try:
    '{0:+c}'.format(1)
except ValueError:
    print('ValueError')
try:
    '{0:s}'.format(1)
except ValueError:
    print('ValueError')
try:
    '{:*"1"}'.format('zz')
except ValueError:
    print('ValueError')
try:
    '{:X}'.format('zz')
except ValueError:
    print('ValueError')
