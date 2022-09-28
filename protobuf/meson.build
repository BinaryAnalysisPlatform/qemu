
subdir('raw') # need to use subdir because meson doesn't allow generating in other dirs otherwise
py = import('python').find_installation('python3')

proto_src_fixed = custom_target('proto-fixed',
  input: ['fix_proto_src.py', proto_src_raw],
  output: ['frame.piqi.pb-c.c', 'frame.piqi.pb-c.h'],
  command: [py, '@INPUT@', '-o', '@OUTPUT@'])

libprotoframes = static_library('protoframes',
  proto_src_fixed)

protobuf_c = dependency('libprotobuf-c')

protoframes = declare_dependency(
  link_with: libprotoframes,
  include_directories: ['.', '../' / tracewrap_dir / 'libtrace' / 'src'],
  dependencies: protobuf_c)
