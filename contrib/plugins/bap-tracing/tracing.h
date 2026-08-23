// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACING_H
#define BAP_TRACING_H

#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>

#include "frame.piqi.pb-c-patched.h"
#include "frame_arch.h"
#include "frame_buffer.h"
#include "trace_consts.h"

struct arch_enum_entry {
  const char *name;
  enum frame_architecture arch;
  size_t machine;
};

static struct arch_enum_entry arch_map[] = {
    {.name = "unknown", .arch = frame_arch_unknown, .machine = 0},
    {.name = "obscure", .arch = frame_arch_obscure, .machine = 0},
    {.name = "m68k", .arch = frame_arch_m68k, .machine = 0},
    {.name = "vax", .arch = frame_arch_vax, .machine = 0},
    {.name = "i960", .arch = frame_arch_i960, .machine = 0},
    {.name = "or32", .arch = frame_arch_or32, .machine = 0},
    {.name = "sparc",
     .arch = frame_arch_sparc,
     .machine = frame_mach_sparc_v8plusa},
    {.name = "sparc64",
     .arch = frame_arch_sparc,
     .machine = frame_mach_sparc_v9b},
    {.name = "spu", .arch = frame_arch_spu, .machine = 0},
    {.name = "mips", .arch = frame_arch_mips, .machine = 0},
    {.name = "i386", .arch = frame_arch_i386, .machine = 0},
    {.name = "l1om", .arch = frame_arch_l1om, .machine = 0},
    {.name = "we32k", .arch = frame_arch_we32k, .machine = 0},
    {.name = "tahoe", .arch = frame_arch_tahoe, .machine = 0},
    {.name = "i860", .arch = frame_arch_i860, .machine = 0},
    {.name = "i370", .arch = frame_arch_i370, .machine = 0},
    {.name = "romp", .arch = frame_arch_romp, .machine = 0},
    {.name = "convex", .arch = frame_arch_convex, .machine = 0},
    {.name = "m88k", .arch = frame_arch_m88k, .machine = 0},
    {.name = "m98k", .arch = frame_arch_m98k, .machine = 0},
    {.name = "pyramid", .arch = frame_arch_pyramid, .machine = 0},
    {.name = "h8300", .arch = frame_arch_h8300, .machine = 0},
    {.name = "pdp11", .arch = frame_arch_pdp11, .machine = 0},
    {.name = "plugin", .arch = frame_arch_plugin, .machine = 0},
    {.name = "ppc", .arch = frame_arch_powerpc, .machine = frame_mach_ppc},
    {.name = "ppc64", .arch = frame_arch_powerpc, .machine = frame_mach_ppc64},
    {.name = "rs6000", .arch = frame_arch_rs6000, .machine = 0},
    {.name = "hppa", .arch = frame_arch_hppa, .machine = 0},
    {.name = "d10v", .arch = frame_arch_d10v, .machine = 0},
    {.name = "d30v", .arch = frame_arch_d30v, .machine = 0},
    {.name = "dlx", .arch = frame_arch_dlx, .machine = 0},
    {.name = "m68hc11", .arch = frame_arch_m68hc11, .machine = 0},
    {.name = "m68hc12", .arch = frame_arch_m68hc12, .machine = 0},
    {.name = "z8k", .arch = frame_arch_z8k, .machine = 0},
    {.name = "h8500", .arch = frame_arch_h8500, .machine = 0},
    {.name = "sh", .arch = frame_arch_sh, .machine = 0},
    {.name = "alpha", .arch = frame_arch_alpha, .machine = 0},
    {.name = "arm", .arch = frame_arch_arm, .machine = 0},
    {.name = "ns32k", .arch = frame_arch_ns32k, .machine = 0},
    {.name = "w65", .arch = frame_arch_w65, .machine = 0},
    {.name = "tic30", .arch = frame_arch_tic30, .machine = 0},
    {.name = "tic4x", .arch = frame_arch_tic4x, .machine = 0},
    {.name = "tic54x", .arch = frame_arch_tic54x, .machine = 0},
    {.name = "tic6x", .arch = frame_arch_tic6x, .machine = 0},
    {.name = "tic80", .arch = frame_arch_tic80, .machine = 0},
    {.name = "v850", .arch = frame_arch_v850, .machine = 0},
    {.name = "arc", .arch = frame_arch_arc, .machine = 0},
    {.name = "m32c", .arch = frame_arch_m32c, .machine = 0},
    {.name = "m32r", .arch = frame_arch_m32r, .machine = 0},
    {.name = "mn10200", .arch = frame_arch_mn10200, .machine = 0},
    {.name = "mn10300", .arch = frame_arch_mn10300, .machine = 0},
    {.name = "fr30", .arch = frame_arch_fr30, .machine = 0},
    {.name = "frv", .arch = frame_arch_frv, .machine = 0},
    {.name = "moxie", .arch = frame_arch_moxie, .machine = 0},
    {.name = "mcore", .arch = frame_arch_mcore, .machine = 0},
    {.name = "mep", .arch = frame_arch_mep, .machine = 0},
    {.name = "ia64", .arch = frame_arch_ia64, .machine = 0},
    {.name = "ip2k", .arch = frame_arch_ip2k, .machine = 0},
    {.name = "iq2000", .arch = frame_arch_iq2000, .machine = 0},
    {.name = "mt", .arch = frame_arch_mt, .machine = 0},
    {.name = "pj", .arch = frame_arch_pj, .machine = 0},
    {.name = "avr", .arch = frame_arch_avr, .machine = 0},
    {.name = "bfin", .arch = frame_arch_bfin, .machine = 0},
    {.name = "cr16", .arch = frame_arch_cr16, .machine = 0},
    {.name = "cr16c", .arch = frame_arch_cr16c, .machine = 0},
    {.name = "crx", .arch = frame_arch_crx, .machine = 0},
    {.name = "cris", .arch = frame_arch_cris, .machine = 0},
    {.name = "rx", .arch = frame_arch_rx, .machine = 0},
    {.name = "s390", .arch = frame_arch_s390, .machine = 0},
    {.name = "score", .arch = frame_arch_score, .machine = 0},
    {.name = "openrisc", .arch = frame_arch_openrisc, .machine = 0},
    {.name = "mmix", .arch = frame_arch_mmix, .machine = 0},
    {.name = "xstormy16", .arch = frame_arch_xstormy16, .machine = 0},
    {.name = "msp430", .arch = frame_arch_msp430, .machine = 0},
    {.name = "xc16x", .arch = frame_arch_xc16x, .machine = 0},
    {.name = "xtensa", .arch = frame_arch_xtensa, .machine = 0},
    {.name = "z80", .arch = frame_arch_z80, .machine = 0},
    {.name = "lm32", .arch = frame_arch_lm32, .machine = 0},
    {.name = "microblaze", .arch = frame_arch_microblaze, .machine = 0},
    {.name = "6502", .arch = frame_arch_6502, .machine = 0},
    {.name = "aarch64", .arch = frame_arch_aarch64, .machine = 0},
    {.name = "8051", .arch = frame_arch_8051, .machine = 0},
    {.name = "sm83", .arch = frame_arch_sm83, .machine = 0},
    {.name = "hexagon", .arch = frame_arch_hexagon, .machine = 0},
    {.name = "tricore",
     .arch = frame_arch_tricore,
     .machine = frame_mach_tricore_162},
    {.name = NULL, .arch = frame_arch_last, .machine = 0},
};

