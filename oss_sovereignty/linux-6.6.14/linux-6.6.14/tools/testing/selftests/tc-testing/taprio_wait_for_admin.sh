TC="$1"; shift
ETH="$1"; shift
while :; do
	has_admin="$($TC -j qdisc show dev $ETH root | jq '.[].options | has("admin")')"
	if [ "$has_admin" = "false" ]; then
		break;
	fi
	sleep 1
done
