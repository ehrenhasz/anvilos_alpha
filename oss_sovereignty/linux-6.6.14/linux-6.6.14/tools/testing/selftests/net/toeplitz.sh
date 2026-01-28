source setup_loopback.sh
readonly SERVER_IP4="192.168.1.200/24"
readonly SERVER_IP6="fda8::1/64"
readonly SERVER_MAC="aa:00:00:00:00:02"
readonly CLIENT_IP4="192.168.1.100/24"
readonly CLIENT_IP6="fda8::2/64"
readonly CLIENT_MAC="aa:00:00:00:00:01"
PORT=8000
KEY="$(</proc/sys/net/core/netdev_rss_key)"
TEST_RSS=false
RPS_MAP=""
PROTO_FLAG=""
IP_FLAG=""
DEV="eth0"
get_rss_cfg_num_rxqs() {
	echo $(ethtool -x "${DEV}" |
		grep -E [[:space:]]+[0-9]+:[[:space:]]+ |
		cut -d: -f2- |
		awk '{$1=$1};1' |
		tr ' ' '\n' |
		sort -u |
		wc -l)
}
get_rx_irq_cpus() {
	CPUS=""
	SORTED_IRQS=$(for i in /sys/kernel/irq/*; do echo $i; done | sort -V)
	RSS_CFG_NUM_RXQS=$(get_rss_cfg_num_rxqs)
	RXQ_COUNT=0
	for i in ${SORTED_IRQS}
	do
		[[ "${RXQ_COUNT}" -lt "${RSS_CFG_NUM_RXQS}" ]] || break
		[[ -e "$i/actions" ]] || continue
		cat "$i/actions" | grep -q "${IRQ_PATTERN}" || continue
		irqname=$(<"$i/actions")
		irqcount=$(cat "$i/per_cpu_count" | tr -d '0,')
		[[ -n "${irqcount}" ]] || continue
		irq=$(basename "$i")
		cpu=$(cat "/proc/irq/$irq/smp_affinity_list")
		if [[ -z "${CPUS}" ]]; then
			CPUS="${cpu}"
		else
			CPUS="${CPUS},${cpu}"
		fi
		RXQ_COUNT=$((RXQ_COUNT+1))
	done
	echo "${CPUS}"
}
get_disable_rfs_cmd() {
	echo "echo 0 > /proc/sys/net/core/rps_sock_flow_entries;"
}
get_set_rps_bitmaps_cmd() {
	CMD=""
	for i in /sys/class/net/${DEV}/queues/rx-*/rps_cpus
	do
		CMD="${CMD} echo $1 > ${i};"
	done
	echo "${CMD}"
}
get_disable_rps_cmd() {
	echo "$(get_set_rps_bitmaps_cmd 0)"
}
die() {
	echo "$1"
	exit 1
}
check_nic_rxhash_enabled() {
	local -r pattern="receive-hashing:\ on"
	ethtool -k "${DEV}" | grep -q "${pattern}" || die "rxhash must be enabled"
}
parse_opts() {
	local prog=$0
	shift 1
	while [[ "$1" =~ "-" ]]; do
		if [[ "$1" = "-irq_prefix" ]]; then
			shift
			IRQ_PATTERN="^$1-[0-9]*$"
		elif [[ "$1" = "-u" || "$1" = "-t" ]]; then
			PROTO_FLAG="$1"
		elif [[ "$1" = "-4" ]]; then
			IP_FLAG="$1"
			SERVER_IP="${SERVER_IP4}"
			CLIENT_IP="${CLIENT_IP4}"
		elif [[ "$1" = "-6" ]]; then
			IP_FLAG="$1"
			SERVER_IP="${SERVER_IP6}"
			CLIENT_IP="${CLIENT_IP6}"
		elif [[ "$1" = "-rss" ]]; then
			TEST_RSS=true
		elif [[ "$1" = "-rps" ]]; then
			shift
			RPS_MAP="$1"
		elif [[ "$1" = "-i" ]]; then
			shift
			DEV="$1"
		else
			die "Usage: ${prog} (-i <iface>) -u|-t -4|-6 \
			     [(-rss -irq_prefix <irq-pattern-prefix>)|(-rps <rps_map>)]"
		fi
		shift
	done
}
setup() {
	setup_loopback_environment "${DEV}"
	setup_macvlan_ns "${DEV}" server_ns server \
	"${SERVER_MAC}" "${SERVER_IP}"
	setup_macvlan_ns "${DEV}" client_ns client \
	"${CLIENT_MAC}" "${CLIENT_IP}"
}
cleanup() {
	cleanup_macvlan_ns server_ns server client_ns client
	cleanup_loopback "${DEV}"
}
parse_opts $0 $@
setup
trap cleanup EXIT
check_nic_rxhash_enabled
if [[ "${TEST_RSS}" = true ]]; then
	eval "$(get_disable_rfs_cmd) $(get_disable_rps_cmd)" \
	  ip netns exec server_ns ./toeplitz "${IP_FLAG}" "${PROTO_FLAG}" \
	  -d "${PORT}" -i "${DEV}" -k "${KEY}" -T 1000 \
	  -C "$(get_rx_irq_cpus)" -s -v &
elif [[ ! -z "${RPS_MAP}" ]]; then
	eval "$(get_disable_rfs_cmd) $(get_set_rps_bitmaps_cmd ${RPS_MAP})" \
	  ip netns exec server_ns ./toeplitz "${IP_FLAG}" "${PROTO_FLAG}" \
	  -d "${PORT}" -i "${DEV}" -k "${KEY}" -T 1000 \
	  -r "0x${RPS_MAP}" -s -v &
else
	ip netns exec server_ns ./toeplitz "${IP_FLAG}" "${PROTO_FLAG}" \
	  -d "${PORT}" -i "${DEV}" -k "${KEY}" -T 1000 -s -v &
fi
server_pid=$!
ip netns exec client_ns ./toeplitz_client.sh "${PROTO_FLAG}" \
  "${IP_FLAG}" "${SERVER_IP%%/*}" "${PORT}" &
client_pid=$!
wait "${server_pid}"
exit_code=$?
kill -9 "${client_pid}"
if [[ "${exit_code}" -eq 0 ]]; then
	echo "Test Succeeded!"
fi
exit "${exit_code}"
