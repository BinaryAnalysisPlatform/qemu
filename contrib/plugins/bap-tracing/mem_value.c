// SPDX-FileCopyrightText: 2026 Billow <billow.fun@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "mem_value.h"

#include <stdlib.h>

static void write_le64(uint8_t *output, uint64_t value) {
  for (size_t i = 0; i < 8; ++i) {
    output[i] = (uint8_t)(value >> (i * 8));
  }
}

size_t bap_tracing_mem_value_to_le(const qemu_plugin_mem_value *value,
                                   uint8_t output[16]) {
  switch (value->type) {
  case QEMU_PLUGIN_MEM_VALUE_U8:
    output[0] = value->data.u8;
    return 1;
  case QEMU_PLUGIN_MEM_VALUE_U16:
    output[0] = (uint8_t)value->data.u16;
    output[1] = (uint8_t)(value->data.u16 >> 8);
    return 2;
  case QEMU_PLUGIN_MEM_VALUE_U32:
    output[0] = (uint8_t)value->data.u32;
    output[1] = (uint8_t)(value->data.u32 >> 8);
    output[2] = (uint8_t)(value->data.u32 >> 16);
    output[3] = (uint8_t)(value->data.u32 >> 24);
    return 4;
  case QEMU_PLUGIN_MEM_VALUE_U64:
    write_le64(output, value->data.u64);
    return 8;
  case QEMU_PLUGIN_MEM_VALUE_U128:
    write_le64(output, value->data.u128.low);
    write_le64(output + 8, value->data.u128.high);
    return 16;
  default:
    abort();
  }
}
