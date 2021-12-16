#!/bin/bash
set -eo pipefail
ICOUTILS_VERSION=0.32.3

if [ ! -e "icoutils-$ICOUTILS_VERSION" ]; then
  wget -O /tmp/icoutils.tar.bz2 http://savannah.nongnu.org/download/icoutils/icoutils-$ICOUTILS_VERSION.tar.bz2
  tar xvf /tmp/icoutils.tar.bz2
fi

cd "icoutils-$ICOUTILS_VERSION"
cp ../files/main.c wrestool/main.c

export EMCC_CFLAGS="-s USE_LIBPNG=1 -lworkerfs.js -lnodefs.js -s EXPORTED_RUNTIME_METHODS='ccall,cwrap,UTF8ToString,StringToUTF8,stackTrace,FS,MEMFS,WORKERFS,NODEFS,writeArrayToMemory,writeStringToMemory,writeAsciiToMemory'"

emconfigure ./configure
emmake make -j4

mkdir -p ../out
cp wrestool/wrestool ../out/wrestool.js
cp wrestool/wrestool.wasm ../out/wrestool.wasm
cp COPYING ../out
