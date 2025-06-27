#ifndef BAP_TRACE_CONSTS_H
#define BAP_TRACE_CONSTS_H

#include <stdint.h>

// Trace header constants

static const uint64_t magic_number = 7456879624156307493LL;
static const uint64_t magic_number_offset = 0LL;
static const uint64_t trace_version_offset = 8LL;
static const uint64_t bfd_arch_offset = 16LL;
static const uint64_t bfd_machine_offset = 24LL;
static const uint64_t num_trace_frames_offset = 32LL;
static const uint64_t toc_offset_offset = 40LL;
static const uint64_t first_frame_offset = 48LL;
static const uint64_t out_trace_version = 2LL;

// Arch specific

#endif
