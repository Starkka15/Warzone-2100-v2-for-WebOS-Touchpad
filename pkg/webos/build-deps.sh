#!/bin/bash
# Builds the static libraries Warzone links (ogg, vorbis, theora, physfs, popt)
# with the CodeSourcery 2011.03 toolchain, tuned for the TouchPad's Cortex-A8.
# Run in WSL on an ext4 disk: the 32-bit toolchain cannot read Windows drives.
#   build-deps.sh [work dir]   (default /mnt/luneos/webos-pdk/build/wz-deps)
set -e
WORK=${1:-/mnt/luneos/webos-pdk/build/wz-deps}
TC=${TC:-/mnt/luneos/webos-pdk/arm-cs2011.03-42-toolchain/arm-2011.03}
HOST=arm-none-linux-gnueabi
PREFIX=$WORK/install
export PATH=$TC/bin:$PATH
export CC=$HOST-gcc CXX=$HOST-g++ AR=$HOST-ar RANLIB=$HOST-ranlib
export CFLAGS="-O2 -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -fPIC"
export CPPFLAGS="-I$PREFIX/include"
export LDFLAGS="-L$PREFIX/lib"
export PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig PKG_CONFIG_PATH=

mkdir -p "$WORK/src" "$PREFIX"
cd "$WORK/src"

fetch() { [ -f "${2:-$(basename "$1")}" ] || wget -q -O "${2:-$(basename "$1")}" "$1"; }
fetch https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.xz
fetch https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz
fetch https://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.bz2
fetch https://ftp.osuosl.org/pub/rpm/popt/releases/popt-1.x/popt-1.18.tar.gz
fetch https://icculus.org/physfs/downloads/physfs-2.0.3.tar.bz2

conf() { # dir, extra configure args
	rm -rf "$1"; tar xf "$1".tar.*
	(cd "$1" && ./configure --host=$HOST --prefix="$PREFIX" --disable-shared --enable-static "${@:2}" >../$1.log 2>&1 && make -j8 >>../$1.log 2>&1 && make install >>../$1.log 2>&1) || { tail -30 $1.log; exit 1; }
	echo "built $1"
}

conf libogg-1.3.5
conf libvorbis-1.3.7 --disable-oggtest --disable-docs --disable-examples
conf libtheora-1.1.1 --disable-oggtest --disable-vorbistest --disable-sdltest --disable-examples --disable-spec --disable-encode
conf popt-1.18 --disable-nls

rm -rf physfs-2.0.3; tar xf physfs-2.0.3.tar.bz2
mkdir -p physfs-2.0.3/build && cd physfs-2.0.3/build
cat > tc.cmake <<EOT
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER $TC/bin/$HOST-gcc)
set(CMAKE_CXX_COMPILER $TC/bin/$HOST-g++)
EOT
(cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_TOOLCHAIN_FILE=tc.cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
	-DPHYSFS_BUILD_SHARED=OFF -DPHYSFS_BUILD_TEST=OFF -DPHYSFS_BUILD_WX_TEST=OFF \
	-DPHYSFS_HAVE_CDROM_SUPPORT=OFF && make -j8 && make install) >../../physfs.log 2>&1 || { tail -30 ../../physfs.log; exit 1; }
echo "built physfs-2.0.3"
ls "$PREFIX/lib"
