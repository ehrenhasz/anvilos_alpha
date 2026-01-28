ksft_skip=4
[ -e /dev/tpmrm0 ] || exit $ksft_skip
python3 -m unittest -v tpm2_tests.SpaceTest
