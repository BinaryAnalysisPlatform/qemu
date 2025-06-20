#pragma once

#include "trace_info.h"

const uint64_t magic_number = 7456879624156307493LL;
const uint64_t magic_number_offset = 0LL;
const uint64_t trace_version_offset = 8LL;
const uint64_t bfd_arch_offset = 16LL;
const uint64_t bfd_machine_offset = 24LL;
const uint64_t num_trace_frames_offset = 32LL;
const uint64_t toc_offset_offset = 40LL;
const uint64_t first_frame_offset = 48LL;
const uint64_t out_trace_version = 2LL;
