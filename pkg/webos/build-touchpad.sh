#!/bin/bash
# Builds Warzone 2100 for the HP TouchPad in WSL.
# The 2011 toolchain cannot read Windows drives, so the tree is mirrored to an
# ext4 disk and built there; the stripped binary is copied back to pkg/webos.
#   build-touchpad.sh [make args]
# Needs the libraries from build-deps.sh and the PDK at $PDK.
set -e
SRC=${SRC:-/mnt/h/warzone2100-webos}
BUILD=${BUILD:-/mnt/luneos/webos-pdk/build/warzone2100-webos}
DEPS=${DEPS:-/mnt/luneos/webos-pdk/build/wz-deps/install}
PDK=${PDK:-/opt/PalmPDK}
TC=${TC:-/mnt/luneos/webos-pdk/arm-cs2011.03-42-toolchain/arm-2011.03}
HOST=arm-none-linux-gnueabi

mkdir -p "$BUILD"
rsync -a --exclude .git --exclude '*.o' --exclude '*.a' --exclude '*.ipk' \
	--exclude 'pkg/webos/build' --exclude 'pkg/webos/bundle' --exclude '*.tar.gz' \
	--exclude 'src/warzone2100' --exclude 'src/warzone2100.debug' \
	--exclude Makefile --exclude Makefile.in --exclude configure --exclude config.status \
	--exclude config.h --exclude config.log --exclude autom4te.cache --exclude aclocal.m4 \
	"$SRC/" "$BUILD/"
cd "$BUILD"

# The Windows checkout has CRLF line endings; the autotools and shell need LF
grep -rIl $'\r' --exclude-dir=.deps . | xargs -r sed -i 's/\r$//'

export PATH=$TC/bin:$PATH
OPT=${OPT-"-O2 -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp"}
INC="-I$DEPS/include -I$PDK/include -I$PDK/include/SDL"
export CC=$HOST-gcc CXX=$HOST-g++
export CFLAGS="$OPT -DUSE_GLES -Wno-cast-align $INC"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$DEPS/lib -L$PDK/device/lib -Wl,-rpath-link,$PDK/device/lib"
export PKG_CONFIG_LIBDIR=$DEPS/lib/pkgconfig PKG_CONFIG_PATH=

# The PDK has no pkg-config files for its own libraries
export SDL_CFLAGS="-I$PDK/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT" SDL_LIBS="-lSDL"
export OPENAL_CFLAGS="-I$PDK/include" OPENAL_LIBS="-lopenal"
export PNG_CFLAGS="-I$PDK/include" PNG_LIBS="-lpng12 -lz"
export OGGVORBIS_CFLAGS="-I$DEPS/include" OGGVORBIS_LIBS="$DEPS/lib/libvorbisfile.a $DEPS/lib/libvorbis.a $DEPS/lib/libogg.a"
export THEORA_CFLAGS="-I$DEPS/include" THEORA_LIBS="$DEPS/lib/libtheoradec.a $DEPS/lib/libogg.a"

if [ ! -f configure ]; then
	./autogen.sh
fi
if [ ! -f config.status ]; then
	./configure --host=$HOST --disable-nls --disable-motif --disable-shared --enable-debug=${DEBUG:-no}
fi
make -j8 "$@"

$HOST-strip -o "$SRC/pkg/webos/${OUT:-warzone2100}" src/warzone2100
ls -la "$SRC/pkg/webos/${OUT:-warzone2100}"
