for F_FILE in Documentation/features/*/*/arch-support.txt; do
	F=$(grep "^#         Kconfig:" "$F_FILE" | cut -c26-)
	O=""
	K=$F
	if [[ "$F" == !* ]]; then
		O="not"
		K=$(echo $F | sed -e 's/^!//g')
	fi
	F_VALID="false"
	for ARCH_DIR in arch/*/; do
		K_FILES=$(find $ARCH_DIR -name "Kconfig*")
		K_GREP=$(grep "$K" $K_FILES)
		if [ ! -z "$K_GREP" ]; then
			F_VALID="true"
			break
		fi
	done
	if [ "$F_VALID" = "false" ]; then
		printf "WARNING: '%s' is not a valid Kconfig\n" "$F"
	fi
	T_FILE="$F_FILE.tmp"
	grep "^#" $F_FILE > $T_FILE
	echo "    -----------------------" >> $T_FILE
	echo "    |         arch |status|" >> $T_FILE
	echo "    -----------------------" >> $T_FILE
	for ARCH_DIR in arch/*/; do
		ARCH=$(echo $ARCH_DIR | sed -e 's/^arch//g' | sed -e 's/\///g')
		K_FILES=$(find $ARCH_DIR -name "Kconfig*")
		K_GREP=$(grep "$K" $K_FILES)
		if [ "$O" = "" ] && [ ! -z "$K_GREP" ]; then
			printf "    |%12s: |  ok  |\n" "$ARCH" >> $T_FILE
		elif [ "$O" = "not" ] && [ -z "$K_GREP" ]; then
			printf "    |%12s: |  ok  |\n" "$ARCH" >> $T_FILE
		else
			S=$(grep -v "^#" "$F_FILE" | grep " $ARCH:")
			if [ ! -z "$S" ]; then
				echo "$S" >> $T_FILE
			else
				printf "    |%12s: | TODO |\n" "$ARCH" \
					>> $T_FILE
			fi
		fi
	done
	echo "    -----------------------" >> $T_FILE
	mv $T_FILE $F_FILE
done
