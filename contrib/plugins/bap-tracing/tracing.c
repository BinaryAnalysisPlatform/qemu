// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"
#include "frame_arch.h"
#include "frame_buffer.h"
#include "machine.h"
#include "mem_value.h"
#include "qemu-plugin.h"
#include "trace_consts.h"
#include "trace_meta.h"
#include "tracing.h"

static TraceState state = {0};

static bool resolve_frame_arch_mach(const char *target_name,
                                    const char *machine_name, uint64_t *arch,
                                    uint64_t *machine) {
  if (!get_frame_arch_mach(target_name, arch, machine)) {
    return false;
  }

  if (*arch == frame_arch_m68k) {
    if (!machine_name) {
      qemu_plugin_outs("'machine' argument is required for M68K.\n");
      qemu_plugin_outs(
          "Pass machine=any/cfv4e/m5206/m5208/m68000/m68010/m68020/m68030/"
          "m68040/m68060.\n");
      return false;
    }
    if (!bap_tracing_m68k_machine(machine_name, machine)) {
      qemu_plugin_outs("Unknown or unsupported M68K machine: ");
      qemu_plugin_outs(machine_name);
      qemu_plugin_outs("\n");
      return false;
    }
  } else if (machine_name) {
    qemu_plugin_outs(
        "'machine' currently accepts only M68K CPU model names, but target "
        "is not M68K.\n");
    return false;
  }
  return true;
}

static void add_mem_op(VCPU *vcpu, unsigned int vcpu_index, FrameBuffer *fbuf,
                       uint64_t vaddr, qemu_plugin_mem_value *mval,
                       bool is_store) {
  uint8_t *buf = g_malloc(16);
  size_t mval_bytes = bap_tracing_mem_value_to_le(mval, buf);
  if (!frame_buffer_append_mem_info_take(fbuf, vaddr, buf, mval_bytes * 8,
                                         is_store)) {
    qemu_plugin_outs("Failed to append memory info\n");
  }
  return;
}

static void log_insn_mem_access(unsigned int vcpu_index,
                                qemu_plugin_meminfo_t info, uint64_t vaddr,
                                void *userdata) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);
  g_rw_lock_reader_lock(&state.frame_buffer_lock);

  VCPU *vcpu = g_ptr_array_index(state.vcpus, vcpu_index);
  g_assert(vcpu);
  FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, vcpu_index);

  bool is_store = qemu_plugin_mem_is_store(info);
  qemu_plugin_mem_value mval = qemu_plugin_mem_get_value(info);

  add_mem_op(vcpu, vcpu_index, fbuf, vaddr, &mval, is_store);

  g_rw_lock_reader_unlock(&state.frame_buffer_lock);
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);
}

static void add_post_reg_state(VCPU *vcpu, unsigned int vcpu_index,
                               GArray *current_regs, FrameBuffer *fbuf) {
  g_autoptr(GByteArray) rdata = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    Register *prev_reg = vcpu->registers->pdata[i];

    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    int s = qemu_plugin_read_register(reg->handle, rdata);
    assert(s == prev_reg->content->len);
    swap_to_le(rdata->data, s, state.is_big_endian);
    if (!memcmp(rdata->data, prev_reg->content->data, s)) {
      // No change
      // Flush byte array
      g_byte_array_set_size(rdata, 0);
      continue;
    }

    if (!frame_buffer_append_reg_info(fbuf, reg->name, rdata, s,
                                      OperandWritten)) {
      qemu_plugin_outs("Failed to append opinfo.\n");
      return;
    }
    // Flush byte array
    g_byte_array_set_size(rdata, 0);
  }
}

static void add_pre_reg_state(VCPU *vcpu, unsigned int vcpu_index,
                              GArray *current_regs, FrameBuffer *fbuf) {
  g_autoptr(GByteArray) rdata = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    size_t s = qemu_plugin_read_register(reg->handle, rdata);
    Register *prev_reg = g_ptr_array_index(vcpu->registers, i);
    g_assert(!g_ascii_strcasecmp(prev_reg->name, reg->name) &&
             prev_reg->handle == reg->handle);
    memcpy_le(prev_reg->content->data, rdata->data, prev_reg->content->len,
              state.is_big_endian);
    frame_buffer_append_reg_info(fbuf, reg->name, prev_reg->content, s,
                                 OperandRead);
    // Flush byte array
    g_byte_array_set_size(rdata, 0);
  }
}

