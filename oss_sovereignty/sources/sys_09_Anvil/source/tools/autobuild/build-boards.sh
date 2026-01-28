function copy_artefacts {
    local dest_dir=$1
    local descr=$2
    local fw_tag=$3
    local build_dir=$4
    shift 4
    for ext in $@; do
        dest=$dest_dir/$descr$fw_tag.$ext
        if [ -r $build_dir/firmware.$ext ]; then
            mv $build_dir/firmware.$ext $dest
        elif [ -r $build_dir/micropython.$ext ]; then
            mv $build_dir/micropython.$ext $dest
        elif [ $ext = app-bin -a -r $build_dir/micropython.bin ]; then
            mv $build_dir/micropython.bin $dest
        fi
    done
}
function build_board {
    if [ $
        echo "usage: $0 <board-json-file> <fw-tag> <dest-dir> <exts...>"
        return 1
    fi
    local board_json=$1
    local fw_tag=$2
    local dest_dir=$3
    shift 3
    local board=$(echo $board_json | awk -F '/' '{ print $2 }')
    local descr=$(cat $board_json | python3 -c "import json,sys; print(json.load(sys.stdin).get('id', '$board'))")
    echo "building $descr $board"
    local build_dir=/tmp/micropython-build-$board
    $MICROPY_AUTOBUILD_MAKE BOARD=$board BUILD=$build_dir && copy_artefacts $dest_dir $descr $fw_tag $build_dir $@
    rm -rf $build_dir
    for variant in `cat $board_json | python3 -c "import json,sys; print(' '.join(v for v in json.load(sys.stdin).get('variants', {}).keys() if v != 'IDF3'))"`; do
        local variant_build_dir=$build_dir-$variant
        echo "building variant $descr $board $variant"
        $MICROPY_AUTOBUILD_MAKE BOARD=$board BOARD_VARIANT=$variant BUILD=$variant_build_dir && copy_artefacts $dest_dir $descr-$variant $fw_tag $variant_build_dir $@
        rm -rf $variant_build_dir
    done
}
function build_boards {
    if [ $
        echo "usage: $0 <check-file> <fw-tag> <dest-dir> <exts...>"
        return 1
    fi
    local check_file=$1
    shift
    if [ ! -r $check_file ]; then
        echo "must be in directory containing $check_file"
        return 1
    fi
    for board_json in $(find boards/ -name board.json | sort); do
        build_board $board_json $@
    done
}
function build_cc3200_boards {
    build_boards hal/cc3200_hal.c $1 $2 zip
}
function build_esp32_boards {
    build_boards modesp32.c $1 $2 bin elf map uf2 app-bin
}
function build_esp8266_boards {
    build_boards modesp.c $1 $2 bin elf map
}
function build_mimxrt_boards {
    build_boards modmimxrt.c $1 $2 bin hex
}
function build_nrf_boards {
    build_boards nrfx_glue.h $1 $2 bin hex uf2
}
function build_renesas_ra_boards {
    build_boards ra_it.c $1 $2 bin hex
}
function build_rp2_boards {
    build_boards modrp2.c $1 $2 uf2
}
function build_samd_boards {
    build_boards samd_soc.c $1 $2 uf2
}
function build_stm32_boards {
    build_boards modpyb.c $1 $2 dfu hex
}
