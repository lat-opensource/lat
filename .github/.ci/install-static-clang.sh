#!/bin/bash

# Stop at any error, show all commands
set -exuo pipefail

TOOLCHAIN_PATH=/opt/clang

# Download static-clang
DEFAULT_ARCH="$(uname -m)"
if [ "${STATIC_CLANG_ARCH:-}" == "" ]; then
	STATIC_CLANG_ARCH="${RUNNER_ARCH:-${DEFAULT_ARCH}}"
fi
case "${STATIC_CLANG_ARCH}" in
	ARM64|aarch64|arm64|arm64/*) GO_ARCH=arm64;;
	ARM|armv7l|armv8l|arm|arm/v7) GO_ARCH=arm;;  # assume arm/v7 for arm
	X64|x86_64|amd64|amd64/*) GO_ARCH=amd64;;
	X86|i686|386) GO_ARCH=386;;
	loongarch64) GO_ARCH=loong64;;
	ppc64le) GO_ARCH=ppc64le;;
	riscv64) GO_ARCH=riscv64;;
	s390x) GO_ARCH=s390x;;
	*) echo "No static-clang toolchain for ${STATIC_CLANG_ARCH}">&2; exit 1;;
esac
STATIC_CLANG_VERSION=22.1.8.1
STATIC_CLANG_FILENAME="static-clang-linux-${GO_ARCH}.tar.xz"
STATIC_CLANG_URL="https://github.com/mayeut/static-clang-images/releases/download/v${STATIC_CLANG_VERSION}/${STATIC_CLANG_FILENAME}"
pushd /tmp
cat<<'EOF' | grep "${STATIC_CLANG_FILENAME}" > "${STATIC_CLANG_FILENAME}.sha256"
f3f643d7d6b0a7fdce6060d1e05e00e0f00a71107b38b32280a02d1be67faa5d  static-clang-linux-386.tar.xz
6a5419dbb658dd9c379e4cddc2a130d33aef458add0c7ccd1f529f12b0a4d67a  static-clang-linux-amd64.tar.xz
8e789c1883562f5b51f2d0e4e74c3930e2a9cce1147c63553b4dd13d99f087e7  static-clang-linux-arm.tar.xz
380e282fcf11d716f15a57bafbdb6a35ca2018f58fa343a01cb0b274887ef988  static-clang-linux-arm64.tar.xz
54020fb3dee9e6af801f1f9f314f96368b7ea5aef460dc1cbda8a8527b88b78b  static-clang-linux-loong64.tar.xz
f7a82ff0e2a9dbef1dd5ee8d5bbf8c8cbaf4c05e36ebd8ae969b8225c66f4b6d  static-clang-linux-ppc64le.tar.xz
ee928c2a94b39e895c3e800fc2ea89c326e20d5daf4af37af6cf12fe4a3d1f03  static-clang-linux-riscv64.tar.xz
84f6508a7eaca447e91a15ce61b07eff7a38383255ef3de43b88f5ab7afe9ea9  static-clang-linux-s390x.tar.xz
EOF
curl -fsSLO "${STATIC_CLANG_URL}"
sha256sum -c "${STATIC_CLANG_FILENAME}.sha256"
tar -C /opt -xf "${STATIC_CLANG_FILENAME}"
popd

# configure target triple
DEFAULT_POLICY="manylinux_2_38"
if ldd /bin/ls 2>&1 | grep -q 'musl'; then
	DEFAULT_POLICY="musllinux_1_2"
fi
if [ "${AUDITWHEEL_POLICY:-}" == "" ]; then
	AUDITWHEEL_POLICY="${DEFAULT_POLICY}"
fi
if [ "${AUDITWHEEL_ARCH:-}" == "" ]; then
	AUDITWHEEL_ARCH="${DEFAULT_ARCH}"
fi
if [ "${AUDITWHEEL_PLAT:-}" == "" ]; then
	AUDITWHEEL_PLAT="${AUDITWHEEL_POLICY}-${AUDITWHEEL_ARCH}"
fi
case "${AUDITWHEEL_PLAT}" in
	manylinux*-armv7l) TARGET_TRIPLE=armv7-unknown-linux-gnueabihf;;
	musllinux*-armv7l) TARGET_TRIPLE=armv7-alpine-linux-musleabihf;;
	manylinux*-ppc64le) TARGET_TRIPLE=powerpc64le-unknown-linux-gnu;;
	musllinux*-ppc64le) TARGET_TRIPLE=powerpc64le-alpine-linux-musl;;
	manylinux*-*) TARGET_TRIPLE=${AUDITWHEEL_ARCH}-unknown-linux-gnu;;
	musllinux*-*) TARGET_TRIPLE=${AUDITWHEEL_ARCH}-alpine-linux-musl;;
esac
case "${AUDITWHEEL_PLAT}" in
	*-riscv64) M_ARCH="-march=rv64gc";;
	*-x86_64) M_ARCH="-march=x86-64";;
	*-armv7l) M_ARCH="-march=armv7a";;
	manylinux*-i686) M_ARCH="-march=k8 -mtune=generic";;  # same as gcc manylinux2014 / manylinux_2_28
	musllinux*-i686) M_ARCH="-march=pentium-m -mtune=generic";;  # same as gcc musllinux_1_2
esac
GCC_TRIPLE=$(gcc -dumpmachine)

pushd "${TOOLCHAIN_PATH}/bin"
ln -s clang "${TOOLCHAIN_PATH}/bin/gcc"
ln -s clang "${TOOLCHAIN_PATH}/bin/cc"
ln -s clang-cpp "${TOOLCHAIN_PATH}/bin/cpp"
ln -s clang++ "${TOOLCHAIN_PATH}/bin/g++"
ln -s clang++ "${TOOLCHAIN_PATH}/bin/c++"
ln -s lld "${TOOLCHAIN_PATH}/bin/ld"
popd

cat<<EOF >"${TOOLCHAIN_PATH}/bin/${AUDITWHEEL_PLAT}.cfg"
	-target ${TARGET_TRIPLE}
	${M_ARCH:-}
	--gcc-toolchain=${DEVTOOLSET_ROOTPATH:-}/usr
	--gcc-triple=${GCC_TRIPLE}
EOF

cat<<EOF >"${TOOLCHAIN_PATH}/bin/clang.cfg"
	@${AUDITWHEEL_PLAT}.cfg
	-fuse-ld=lld
EOF

cat<<EOF >"${TOOLCHAIN_PATH}/bin/clang++.cfg"
	@${AUDITWHEEL_PLAT}.cfg
	-fuse-ld=lld
EOF

cat<<EOF >"${TOOLCHAIN_PATH}/bin/clang-cpp.cfg"
	@${AUDITWHEEL_PLAT}.cfg
EOF