static GPtrArray *registers_init(void) {
  g_autoptr(GArray) reg_list = qemu_plugin_get_registers();

  if (reg_list->len == 0) {
    return NULL;
  }
  GPtrArray *registers = g_ptr_array_new();
  for (size_t r = 0; r < reg_list->len; r++) {
    qemu_plugin_reg_descriptor *rd =
        &g_array_index(reg_list, qemu_plugin_reg_descriptor, r);
    Register *reg = init_vcpu_register(rd);
    g_ptr_array_add(registers, reg);
  }

  return registers->len ? g_steal_pointer(&registers) : NULL;
}

static void flush_frame_buffer(FrameBuffer *fbuf, bool preserve_open_frame) {
  g_rw_lock_writer_lock(&state.file_lock);
  g_rw_lock_writer_lock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_lock(&state.total_num_frames_lock);

  Frame *open_frame = preserve_open_frame && !frame_buffer_is_full(fbuf)
                          ? fbuf->fbuf[fbuf->idx]
                          : NULL;

  for (size_t i = 0; i < fbuf->idx; i++) {
    if (state.total_num_frames > 0 &&
        state.total_num_frames % frames_per_toc_entry == 0) {
      uint64_t toc_entry = ftell(state.file);
      g_array_append_val(state.toc_entries_offsets, toc_entry);
    }
    frame_buffer_write_frame_to_file(fbuf, state.file, i);
    state.total_num_frames++;
  }
  frame_buffer_clean(fbuf);
  if (open_frame) {
    /*
     * At process exit, an instruction can still be open because there is no
     * later instruction callback from which to read its post-state.  Keep it
     * out of the trace, but preserve the buffer invariant while the exit
     * callback finishes.  Every earlier frame is complete and safe to write.
     */
    fbuf->fbuf[0] = open_frame;
  }

  g_rw_lock_writer_unlock(&state.total_num_frames_lock);
  g_rw_lock_writer_unlock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_unlock(&state.file_lock);
}

static void log_insn_reg_access(unsigned int vcpu_index, void *udata) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);
  g_rw_lock_writer_lock(&state.frame_buffer_lock);

  FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, vcpu_index);
  VCPU *vcpu = g_ptr_array_index(state.vcpus, vcpu_index);
  g_assert(vcpu);
  g_autoptr(GArray) current_regs = qemu_plugin_get_registers();
  g_assert(current_regs->len == vcpu->registers->len);

  if (!frame_buffer_is_empty(fbuf)) {
    add_post_reg_state(vcpu, vcpu_index, current_regs, fbuf);
    frame_buffer_close_frame(fbuf);
  }

  if (frame_buffer_is_full(fbuf)) {
    flush_frame_buffer(fbuf, false);
  }

  // Open new one.
  Instruction *insn = udata;
  g_rw_lock_reader_lock(&state.vcpu_mode_lock);
  if (!frame_buffer_new_frame_std(
          fbuf, vcpu_index, insn->vaddr,
          g_ptr_array_index(state.vcpu_modes, vcpu_index), insn->bytes,
          insn->size)) {
    err(1, "Failed to add new frame.\n");
  }
  g_rw_lock_reader_unlock(&state.vcpu_mode_lock);

  add_pre_reg_state(vcpu, vcpu_index, current_regs, fbuf);

  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);
}

Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc) {
  Register *reg = g_new0(Register, 1);
  g_autofree gchar *lower = g_utf8_strdown(desc->name, -1);

  reg->handle = desc->handle;
  reg->name = g_intern_string(lower);
  reg->content = g_byte_array_new();

  /* read the initial value */
  int r = qemu_plugin_read_register(reg->handle, reg->content);
  g_assert(r > 0);
  return reg;
}

