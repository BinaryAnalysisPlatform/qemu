// SPDX-FileCopyrightText: 2026 Billow <billow.fun@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "mem_value.h"

#include <assert.h>
#include <string.h>

static void check_value(qemu_plugin_mem_value value, const uint8_t *expected,
                        size_t expected_size) {
  uint8_t output[16] = {0};
  size_t size = bap_tracing_mem_value_to_le(&value, output);
  assert(size == expected_size);
  assert(memcmp(output, expected, size) == 0);
}

int main(void) {
  const uint8_t u8[] = {0xa5};
  const uint8_t u16[] = {0x34, 0x12};
  const uint8_t u32[] = {0x78, 0x56, 0x34, 0x12};
  const uint8_t u64[] = {0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01};
  const uint8_t u128[] = {
      0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
      0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f,
  };

  check_value((qemu_plugin_mem_value){
                  .type = QEMU_PLUGIN_MEM_VALUE_U8,
                  .data.u8 = 0xa5,
              },
              u8, sizeof(u8));
  check_value((qemu_plugin_mem_value){
                  .type = QEMU_PLUGIN_MEM_VALUE_U16,
                  .data.u16 = 0x1234,
              },
              u16, sizeof(u16));
  check_value((qemu_plugin_mem_value){
                  .type = QEMU_PLUGIN_MEM_VALUE_U32,
                  .data.u32 = 0x12345678,
              },
              u32, sizeof(u32));
  check_value((qemu_plugin_mem_value){
                  .type = QEMU_PLUGIN_MEM_VALUE_U64,
                  .data.u64 = UINT64_C(0x0123456789abcdef),
              },
              u64, sizeof(u64));
  check_value((qemu_plugin_mem_value){
                  .type = QEMU_PLUGIN_MEM_VALUE_U128,
                  .data.u128 = {
                      .low = UINT64_C(0x0123456789abcdef),
                      .high = UINT64_C(0x0f1e2d3c4b5a6978),
                  },
              },
              u128, sizeof(u128));
  return 0;
}
