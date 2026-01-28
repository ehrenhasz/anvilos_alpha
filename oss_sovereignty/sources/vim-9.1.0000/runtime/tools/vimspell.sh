INFILE=$1
tmp="${TMPDIR-/tmp}"
OUTFILE=`mktemp -t vimspellXXXXXX || tempfile -p vimspell || echo none`
if test "$OUTFILE" = none; then
        OUTFILE=$tmp/vimspell$$
	[ -e $OUTFILE ] && { echo "Cannot use temporary file $OUTFILE, it already exists!"; exit 1 ; } 
        (umask 077; touch $OUTFILE)
fi
LOCAL_DICT=${LOCAL_DICT-$HOME/local/lib/local_dict}
if [ -f $LOCAL_DICT ]
then
	SPELL_ARGS="+$LOCAL_DICT"
fi
spell $SPELL_ARGS $INFILE | sort -u |
awk '
      {
	printf "syntax match SpellErrors \"\\<%s\\>\"\n", $0 ;
      }
END   {
	printf "highlight link SpellErrors ErrorMsg\n\n" ;
      }
' > $OUTFILE
echo "!rm $OUTFILE" >> $OUTFILE
echo $OUTFILE
