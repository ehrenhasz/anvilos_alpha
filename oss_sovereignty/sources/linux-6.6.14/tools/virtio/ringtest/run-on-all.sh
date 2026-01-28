CPUS_ONLINE=$(lscpu --online -p=cpu|grep -v -e '
HOST_AFFINITY=$(echo "${CPUS_ONLINE}"|tail -n 1)
for cpu in $CPUS_ONLINE
do
	if
		(echo "$@" | grep -e "--sleep" > /dev/null) || \
			test $HOST_AFFINITY '!=' $cpu
	then
		echo "GUEST AFFINITY $cpu"
		"$@" --host-affinity $HOST_AFFINITY --guest-affinity $cpu
	fi
done
echo "NO GUEST AFFINITY"
"$@" --host-affinity $HOST_AFFINITY
echo "NO AFFINITY"
"$@"
