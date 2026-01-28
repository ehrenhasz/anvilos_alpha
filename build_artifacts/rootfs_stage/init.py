import sys
import os
import time
print('-' * 40)
print('ANVIL KERNEL (USERLAND) ONLINE')
print('Identity: Sovereign')
print(f'Python: {sys.version}')
print('-' * 40)
def main():
    print('Mounting Virtual Filesystems...')
    try:
        print('Filesystems active.')
    except Exception as e:
        print(f'FS Error: {e}')
    print('Welcome to the Hold.')
    while True:
        try:
            cmd = input('anvil> ')
            if cmd == 'exit': break
            exec(cmd)
        except Exception as e:
            print(f'Error: {e}')
if __name__ == '__main__':
    main()
