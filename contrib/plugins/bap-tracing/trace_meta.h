// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACE_META_H
#define BAP_TRACE_META_H

#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/**
 * \brief Empty macros indicate the argument, variable etc.
 * must be locked for writing.
 */
#define WLOCKED

#define WRITE(x)                                                               \
  do {                                                                         \
    if (fwrite(&(x), sizeof(x), 1, file) != 1)                                 \
      err(1, "fwrite failed");                                                 \
  } while (0)

#define WRITE_BUF(x, n)                                                        \
  do {                                                                         \
    if (fwrite((x), 1, (n), file) != n)                                        \
      err(1, "fwrite failed");                                                 \
  } while (0)

#define SEEK(off)                                                              \
  do {                                                                         \
    if (fseek(file, (off), SEEK_SET) < 0)                                      \
      err(1, "stream not seekable");                                           \
  } while (0)

void write_meta(WLOCKED FILE *file, char **plugin_argv, size_t plugin_argc);
char *get_argv_val(char **argv, int argc, const char *key);
void file_exists_exit(const char *file);
void memcpy_le(uint8_t *dst, const uint8_t *src, size_t len, bool big_endian);
void swap_to_le(uint8_t *buf, size_t len, bool big_endian);

#endif
