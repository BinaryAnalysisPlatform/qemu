// SPDX-FileCopyrightText: 2026 Billow <billow.fun@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACING_MACHINE_H
#define BAP_TRACING_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Resolve an M68K QEMU CPU model name to the corresponding bap-frames
 * machine value. All concrete QEMU models use their closest exact profile;
 * the synthetic "any" model uses the closest standardized BAP profile.
 */
bool bap_tracing_m68k_machine(const char *name, uint64_t *machine);

#endif