static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index) {
  g_rw_lock_writer_lock(&state.vcpus_array_lock);
  g_rw_lock_writer_lock(&state.frame_buffer_lock);
  g_rw_lock_writer_lock(&state.vcpu_mode_lock);

  VCPU *vcpu = g_malloc0(sizeof(VCPU));
  vcpu->registers = registers_init();
  g_assert(vcpu->registers);
  g_ptr_array_insert(state.vcpus, vcpu_index, vcpu);

  FrameBuffer *vcpu_frame_buffer = frame_buffer_new();
  g_ptr_array_insert(state.frame_buffer, vcpu_index, vcpu_frame_buffer);

  const char *mode = FRAME_MODE_NONE;
  if (state.frame_arch == frame_arch_powerpc &&
      state.frame_machine == frame_mach_ppc64) {
    mode = FRAME_MODE_PPC64;
  } else if (state.frame_arch == frame_arch_powerpc &&
             state.frame_machine == frame_mach_ppc) {
    mode = FRAME_MODE_PPC32;
  }
  // TODO: handle ARM
  g_ptr_array_insert(state.vcpu_modes, vcpu_index,
                     mode ? g_strdup(mode) : NULL);

  g_rw_lock_writer_unlock(&state.vcpu_mode_lock);
  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_writer_unlock(&state.vcpus_array_lock);
}

static void finalize_vcpu_trace(qemu_plugin_id_t id, unsigned int vcpu_index) {
  g_rw_lock_reader_lock(&state.vcpus_array_lock);
  g_rw_lock_writer_lock(&state.frame_buffer_lock);

  if (vcpu_index >= state.vcpus->len || vcpu_index >= state.frame_buffer->len) {
    qemu_plugin_outs("Mismatched vCPU index while finalizing trace.\n");
    goto out;
  }

  VCPU *vcpu = g_ptr_array_index(state.vcpus, vcpu_index);
  FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, vcpu_index);
  if (!vcpu || !fbuf) {
    qemu_plugin_outs("Missing vCPU state while finalizing trace.\n");
    goto out;
  }

  if (!frame_buffer_is_empty(fbuf)) {
    g_autoptr(GArray) current_regs = qemu_plugin_get_registers();
    g_assert(current_regs->len == vcpu->registers->len);
    add_post_reg_state(vcpu, vcpu_index, current_regs, fbuf);
    frame_buffer_close_frame(fbuf);
  }

  if (fbuf->idx > 0) {
    flush_frame_buffer(fbuf, false);
  }

out:
  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_reader_unlock(&state.vcpus_array_lock);
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
                                     QEMU_PLUGIN_CB_NO_REGS, QEMU_PLUGIN_MEM_RW,
                                     NULL);
  }
}

static void plugin_exit(qemu_plugin_id_t id, void *udata) {
  qemu_plugin_outs("Exiting bap-tracing plugin\n");

  /*
   * System emulation can request shutdown from inside the current vCPU.  In
   * that path neither the vCPU-exit nor idle callback is guaranteed to run,
   * so complete frames below the still-open instruction would otherwise be
   * silently dropped.  They already contain both pre- and post-state and can
   * be committed without reading registers outside vCPU context.
   */
  g_rw_lock_writer_lock(&state.frame_buffer_lock);
  for (size_t i = 0; i < state.frame_buffer->len; i++) {
    FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, i);
    if (fbuf && fbuf->idx > 0) {
      flush_frame_buffer(fbuf, true);
    }
  }
  g_rw_lock_writer_unlock(&state.frame_buffer_lock);

  g_rw_lock_writer_lock(&state.file_lock);
  g_rw_lock_writer_lock(&state.toc_entries_offsets_lock);
  g_rw_lock_reader_lock(&state.total_num_frames_lock);

  FILE *file = state.file;

  // Update fields in the header
  uint64_t toc_index_offset = ftell(file);
  SEEK(offset_toc_index_offset);
  WRITE(toc_index_offset);
  SEEK(offset_total_num_frames);
  WRITE(state.total_num_frames);

  // Write the TOC index
  SEEK(toc_index_offset);
  WRITE(frames_per_toc_entry);
  size_t add = state.total_num_frames % frames_per_toc_entry != 0 ? 1 : 0;
  size_t entries = ((state.total_num_frames) / frames_per_toc_entry) + add;

  /*
   * The version 3 reader expects one trailing, unused TOC entry for the final
   * (possibly partial) block. Boundary offsets collected while writing point
   * at frames 64, 128, ...; pad only that final entry with the end of data.
   */
  while (state.toc_entries_offsets->len < entries) {
    g_array_append_val(state.toc_entries_offsets, toc_index_offset);
  }
  g_assert(state.toc_entries_offsets->len == entries);

  for (size_t i = 0; i < entries; ++i) {
    uint64_t toc_entry_off =
        g_array_index(state.toc_entries_offsets, uint64_t, i);
    WRITE(toc_entry_off);
  }
  fclose(file);

  g_rw_lock_reader_unlock(&state.total_num_frames_lock);
  g_rw_lock_writer_unlock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_unlock(&state.file_lock);
  qemu_plugin_outs("Finished trace\n");
}

