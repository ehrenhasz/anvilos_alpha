if [ ! -d "$IDF_PATH_V50" ]; then
    echo "must set IDF_PATH_V50"
    exit 1
fi
if [ ! -d "$MICROPY_AUTOBUILD_MICROPYTHON_REPO" ]; then
    echo "must set MICROPY_AUTOBUILD_MICROPYTHON_REPO"
    exit 1
fi
if [ -z "$MICROPY_AUTOBUILD_MAKE" ]; then
    echo "must set MICROPY_AUTOBUILD_MAKE"
    exit 1
fi
AUTODIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source ${AUTODIR}/build-boards.sh
LOCAL_FIRMWARE=/tmp/autobuild-firmware-$$
mkdir -p ${LOCAL_FIRMWARE}
git -C ${MICROPY_AUTOBUILD_MICROPYTHON_REPO} pull
git -C ${MICROPY_AUTOBUILD_MICROPYTHON_REPO} submodule update --init
git -C ${MICROPY_AUTOBUILD_MICROPYTHON_REPO}/lib/pico-sdk submodule update --init
pushd ${MICROPY_AUTOBUILD_MICROPYTHON_REPO}
make -C mpy-cross
FW_DATE=$(date '+%Y%m%d')
FW_GIT_TAG="$(git describe --tags --dirty --always --match 'v[1-9].*')"
FW_SEMVER_MAJOR_MINOR_PATCH="$(echo $FW_GIT_TAG | cut -d'-' -f1)"
FW_SEMVER_PRERELEASE="$(echo $FW_GIT_TAG | cut -s -d'-' -f2-)"
if [ -z "$FW_SEMVER_PRERELEASE" ]; then
    FW_SEMVER="$FW_SEMVER_MAJOR_MINOR_PATCH"
else
    FW_SEMVER="$FW_SEMVER_MAJOR_MINOR_PATCH-$(echo $FW_SEMVER_PRERELEASE | tr - .)"
fi
FW_TAG="-$FW_DATE-$FW_SEMVER"
cd ports/cc3200
build_cc3200_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../esp8266
build_esp8266_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../esp32
(source ${IDF_PATH_V50}/export.sh && build_esp32_boards ${FW_TAG} ${LOCAL_FIRMWARE})
cd ../mimxrt
build_mimxrt_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../nrf
build_nrf_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../renesas-ra
build_renesas_ra_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../rp2
build_rp2_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../samd
build_samd_boards ${FW_TAG} ${LOCAL_FIRMWARE}
cd ../stm32
build_stm32_boards ${FW_TAG} ${LOCAL_FIRMWARE}
popd
if [ -z "$MICROPY_AUTOBUILD_REMOTE_MACHINE" -o -z "$MICROPY_AUTOBUILD_REMOTE_DIR" ]; then
    echo "No remote given, leaving firmware in ${LOCAL_FIRMWARE}"
    exit 0
fi
scp ${LOCAL_FIRMWARE}/* ${MICROPY_AUTOBUILD_REMOTE_MACHINE}:${MICROPY_AUTOBUILD_REMOTE_DIR}/
${AUTODIR}/remove_old_firmware.py ${MICROPY_AUTOBUILD_REMOTE_MACHINE} ${MICROPY_AUTOBUILD_REMOTE_DIR}
/bin/rm -v ${LOCAL_FIRMWARE}/*
/bin/rmdir ${LOCAL_FIRMWARE}
