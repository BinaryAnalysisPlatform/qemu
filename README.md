# BAP emulation trace generator

This QEMU fork implements the TCG plugin to generate execution traces in the
[bap-frame](https://github.com/BinaryAnalysisPlatform/bap-frames) format.

This plugin does not yet support all targets.
If not listed below it is untested.

Known to work:

- Sparc
- Hexagon
- PPC
- TriCore
- M68K system emulation

Needs fixes:

- ARM (cannot get current mode of VCPU if target can switch between ARM/Thumb).

Previous traces were generated with a patched QEMU.
You can find these in tracewrap-\* branches.

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

## Tracing a binary

The plugin takes the following arguments:

`bin_path`: The path to the binary emulated. Due to a [QEMU bug](https://gitlab.com/qemu-project/qemu/-/issues/3014) this cannot be inferred.
`out`: The output file to save the trace into.
`endianness`: The architecture endanness.
`machine`: Required for M68K so the trace header identifies the exact CPU
profile. Accepted values cover every QEMU M68K CPU model: `any`, `cfv4e`,
`m5206`, `m5208`, `m68000`, `m68010`, `m68020`, `m68030`, `m68040`, and
`m68060`. QEMU's synthetic `any` model uses the closest standardized BAP
profile, `mcf_isa_b_float_emac`; all concrete models retain their specific
profile. The argument is rejected for non-M68K targets.

```bash
./qemu-sparc64 -plugin file=buil/contrib/plugins/bap-tracing/libbap_tracing.so,bin_path=<bin_path>,out=<output-file>,endianness=[b/l] -d plugin <bin_path>
ls <output-file>
```

For example, an M68020 system trace uses
`-plugin file=libbap_tracing.so,bin_path=<bin_path>,out=trace.frames,endianness=b,machine=m68020`.

You can also use the helper shell script:

```bash
./gen-trace.sh ./build/ sparc64 b <path_to_bin>
```

> [!NOTE]
> The trace plugin currently only generates standard frames.
> This is due to the limitations of the QEMU plugin API.
>
> If the traced binary exits due to an exception it can only indirectly be observed.
> It will produce a standard frame without any logged post register state.
> Any completed memory read/write might still be logged.
>
> If you suspect this, execute the binary with the `execlog` plugin (see `gen-trace.sh` or `gen-execlog.sh`)
> to check of the execution stops earlier than expected.

## Trace format

The generated trace consists of three parts: the header,
a table of contents (TOC) holding the frame entries, and an index into the TOC.

Each frame entry starts with the size of the frame, followed by the actual frame data.
A fixed number of frame entries are considered one _entry_ in the TOC.

The TOC index is stored at the end.

For specifics about the frame contents, please refer
to the [definitions](https://github.com/BinaryAnalysisPlatform/bap-frames/tree/master/piqi) in
the BAP-frames repository.

**Format**

| Offset                 | Type            | Field                                | Trace section   |
| ---------------------- | --------------- | ------------------------------------ | --------------- |
| 0x0                    | uint64_t        | magic number (7456879624156307493LL) | Header begin    |
| 0x8                    | uint64_t        | trace version number                 |                 |
| 0x10                   | uint64_t        | frame_architecture                   |                 |
| 0x18                   | uint64_t        | frame_machine, 0 for unspecified.    |                 |
| 0x20                   | uint64_t        | n = total number of frames in trace. |                 |
| 0x28                   | uint64_t        | T = offset to TOC index.             |                 |
| 0x30                   | uint64_t        | sizeof(frame_0)                      | TOC begin       |
| 0x38                   | meta_frame      | frame_0                              |                 |
| 0x40                   | uint64_t        | sizeof(frame_1)                      |                 |
| 0x48                   | type(frame_1)   | frame_1                              |                 |
| ...                    | ...             | ...                                  |                 |
| T-0x10                 | uint64_t        | sizeof(frame_n-1)                    |                 |
| T-0x8                  | type(frame_n-1) | frame_n-1                            |                 |
| T+0                    | uint64_t        | m = number of frames per TOC entry   | TOC index begin |
| T+0x8                  | uint64_t        | offset toc_entry(0)                  |                 |
| T+0x10                 | uint64_t        | offset toc_entry(1)                  |                 |
| ...                    | ...             | ...                                  |                 |
| T+0x8+(0x8\*ceil(n/m)) | uint64_t        | offset toc_entry(ceil(n/m))          |                 |
