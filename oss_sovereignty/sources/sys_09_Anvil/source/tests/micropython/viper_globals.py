gl = {}
exec(
    """
@micropython.viper
def f():
    return x
""",
    gl,
)
try:
    print(gl["f"]())
except NameError:
    print("NameError")
gl["x"] = 123
print(gl["f"]())
