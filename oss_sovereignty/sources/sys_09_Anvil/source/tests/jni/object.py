import jni
try:
    Integer = jni.cls("java/lang/Integer")
except:
    print("SKIP")
    raise SystemExit
i = Integer(42)
print(i)
print(i.hashCode())
System = jni.cls("java/lang/System")
System.out.println(i)
