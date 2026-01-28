if [ "$V" = "1" ]; then
	set -x
fi
if [ $
	echo "$0 [path to nm] [path to vmlinux]" 1>&2
	exit 1
fi
nm="$1"
vmlinux="$2"
$nm "$vmlinux" | grep -e " [TA] _stext$" -e " t start_first_256B$" -e " a text_start$" -e " t start_text$" > .tmp_symbols.txt
vma=$(grep -e " [TA] _stext$" .tmp_symbols.txt | cut -d' ' -f1)
expected_start_head_addr="$vma"
start_head_addr=$(grep " t start_first_256B$" .tmp_symbols.txt | cut -d' ' -f1)
if [ "$start_head_addr" != "$expected_start_head_addr" ]; then
	echo "ERROR: head code starts at $start_head_addr, should be $expected_start_head_addr" 1>&2
	echo "ERROR: try to enable LD_HEAD_STUB_CATCH config option" 1>&2
	echo "ERROR: see comments in arch/powerpc/tools/head_check.sh" 1>&2
	exit 1
fi
top_vma=$(echo "$vma" | cut -d'0' -f1)
expected_start_text_addr=$(grep " a text_start$" .tmp_symbols.txt | cut -d' ' -f1 | sed "s/^0/$top_vma/")
start_text_addr=$(grep " t start_text$" .tmp_symbols.txt | cut -d' ' -f1)
if [ "$start_text_addr" != "$expected_start_text_addr" ]; then
	echo "ERROR: start_text address is $start_text_addr, should be $expected_start_text_addr" 1>&2
	echo "ERROR: try to enable LD_HEAD_STUB_CATCH config option" 1>&2
	echo "ERROR: see comments in arch/powerpc/tools/head_check.sh" 1>&2
	exit 1
fi
rm -f .tmp_symbols.txt
