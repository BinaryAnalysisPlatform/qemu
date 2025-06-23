// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>

#include "tracing.h"

static TraceState state;

static void log_insn_mem_access(unsigned int vcpu_index,
                                qemu_plugin_meminfo_t info, uint64_t vaddr,
                                void *userdata) {}

static void add_post_state_regs(VCPU *vcpu, unsigned int vcpu_index, GArray *current_regs) {
  GByteArray *rtmp = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    Register *prev_reg = vcpu->registers->pdata[i];

    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    int s = qemu_plugin_read_register(reg->handle, rtmp);
    assert(s == prev_reg->content->len);
    if (!memcmp(rtmp->data, prev_reg->content->data, s)) {
      // No change
      continue;
    }

    OperandInfo *rinfo = init_reg_operand_info(prev_reg->name, rtmp->data,
                                               rtmp->len, OperandRead);
    g_assert(rinfo);

    g_rw_lock_writer_lock(&state.frame_buffer_lock);
    FrameBuffer *fb = g_ptr_array_index(state.frame_buffer, vcpu_index);
    frame_buffer_append_op_info(fb, rinfo);
    g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  }
}

static void log_insn_reg_access(unsigned int vcpu_index, void *udata) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);

  VCPU *vcpu = &g_array_index(state.vcpus, VCPU, vcpu_index);
  GArray *current_regs = qemu_plugin_get_registers();
  g_assert(current_regs->len == vcpu->registers->len);

  add_post_state_regs(vcpu, vcpu_index, current_regs);
  // Check if buffer should be dumped to file.

  // Open new one.
  Instruction *insn = udata;
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);

  return;
}

Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc) {
  Register *reg = g_new0(Register, 1);
  g_autofree gchar *lower = g_utf8_strdown(desc->name, -1);
  int r;

  reg->handle = desc->handle;
  reg->name = g_intern_string(lower);
  reg->content = g_byte_array_new();

  /* read the initial value */
  r = qemu_plugin_read_register(reg->handle, reg->content);
  g_assert(r > 0);
  return reg;
}

static GPtrArray *registers_init(int vcpu_index) {
  g_autoptr(GPtrArray) registers = g_ptr_array_new();
  g_autoptr(GArray) reg_list = qemu_plugin_get_registers();

  if (!reg_list->len) {
    return NULL;
  }
  for (int r = 0; r < reg_list->len; r++) {
    qemu_plugin_reg_descriptor *rd =
        &g_array_index(reg_list, qemu_plugin_reg_descriptor, r);
    Register *reg = init_vcpu_register(rd);
    g_ptr_array_add(registers, reg);
  }

  return registers->len ? g_steal_pointer(&registers) : NULL;
}

static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index) {
  g_rw_lock_writer_lock(&state.vcpus_array_lock);
  g_rw_lock_writer_lock(&state.frame_buffer_lock);

  VCPU *vcpu = g_malloc0(sizeof(VCPU));
  vcpu->registers = registers_init(vcpu_index);
  g_array_insert_vals(state.vcpus, vcpu_index, &vcpu, 1);
  FrameBuffer *vcpu_frame_buffer = frame_buffer_init(FRAME_BUFFER_SIZE_DEFAULT);
  g_ptr_array_insert(state.frame_buffer, vcpu_index, &vcpu_frame_buffer);

  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_writer_unlock(&state.vcpus_array_lock);
}

OperandInfo *init_reg_operand_info(const char *name, const uint8_t *value,
                                   size_t value_size, OperandAccess access) {
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
  oi->bit_length = 0;
  oi->operand_info_specific = ois;
  oi->operand_usage = ou;
  oi->value.len = value_size;
  oi->value.data = g_malloc(oi->value.len);
  memcpy(oi->value.data, value, value_size);

  return oi;
}

Instruction *init_insn(struct qemu_plugin_insn *tb_insn) {
  Instruction *insn = g_malloc0(sizeof(Instruction));
  qemu_plugin_insn_data(tb_insn, &insn->bytes, sizeof(insn->bytes));
  insn->size = qemu_plugin_insn_size(tb_insn);
  insn->vaddr = qemu_plugin_insn_vaddr(tb_insn);
  return insn;
}

static void cb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  // Add a callback for each instruction in every translated block.
  struct qemu_plugin_insn *tb_insn;
  size_t n_insns = qemu_plugin_tb_n_insns(tb);
  for (size_t i = 0; i < n_insns; i++) {
    tb_insn = qemu_plugin_tb_get_insn(tb, i);
    Instruction *insn_data = init_insn(tb_insn);
    qemu_plugin_register_vcpu_insn_exec_cb(tb_insn, log_insn_reg_access,
                                           QEMU_PLUGIN_CB_R_REGS, insn_data);
    qemu_plugin_register_vcpu_mem_cb(tb_insn, log_insn_mem_access,
                                     QEMU_PLUGIN_CB_R_REGS, QEMU_PLUGIN_MEM_R,
                                     NULL);
  }
}

static void plugin_exit(qemu_plugin_id_t id, void *udata) {
  // Dump rest of frames to file.
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv) {
  const char *target_path = "/tmp/test.trace";
  state.frame_buffer = g_ptr_array_new();
  state.vcpus = g_array_new(false, true, sizeof(VCPU));
  state.file = fopen(target_path, "r");
  if (!(state.frame_buffer || state.vcpus || state.file)) {
    return 1;
  }

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, cb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
