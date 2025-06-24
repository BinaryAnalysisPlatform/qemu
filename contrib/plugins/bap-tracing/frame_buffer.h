// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_FRAME_BUFFER_H
#define BAP_FRAME_BUFFER_H

#include <qemu-plugin.h>
#include <stdio.h>
#include <glib.h>

#include "frame.piqi.pb-c-patched.h"

typedef struct {
  Frame **fbuf; ///< The frames buffered.
  size_t idx; ///< Points to currently open frame.
  size_t max_size; ///< Maximum number of elements fbuf can hold.
} FrameBuffer;

/**
 * \brief Initializes a frame buffer with space for \p size frames.
 * Returns the buffer or NULL in case of failure.
 */
FrameBuffer *frame_buffer_new(size_t size);

void frame_buffer_flush_to_file(FrameBuffer *buf, FILE *file);
bool frame_buffer_is_full(const FrameBuffer *buf);

StdFrame *frame_buffer_new_frame_std(FrameBuffer *buf);
void frame_buffer_append_op_info(FrameBuffer *buf, OperandInfo *oi);

#endif
