import os
PROJECT_ROOT = "/home/aimeat/anvilos"
def get_actual_src(src_path):
    if os.path.exists(src_path): return src_path
    if "/sources/" in src_path:
        alt = src_path.replace("/sources/", "/")
        if os.path.exists(alt): return alt
    if "oss_sovereignty/" in src_path and "/sources/" not in src_path:
        alt = src_path.replace("oss_sovereignty/", "oss_sovereignty/sources/")
        if os.path.exists(alt): return alt
    return src_path

src = "/home/aimeat/anvilos/oss_sovereignty/sources/util-linux-2.39.3/libfdisk/src/init.c"
print(f"Input: {src}")
print(f"Output: {get_actual_src(src)}")
