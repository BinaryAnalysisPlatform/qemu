// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include "frame_buffer.h"
#include "trace_meta.h"

static Frame *frame_new_std(uint64_t addr, int vcpu_id, const char *mode_id,
                            uint8_t *bytes, size_t bytes_len) {
  Frame *frame = g_new(Frame, 1);
  frame__init(frame);

  StdFrame *sframe = g_new(StdFrame, 1);
  std_frame__init(sframe);
  frame->std_frame = sframe;

  sframe->address = addr;
  if (mode_id) {
    sframe->mode = g_strdup(mode_id);
  }
  sframe->thread_id = vcpu_id;
  sframe->rawbytes.len = bytes_len;
  sframe->rawbytes.data = g_malloc(bytes_len);
  memcpy(sframe->rawbytes.data, bytes, bytes_len);

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

  // Free reg-operand
  RegOperand *ro = ois->reg_operand;
  if (ro && ro->name)
    g_free(ro->name);
  g_free(ro);

  // Free mem-operand
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

static bool std_frame_add_operand(StdFrame *std_frame, OperandInfo *oi) {
  OperandValueList *ol;
  if (oi->operand_usage->written) {
    ol = std_frame->operand_post_list;
  } else {
    ol = std_frame->operand_pre_list;
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

FrameBuffer *frame_buffer_new(void) {
  FrameBuffer *fb = g_malloc0(sizeof(FrameBuffer));
  return fb;
}

bool frame_buffer_is_full(const FrameBuffer *buf) {
  return buf->idx >= frames_per_toc_entry;
}

void frame_buffer_close_frame(FrameBuffer *buf) {
  char *str = frame_buffer_as_str(buf);
  // qemu_plugin_outs("Close frame: ");
  // qemu_plugin_outs(str);
  // qemu_plugin_outs("\n\n");
  g_free(str);
  buf->idx++;
}

#define FRAME_STR_SIZE 8192

#define APPEND(...)                                                            \
  snprintf(str + off, max - off, __VA_ARGS__);                                 \
  off = strlen(str);

char *frame_buffer_as_str(const FrameBuffer *buf) {
  char *str = g_malloc0(FRAME_STR_SIZE);
  const Frame *frame = buf->fbuf[buf->idx];
  if (!frame) {
    snprintf(str, FRAME_STR_SIZE, "<NULL>");
    return str;
  }
  size_t max = FRAME_STR_SIZE - 1;
  snprintf(str, max, "{ pre: [ ");
  size_t off = strlen(str);

  StdFrame *sframe = frame->std_frame;
  for (size_t i = 0; i < sframe->operand_pre_list->n_elem; i++) {
    OperandInfo *oi = sframe->operand_pre_list->elem[i];
    if (oi->operand_info_specific->reg_operand) {
      APPEND("r:%s=", oi->operand_info_specific->reg_operand->name);
    } else {
      APPEND("m:0x%016lx=", oi->operand_info_specific->mem_operand->address);
    }

    for (size_t k = 0; k < oi->value.len; ++k) {
      APPEND("%02x", oi->value.data[k]);
    }
    APPEND(", ");
  }
  APPEND(" ], post: [ ");
  for (size_t i = 0; i < sframe->operand_post_list->n_elem; i++) {
    OperandInfo *oi = sframe->operand_post_list->elem[i];
    if (oi->operand_info_specific->reg_operand) {
      APPEND("r:%s=", oi->operand_info_specific->reg_operand->name);
    } else {
      APPEND("m:0x%016lx=", oi->operand_info_specific->mem_operand->address);
    }

    for (size_t k = 0; k < oi->value.len; ++k) {
      APPEND("%02x", oi->value.data[k]);
    }
    APPEND(", ");
  }

  APPEND("]}");
  return str;
}

bool frame_buffer_is_empty(const FrameBuffer *buf) {
  return buf->fbuf[buf->idx] == NULL;
}

void frame_buffer_clean(FrameBuffer *buf) {
  memset(buf->fbuf, 0, sizeof(buf->fbuf));
  buf->idx = 0;
}

bool frame_buffer_write_frame_to_file(FrameBuffer *buf, WLOCKED FILE *file,
                                      size_t i) {
  if (i > buf->idx) {
    return false;
  }
  Frame *frame = buf->fbuf[i];
  size_t msg_size = frame__get_packed_size(frame);
  uint8_t *packed_buffer = g_alloca(msg_size);
  uint64_t packed_size = frame__pack(frame, packed_buffer);
  WRITE(packed_size);
  WRITE_BUF(packed_buffer, packed_size);
  frame_free(frame);
  return true;
}

/// @brief Dumps the file buffer as TOC entry into the file.
uint64_t frame_buffer_flush_to_file(FrameBuffer *buf, WLOCKED FILE *file) {
  uint64_t n = 0;
  for (size_t i = 0; i < buf->idx; ++i) {
    frame_buffer_write_frame_to_file(buf, file, i);
    n++;
  }
  frame_buffer_clean(buf);
  return n;
}

bool frame_buffer_new_frame_std(FrameBuffer *buf, unsigned int thread_id,
                                uint64_t vaddr, const char *mode,
                                uint8_t *bytes, size_t bytes_len) {
  if (frame_buffer_is_full(buf)) {
    return false;
  }
  Frame *frame = frame_new_std(vaddr, thread_id, mode, bytes, bytes_len);
  if (!frame) {
    return false;
  }
  buf->fbuf[buf->idx] = frame;
  return true;
}

static bool append_op_info(FrameBuffer *buf, OperandInfo *oi) {
  Frame *frame = buf->fbuf[buf->idx];
  if (!frame || !frame->std_frame) {
    qemu_plugin_outs(
        "Attempt to append operand info to a uninitialzied frame.");
    return false;
  }
  return std_frame_add_operand(frame->std_frame, oi);
}

bool frame_buffer_append_reg_info(FrameBuffer *buf, const char *name,
                                  const GByteArray *content, size_t reg_size,
                                  OperandAccess acc) {
  OperandInfo *oi = frame_init_reg_operand_info(
      name, content->data + content->len - reg_size, reg_size, acc);
  g_assert(oi);
  return append_op_info(buf, oi);
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

static size_t mval_type_to_int(enum qemu_plugin_mem_value_type type) {
  switch (type) {
  case QEMU_PLUGIN_MEM_VALUE_U8:
    return 8;
  case QEMU_PLUGIN_MEM_VALUE_U16:
    return 16;
  case QEMU_PLUGIN_MEM_VALUE_U32:
    return 32;
  case QEMU_PLUGIN_MEM_VALUE_U64:
    return 64;
  case QEMU_PLUGIN_MEM_VALUE_U128:
    return 128;
  default:
    g_assert(false);
  }
  return 0;
}

static void mval_to_buf(qemu_plugin_mem_value *val, uint8_t *buf) {
  switch (val->type) {
  case QEMU_PLUGIN_MEM_VALUE_U8:
    buf[0] = val->data.u8;
    return;
  case QEMU_PLUGIN_MEM_VALUE_U16:
    buf[0] = (uint8_t)val->data.u16;
    buf[1] = (uint8_t)(val->data.u16 >> 8);
    return;
  case QEMU_PLUGIN_MEM_VALUE_U32:
    buf[0] = (uint8_t)val->data.u32;
    buf[1] = (uint8_t)(val->data.u32 >> 8);
    buf[2] = (uint8_t)(val->data.u32 >> 16);
    buf[3] = (uint8_t)(val->data.u32 >> 24);
    return;
  case QEMU_PLUGIN_MEM_VALUE_U64:
    for (size_t i = 0; i < 8; ++i) {
      buf[i] = (uint8_t)(val->data.u64 >> (i * 8));
    }
    return;
  case QEMU_PLUGIN_MEM_VALUE_U128:
    for (size_t i = 0; i < 8; ++i) {
      buf[i] = (uint8_t)(val->data.u128.low >> (i * 8));
    }
    for (size_t i = 0; i < 8; ++i) {
      buf[i + 8] = (uint8_t)(val->data.u128.high >> (i * 8));
    }
    return;
  default:
    g_assert(false);
  }
}

static OperandInfo *frame_init_mem_operand_info(uint64_t vaddr,
                                                qemu_plugin_mem_value *mval,
                                                bool is_store) {
  MemOperand *ro = g_new(MemOperand, 1);
  mem_operand__init(ro);
  ro->address = vaddr;

  OperandInfoSpecific *ois = g_new(OperandInfoSpecific, 1);
  operand_info_specific__init(ois);
  ois->mem_operand = ro;

  size_t byte_width = mval_type_to_int(mval->type) / 8;
  OperandUsage *ou = g_new(OperandUsage, 1);
  operand_usage__init(ou);
  ou->read = !is_store;
  ou->written = is_store;
  OperandInfo *oi = g_new(OperandInfo, 1);
  operand_info__init(oi);
  oi->bit_length = mval_type_to_int(mval->type);
  oi->operand_info_specific = ois;
  oi->operand_usage = ou;
  oi->value.len = byte_width;
  oi->value.data = g_malloc(oi->value.len);
  mval_to_buf(mval, oi->value.data);

  return oi;
}

bool frame_buffer_append_mem_info(FrameBuffer *fbuf, uint64_t vaddr,
                                  qemu_plugin_mem_value *mval, bool is_store) {
  OperandInfo *oi = frame_init_mem_operand_info(vaddr, mval, is_store);
  g_assert(oi);
  return append_op_info(fbuf, oi);
}
