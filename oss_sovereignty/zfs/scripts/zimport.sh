BASE_DIR=$(dirname "$0")
SCRIPT_COMMON=common.sh
if [[ -f "${BASE_DIR}/${SCRIPT_COMMON}" ]]; then
	. "${BASE_DIR}/${SCRIPT_COMMON}"
else
	echo "Missing helper script ${SCRIPT_COMMON}" && exit 1
fi
PROG=zimport.sh
SRC_TAGS="zfs-0.6.5.11 master"
POOL_TAGS="all master"
POOL_CREATE_OPTIONS=
TEST_DIR=$(mktemp -u -d -p /var/tmp zimport.XXXXXXXX)
KEEP="no"
VERBOSE="no"
COLOR="yes"
REPO="https://github.com/openzfs"
IMAGES_DIR="${BASE_DIR}/zfs-images/"
IMAGES_TAR="https://github.com/openzfs/zfs-images/tarball/master"
ERROR=0
CONFIG_LOG="configure.log"
CONFIG_OPTIONS=${CONFIG_OPTIONS:-""}
MAKE_LOG="make.log"
MAKE_OPTIONS=${MAKE_OPTIONS:-"-s -j$(nproc)"}
COLOR_GREEN="\033[0;32m"
COLOR_RED="\033[0;31m"
COLOR_BROWN="\033[0;33m"
COLOR_RESET="\033[0m"
usage() {
cat << EOF
USAGE:
zimport.sh [hvl] [-r repo] [-s src-tag] [-i pool-dir] [-p pool-tag]
    [-f path] [-o options]
DESCRIPTION:
	ZPOOL import verification tests
OPTIONS:
	-h                Show this message
	-v                Verbose
	-c                No color
	-k                Keep temporary directory
	-r <repo>         Source repository ($REPO)
	-s <src-tag>...   Verify OpenZFS versions with the listed tags
	-i <pool-dir>     Pool image directory
	-p <pool-tag>...  Verify pools created with the listed tags
	-f <path>         Temporary directory to use
	-o <options>      Additional options to pass to 'zpool create'
EOF
}
while getopts 'hvckr:s:i:p:f:o:?' OPTION; do
	case $OPTION in
	h)
		usage
		exit 1
		;;
	v)
		VERBOSE="yes"
		;;
	c)
		COLOR="no"
		;;
	k)
		KEEP="yes"
		;;
	r)
		REPO="$OPTARG"
		;;
	s)
		SRC_TAGS="$OPTARG"
		;;
	i)
		IMAGES_DIR="$OPTARG"
		;;
	p)
		POOL_TAGS="$OPTARG"
		;;
	f)
		TEST_DIR="$OPTARG"
		;;
	o)
		POOL_CREATE_OPTIONS="$OPTARG"
		;;
	*)
		usage
		exit 1
		;;
	esac
done
if lsmod | grep zfs >/dev/null; then
	echo "ZFS modules must be unloaded"
	exit 1
fi
populate() {
	local ROOT=$1
	local MAX_DIR_SIZE=$2
	local MAX_FILE_SIZE=$3
	mkdir -p "$ROOT"/{a,b,c,d,e,f,g}/{h,i}
	DIRS=$(find "$ROOT")
	for DIR in $DIRS; do
		COUNT=$((RANDOM % MAX_DIR_SIZE))
		for _ in $(seq "$COUNT"); do
			FILE=$(mktemp -p "$DIR")
			SIZE=$((RANDOM % MAX_FILE_SIZE))
			dd if=/dev/urandom of="$FILE" bs=1k \
			    count="$SIZE" &>/dev/null
		done
	done
	return 0
}
SRC_DIR=$(mktemp -d -p /var/tmp/ zfs.src.XXXXXXXX)
trap 'rm -Rf "$SRC_DIR"' INT TERM EXIT
populate "$SRC_DIR" 10 100
SRC_DIR="$TEST_DIR/src"
SRC_DIR_ZFS="$SRC_DIR/zfs"
if [[ "$COLOR" = "no" ]]; then
	COLOR_GREEN=""
	COLOR_BROWN=""
	COLOR_RED=""
	COLOR_RESET=""
