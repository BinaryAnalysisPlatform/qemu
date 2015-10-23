# Overview

Qemu tracer - tracer based on [qemu](https://github.com/qemu/qemu)
project. It executes binary executable and saves trace data using
[Protocol Buffer](https://developers.google.com/protocol-buffers/)
format. The contents of the trace data defined in
[bap-traces](https://github.com/BinaryAnalysisPlatform/bap-traces)
project.

# Preparing to build

Note: building instructions assume that you're using Ubuntu, but it
may work on other systems, that uses apt-get.

Before build qemu tracer, you need to install following packages:
   * qemu build dependencies
   * autoconf, libtool, protobuf-c-compiler
   * [piqi library](http://piqi.org/doc/ocaml)

To install qemu build dependencies, use the following command

```bash
$ sudo apt-get --no-install-recommends -y build-dep qemu
```

To install autoconf, libtool, protobuf-c-compiler, use the
following command

```bash
$ sudo apt-get autoconf libtool protobuf-c-compiler
```

To install [piqi library](http://piqi.org/doc/ocaml) with
[opam](https://opam.ocaml.org/doc/Install.html), use the following command
```bash
$ opam install piqi
```

# Build process

Download [bap-traces](https://github.com/BinaryAnalysisPlatform/bap-traces) with
following command

```bash
$ git clone https://github.com/BinaryAnalysisPlatform/bap-traces.git
```

Download qemu tracer with following command

```bash
$ git clone clone git@github.com:BinaryAnalysisPlatform/qemu.git -b tracewrap
```

Change folder to qemu and build tracer with command
```bash
$ ./configure --prefix=$HOME --with-tracewrap=`realpath ../bap-traces` \
--extra-ldflags=-Lprotobuf --target-list=arm-linux-user
$ make -C protobuf
$ make
$ make install
```

# Usage

To run executable `exec` and to save the trace data to `exec.trace`, use

```bash
$ qemu-arm -tracefile exec.trace exec
```

Hints: use option -L to set the elf interpreter prefix to 'path'. Use
fetchlibs.sh to download arm libraries.

# Notes
  Only ARM target is supported in this branch.
