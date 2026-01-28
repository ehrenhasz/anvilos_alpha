basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh
trap_exit
if [ -z "$DEST_IP" ]; then
    [ -z "$IP6" ] && DEST_IP="198.18.0.42" || DEST_IP="FD00::1"
fi
[ -z "$CLONE_SKB" ] && CLONE_SKB="0"
[ -z "$DST_MAC" ] && usage && err 2 "Must specify -m dst_mac"
[ -z "$COUNT" ]   && COUNT="100000" 
if [ -n "$DEST_IP" ]; then
    validate_addr${IP6} $DEST_IP
    read -r DST_MIN DST_MAX <<< $(parse_addr${IP6} $DEST_IP)
fi
if [ -n "$DST_PORT" ]; then
    read -r UDP_DST_MIN UDP_DST_MAX <<< $(parse_ports $DST_PORT)
    validate_ports $UDP_DST_MIN $UDP_DST_MAX
fi
UDP_SRC_MIN=9
UDP_SRC_MAX=109
[ -z "$APPEND" ] && pg_ctrl "reset"
thread=0
[ -z "$APPEND" ] && pg_thread $thread "rem_device_all"
pg_thread $thread "add_device" $DEV
pg_set $DEV "count $COUNT"
pg_set $DEV "clone_skb $CLONE_SKB"
pg_set $DEV "pkt_size $PKT_SIZE"
pg_set $DEV "delay $DELAY"
pg_set $DEV "flag NO_TIMESTAMP"
pg_set $DEV "dst_mac $DST_MAC"
pg_set $DEV "dst${IP6}_min $DST_MIN"
pg_set $DEV "dst${IP6}_max $DST_MAX"
if [ -n "$DST_PORT" ]; then
    pg_set $DEV "flag UDPDST_RND"
    pg_set $DEV "udp_dst_min $UDP_DST_MIN"
    pg_set $DEV "udp_dst_max $UDP_DST_MAX"
fi
[ ! -z "$UDP_CSUM" ] && pg_set $dev "flag UDPCSUM"
pg_set $DEV "flag UDPSRC_RND"
pg_set $DEV "udp_src_min $UDP_SRC_MIN"
pg_set $DEV "udp_src_max $UDP_SRC_MAX"
function print_result() {
    echo "Result device: $DEV"
    cat /proc/net/pktgen/$DEV
}
trap true SIGINT
if [ -z "$APPEND" ]; then
    echo "Running... ctrl^C to stop" >&2
    pg_ctrl "start"
    echo "Done" >&2
    print_result
else
    echo "Append mode: config done. Do more or use 'pg_ctrl start' to run"
fi
