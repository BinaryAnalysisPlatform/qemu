#ifndef BAP_TRACE_CONSTS_H
#define BAP_TRACE_CONSTS_H

#include <stdint.h>

// Trace header constants

static const uint64_t magic_number = 7456879624156307493LL;

static const uint64_t offset_magic_number = 0LL;
static const uint64_t offset_trace_version = 8LL;
static const uint64_t offset_target_arch = 16LL;
static const uint64_t offset_target_machine = 24LL;
static const uint64_t offset_frames_per_toc_entry = 32LL;
static const uint64_t offset_toc_index_offset = 40LL;
static const uint64_t offset_toc_start = 48LL;
static const uint64_t offset_first_frame = 48LL;

static const uint64_t trace_version = 3LL;

#define FRAMES_PER_TOC_ENTRY_ 64LL
static const uint64_t frames_per_toc_entry = FRAMES_PER_TOC_ENTRY_;

// Arch specific

#endif
