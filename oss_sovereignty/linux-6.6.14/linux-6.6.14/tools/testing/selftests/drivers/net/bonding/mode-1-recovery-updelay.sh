REQUIRE_MZ=no
REQUIRE_JQ=no
NUM_NETIFS=0
lib_dir=$(dirname "$0")
source "$lib_dir"/net_forwarding_lib.sh
source "$lib_dir"/lag_lib.sh
cleanup()
{
	lag_cleanup
}
trap cleanup 0 1 2
lag_setup_network
test_bond_recovery mode 1 miimon 100 updelay 0
test_bond_recovery mode 1 miimon 100 updelay 200
test_bond_recovery mode 1 miimon 100 updelay 500
test_bond_recovery mode 1 miimon 100 updelay 1000
test_bond_recovery mode 1 miimon 100 updelay 2000
test_bond_recovery mode 1 miimon 100 updelay 5000
test_bond_recovery mode 1 miimon 100 updelay 10000
exit "$EXIT_STATUS"
