// SPDX-FileCopyrightText: 2026 Billow <billow.fun@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "machine.h"

#include <stddef.h>
#include <string.h>

#include "frame_arch.h"

struct m68k_machine_entry {
  const char *name;
  uint64_t machine;
};

static const struct m68k_machine_entry m68k_machines[] = {
    /* QEMU's synthetic "any" model has no dedicated BAP machine value. */
    {.name = "any", .machine = frame_mach_mcf_isa_b_float_emac},
    {.name = "cfv4e", .machine = frame_mach_mcf_isa_b_float_emac},
    {.name = "m5206", .machine = frame_mach_mcf_isa_a},
    {.name = "m5208", .machine = frame_mach_mcf_isa_aplus_emac},
    {.name = "m68000", .machine = frame_mach_m68000},
    {.name = "m68010", .machine = frame_mach_m68010},
    {.name = "m68020", .machine = frame_mach_m68020},
    {.name = "m68030", .machine = frame_mach_m68030},
    {.name = "m68040", .machine = frame_mach_m68040},
    {.name = "m68060", .machine = frame_mach_m68060},
};

bool bap_tracing_m68k_machine(const char *name, uint64_t *machine) {
  if (!name || !machine) {
    return false;
  }
  for (size_t i = 0; i < sizeof(m68k_machines) / sizeof(m68k_machines[0]);
       i++) {
    if (!strcmp(name, m68k_machines[i].name)) {
      *machine = m68k_machines[i].machine;
      return true;
    }
  }
  return false;
}
