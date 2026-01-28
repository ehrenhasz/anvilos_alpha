set -e
DEST_DIR="oss_sovereignty/ctx_07_vite"
mkdir -p oss_sovereignty
git clone https://github.com/vitejs/vite.git "$DEST_DIR"
rm -rf "$DEST_DIR/.git"
echo "Vite repository cloned and git ties severed in $DEST_DIR"
