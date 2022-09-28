
piqi = find_program('piqi')
protoc_c = find_program('protoc-c')

tracewrap_dir = get_option('tracewrap_dir')

piqi_src = custom_target('piqi',
  input: join_paths(meson.project_source_root(), tracewrap_dir) / 'piqi/frame.piqi',
  output: 'frame.piqi.proto',
  command: [piqi, 'to-proto', '@INPUT@', '-o', '@OUTPUT@'])

proto_src_raw = custom_target('proto',
  input: piqi_src,
  output: ['frame.piqi.pb-c.c', 'frame.piqi.pb-c.h'],
  command: [protoc_c, '--c_out=.', '@INPUT@'],
  depends: piqi_src)
