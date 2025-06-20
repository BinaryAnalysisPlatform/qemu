#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "qemu/osdep.h"
#include "cpu.h"

#include "frame.piqi.pb-c.h"


/** initializes trace subsystem.

    All pointers are owned by the caller.

    @param filename a name of filesystem entry where trace will be dumpled,
           if NULL then the name is basename(argv[0]).frames

    @param targetname a path to the executable, must be non NULL


    @param argv a full list of arguments passed to the tracer, NULL terminated.
           Can be NULL or empty (i.e., contain only a NULL element).
           The list may include target arguments.

    @param envp a null terminated list of environment parameters,
           can be NULL or empty.

    @param target_argv a null terminated list of target arguments,
           can be NULL or empty.

    @param target_envp a null terminated list of target environment,
           can be NULL or empty.
 */
void qemu_trace_init(const char *filename, const char *targetname,
                     char **argv, char **envp,
                     char **target_argv,
                     char **target_envp);
void qemu_trace_newframe(target_ulong addr, int tread_id);
void qemu_trace_add_operand(OperandInfo *oi, int inout);
void qemu_trace_endframe(CPUArchState *env, target_ulong pc, target_ulong size);
void qemu_trace_finish(uint32_t exit_code);

OperandInfo * load_store_reg(target_ulong reg, target_ulong val, int ls);
OperandInfo * load_store_mem(target_ulong addr, target_ulong val, int ls, int len);

#define REG_EFLAGS 66
#define REG_LO 33
#define REG_HI 34

#define REG_CPSR 64
#define REG_APSR 65
#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

#define REG_NF 94
#define REG_ZF 95
#define REG_CF 96
#define REG_VF 97
#define REG_QF 98
#define REG_GE 99

#define SEG_BIT 8
