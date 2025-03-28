#!/usr/bin/bash

# apt-get install aptitude
# aptitude install git ninja-build libssl-dev libc6 gcc g++ pkg-config libglib2.0-dev libdrm-dev

export CFLAGS="-Wno-error=unused-but-set-variable -Wno-error=unused-function -Wformat -Werror=format-y2k"

source_dir=$(realpath "$(dirname "$0")/../")
bin64=$source_dir/build64-release/latx-x86_64
bin32=$source_dir/build32-release/latx-i386
os=$(lsb_release -si)
install_prefix=lat-$os
install_dir=$source_dir
tag=$1
name=""

help() {
    echo "Usage:"
    echo "  -n              name"
    echo "  -h              help"
}

parseArgs() {
    while getopts "n:h" opt; do
        case ${opt} in
        n)
            name=-"$OPTARG"
            ;;
        h)
            help
            exit
            ;;
        # 若选项需要参数但未收到，则走冒号分支
        :)
            help
            exit
            ;;
        # 若遇到未指定的选项，会走问号分支
        ?)
            help
            exit
            ;;
        esac
    done
}

make_cmd() {
    install_prefix=$install_prefix$name
    install_dir=$install_dir/$install_prefix
    cd $source_dir
    rm -rf $install_prefix
    rm -f $install_prefix.tar

    # build latx-x86_64
    rm -rf build64-release
    mkdir -p build64-release
    cd build64-release

    ../configure --target-list=x86_64-linux-user --enable-latx \
                --disable-debug-info --optimize-O1 --extra-ldflags=-ldl --enable-kzt \
                --prefix=$install_dir --disable-blobs --disable-docs \
                --disable-werror --disable-linux-io-uring

    if [ ! -f "/usr/bin/ninja" ]; then
        make -j $(nproc)
    else
        ninja
        ninja install
    fi

    if [ ! -e $bin64 ]; then
        echo "latx-x86_64 ERROR"
        exit 1
    fi

    message64="latx-x86_64:\n"
    message64="$message64 \t before strip: $(ls -lh $bin64| awk '{print $5}')"
    strip latx-x86_64
    message64="$message64 \t after strip:  $(ls -lh $bin64| awk '{print $5}')\n"
    message64="$message64 \t MD5: $(md5sum $bin64)\n"
    message64="$message64 \t VERSION: $($bin64 --version)\n"

    # build latx-i386
    cd $source_dir
    rm -rf build32-release
    mkdir -p build32-release
    cd build32-release

    ../configure --target-list=i386-linux-user --enable-latx \
                --enable-guest-base-zero --disable-debug-info --optimize-O1 --static \
                --extra-ldflags=-ldl --prefix=$install_dir --disable-blobs --disable-docs  \
                --disable-werror --disable-pie --disable-linux-io-uring

    if [ ! -f "/usr/bin/ninja" ]; then
        make -j $(nproc)
    else
        ninja
        ninja install
    fi

    if [ ! -e $bin32 ]; then
        echo "latx-i386 ERROR"
        exit 1
    fi

    message32="latx-i386:\n"
    message32="$message32 \t before strip: $(ls -lh $bin32| awk '{print $5}')"
    strip latx-i386
    message32="$message32 \t after strip:  $(ls -lh $bin32| awk '{print $5}')\n"
    message32="$message32 \t MD5: $(md5sum $bin32)\n"
    message32="$message32 \t VERSION: $($bin32 --version)\n"
    cd $source_dir

    if [ -n "$tag" ]; then
        commit_message=$(git show "$tag" --no-patch --format=%B)
        printf "%s\n" "$commit_message" >> $install_dir/README
    fi

    cpu_info=$(lscpu)
    model_name=$(echo "$cpu_info" | grep "Model name" | awk -F ':' '{print $2}' | xargs)
    echo -e "\n\n\n////////////////////// $os on $model_name //////////////////////\n" >> $install_dir/README
    echo -e "uname -a" >> $install_dir/README
    uname -a  >> $install_dir/README
    echo -e >> $install_dir/README
    echo -e "/etc/os-release:" >> $install_dir/README
    cat /etc/os-release >> $install_dir/README
    echo -e >> $install_dir/README
    echo -e "gcc version:" >> $install_dir/README
    gcc --version | head -n 1  >> $install_dir/README
    echo -e >> $install_dir/README
    echo -e $message64 >> $install_dir/README
    echo -e >> $install_dir/README
    echo -e $message32 >> $install_dir/README
    cat $install_dir/README

    tar -czvf $install_prefix.tar.gz $install_prefix
}

parseArgs "$@"
make_cmd
