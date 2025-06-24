// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include "frame_buffer.h"

#define WRITE(x)                                                               \
  do {                                                                         \
    if (fwrite(&(x), sizeof(x), 1, file) != 1)                                 \
      qemu_plugin_outs("fwrite failed");                                       \
  } while (0)

#define WRITE_BUF(x, n)                                                        \
  do {                                                                         \
    if (fwrite((x), 1, (n), file) != n)                                        \
      qemu_plugin_outs("fwrite failed");                                       \
  } while (0)

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

static inline void free_operand(OperandInfo *oi) {
    OperandInfoSpecific *ois = oi->operand_info_specific;

    //Free reg-operand
    RegOperand *ro = ois->reg_operand;
    if (ro && ro->name)
        g_free(ro->name);
    g_free(ro);

    //Free mem-operand
    MemOperand *mo = ois->mem_operand;
    g_free(mo);
    g_free(oi->value.data);
    g_free(oi->taint_info);
    g_free(ois);
    g_free(oi->operand_usage);
    g_free(oi);
}

static void frame_free(Frame *frame) {
  if (!frame) {
    return;
  }
  StdFrame *sframe = frame->std_frame;
  for (size_t i = 0; i < sframe->operand_pre_list->n_elem; i++) {
    free_operand(sframe->operand_pre_list->elem[i]);
  }
  g_free(sframe->operand_pre_list->elem);
  g_free(sframe->operand_pre_list);

  for (size_t i = 0; i < sframe->operand_post_list->n_elem; i++) {
    free_operand(sframe->operand_post_list->elem[i]);
  }
  g_free(sframe->operand_post_list->elem);
  g_free(sframe->operand_post_list);

  g_free(sframe->rawbytes.data);
  g_free(sframe);
  g_free(frame);
}

static bool frame_add_operand(Frame *frame, OperandInfo *oi) {
  if (!frame->std_frame) {
    qemu_plugin_outs(
        "Append operand info to non-std frames is not implemented.");
    return false;
  }
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
  return true;
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

void frame_buffer_flush_to_file(FrameBuffer *buf, WLOCKED FILE *file) {
  for (size_t i = 0; i <= buf->idx && i < buf->max_size; ++i) {
    Frame *frame = buf->fbuf[i];
    size_t msg_size = frame__get_packed_size(frame);
    uint8_t *packed_buffer = g_alloca(msg_size);
    uint64_t packed_size = frame__pack(frame, packed_buffer);
    WRITE(packed_size);
    WRITE_BUF(packed_buffer, packed_size);
    buf->frames_written++;
    frame_free(frame);
  }
  memset(buf->fbuf, 0, sizeof(Frame *) * buf->max_size);
  buf->idx = 0;
  // toc_update(); ??
}

bool frame_buffer_new_frame_std(FrameBuffer *buf, unsigned int thread_id,
                                uint64_t vaddr, uint8_t *bytes,
                                size_t bytes_len) {
  if (frame_buffer_is_full(buf)) {
    return false;
  }
  Frame *frame = frame_new_std(0, -1);
  frame__init(frame);

  StdFrame *stdframe = g_new(StdFrame, 1);
  std_frame__init(stdframe);
  frame->std_frame = stdframe;

  stdframe->thread_id = thread_id;
  stdframe->address = vaddr;
  stdframe->rawbytes.len = bytes_len;
  stdframe->rawbytes.data = g_malloc(bytes_len);
  memcpy(stdframe->rawbytes.data, bytes, bytes_len);

  OperandValueList *ol_in = g_new(OperandValueList, 1);
  operand_value_list__init(ol_in);
  ol_in->n_elem = 0;
  stdframe->operand_pre_list = ol_in;

  OperandValueList *ol_out = g_new(OperandValueList, 1);
  operand_value_list__init(ol_out);
  ol_out->n_elem = 0;
  stdframe->operand_post_list = ol_out;

  buf->fbuf[buf->idx++] = frame;
  return true;
}

bool frame_buffer_append_reg_info(FrameBuffer *buf, const char *name,
                                  const GByteArray *content,
                                  OperandAccess acc) {
  OperandInfo *oi =
      frame_init_reg_operand_info(name, content->data, content->len, acc);
  g_assert(oi);
  Frame *frame = buf->fbuf[buf->idx];
  if (!frame) {
    qemu_plugin_outs(
        "Attempt to append operand info to a uninitialzied frame.");
    return false;
  }
  return frame_add_operand(frame, oi);
}

OperandInfo *frame_init_reg_operand_info(const char *name, const uint8_t *value,
                                         size_t value_size,
                                         OperandAccess access) {
  RegOperand *ro = g_new(RegOperand, 1);
  reg_operand__init(ro);
  ro->name = strdup(name);

  OperandInfoSpecific *ois = g_new(OperandInfoSpecific, 1);
  operand_info_specific__init(ois);
  ois->reg_operand = ro;

  OperandUsage *ou = g_new(OperandUsage, 1);
  operand_usage__init(ou);
  ou->read = access & OperandRead;
  ou->written = access & OperandWritten;
  OperandInfo *oi = g_new(OperandInfo, 1);
  operand_info__init(oi);
  oi->bit_length = value_size * 8;
  oi->operand_info_specific = ois;
  oi->operand_usage = ou;
  oi->value.len = value_size;
  oi->value.data = g_malloc(oi->value.len);
  memcpy(oi->value.data, value, value_size);

  return oi;
}
