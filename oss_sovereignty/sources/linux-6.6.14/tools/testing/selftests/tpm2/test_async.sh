ksft_skip=4
[ -e /dev/tpm0 ] || exit $ksft_skip
[ -e /dev/tpmrm0 ] || exit $ksft_skip
python3 -m unittest -v tpm2_tests.AsyncTest
