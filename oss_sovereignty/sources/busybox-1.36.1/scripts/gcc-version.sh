compiler="$*"
MAJ_MIN=$(echo __GNUC__ __GNUC_MINOR__ | $compiler -E -xc - | tr -d '\r' | tail -n 1)
printf '%02d%02d\n' $MAJ_MIN
