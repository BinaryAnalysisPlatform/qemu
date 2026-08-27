// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#ifndef BAP_TRACE_RISCV_H
#define BAP_TRACE_RISCV_H

/**
 * \brief Read the RISC-V ISA string from a binary's ELF attributes section.
 *
 * Parses the SHT_RISCV_ATTRIBUTES section of the ELF binary at \p path,
 * locates the "riscv" vendor sub-section, and returns the value of
 * Tag_RISCV_arch (tag 5) verbatim as a newly-allocated string.
 *
 * \param path Path to the ELF binary.
 * \return Heap-allocated ISA string (caller must g_free), or NULL if the
 *         binary is not a RISC-V ELF or the attributes section is absent.
 */
char *riscv_isa_from_elf(const char *path);

#endif
