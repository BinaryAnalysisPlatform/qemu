// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_FRAME_BUFFER_H
#define BAP_FRAME_BUFFER_H

#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>

#include "frame.piqi.pb-c-patched.h"
#include "trace_consts.h"
#include "trace_meta.h"

typedef enum {
  OperandRead = 1,
  OperandWritten = 2,
} OperandAccess;

typedef struct {
  Frame *fbuf[FRAMES_PER_TOC_ENTRY_]; ///< The frames buffered.
  size_t idx;                         ///< Points to currently open frame.
} FrameBuffer;

/**
 * \brief Initializes a frame buffer with space for \p size frames.
 * Returns the buffer or NULL in case of failure.
 */
FrameBuffer *frame_buffer_new(void);

uint64_t frame_buffer_flush_to_file(FrameBuffer *buf, WLOCKED FILE *file, bool add_padding);
bool frame_buffer_is_full(const FrameBuffer *buf);
bool frame_buffer_is_empty(const FrameBuffer *buf);
void frame_buffer_close_frame(FrameBuffer *buf);
char *frame_buffer_as_str(const FrameBuffer *buf);

bool frame_buffer_new_frame_std(FrameBuffer *buf, unsigned int thread_id,
                                uint64_t vaddr, const char *mode_id,
                                uint8_t *bytes, size_t bytes_len);

bool frame_buffer_append_mem_info(FrameBuffer *fbuf, uint64_t vaddr,
                                  qemu_plugin_mem_value *mval, bool is_store);

/**
 * \brief Appends the given operand info to the open frame.
 */
bool frame_buffer_append_reg_info(FrameBuffer *buf, const char *name,
                                  const GByteArray *content, size_t reg_size,
                                  OperandAccess acc);

OperandInfo *frame_init_reg_operand_info(const char *name, const uint8_t *value,
                                         size_t value_size,
                                         OperandAccess access);

#endif
