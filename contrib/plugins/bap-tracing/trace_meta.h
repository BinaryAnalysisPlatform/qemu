// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACE_META_H
#define BAP_TRACE_META_H

/**
 * \brief Empty macros indicate the argument, variable etc.
 * must be locked for writing.
 */
#define WLOCKED

#define WRITE(x)                                                               \
  do {                                                                         \
    if (fwrite(&(x), sizeof(x), 1, file) != 1)                                 \
      qemu_plugin_outs("fwrite failed");                                       \
  } while (0)

#define WRITE_BUF(x, n)                                                        \
  do {                                                                         \
    if (fwrite((x), 1, (n), file) != n)                                        \
      qemu_plugin_outs("fwrite failed");                                       \
  } while (0)

#endif
