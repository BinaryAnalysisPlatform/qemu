#!/bin/sh

if [ "$#" -lt 4 ]; then
  echo "$0 <build_dir> <arch> <endianess=[b/l]> <bin_path> [args...]"
  echo "<arch> is appended to qemu-<arch>"
  exit 1
fi

BUILDIR=$1
ARCH=$2
EN=$3
BIN=$4
BNAME=$(basename $BIN)

$BUILDIR/qemu-$ARCH -plugin file="$BUILDIR/contrib/plugins/bap-tracing/libbap_tracing.so,bin_path=$BIN",out="$BNAME.trace",endianness="$EN" -d plugin "$BIN" ""${@:5}""
ls -lh "$BNAME.trace"
# $BUILDIR/qemu-$ARCH -plugin file="$BUILDIR/contrib/plugins/libexeclog.so" -d plugin "$BIN"
