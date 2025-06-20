// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct {
  // Current instruction related things.
} VCPU;

typedef struct {
  GRWLock vcpus_array_lock;
  GArray *vcpus;

  GRWLock frame_buffer_lock;
  GPtrArray *frame_buffer;
} TraceState;

static TraceState state;

static VCPU *get_vcpu(TraceState *state, int vcpu_index) {
  VCPU *c;
  g_rw_lock_reader_lock(&state->vcpus_array_lock);
  c = &g_array_index(state->vcpus, VCPU, vcpu_index);
  g_rw_lock_reader_unlock(&state->vcpus_array_lock);

  return c;
}

static void log_insn_frame(unsigned int cpu_index, void *udata) {
  // VCPU *vcpu = get_vcpu(state, cpu_index);

  // Add change to previous frame
  // Finish previous frame
  // Check if buffer should be dumped to file.
  // Open new one.
  return;
}

static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index) {
  // Add new vcpu
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
    qemu_plugin_register_vcpu_insn_exec_cb(insn, log_insn_frame,
                                           QEMU_PLUGIN_CB_R_REGS, &state);
  }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv) {
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, cb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  // Get reg names
  // qemu_plugin_get_registers
  //
  // Logging
  // qemu_plugin_outs

  return 0;
}
