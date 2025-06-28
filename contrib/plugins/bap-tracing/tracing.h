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
  enum frame_architecture val;
};

static struct arch_enum_entry arch_map[] = {
    {.name = "unknown", .val = frame_arch_unknown},
    {.name = "obscure", .val = frame_arch_obscure},
    {.name = "m68k", .val = frame_arch_m68k},
    {.name = "vax", .val = frame_arch_vax},
    {.name = "i960", .val = frame_arch_i960},
    {.name = "or32", .val = frame_arch_or32},
    {.name = "sparc", .val = frame_arch_sparc},
    {.name = "spu", .val = frame_arch_spu},
    {.name = "mips", .val = frame_arch_mips},
    {.name = "i386", .val = frame_arch_i386},
    {.name = "l1om", .val = frame_arch_l1om},
    {.name = "we32k", .val = frame_arch_we32k},
    {.name = "tahoe", .val = frame_arch_tahoe},
    {.name = "i860", .val = frame_arch_i860},
    {.name = "i370", .val = frame_arch_i370},
    {.name = "romp", .val = frame_arch_romp},
    {.name = "convex", .val = frame_arch_convex},
    {.name = "m88k", .val = frame_arch_m88k},
    {.name = "m98k", .val = frame_arch_m98k},
    {.name = "pyramid", .val = frame_arch_pyramid},
    {.name = "h8300", .val = frame_arch_h8300},
    {.name = "pdp11", .val = frame_arch_pdp11},
    {.name = "plugin", .val = frame_arch_plugin},
    {.name = "powerpc", .val = frame_arch_powerpc},
    {.name = "rs6000", .val = frame_arch_rs6000},
    {.name = "hppa", .val = frame_arch_hppa},
    {.name = "d10v", .val = frame_arch_d10v},
    {.name = "d30v", .val = frame_arch_d30v},
    {.name = "dlx", .val = frame_arch_dlx},
    {.name = "m68hc11", .val = frame_arch_m68hc11},
    {.name = "m68hc12", .val = frame_arch_m68hc12},
    {.name = "z8k", .val = frame_arch_z8k},
    {.name = "h8500", .val = frame_arch_h8500},
    {.name = "sh", .val = frame_arch_sh},
    {.name = "alpha", .val = frame_arch_alpha},
    {.name = "arm", .val = frame_arch_arm},
    {.name = "ns32k", .val = frame_arch_ns32k},
    {.name = "w65", .val = frame_arch_w65},
    {.name = "tic30", .val = frame_arch_tic30},
    {.name = "tic4x", .val = frame_arch_tic4x},
    {.name = "tic54x", .val = frame_arch_tic54x},
    {.name = "tic6x", .val = frame_arch_tic6x},
    {.name = "tic80", .val = frame_arch_tic80},
    {.name = "v850", .val = frame_arch_v850},
    {.name = "arc", .val = frame_arch_arc},
    {.name = "m32c", .val = frame_arch_m32c},
    {.name = "m32r", .val = frame_arch_m32r},
    {.name = "mn10200", .val = frame_arch_mn10200},
    {.name = "mn10300", .val = frame_arch_mn10300},
    {.name = "fr30", .val = frame_arch_fr30},
    {.name = "frv", .val = frame_arch_frv},
    {.name = "moxie", .val = frame_arch_moxie},
    {.name = "mcore", .val = frame_arch_mcore},
    {.name = "mep", .val = frame_arch_mep},
    {.name = "ia64", .val = frame_arch_ia64},
    {.name = "ip2k", .val = frame_arch_ip2k},
    {.name = "iq2000", .val = frame_arch_iq2000},
    {.name = "mt", .val = frame_arch_mt},
    {.name = "pj", .val = frame_arch_pj},
    {.name = "avr", .val = frame_arch_avr},
    {.name = "bfin", .val = frame_arch_bfin},
    {.name = "cr16", .val = frame_arch_cr16},
    {.name = "cr16c", .val = frame_arch_cr16c},
    {.name = "crx", .val = frame_arch_crx},
    {.name = "cris", .val = frame_arch_cris},
    {.name = "rx", .val = frame_arch_rx},
    {.name = "s390", .val = frame_arch_s390},
    {.name = "score", .val = frame_arch_score},
    {.name = "openrisc", .val = frame_arch_openrisc},
    {.name = "mmix", .val = frame_arch_mmix},
    {.name = "xstormy16", .val = frame_arch_xstormy16},
    {.name = "msp430", .val = frame_arch_msp430},
    {.name = "xc16x", .val = frame_arch_xc16x},
    {.name = "xtensa", .val = frame_arch_xtensa},
    {.name = "z80", .val = frame_arch_z80},
    {.name = "lm32", .val = frame_arch_lm32},
    {.name = "microblaze", .val = frame_arch_microblaze},
    {.name = "6502", .val = frame_arch_6502},
    {.name = "aarch64", .val = frame_arch_aarch64},
    {.name = "8051", .val = frame_arch_8051},
    {.name = "sm83", .val = frame_arch_sm83},
    {.name = "hexagon", .val = frame_arch_hexagon},
    {.name = NULL, .val = frame_arch_last},
};

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/**
 * \brief VLIW architecture have instructions longer than 4 or 8bytes.
 */
#define MAX_INSTRUCTION_SIZE 64

typedef struct {
  uint8_t bytes[MAX_INSTRUCTION_SIZE]; ///< Instruction bytes.
  size_t size;                         ///< Len of instruction in bytes.
  uint64_t vaddr;
} Instruction;

typedef struct {
  struct qemu_plugin_register *handle; ///< Passed to qemu API.
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
  GPtrArray /*<FrameBuffer>*/ *frame_buffer; ///< Indexed by vcpu id

  GRWLock toc_entries_offsets_lock;
  GArray /*<uint64_t>*/ *toc_entries_offsets;

  GRWLock file_lock;
  FILE *file;
} TraceState;

VCPU *vcpu_new(void);
Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc);
Instruction *init_insn(struct qemu_plugin_insn *insn);

#endif
