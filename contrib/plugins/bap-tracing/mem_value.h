// SPDX-FileCopyrightText: 2026 Aya contributors
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACING_MEM_VALUE_H
#define BAP_TRACING_MEM_VALUE_H

#include <stddef.h>
#include <stdint.h>

#include "qemu-plugin.h"

size_t bap_tracing_mem_value_to_le(const qemu_plugin_mem_value *value,
                                   uint8_t output[16]);

#endif
