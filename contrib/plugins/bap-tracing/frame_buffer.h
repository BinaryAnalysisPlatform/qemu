// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_FRAME_BUFFER_H
#define BAP_FRAME_BUFFER_H

#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>

#include "trace_meta.h"
#include "frame.piqi.pb-c-patched.h"

typedef enum {
  OperandRead = 1,
  OperandWritten = 2,
} OperandAccess;

typedef struct {
  Frame **fbuf;    ///< The frames buffered.
  size_t idx;      ///< Points to currently open frame.
  size_t max_size; ///< Maximum number of elements fbuf can hold.
  size_t frames_written; ///< Number of frames written from buffer to file.
} FrameBuffer;

/**
 * \brief Initializes a frame buffer with space for \p size frames.
 * Returns the buffer or NULL in case of failure.
 */
FrameBuffer *frame_buffer_new(size_t size);

void frame_buffer_flush_to_file(FrameBuffer *buf, WLOCKED FILE *file);
bool frame_buffer_is_full(const FrameBuffer *buf);

bool frame_buffer_new_frame_std(FrameBuffer *buf,
                                unsigned int thread_id, uint64_t vaddr,
                                uint8_t *bytes, size_t bytes_len);

/**
 * \brief Appends the given operand info to the open frame.
 */
bool frame_buffer_append_reg_info(FrameBuffer *buf, const char *name,
                                  const GByteArray *content,
                                  OperandAccess acc);

OperandInfo *frame_init_reg_operand_info(const char *name, const uint8_t *value,
                                         size_t value_size,
                                         OperandAccess access);

#endif
