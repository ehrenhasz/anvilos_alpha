SCRIPT_DIR=$(dirname $(realpath $0))
BPFTOOL_INSTALL_PATH="$SCRIPT_DIR"/tools/sbin
export PATH=$SCRIPT_DIR:$BPFTOOL_INSTALL_PATH:$PATH
python3 -m unittest -v test_bpftool.TestBpftool
