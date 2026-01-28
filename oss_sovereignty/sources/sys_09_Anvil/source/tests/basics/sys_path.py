import sys
if "__file__" not in globals() or "sys_path.py" not in __file__:
    print("SKIP")
    raise SystemExit
with open(sys.path[0] + "/sys_path.py") as f:
    for _ in range(4):
        print(f.readline())
