basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh
trap_exit
if [ -z "$DEST_IP" ]; then
    [ -z "$IP6" ] && DEST_IP="198.18.0.42" || DEST_IP="FD00::1"
fi
[ -z "$DST_MAC" ]   && DST_MAC="90:e2:ba:ff:ff:ff"
[ -z "$BURST" ]     && BURST=32
[ -z "$CLONE_SKB" ] && CLONE_SKB="0" 
[ -z "$COUNT" ]     && COUNT="0" 
if [ -n "$DEST_IP" ]; then
    validate_addr${IP6} $DEST_IP
    read -r DST_MIN DST_MAX <<< $(parse_addr${IP6} $DEST_IP)
fi
if [ -n "$DST_PORT" ]; then
    read -r UDP_DST_MIN UDP_DST_MAX <<< $(parse_ports $DST_PORT)
    validate_ports $UDP_DST_MIN $UDP_DST_MAX
fi
[ -z "$APPEND" ] && pg_ctrl "reset"
for ((thread = $F_THREAD; thread <= $L_THREAD; thread++)); do
    dev=${DEV}@${thread}
    [ -z "$APPEND" ] && pg_thread $thread "rem_device_all"
    pg_thread $thread "add_device" $dev
    pg_set $dev "flag QUEUE_MAP_CPU"
    pg_set $dev "count $COUNT"
    pg_set $dev "clone_skb $CLONE_SKB"
    pg_set $dev "pkt_size $PKT_SIZE"
    pg_set $dev "delay $DELAY"
    pg_set $dev "flag NO_TIMESTAMP"
    pg_set $dev "dst_mac $DST_MAC"
    pg_set $dev "dst${IP6}_min $DST_MIN"
    pg_set $dev "dst${IP6}_max $DST_MAX"
    if [ -n "$DST_PORT" ]; then
	pg_set $dev "flag UDPDST_RND"
	pg_set $dev "udp_dst_min $UDP_DST_MIN"
	pg_set $dev "udp_dst_max $UDP_DST_MAX"
    fi
    [ ! -z "$UDP_CSUM" ] && pg_set $dev "flag UDPCSUM"
    if [[ ${BURST} -ne 0 ]]; then
	pg_set $dev "burst $BURST"
    else
	info "$dev: Not using burst"
    fi
done
function print_result() {
    for ((thread = $F_THREAD; thread <= $L_THREAD; thread++)); do
	dev=${DEV}@${thread}
	echo "Device: $dev"
	cat /proc/net/pktgen/$dev | grep -A2 "Result:"
    done
}
trap true SIGINT
if [ -z "$APPEND" ]; then
    echo "Running... ctrl^C to stop" >&2
    pg_ctrl "start"
    print_result
else
    echo "Append mode: config done. Do more or use 'pg_ctrl start' to run"
fi
