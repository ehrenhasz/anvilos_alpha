set -e
SCRIPT_REL_PATH=$(realpath $0)
SCRIPT_REL_DIR=$(dirname $SCRIPT_REL_PATH)
KDIR_ROOT_DIR=$(realpath $SCRIPT_REL_DIR/../../../../)
SCRIPT_REL_DIR=$(dirname $(realpath --relative-to=$KDIR_ROOT_DIR $SCRIPT_REL_PATH))
cd $KDIR_ROOT_DIR
if [ ! -e $PWD/$SCRIPT_REL_DIR/Makefile ]; then
	echo -e "skip:    bpftool files not found!\n"
	exit 4 
fi
for tgt in docs docs-clean; do
	make -s -C $PWD/$SCRIPT_REL_DIR $tgt;
done
