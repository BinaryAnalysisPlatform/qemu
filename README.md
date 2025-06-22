# BAP emulation trace generator

This QEMU fork implements the TCG plugin to generate execution traces in the
[bap-frame](https://github.com/BinaryAnalysisPlatform/bap-frames) format.

Previous traces were generated with a patched QEMU.
You can find these in the branches tracewrap-6.2.0 for ARM and x86 and tracewrap-8.1 for Hexagon.

## Dependencies

1. Install [piqi](https://piqi.org/downloads/) so you have the `piqi` binary in `PATH`.
2. Install the developer package of `protobuf-c`. E.g. `protobuf-c-devel` (Fedora), `libprotobuf-c-dev` (Debian).
3. QEMU dependencies (see [QEMU docs](https://www.qemu.org/docs/master/devel/build-environment.html)).

## Building

```bash
mkdir build
cd build
# See `../configure --help` for a list of targets.
../configure --enable-plugins --target-list=<target>
make
```
