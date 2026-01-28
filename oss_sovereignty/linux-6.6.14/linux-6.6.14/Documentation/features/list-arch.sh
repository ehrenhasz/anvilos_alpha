ARCH=${1:-$(uname -m | sed 's/x86_64/x86/' | sed 's/i386/x86/')}
$(dirname $0)/../../scripts/get_feat.pl list --arch $ARCH
