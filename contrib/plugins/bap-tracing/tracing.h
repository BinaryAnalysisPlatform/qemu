#ifndef BAP_TRACING_H
#define BAP_TRACING_H

#include <qemu-plugin.h>
#include <stdio.h>

#include "frame.piqi.pb-c-patched.h"
#include "glib.h"
#include "tracewrap.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define FRAME_BUFFER_SIZE_DEFAULT 1024

typedef enum {
  OperandRead = 1,
  OperandWritten = 2,
} OperandAccess;

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
  GPtrArray /*<FrameBuffer>*/ *frame_buffer; ///< Indexed by vcpu id

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

void frame_buffer_flush_to_file(FrameBuffer *buf, FILE *file);
bool frame_buffer_is_full(const FrameBuffer *buf);

Frame *frame_buffer_new_frame(FrameBuffer *buf);
void frame_buffer_append_op_info(FrameBuffer *buf, OperandInfo *oi);

/**
 * \brief Create new std frame
 */
Frame *frame_new_std(uint64_t addr, int vcpu_id);

void frame_add_operand(Frame *frame, OperandInfo *oi);

Register *init_vcpu_register(qemu_plugin_reg_descriptor *desc);
Instruction *init_insn(struct qemu_plugin_insn *insn);

OperandInfo *init_reg_operand_info(const char *name, const uint8_t *value,
                                   size_t value_size, OperandAccess access);

#endif
