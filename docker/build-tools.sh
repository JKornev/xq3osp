#!/bin/sh
# Build the QVM toolchain (q3lcc, q3cpp, q3rcc, q3asm) and the q3cpp date shim.
#
#   build-tools.sh <repo-root> <prefix>
#
# Sources:  <repo-root>/src/tools/lcc      id Software Quake III 1.32b lcc
#           <repo-root>/src/tools/asm      id Software Quake III 1.32b q3asm
#           <repo-root>/docker/cppdate.c   LD_PRELOAD shim pinning q3cpp's __DATE__/__TIME__
# Output:   <prefix>/bin/{q3lcc,q3cpp,q3rcc,q3asm}, <prefix>/lib/cppdate.so
#
# Sources are copied to a scratch directory first so the tree under <repo-root> is never
# written to.
set -eu

ROOT=${1:?repo root}
PREFIX=${2:?install prefix}

# Host compiler flags are id's own defaults: lcc/makefile CFLAGS=-g, q3asm/Makefile
# CFLAGS=-g -Wall. With -m32 these sources build unmodified on Debian bookworm GCC 12.
HOSTCC="gcc -m32"
LCC_CFLAGS="-g"
ASM_CFLAGS="-g -Wall"

WORK=$(mktemp -d /tmp/q3tools.XXXXXX)
mkdir -p "$PREFIX/bin" "$PREFIX/lib"

echo "== lcc (q3lcc/q3cpp/q3rcc) from $ROOT/src/tools/lcc"
cp -r "$ROOT/src/tools/lcc" "$WORK/lcc"
mkdir -p "$WORK/lccbuild"
# A git checkout does not keep mtimes, so lburg/gram.y can look newer than id's shipped
# lburg/gram.c and make's built-in %.c: %.y rule would run yacc, which the image lacks.
# Mark the shipped parser as up to date so it is used as is.
touch "$WORK/lcc/lburg/gram.c"
# id's unix makefile; all knobs overridden on the command line, makefile left untouched.
make -C "$WORK/lcc" -f makefile \
    BUILDDIR="$WORK/lccbuild" HOSTFILE=etc/linux.c TEMPDIR=/tmp \
    CC="$HOSTCC" LD="$HOSTCC" CFLAGS="$LCC_CFLAGS" \
    rcc lburg cpp lcc
install -m 755 "$WORK/lccbuild/lcc" "$PREFIX/bin/q3lcc"
install -m 755 "$WORK/lccbuild/cpp" "$PREFIX/bin/q3cpp"
install -m 755 "$WORK/lccbuild/rcc" "$PREFIX/bin/q3rcc"

echo "== q3asm from $ROOT/src/tools/asm"
cp -r "$ROOT/src/tools/asm" "$WORK/asm"
$HOSTCC $ASM_CFLAGS -o "$PREFIX/bin/q3asm" "$WORK/asm/q3asm.c" "$WORK/asm/cmdlib.c"

echo "== cppdate.so (q3cpp __DATE__/__TIME__ shim, see docker/cppdate.c)"
$HOSTCC -O2 -Wall -shared -fPIC -o "$PREFIX/lib/cppdate.so" "$ROOT/docker/cppdate.c" -ldl

rm -rf "$WORK"
echo "== installed:"; ls -l "$PREFIX/bin" "$PREFIX/lib"