static inline bool get_frame_arch_mach(const char *target_name, uint64_t *arch,
                                       uint64_t *mach) {
  *mach = 0;
  *arch = frame_arch_last;
  const char *aname = arch_map[0].name;
  for (size_t i = 0; arch_map[i].name; ++i) {
    aname = arch_map[i].name;
    if (!strcmp(aname, target_name)) {
      *arch = arch_map[i].arch;
      *mach = arch_map[i].machine;
      break;
    }
  }
  if (*arch == frame_arch_last) {
    qemu_plugin_outs("Could not find frame_arch/mach value for target name: ");
    qemu_plugin_outs(target_name);
    qemu_plugin_outs("\nConsider adding it.\n");
  }
  return *arch != frame_arch_last;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/**
 * \brief VLIW architecture have instructions longer than 4 or 8bytes.
 */
#define MAX_INSTRUCTION_SIZE 64

typedef struct {
  uint8_t bytes[MAX_INSTRUCTION_SIZE];  ///< Instruction bytes.
  size_t size;  ///< Len of instruction in bytes.
  uint64_t vaddr;
} Instruction;

typedef struct {
  struct qemu_plugin_register *handle;  ///< Passed to qemu API.
  GByteArray *content;
  const char *name;
} Register;

typedef struct {
  GPtrArray /*<Register>*/ *registers;
} VCPU;

typedef struct {
  GRWLock vcpus_array_lock;
  GPtrArray /*<VCPU>*/ *vcpus;

  GRWLock frame_buffer_lock;
  GPtrArray /*<FrameBuffer>*/ *frame_buffer;  ///< Indexed by vcpu id

  GRWLock toc_entries_offsets_lock;
  GArray /*<uint64_t>*/ *toc_entries_offsets;

  GRWLock total_num_frames_lock;
  uint64_t total_num_frames;

  GRWLock file_lock;
  FILE *file;

  GRWLock vcpu_mode_lock;
  GPtrArray /*<const char *>*/ *vcpu_modes;  ///< Indexed by vcpu id.

  const char *target_name;
  uint64_t frame_arch;
  uint64_t frame_machine;
  bool is_big_endian;
} TraceState;

VCPU *vcpu_new(void);
Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc);
Instruction *init_insn(struct qemu_plugin_insn *insn);

#endif
