TARGET_DIR="oss_sovereignty/bld_01_GCC"
mkdir -p oss_sovereignty
git clone git://gcc.gnu.org/git/gcc.git "$TARGET_DIR"
rm -rf "$TARGET_DIR/.git"
echo ">> GCC cloned and Git history removed from $TARGET_DIR"
