try:
    import btree, io, gc
except ImportError:
    print("SKIP")
    raise SystemExit
N = 80
db = btree.open(io.BytesIO(), pagesize=512)
x = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
for i in range(N):
    db[b"thekey" + str(i)] = b"thelongvalue" + str(i)
    print(db[b"thekey" + str(i)])
    gc.collect()
db.close()
