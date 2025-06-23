// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>

#include "tracing.h"

static TraceState state;

static void log_insn_reg_access(unsigned int vcpu_index, void *udata) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);
  // VCPU *c = &g_array_index(state.vcpus, VCPU, vcpu_index);

  g_rw_lock_writer_lock(&state.frame_buffer_lock);
  // Add change to previous frame
  // Finish previous frame
  // Check if buffer should be dumped to file.
  // Open new one.
  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);

  return;
}

static Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc)
{
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
  VCPU *vcpu = calloc(sizeof(VCPU), 1);
  vcpu->registers = registers_init(vcpu_index);
  g_array_insert_vals(state.vcpus, vcpu_index, &vcpu, 1);
  g_rw_lock_writer_unlock(&state.vcpus_array_lock);
}

static void plugin_exit(qemu_plugin_id_t id, void *udata) {
  // Dump rest of frames to file.
}

static void cb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  // Add a callback for each instruction in every translated block.
  struct qemu_plugin_insn *insn;
  size_t n_insns = qemu_plugin_tb_n_insns(tb);
  for (size_t i = 0; i < n_insns; i++) {
    insn = qemu_plugin_tb_get_insn(tb, i);
    qemu_plugin_register_vcpu_insn_exec_cb(insn, log_insn_reg_access,
                                           QEMU_PLUGIN_CB_R_REGS, NULL);
  }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv) {
  const char *target_path = "/tmp/test.trace";
  state.frame_buffer = frame_buffer_init(FRAME_BUFFER_SIZE);
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
