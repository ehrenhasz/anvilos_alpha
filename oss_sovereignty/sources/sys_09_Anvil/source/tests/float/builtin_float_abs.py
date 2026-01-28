for val in (
    "1.0",
    "-1.0",
    "0.0",
    "-0.0",
    "nan",
    "-nan",
    "inf",
    "-inf",
):
    print(val, abs(float(val)))
