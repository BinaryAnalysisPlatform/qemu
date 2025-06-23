#ifndef BAP_TRACING_H
#define BAP_TRACING_H

#include <qemu-plugin.h>
#include <stdio.h>

#include "tracewrap.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define FRAME_BUFFER_SIZE_DEFAULT 1024

/**
 * \brief VLIW architecture have instructions longer than 4 or 8bytes.
 */
#define MAX_INSTRUCTION_SIZE 64

typedef struct {
  Frame **fbuf;
  size_t len;
} FrameBuffer;

typedef struct {
  uint8_t bytes[MAX_INSTRUCTION_SIZE]; ///< Instruction bytes.
  size_t size; ///< Len of instruction in bytes.
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
  FrameBuffer *frame_buffer;

  GRWLock file_lock;
  FILE *file;
} TraceState;

VCPU *vcpu_new(void);

/**
 * \brief Initializes a frame buffer with space for \p size frames.
 * Returns the buffer or NULL in case of failure.
 */
FrameBuffer *frame_buffer_init(size_t size);

/**
 * \brief Push a frame into the buffer.
 * Returns true on success. False otherwise.
 */
bool frame_buffer_push(FrameBuffer *buf, Frame *frame);

/**
 * \brief Flusehs the buffer and returns it's content.
 * The size of the returned buffer is written to \p fbuf_size.
 */
Frame **frame_buffer_flush(FrameBuffer *buf, size_t *fbuf_size);

/**
 * \brief Create new std frame
 */
Frame *frame_new_std(uint64_t addr, int vcpu_id);

void frame_add_operand(Frame *frame, OperandInfo *oi, bool is_out);

Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc);
Instruction *init_insn(struct qemu_plugin_insn *insn);

#endif
