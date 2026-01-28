OUTPUT_H_FILE=local_config.h
OUTPUT_MKFILE=local_config.mk
tmpname=$(mktemp)
tmpfile_c=${tmpname}.c
tmpfile_o=${tmpname}.o
echo "
echo "
echo "int func(void) { return 0; }" >> $tmpfile_c
CC=${1:?"Usage: $0 <compiler> 
$CC -c $tmpfile_c -o $tmpfile_o >/dev/null 2>&1
if [ -f $tmpfile_o ]; then
    echo "
    echo "IOURING_EXTRA_LIBS = -luring"          > $OUTPUT_MKFILE
else
    echo "// No liburing support found"          > $OUTPUT_H_FILE
    echo "
    echo "IOURING_EXTRA_LIBS = "                >> $OUTPUT_MKFILE
fi
rm ${tmpname}.*
