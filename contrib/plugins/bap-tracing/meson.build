# SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
# SPDX-License-Identifier: GPL-2.0-only

piqi = find_program('piqi')
protoc_c = find_program('protoc-c')

# piqi -> protobuf file.
piqi_src = custom_target('piqi',
  input: 'bap-frames/piqi/frame.piqi',
  output: 'frame.piqi.proto',
  command: [piqi, 'to-proto', '@INPUT@', '-o', '@OUTPUT@'])

# protobuf file -> C code
frame_proto_src_raw = custom_target('proto',
  input: piqi_src,
  output: ['frame.piqi.pb-c.c', 'frame.piqi.pb-c.h'],
  command: [protoc_c, '--c_out=.', '@INPUT@'],
  depends: piqi_src)

# Patch protobuf header: base -> __base.
# Necessary for the C build.
# See fix_proto_src.py
frame_proto_src = custom_target(
  input: frame_proto_src_raw,
  output: ['frame.piqi.pb-c-patched.c', 'frame.piqi.pb-c-patched.h'],
  command: [files('fix_proto_src.py'), '@INPUT@', '-o', '@OUTPUT@'],
  depends: frame_proto_src_raw
)

libprotobuf = dependency('libprotobuf-c')
frame_protobuf = static_library('protobuf', [frame_proto_src], pic: true)
dep_libprotobuf = declare_dependency(
    sources : frame_proto_src,
    link_with : [frame_protobuf],
    dependencies: [libprotobuf]
)

bap_tracing_src = files(
  'frame_buffer.c',
  'tracing.c',
  'tracewrap.c',
)

if host_os == 'windows'
  plugin_modules += shared_module('bap_tracing', bap_tracing_src,
                    include_directories: '../../../include/qemu',
                    link_depends: [win32_qemu_plugin_api_lib],
                    link_args: win32_qemu_plugin_api_link_flags,
                    dependencies: [glib, dep_libprotobuf])
else
  plugin_modules += shared_module('bap_tracing', bap_tracing_src,
                    include_directories: '../../../include/qemu',
                    dependencies: [glib, dep_libprotobuf])
endif
