set -e
TARGET_DIR="oss_sovereignty/bld_05_Gzip"
mkdir -p oss_sovereignty
git clone https://git.savannah.gnu.org/git/gzip.git ""
rm -rf "/.git"
echo ">> Gzip cloned and .git removed successfully."
