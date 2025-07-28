#!/bin/sh

if [ "$#" -lt 2 ]; then
  echo "$0 <build_dir> <arch> <bin_path> [args...]"
  echo "<arch> is appended to qemu-<arch>"
  exit 1
fi

BUILDIR=$1
ARCH=$2
BIN=$3

$BUILDIR/qemu-$ARCH -plugin file="$BUILDIR/contrib/plugins/libexeclog.so",reg=* -d plugin "$BIN"

