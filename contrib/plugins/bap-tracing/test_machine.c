// SPDX-FileCopyrightText: 2026 Aya contributors
// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "frame_arch.h"
#include "machine.h"

struct expected_machine {
  const char *name;
  uint64_t machine;
};

int main(void) {
  static const struct expected_machine expected[] = {
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

  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
    uint64_t machine = 0;
    assert(bap_tracing_m68k_machine(expected[i].name, &machine));
    assert(machine == expected[i].machine);
  }

  uint64_t machine = 0;
  assert(!bap_tracing_m68k_machine(NULL, &machine));
  assert(!bap_tracing_m68k_machine("", &machine));
  assert(!bap_tracing_m68k_machine("m68008", &machine));
  assert(!bap_tracing_m68k_machine("unknown", &machine));
  assert(!bap_tracing_m68k_machine("m68020", NULL));
  return 0;
}
