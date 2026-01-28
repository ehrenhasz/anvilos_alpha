for val in (
    "0.0",
    "-0.0",
    "1.0",
    "2.0",
    "-12.0",
    "12345.0",
):
    print(val, hash(float(val)))
for val in (
    "0.1",
    "-0.1",
    "10.3",
    "0.4e3",
    "1e16",
    "inf",
    "-inf",
    "nan",
):
    print(val, type(hash(float(val))))
