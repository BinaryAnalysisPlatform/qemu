// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include "frame_buffer.h"

static Frame *frame_new_std(uint64_t addr, int vcpu_id) {
  Frame *frame = g_new(Frame, 1);
  frame__init(frame);

  StdFrame *sframe = g_new(StdFrame, 1);
  std_frame__init(sframe);
  frame->std_frame = sframe;

  sframe->address = addr;
  sframe->thread_id = vcpu_id;

  OperandValueList *ol_in = g_new(OperandValueList, 1);
  operand_value_list__init(ol_in);
  ol_in->n_elem = 0;
  sframe->operand_pre_list = ol_in;

  OperandValueList *ol_out = g_new(OperandValueList, 1);
  operand_value_list__init(ol_out);
  ol_out->n_elem = 0;
  sframe->operand_post_list = ol_out;
  return frame;
}

static void frame_add_operand(Frame *frame, OperandInfo *oi) {
  OperandValueList *ol;
  if (oi->operand_usage->written) {
    ol = frame->std_frame->operand_post_list;
  } else {
    ol = frame->std_frame->operand_pre_list;
  }

  oi->taint_info = g_new(TaintInfo, 1);
  taint_info__init(oi->taint_info);
  oi->taint_info->no_taint = 1;
  oi->taint_info->has_no_taint = 1;

  ol->n_elem += 1;
  ol->elem = g_renew(OperandInfo *, ol->elem, ol->n_elem);
  ol->elem[ol->n_elem - 1] = oi;
}

FrameBuffer *frame_buffer_new(size_t size) {
  FrameBuffer *fb = g_malloc0(sizeof(FrameBuffer));
  fb->fbuf = g_malloc0(sizeof(Frame *) * size);
  fb->max_size = size;
  return fb;
}

bool frame_buffer_is_full(const FrameBuffer *buf) {
  return buf->idx >= buf->max_size;
}

void frame_buffer_flush_to_file(FrameBuffer *buf, FILE *file);

StdFrame *frame_buffer_new_frame_std(FrameBuffer *buf) {
  if (frame_buffer_is_full(buf)) {
    return NULL;
  }
  Frame *frame = frame_new_std(0, -1);
  frame__init(frame);

  StdFrame *sframe = g_new(StdFrame, 1);
  std_frame__init(sframe);
  frame->std_frame = sframe;
  buf->fbuf[buf->idx++] = frame;
  return sframe;
}

void frame_buffer_append_op_info(FrameBuffer *buf, OperandInfo *oi);
