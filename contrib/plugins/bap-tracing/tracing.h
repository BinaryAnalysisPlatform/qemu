// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACING_H
#define BAP_TRACING_H

#include <qemu-plugin.h>
#include <stdio.h>
#include <glib.h>

#include "frame.piqi.pb-c-patched.h"
#include "tracewrap.h"
#include "frame_buffer.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define FRAME_BUFFER_SIZE_DEFAULT 1024

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
  GArray /*<VCPU>*/ *vcpus;

  GRWLock frame_buffer_lock;
  GPtrArray /*<FrameBuffer>*/ *frame_buffer; ///< Indexed by vcpu id

  GRWLock file_lock;
  FILE *file;
} TraceState;

VCPU *vcpu_new(void);
Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc);
Instruction *init_insn(struct qemu_plugin_insn *insn);

#endif
