BPF_FILE=xdp2skb_meta_kern.o
DIR=$(dirname $0)
[ -z "$TC" ] && TC=tc
[ -z "$IP" ] && IP=ip
function usage() {
    echo ""
    echo "Usage: $0 [-vfh] --dev ethX"
    echo "  -d | --dev     :             Network device (required)"
    echo "  --flush        :             Cleanup flush TC and XDP progs"
    echo "  --list         : (\$LIST)     List TC and XDP progs"
    echo "  -v | --verbose : (\$VERBOSE)  Verbose"
    echo "  --dry-run      : (\$DRYRUN)   Dry-run only (echo commands)"
    echo ""
}
function err() {
    local exitcode=$1
    shift
    echo "ERROR: $@" >&2
    exit $exitcode
}
function info() {
    if [[ -n "$VERBOSE" ]]; then
	echo "# $@"
    fi
}
function _call_cmd() {
    local cmd="$1"
    local allow_fail="$2"
    shift 2
    if [[ -n "$VERBOSE" ]]; then
	echo "$cmd $@"
    fi
    if [[ -n "$DRYRUN" ]]; then
	return
    fi
    $cmd "$@"
    local status=$?
    if (( $status != 0 )); then
	if [[ "$allow_fail" == "" ]]; then
	    err 2 "Exec error($status) occurred cmd: \"$cmd $@\""
	fi
    fi
}
function call_tc() {
    _call_cmd "$TC" "" "$@"
}
function call_tc_allow_fail() {
    _call_cmd "$TC" "allow_fail" "$@"
}
function call_ip() {
    _call_cmd "$IP" "" "$@"
}
OPTIONS=$(getopt -o vfhd: \
    --long verbose,flush,help,list,dev:,dry-run -- "$@")
if (( $? != 0 )); then
    err 4 "Error calling getopt"
fi
eval set -- "$OPTIONS"
unset DEV
unset FLUSH
while true; do
    case "$1" in
	-d | --dev ) 
	    DEV=$2
	    info "Device set to: DEV=$DEV" >&2
	    shift 2
	    ;;
	-v | --verbose)
	    VERBOSE=yes
	    shift
	    ;;
	--dry-run )
	    DRYRUN=yes
	    VERBOSE=yes
	    info "Dry-run mode: enable VERBOSE and don't call TC+IP" >&2
	    shift
            ;;
	-f | --flush )
	    FLUSH=yes
	    shift
	    ;;
	--list )
	    LIST=yes
	    shift
	    ;;
	-- )
	    shift
	    break
	    ;;
	-h | --help )
	    usage;
	    exit 0
	    ;;
	* )
	    shift
	    break
	    ;;
    esac
done
FILE="$DIR/$BPF_FILE"
if [[ ! -e $FILE ]]; then
    err 3 "Missing BPF object file ($FILE)"
fi
if [[ -z $DEV ]]; then
    usage
    err 2 "Please specify network device -- required option --dev"
fi
function list_tc()
{
    local device="$1"
    shift
    info "Listing current TC ingress rules"
    call_tc filter show dev $device ingress
}
function list_xdp()
{
    local device="$1"
    shift
    info "Listing current XDP device($device) setting"
    call_ip link show dev $device | grep --color=auto xdp
}
function flush_tc()
{
    local device="$1"
    shift
    info "Flush TC on device: $device"
    call_tc_allow_fail filter del dev $device ingress
    call_tc_allow_fail qdisc del dev $device clsact
}
function flush_xdp()
{
    local device="$1"
    shift
    info "Flush XDP on device: $device"
    call_ip link set dev $device xdp off
}
function attach_tc_mark()
{
    local device="$1"
    local file="$2"
    local prog="tc_mark"
    shift 2
    call_tc_allow_fail qdisc del dev $device clsact 2> /dev/null
    call_tc            qdisc add dev $device clsact
    call_tc filter add dev $device ingress \
	    prio 1 handle 1 bpf da obj $file sec $prog
}
function attach_xdp_mark()
{
    local device="$1"
    local file="$2"
    local prog="xdp_mark"
    shift 2
    flush_xdp $device
    call_ip link set dev $device xdp obj $file sec $prog
}
if [[ -n $FLUSH ]]; then
    flush_tc  $DEV
    flush_xdp $DEV
    exit 0
fi
if [[ -n $LIST ]]; then
    list_tc  $DEV
    list_xdp $DEV
    exit 0
fi
attach_tc_mark  $DEV $FILE
attach_xdp_mark $DEV $FILE