static bool write_header(FILE *file, uint64_t frame_arch, uint64_t frame_mach) {
  uint64_t total_num_frames = 0ULL;
  uint64_t toc_index_offset = 0ULL;
  WRITE(magic_number);
  WRITE(trace_version);
  WRITE(frame_arch);
  WRITE(frame_mach);
  WRITE(total_num_frames); // Gets updated later
  WRITE(toc_index_offset); // Gets updated later
  return true;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv) {
  qemu_plugin_outs("Target name: ");
  qemu_plugin_outs(info->target_name);
  qemu_plugin_outs("\n");
  g_autofree char *output = get_argv_val(argv, argc, "out");
  if (!output) {
    qemu_plugin_outs("'out' argument is missing.\n");
    qemu_plugin_outs("This is required.\n");
    qemu_plugin_outs("Pass it with 'out=<output_file>'.\n\n");
    exit(1);
  }
  g_autofree char *endianness = get_argv_val(argv, argc, "endianness");
  if (!endianness || (strcmp(endianness, "b") && strcmp(endianness, "l"))) {
    qemu_plugin_outs(
        "'endianness' argument is missing or is not 'b' or 'l'.\n");
    qemu_plugin_outs("This is required until QEMU plugins get a richer API.\n");
    qemu_plugin_outs("Pass it with 'endianness=[b/l]'.\n\n");
    exit(1);
  }
  state.is_big_endian = endianness[0] == 'b';

  g_autofree char *machine = get_argv_val(argv, argc, "machine");
  if (!resolve_frame_arch_mach(info->target_name, machine, &state.frame_arch,
                               &state.frame_machine)) {
    return 1;
  }

  state.target_name = g_strdup(info->target_name);
  state.frame_buffer = g_ptr_array_new();
  state.toc_entries_offsets = g_array_new(false, true, sizeof(uint64_t));
  state.vcpus = g_ptr_array_new();
  state.vcpu_modes = g_ptr_array_new();
  state.file = fopen(output, "wb");
  if (!state.frame_buffer || !state.vcpus || !state.vcpu_modes || !state.file ||
      !state.toc_entries_offsets) {
    return 1;
  }
  if (!write_header(state.file, state.frame_arch, state.frame_machine)) {
    qemu_plugin_outs("Failed to write header.\n");
    return 1;
  }
  write_meta(state.file, argv, argc);

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_exit_cb(id, finalize_vcpu_trace);
  /*
   * System emulation does not unrealize vCPUs during a normal shutdown, so
   * the exit callback is not sufficient on its own. The idle callback runs
   * in vCPU context after a debugger stop and can still read final register
   * state. Reusing the idempotent finalizer here also preserves traces that
   * never fill a complete frame buffer.
   */
  qemu_plugin_register_vcpu_idle_cb(id, finalize_vcpu_trace);
  qemu_plugin_register_vcpu_tb_trans_cb(id, cb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
