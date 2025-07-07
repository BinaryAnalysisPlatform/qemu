// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "frame_arch.h"
#include "frame_buffer.h"
#include "qemu-plugin.h"
#include "trace_consts.h"
#include "tracing.h"

static TraceState state = {0};

static void add_mem_op(VCPU *vcpu, unsigned int vcpu_index, FrameBuffer *fbuf,
                       uint64_t vaddr, qemu_plugin_mem_value *mval,
                       bool is_store) {
  if (!frame_buffer_append_mem_info(fbuf, vaddr, mval, is_store)) {
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

  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
  g_rw_lock_writer_unlock(&state.vcpus_array_lock);
}

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
  GByteArray *rdata = g_byte_array_new();
  for (size_t i = 0; i < current_regs->len; ++i) {
    qemu_plugin_reg_descriptor *reg =
        &g_array_index(current_regs, qemu_plugin_reg_descriptor, i);
    size_t s = qemu_plugin_read_register(reg->handle, rdata);
    Register *prev_reg = g_ptr_array_index(vcpu->registers, i);
    g_assert(!strcmp(prev_reg->name, reg->name) &&
             prev_reg->handle == reg->handle);
    memcpy(prev_reg->content->data, rdata->data, prev_reg->content->len);
    frame_buffer_append_reg_info(fbuf, reg->name, rdata, s, OperandRead);
    // Flush byte array
    g_byte_array_set_size(rdata, 0);
  }
}

static GPtrArray *registers_init(void) {
  GArray *reg_list = qemu_plugin_get_registers();

  if (reg_list->len == 0) {
    g_array_free(reg_list, false);
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

static void flush_and_write_toc_entry(FrameBuffer *fbuf) {
  g_rw_lock_writer_lock(&state.file_lock);
  g_rw_lock_writer_lock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_lock(&state.total_num_frames_lock);

  state.total_num_frames += frame_buffer_flush_to_file(fbuf, state.file);
  uint64_t next_toc_entry = ftell(state.file);
  g_array_append_val(state.toc_entries_offsets, next_toc_entry);

  g_rw_lock_writer_unlock(&state.total_num_frames_lock);
  g_rw_lock_writer_unlock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_unlock(&state.file_lock);
}

static void flush_all_frame_bufs(void) {
  g_rw_lock_writer_lock(&state.file_lock);
  g_rw_lock_writer_lock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_lock(&state.total_num_frames_lock);
  g_rw_lock_writer_lock(&state.frame_buffer_lock);

  FILE *file = state.file;

  // Dump the rest of the frames but be mindeful about the
  // maximum number of frames per TOC entry.

  size_t total_to_write = 0;
  for (size_t i = 0; i < state.vcpus->len; ++i) {
    // Add post operands to last instructions.
    FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, i);
    VCPU *vcpu = g_ptr_array_index(state.vcpus, i);
    g_assert(vcpu);
    GArray *current_regs = qemu_plugin_get_registers();
    g_assert(current_regs->len == vcpu->registers->len);
    add_post_reg_state(vcpu, i, current_regs, fbuf);
    frame_buffer_close_frame(fbuf);

    total_to_write += fbuf->idx;
  }

  size_t entry_count = 0;
  for (size_t i = 0; i < state.vcpus->len && total_to_write > 0; ++i) {
    if (entry_count == frames_per_toc_entry) {
      entry_count = 0;
      uint64_t next_toc_entry = ftell(state.file);
      g_array_append_val(state.toc_entries_offsets, next_toc_entry);
    }

    FrameBuffer *fbuf = g_ptr_array_index(state.frame_buffer, i);
    for (size_t k = 0; k < fbuf->idx; ++k) {
      frame_buffer_write_frame_to_file(fbuf, file, k);
      entry_count++;
      state.total_num_frames++;
      total_to_write--;
    }
  }

  g_rw_lock_writer_unlock(&state.frame_buffer_lock);
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
  GArray *current_regs = qemu_plugin_get_registers();
  g_assert(current_regs->len == vcpu->registers->len);

  if (!frame_buffer_is_empty(fbuf)) {
    add_post_reg_state(vcpu, vcpu_index, current_regs, fbuf);
    frame_buffer_close_frame(fbuf);
  }

  if (frame_buffer_is_full(fbuf)) {
    flush_and_write_toc_entry(fbuf);
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

  uint64_t frame_arch = 0;
  uint64_t frame_mach = 0;
  if (!get_frame_arch_mach(state.target_name, &frame_arch, &frame_mach)) {
    qemu_plugin_outs("Failed to get arch/mach.\n");
  }
  const char *mode = FRAME_MODE_NONE;
  if (frame_arch == frame_arch_powerpc && frame_mach == frame_mach_ppc64) {
    mode = FRAME_MODE_PPC64;
  } else if (frame_arch == frame_arch_powerpc && frame_mach == frame_mach_ppc) {
    mode = FRAME_MODE_PPC32;
  }
  // TODO: handle ARM
  g_ptr_array_insert(state.vcpu_modes, vcpu_index,
                     mode ? g_strdup(mode) : NULL);

  g_rw_lock_writer_unlock(&state.vcpu_mode_lock);
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
                                     QEMU_PLUGIN_CB_NO_REGS, QEMU_PLUGIN_MEM_R,
                                     NULL);
  }
}

static void plugin_exit(qemu_plugin_id_t id, void *udata) {
  flush_all_frame_bufs();

  g_rw_lock_writer_lock(&state.file_lock);
  g_rw_lock_reader_lock(&state.toc_entries_offsets_lock);
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

  for (size_t i = 0; i < entries; ++i) {
    uint64_t toc_entry_off =
        g_array_index(state.toc_entries_offsets, uint64_t, i);
    WRITE(toc_entry_off);
  }
  fclose(file);

  g_rw_lock_reader_unlock(&state.total_num_frames_lock);
  g_rw_lock_reader_unlock(&state.toc_entries_offsets_lock);
  g_rw_lock_writer_unlock(&state.file_lock);
  qemu_plugin_outs("Finished trace\n");
}

static bool write_header(FILE *file, const char *target_name) {
  uint64_t frame_arch = 0;
  uint64_t frame_mach = 0;
  if (!get_frame_arch_mach(target_name, &frame_arch, &frame_mach)) {
    qemu_plugin_outs("Failed to get arch/mach.\n");
    return false;
  }
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
  char *output = get_argv_val(argv, argc, "out");
  if (!output) {
    qemu_plugin_outs("'out' argument is missing.\n");
    qemu_plugin_outs("This is required.\n");
    qemu_plugin_outs("Pass it with 'out=<output_file>'.\n\n");
    exit(1);
  }

  state.target_name = g_strdup(info->target_name);
  state.frame_buffer = g_ptr_array_new();
  state.toc_entries_offsets = g_array_new(false, true, sizeof(uint64_t));
  state.vcpus = g_ptr_array_new();
  state.vcpu_modes = g_ptr_array_new();
  state.file = fopen(output, "wb");
  if (!(state.frame_buffer || state.vcpus || state.file ||
        !state.toc_entries_offsets)) {
    return 1;
  }
  g_free(output);
  if (!write_header(state.file, info->target_name)) {
    qemu_plugin_outs("Failed to write header.\n");
    return 1;
  }
  write_meta(state.file, argv, argc);

  g_array_append_val(state.toc_entries_offsets, offset_toc_start);

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, cb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
