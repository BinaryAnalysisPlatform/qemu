// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include "trace_riscv.h"

#include <elf.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TAG_RISCV_ARCH    5
#define RISCV_ATTR_VENDOR "riscv"

/*
 * A 64-bit value needs at most ceil(64/7) = 10 bytes in ULEB128.
 * Any sequence requiring more bytes cannot fit in uint64_t.
 */
#define ULEB128_MAX_BYTES 10

/*
 * Decode one ULEB128 integer from buf[0..buf_len).
 * Sets *out_bytes to the number of bytes consumed, or 0 on failure
 * (buffer too short mid-sequence, or value exceeds 64 bits).
 */
static uint64_t uleb128_read(const uint8_t *buf, size_t buf_len,
                             size_t *out_bytes) {
    uint64_t result = 0;
    for (size_t i = 0; i < buf_len && i < ULEB128_MAX_BYTES; i++) {
        uint8_t b = buf[i];
        uint8_t data = b & 0x7f;
        uint32_t shift = 7 * (uint32_t)i;
        /*
         * At i=9 (shift=63), bit 0 of data maps to bit 63 of result.
         * Bits 1-6 of data would map to bits 64-69: overflow.
         */
        if (shift == 63 && (data & 0x7e)) {
            if (out_bytes) {
                *out_bytes = 0;
            }
            return 0;
        }
        result |= (uint64_t)data << shift;
        if (!(b & 0x80)) {
            if (out_bytes) {
                *out_bytes = i + 1;
            }
            return result;
        }
    }
    /* Buffer exhausted mid-sequence, or 10th byte still had continuation. */
    if (out_bytes) {
        *out_bytes = 0;
    }
    return 0;
}

/*
 * Parse the .riscv.attributes section data and return a copy of the
 * Tag_RISCV_arch string, or NULL if not found.
 *
 * Format (GNU/ARM ELF attribute ABI, adopted by RISC-V):
 *   'A'                                      (1 byte, format version)
 *   [vendor sub-section]:
 *     sub_size                               (4 bytes LE, includes itself)
 *     vendor_name                            (null-terminated)
 *     [file sub-sub-section, tag = 1]:
 *       tag                                  (1 byte)
 *       ssz    (4 bytes LE, includes tag byte + itself + content)
 *       [attribute pairs: ULEB128 tag, then value]
 *
 * Tag parity convention: odd tags have NTBS values, even have ULEB128 int.
 */
static char *parse_riscv_attrs(const uint8_t *data, size_t size) {
    if (size < 1 || data[0] != 'A') {
        return NULL;
    }

    size_t voff = 1;
    while (voff + 5 <= size) {
        uint32_t sub_size;
        memcpy(&sub_size, data + voff, 4);
        if (sub_size < 5 || voff + sub_size > size) {
            break;
        }

        const char *vendor = (const char *)(data + voff + 4);
        size_t vendor_len = strnlen(vendor, sub_size - 4) + 1;

        if (!strcmp(vendor, RISCV_ATTR_VENDOR)) {
            size_t soff = voff + 4 + vendor_len;
            size_t vend = voff + sub_size;

            while (soff + 5 <= vend) {
                uint8_t sub_tag = data[soff];
                uint32_t ssz;
                memcpy(&ssz, data + soff + 1, 4);
                size_t attr_end = soff + ssz;

                if (sub_tag == 1 /* Tag_File */ && attr_end <= vend) {
                    size_t p = soff + 5;
                    while (p < attr_end) {
                        size_t tlen;
                        uint64_t tag = uleb128_read(data + p, attr_end - p,
                                                    &tlen);
                        if (!tlen) {
                            break;
                        }
                        p += tlen;
                        if (p >= attr_end) {
                            break;
                        }
                        if (tag == TAG_RISCV_ARCH) {
                            return g_strdup((const char *)(data + p));
                        }
                        /* Skip value: odd tags are NTBS, even are ULEB128. */
                        if (tag & 1) {
                            p += strnlen((const char *)(data + p),
                                         attr_end - p) + 1;
                        } else {
                            size_t skip;
                            uleb128_read(data + p, attr_end - p, &skip);
                            if (!skip) {
                                break;
                            }
                            p += skip;
                        }
                    }
                }
                soff = attr_end;
            }
        }
        voff += sub_size;
    }
    return NULL;
}

/*
 * Expand to a function read_attrs_section<BITS> that reads the ELF section
 * header table using SHDR_T (Elf32_Shdr or Elf64_Shdr) and shoff of type
 * OFF_T (Elf32_Off or Elf64_Off), finds the SHT_RISCV_ATTRIBUTES section,
 * and delegates to parse_riscv_attrs.
 */
#define DEFINE_READ_ATTRS_SECTION(BITS, SHDR_T, OFF_T)                        \
static char *read_attrs_section##BITS(FILE *f, uint16_t shnum, OFF_T shoff) { \
    SHDR_T *shdrs = g_malloc(shnum * sizeof(SHDR_T));                          \
    if (fseeko(f, (off_t)(shoff), SEEK_SET) < 0 ||                            \
        fread(shdrs, sizeof(SHDR_T), shnum, f) != shnum) {                    \
        g_free(shdrs);                                                         \
        return NULL;                                                           \
    }                                                                          \
    char *isa = NULL;                                                          \
    for (int i = 0; i < shnum && !isa; i++) {                                  \
        if (shdrs[i].sh_type != SHT_RISCV_ATTRIBUTES) {                       \
            continue;                                                          \
        }                                                                      \
        uint8_t *buf = g_malloc(shdrs[i].sh_size);                            \
        if (fseeko(f, (off_t)(shdrs[i].sh_offset), SEEK_SET) >= 0 &&         \
            fread(buf, 1, shdrs[i].sh_size, f) == shdrs[i].sh_size) {        \
            isa = parse_riscv_attrs(buf, shdrs[i].sh_size);                   \
        }                                                                      \
        g_free(buf);                                                           \
    }                                                                          \
    g_free(shdrs);                                                             \
    return isa;                                                                \
}

DEFINE_READ_ATTRS_SECTION(32, Elf32_Shdr, Elf32_Off)
DEFINE_READ_ATTRS_SECTION(64, Elf64_Shdr, Elf64_Off)

char *riscv_isa_from_elf(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    unsigned char ident[EI_NIDENT];
    if (fread(ident, 1, EI_NIDENT, f) != EI_NIDENT ||
        ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
        ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
        fclose(f);
        return NULL;
    }

    rewind(f);
    char *isa = NULL;

    if (ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr ehdr;
        if (fread(&ehdr, sizeof(ehdr), 1, f) == 1 &&
            ehdr.e_machine == EM_RISCV) {
            isa = read_attrs_section32(f, ehdr.e_shnum, ehdr.e_shoff);
        }
    } else if (ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr ehdr;
        if (fread(&ehdr, sizeof(ehdr), 1, f) == 1 &&
            ehdr.e_machine == EM_RISCV) {
            isa = read_attrs_section64(f, ehdr.e_shnum, ehdr.e_shoff);
        }
    }

    fclose(f);
    return isa;
}
