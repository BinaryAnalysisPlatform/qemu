#ifndef TRACE_HELPER_X86
#define TRACE_HELPER_X86

#include "cpu.h"
#include "helper.h"
#include "tracewrap.h"
#include "qemu/log.h"

#define x86_flags_t target_ulong

OperandInfo * store_flag(short val, const char *reg_name);
bool is_flag_changed(x86_flags_t old_flags, x86_flags_t flags, x86_flags_t mask);
short flag_value(x86_flags_t flags, x86_flags_t mask);
void store_if_changed(x86_flags_t old_flags, x86_flags_t flags, x86_flags_t mask, const char *reg_name);
void set_x86_flags(x86_flags_t state);
x86_flags_t x86_flags(void);

#endif // TRACE_HELPER_X86
