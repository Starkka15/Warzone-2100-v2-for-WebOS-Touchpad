#!/bin/bash
export PATH=/home/stark/arm-2011.03/bin:$PATH
export CC=arm-none-linux-gnueabi-gcc
export CXX=arm-none-linux-gnueabi-g++
export CFLAGS="-DUSE_GLES -Wno-cast-align -I/home/stark/webos-deps/install/include -I/home/stark/HPwebOS/PDK/include -I/home/stark/HPwebOS/PDK/include/SDL"
export CXXFLAGS="-DUSE_GLES -Wno-cast-align -I/home/stark/webos-deps/install/include -I/home/stark/HPwebOS/PDK/include -I/home/stark/HPwebOS/PDK/include/SDL"
export LDFLAGS="-L/home/stark/webos-deps/install/lib -L/home/stark/HPwebOS/PDK/device/lib"
export PKG_CONFIG_PATH="/home/stark/webos-deps/install/lib/pkgconfig"

# SDL
export SDL_CFLAGS="-I/home/stark/HPwebOS/PDK/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT"
export SDL_LIBS="-L/home/stark/HPwebOS/PDK/device/lib -lSDL"

# OpenAL
export OPENAL_CFLAGS="-I/home/stark/HPwebOS/PDK/include"
export OPENAL_LIBS="-L/home/stark/HPwebOS/PDK/device/lib -lopenal"

# PNG
export PNG_CFLAGS="-I/home/stark/HPwebOS/PDK/include"
export PNG_LIBS="-L/home/stark/HPwebOS/PDK/device/lib -lpng12 -lz"

# Ogg/Vorbis/Theora
export OGGVORBIS_CFLAGS="-I/home/stark/webos-deps/install/include"
export OGGVORBIS_LIBS="-L/home/stark/webos-deps/install/lib -lvorbisfile -lvorbis -logg"
export THEORA_CFLAGS="-I/home/stark/webos-deps/install/include"
export THEORA_LIBS="-L/home/stark/webos-deps/install/lib -ltheoradec -logg"

# Physfs and popt
export PHYSFS_CFLAGS="-I/home/stark/webos-deps/install/include"
export PHYSFS_LIBS="-L/home/stark/webos-deps/install/lib -lphysfs"
export POPT_CFLAGS="-I/home/stark/webos-deps/install/include"
export POPT_LIBS="-L/home/stark/webos-deps/install/lib -lpopt"

./configure --host=arm-none-linux-gnueabi --disable-nls --disable-motif "$@"
