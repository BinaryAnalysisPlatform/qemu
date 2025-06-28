// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACE_META_H
#define BAP_TRACE_META_H

#include <err.h>

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

#endif
