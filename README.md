# Overview

Qemu tracer - a tracer based on [qemu](https://github.com/qemu/qemu)
project. It executes a binary executable and saves trace data using
[Protocol Buffer](https://developers.google.com/protocol-buffers/)
format. The contents of the trace data is defined in
[bap-frames](https://github.com/BinaryAnalysisPlatform/bap-frames)
project.

# Installing released binaries

If you don't want to mess with the source and building, then you can just
dowload a tarball with prebuilt binaries. Look at the latest release and
it might happen, that we have built binaries for your linux distribution,
if it is not the case, then create an issue, and we will build it for you.

Let's pretend, that you're using Ubuntu Trusty, and install it. First
download it with your favorite downloader:

```
wget https://github.com/BinaryAnalysisPlatform/qemu/releases/download/v2.0.0-tracewrap-2.0.0-rc1/qemu-tracewrap-ubuntu-14.04.4-LTS.tgz
```

Install it in the specified prefix with a command like `tar -C <prefix> -xf qemu-tracewrap-ubuntu-14.04.4-LTS.tgz`, e.g.,
to install in your home directory:
```
tar -C $HOME -xf qemu-tracewrap-ubuntu-14.04.4-LTS.tgz
```



# Build

## Preparation

Note: the instructions assume that you're using Ubuntu, but it
may work on other systems, that uses `apt-get`.

Before building the qemu-tracewrap, you need to install the following packages:
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
$ sudo apt-get install autoconf libtool protobuf-c-compiler
```

To install [piqi library](http://piqi.org/doc/ocaml) with
[opam](https://opam.ocaml.org/doc/Install.html), use the following command
```bash
$ opam install piqi
```

## Building

Download [bap-frames](https://github.com/BinaryAnalysisPlatform/bap-frames) with
following command

```bash
$ git clone https://github.com/BinaryAnalysisPlatform/bap-frames.git
```

Download qemu tracer with following command

```bash
$ git clone git@github.com:BinaryAnalysisPlatform/qemu.git
```

Change folder to qemu and build tracer:
```bash
$ cd qemu
$ ./configure --prefix=$HOME --with-tracewrap=../bap-frames --target-list=arm-linux-user
$ ninja -C build
$ ninja -C build install
```

# Usage

To run executable `exec` compiled for `arch`, use `qemu-arch exec` command, e.g.,
`qemu-x86_64 /bin/ls`. It will dump the trace into `ls.frames` file. You can configure
the filename with `-tracefile` option, e.g., `qemu-arm -tracefile arm.ls.frames ls`


Hints: use option -L to set the elf interpreter prefix to 'path'. Use
[fetchlibs.sh](https://raw.githubusercontent.com/BinaryAnalysisPlatform/bap-frames/master/test/fetchlibs.sh)
to download arm and x86 libraries.

# Notes
  Only ARM target is supported in this branch.
