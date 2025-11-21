#!/usr/bin/bash
set -e

pkgname=lat
pkgver=$(cat VERSION)
pkgdate=$(date +%Y%m%d)
srcdir=$(realpath "$(dirname "$0")/../")
pkgdir=$srcdir/pkg
tarballs=$pkgname-$pkgver-$pkgdate.tar.xz

parpare() {
    [ -d $srcdir/build32 ] || mkdir -p $srcdir/build32
    [ -d $srcdir/build64 ] || mkdir -p $srcdir/build64
    [ -d $pkgdir ] && rm -rf $pkgdir
    mkdir -p $pkgdir
}

build() {
    unset CXXFLAGS
    unset CFLAGS
    unset LDFLAGS
    export CFLAGS="-Wno-error=unused-but-set-variable -Wno-error=unused-function -Wformat -Werror=format-y2k"

    local _configure32_flags=(
        --target-list=i386-linux-user
        --disable-capstone
        --disable-docs
        --disable-gcrypt
        --disable-glusterfs
        --disable-gnutls
        --disable-gtk
        --disable-kvm
        --disable-libiscsi
        --disable-libnfs
        --disable-libssh
        --disable-linux-io-uring
        --disable-nettle
        --disable-opengl
        --disable-qom-cast-debug
        --disable-sdl
        --disable-tools
        --disable-tpm
        --disable-vde
        --disable-vhost-crypto
        --disable-vhost-kernel
        --disable-vhost-net
        --disable-vhost-user
        --disable-vnc
        --enable-latx
        --enable-guest-base-zero
        --disable-debug-info
        --optimize-O1
        --extra-ldflags=-ldl
        --disable-blobs
        --disable-docs
        --disable-werror
        --disable-pie
        --disable-linux-io-uring
    )

    local _configure64_flags=(
        --target-list=x86_64-linux-user
        --disable-blobs
        --disable-debug-info
        --disable-docs
        --disable-linux-io-uring
        --disable-werror
        --enable-kzt
        --enable-latx
        --extra-ldflags=-ldl
        --optimize-O1
    )

    pushd $srcdir/build32 >/dev/null
    ../configure "${_configure32_flags[@]}"
    ninja -j$(nproc)
    popd >/dev/null

    pushd $srcdir/build64 >/dev/null
    ../configure "${_configure64_flags[@]}"
    ninja -j$(nproc)
    popd >/dev/null
}

package() {
    mkdir -p $pkgdir/$pkgname-$pkgver/usr/{bin,lib/binfmt.d,lib/sysctl.d}
    install -Dm755 -s $srcdir/build32/latx-i386 $pkgdir/$pkgname-$pkgver/usr/bin/latx-i386
    install -Dm755 -s $srcdir/build64/latx-x86_64 $pkgdir/$pkgname-$pkgver/usr/bin/latx-x86_64
    cat >$pkgdir/$pkgname-$pkgver/usr/lib/binfmt.d/latx-i386.conf <<EOF
:latx-i386:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x03\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\xf4\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/latx-i386:
EOF
    cat >${pkgdir}/${pkgname}-${pkgver}/usr/lib/binfmt.d/latx-x86_64.conf <<EOF
:latx-x86_64:M::\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x3e\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\xf4\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/latx-x86_64:
EOF
    echo "vm.mmap_min_addr = 65536" >${pkgdir}/$pkgname-$pkgver/usr/lib/sysctl.d/mmap_min_addr.conf
    (
        cd $pkgdir
        tar Jcf $srcdir/$tarballs $pkgname-$pkgver
    )
}

parpare
build
package
