// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>

#include "frame_buffer.h"
#include "tracing.h"

static TraceState state;

static void log_insn_mem_access(unsigned int vcpu_index,
                                qemu_plugin_meminfo_t info, uint64_t vaddr,
                                void *userdata) {}

static void add_post_reg_state(VCPU *vcpu, unsigned int vcpu_index,
                               GArray *current_regs, FrameBuffer *fbuf) {
  GByteArray *rdata = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    Register *prev_reg = vcpu->registers->pdata[i];

    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    int s = qemu_plugin_read_register(reg->handle, rdata);
    assert(s == prev_reg->content->len);
    if (!memcmp(rdata->data, prev_reg->content->data, s)) {
      // No change
      continue;
    }

    if (!frame_buffer_append_reg_info(fbuf, reg->name, rdata, OperandWritten)) {
      qemu_plugin_outs("Failed to append opinfo.\n");
      g_assert(false);
    }
  }
}

static void add_pre_reg_state(VCPU *vcpu, unsigned int vcpu_index,
                              GArray *current_regs, FrameBuffer *fbuf) {
  GByteArray *rdata = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    qemu_plugin_read_register(reg->handle, rdata);
    frame_buffer_append_reg_info(fbuf, reg->name, rdata, OperandRead);
  }
}

static void add_new_insn_frame(VCPU *vcpu, unsigned int vcpu_index,
                               FrameBuffer *fbuf, Instruction *insn) {
  frame_buffer_new_frame_std(fbuf, vcpu_index, insn->vaddr, insn->bytes,
                             insn->size);
}

static void log_insn_reg_access(unsigned int vcpu_index, void *udata) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);
  g_rw_lock_reader_lock(&state.frame_buffer_lock);

  FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, vcpu_index);
  VCPU *vcpu = &g_array_index(state.vcpus, VCPU, vcpu_index);
  GArray *current_regs = qemu_plugin_get_registers();
  g_assert(current_regs->len == vcpu->registers->len);

  add_post_reg_state(vcpu, vcpu_index, current_regs, fbuf);

  if (frame_buffer_is_full(fbuf)) {
    g_rw_lock_writer_lock(&state.file_lock);
    frame_buffer_flush_to_file(fbuf, state.file);
    g_rw_lock_writer_unlock(&state.file_lock);
  }

  // Open new one.
  Instruction *insn = udata;
  add_new_insn_frame(vcpu, vcpu_index, fbuf, insn);
  add_pre_reg_state(vcpu, vcpu_index, current_regs, fbuf);

  g_rw_lock_reader_unlock(&state.frame_buffer_lock);
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);
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
  FrameBuffer *vcpu_frame_buffer = frame_buffer_new(FRAME_BUFFER_SIZE_DEFAULT);
  g_ptr_array_insert(state.frame_buffer, vcpu_index, &vcpu_frame_buffer);

  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_writer_unlock(&state.vcpus_array_lock);
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
  state.file = fopen(target_path, "wb");
  if (!(state.frame_buffer || state.vcpus || state.file)) {
    return 1;
  }
  // write_header();
  // write_meta(argv, envp, target_argv, target_envp);

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, cb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