fi
pass_nonewline() {
	echo -n -e "${COLOR_GREEN}Pass${COLOR_RESET}\t\t"
}
skip_nonewline() {
	echo -n -e "${COLOR_BROWN}Skip${COLOR_RESET}\t\t"
}
fail_nonewline() {
	echo -n -e "${COLOR_RED}Fail${COLOR_RESET}\t\t"
}
fail() {
	echo -e "$PROG: $1" >&2
	$ZFS_SH -u >/dev/null 2>&1
	exit 1
}
src_set_vars() {
	local TAG=$1
	ZFS_TAG="$TAG"
	ZFS_DIR="$SRC_DIR_ZFS/$ZFS_TAG"
	ZFS_URL="$REPO/zfs/tarball/$ZFS_TAG"
	if [[ "$TAG" = "installed" ]]; then
		ZPOOL_CMD=$(command -v zpool)
		ZFS_CMD=$(command -v zfs)
		ZFS_SH="/usr/share/zfs/zfs.sh"
	else
		ZPOOL_CMD="./zpool"
		ZFS_CMD="./zfs"
		ZFS_SH="./scripts/zfs.sh"
	fi
}
pool_set_vars() {
	local TAG=$1
	POOL_TAG=$TAG
	POOL_BZIP=$IMAGES_DIR/$POOL_TAG.tar.bz2
	POOL_DIR=$TEST_DIR/pools/$POOL_TAG
	POOL_DIR_PRISTINE=$POOL_DIR/pristine
	POOL_DIR_COPY=$POOL_DIR/copy
	POOL_DIR_SRC="$SRC_DIR_ZFS/${POOL_TAG//zol/zfs}"
}
pool_create() {
	pool_set_vars "$1"
	src_set_vars "$1"
	if [[ "$POOL_TAG" != "installed" ]]; then
		cd "$POOL_DIR_SRC" || fail "Failed 'cd $POOL_DIR_SRC'"
	fi
	$ZFS_SH zfs="spa_config_path=$POOL_DIR_PRISTINE" || \
	    fail "Failed to load kmods"
	truncate -s 1G \
	    "$POOL_DIR_PRISTINE/vdev1" "$POOL_DIR_PRISTINE/vdev2" \
	    "$POOL_DIR_PRISTINE/vdev3" "$POOL_DIR_PRISTINE/vdev4" || \
	    fail "Failed 'truncate -s 1G ...'"
	$ZPOOL_CMD create $POOL_CREATE_OPTIONS "$POOL_TAG" raidz \
	    "$POOL_DIR_PRISTINE/vdev1" "$POOL_DIR_PRISTINE/vdev2" \
	    "$POOL_DIR_PRISTINE/vdev3" "$POOL_DIR_PRISTINE/vdev4" || \
	    fail "Failed '$ZPOOL_CMD create $POOL_CREATE_OPTIONS $POOL_TAG ...'"
	$ZFS_CMD create "$POOL_TAG/fs" || \
	    fail "Failed '$ZFS_CMD create $POOL_TAG/fs'"
	populate "/$POOL_TAG/fs/" 10 100
	$ZFS_CMD snap "$POOL_TAG/fs@snap" || \
	    fail "Failed '$ZFS_CMD snap $POOL_TAG/fs@snap'"
	$ZFS_CMD clone "$POOL_TAG/fs@snap" "$POOL_TAG/clone" || \
	    fail "Failed '$ZFS_CMD clone $POOL_TAG/fs@snap $POOL_TAG/clone'"
	rm -Rf /$POOL_TAG/clone/*
	populate "/$POOL_TAG/clone/" 10 100
	$ZPOOL_CMD scrub "$POOL_TAG" || \
	    fail "Failed '$ZPOOL_CMD scrub $POOL_TAG'"
	sleep 10
	$ZPOOL_CMD export "$POOL_TAG" || \
	    fail "Failed '$ZPOOL_CMD export $POOL_TAG'"
	$ZFS_SH -u || fail "Failed to unload kmods"
}
if [[ ! -d "$IMAGES_DIR" ]]; then
	IMAGES_DIR="$TEST_DIR/zfs-images"
	mkdir -p "$IMAGES_DIR"
	curl -sL "$IMAGES_TAR" | \
	    tar -xz -C "$IMAGES_DIR" --strip-components=1 || \
	    fail "Failed to download pool images"
fi
for TAG in $POOL_TAGS; do
	if  [[ "$TAG" = "all" ]]; then
		ALL_TAGS=$(echo "$IMAGES_DIR"/*.tar.bz2 | \
		    sed "s|$IMAGES_DIR/||g;s|.tar.bz2||g")
		NEW_TAGS="$NEW_TAGS $ALL_TAGS"
	else
		NEW_TAGS="$NEW_TAGS $TAG"
	fi
done
POOL_TAGS="$NEW_TAGS"
if [[ "$VERBOSE" = "yes" ]]; then
	echo "---------------------------- Options ----------------------------"
	echo "VERBOSE=$VERBOSE"
	echo "KEEP=$KEEP"
	echo "REPO=$REPO"
	echo "SRC_TAGS=$SRC_TAGS"
	echo "POOL_TAGS=$POOL_TAGS"
	echo "PATH=$TEST_DIR"
	echo "POOL_CREATE_OPTIONS=$POOL_CREATE_OPTIONS"
	echo
fi
if [[ ! -d "$TEST_DIR" ]]; then
	mkdir -p "$TEST_DIR"
fi
if [[ ! -d "$SRC_DIR" ]]; then
	mkdir -p "$SRC_DIR"
fi
echo "------------------------ OpenZFS Source Versions ----------------"
printf "%-16s" " "
for TAG in $SRC_TAGS; do
	src_set_vars "$TAG"
	if [[ "$TAG" = "installed" ]]; then
		ZFS_VERSION=$(modinfo zfs | awk '/version:/ { print $2; exit }')
		if [[ -n "$ZFS_VERSION" ]]; then
			printf "%-16s" "$ZFS_VERSION"
		else
			fail "ZFS is not installed"
		fi
	else
		printf "%-16s" "$TAG"
	fi
done
echo -e "\n-----------------------------------------------------------------"
printf "%-16s" "Clone ZFS"
for TAG in $SRC_TAGS; do
	src_set_vars "$TAG"
	if [[ -d "$ZFS_DIR" ]]; then
		skip_nonewline
	elif  [[ "$ZFS_TAG" = "installed" ]]; then
		skip_nonewline
	else
		cd "$SRC_DIR" || fail "Failed 'cd $SRC_DIR'"
		if [[ ! -d "$SRC_DIR_ZFS" ]]; then
			mkdir -p "$SRC_DIR_ZFS"
		fi
		git archive --format=tar --prefix="$ZFS_TAG/ $ZFS_TAG" \
		    -o "$SRC_DIR_ZFS/$ZFS_TAG.tar" &>/dev/null || \
		    rm "$SRC_DIR_ZFS/$ZFS_TAG.tar"
		if [[ -s "$SRC_DIR_ZFS/$ZFS_TAG.tar" ]]; then
			tar -xf "$SRC_DIR_ZFS/$ZFS_TAG.tar" -C "$SRC_DIR_ZFS"
			rm "$SRC_DIR_ZFS/$ZFS_TAG.tar"
			echo -n -e "${COLOR_GREEN}Local${COLOR_RESET}\t\t"
		else
			mkdir -p "$ZFS_DIR" || fail "Failed to create $ZFS_DIR"
			curl -sL "$ZFS_URL" | tar -xz -C "$ZFS_DIR" \
			    --strip-components=1 || \
			    fail "Failed to download $ZFS_URL"
			echo -n -e "${COLOR_GREEN}Remote${COLOR_RESET}\t\t"
		fi
	fi
done
printf "\n"
printf "%-16s" "Build ZFS"
for TAG in $SRC_TAGS; do
	src_set_vars "$TAG"
	if [[ -f "$ZFS_DIR/module/zfs/zfs.ko" ]]; then
		skip_nonewline
	elif  [[ "$ZFS_TAG" = "installed" ]]; then
		skip_nonewline
	else
		cd "$ZFS_DIR" || fail "Failed 'cd $ZFS_DIR'"
		make distclean &>/dev/null
		./autogen.sh >>"$CONFIG_LOG" 2>&1 || \
		    fail "Failed ZFS 'autogen.sh'"
		./configure $CONFIG_OPTIONS >>"$CONFIG_LOG" 2>&1 || \
		    fail "Failed ZFS 'configure $CONFIG_OPTIONS'"
		make $MAKE_OPTIONS >>"$MAKE_LOG" 2>&1 || \
		    fail "Failed ZFS 'make $MAKE_OPTIONS'"
		pass_nonewline
	fi
done
printf "\n"
echo "-----------------------------------------------------------------"
for TAG in $POOL_TAGS; do
	pool_set_vars "$TAG"
	SKIP=0
	printf "%-16s" "$POOL_TAG"
	rm -Rf "$POOL_DIR"
	mkdir -p "$POOL_DIR_PRISTINE"
	if [[ -f "$POOL_BZIP" ]]; then
		tar -xjf "$POOL_BZIP" -C "$POOL_DIR_PRISTINE" \
		    --strip-components=1 || \
		    fail "Failed 'tar -xjf $POOL_BZIP"
	elif  [[ "$TAG" = "installed" ]]; then
		pool_create "$TAG"
	elif [[ -d "$POOL_DIR_SRC" ]]; then
		pool_create "$TAG"
	else
		SKIP=1
	fi
	for SRC_TAG in $SRC_TAGS; do
		if [[ "$SKIP" -eq 1 ]]; then
			skip_nonewline
			continue
		fi
		src_set_vars "$SRC_TAG"
		if [[ "$SRC_TAG" != "installed" ]]; then
			cd "$ZFS_DIR" || fail "Failed 'cd $ZFS_DIR'"
		fi
		$ZFS_SH zfs="spa_config_path=$POOL_DIR_COPY"
		cp -a --sparse=always "$POOL_DIR_PRISTINE" \
		    "$POOL_DIR_COPY" || \
		    fail "Failed to copy $POOL_DIR_PRISTINE to $POOL_DIR_COPY"
		POOL_NAME=$($ZPOOL_CMD import -d "$POOL_DIR_COPY" | \
		    awk '/pool:/ { print $2; exit }')
		if ! $ZPOOL_CMD import -N -d "$POOL_DIR_COPY"
		    "$POOL_NAME" &>/dev/null; then
			fail_nonewline
			ERROR=1
		else
			$ZPOOL_CMD export "$POOL_NAME" || \
			    fail "Failed to export pool"
			pass_nonewline
		fi
		rm -Rf "$POOL_DIR_COPY"
		$ZFS_SH -u || fail "Failed to unload kmods"
	done
	printf "\n"
done
if [[ "$KEEP" = "no" ]]; then
	rm -Rf "$TEST_DIR"
fi
exit "$ERROR"
