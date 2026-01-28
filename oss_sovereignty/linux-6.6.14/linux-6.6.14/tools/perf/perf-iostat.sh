if [[ "$1" == "list" ]] || [[ "$1" =~ ([a-f0-9A-F]{1,}):([a-f0-9A-F]{1,2})(,)? ]]; then
        DELIMITER="="
else
        DELIMITER=" "
fi
perf stat --iostat$DELIMITER$*
