// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>
#include <sys/stat.h>

#include "frame.piqi.pb-c-patched.h"
#include "trace_consts.h"
#include "trace_meta.h"

#define MD5LEN 16

static void compute_target_md5(const char *binary_path,
                               guchar target_md5[MD5LEN]) {
  const GChecksumType md5 = G_CHECKSUM_MD5;

  GChecksum *cs = g_checksum_new(md5);
  FILE *target = fopen(binary_path, "r");
  guchar buf[BUFSIZ];
  gsize expected_length = MD5LEN;

  if (!cs)
    qemu_plugin_outs("failed to create a checksum");
  if (!target)
    qemu_plugin_outs("failed to open target binary");
  if (g_checksum_type_get_length(md5) != expected_length)
    abort();

  while (!feof(target)) {
    size_t len = fread(buf, 1, BUFSIZ, target);
    if (ferror(target))
      qemu_plugin_outs("failed to read target binary");
    g_checksum_update(cs, buf, len);
  }

  g_checksum_get_digest(cs, target_md5, &expected_length);
  fclose(target);
}

static void init_tracer(Tracer *tracer, char **argv, int argc) {
  tracer__init(tracer);
  tracer->name = g_strdup(TRACER_NAME);
  tracer->n_args = argc;
  tracer->args = argv;
  tracer->n_envp = 0;
  tracer->envp = NULL;
  tracer->version = g_strdup(TRACER_VERSION);
}

static void init_target(Target *target, const char *bin_path, char **argv,
                        int argc) {
  target__init(target);

  if (bin_path) {
    guchar *target_md5 = g_malloc0(MD5LEN);
    compute_target_md5(bin_path, target_md5);
    target->path = g_strdup(bin_path);
    target->md5sum.len = MD5LEN;
    target->md5sum.data = target_md5;
  }
  target->n_args = argc;
  target->args = argv;
  target->n_envp = 0;
  target->envp = NULL;
}

#ifdef G_OS_UNIX
static bool unix_fill_fstats(Fstats *fstats, const char *path) {
  struct stat stats;
  if (stat(path, &stats) < 0) {
    qemu_plugin_outs("failed to obtain file stats");
    return false;
  }

  fstats->size = stats.st_size;
  fstats->atime = stats.st_atime;
  fstats->mtime = stats.st_mtime;
  fstats->ctime = stats.st_ctime;
  return true;
}
#endif

static bool init_fstats(Fstats *fstats, const char *binary_path) {
  fstats__init(fstats);
#ifdef G_OS_UNIX
  return unix_fill_fstats(fstats, binary_path);
#endif
  return true;
}

char *get_argv_val(char **argv, int argc, const char *key) {
  for (size_t i = 0; i < argc; ++i) {
    if (!strncmp(argv[i], key, strlen(key))) {
      const char *val = argv[i] + strlen(key);
      if (val[0] != '=') {
        qemu_plugin_outs("Invalid argument value for ");
        qemu_plugin_outs(key);
        qemu_plugin_outs("\n");
        qemu_plugin_outs("Should be 'key=val'\n");
        return NULL;
      }
      val++;
      const char *end = strchr(val, ',');
      while (end && *(end - 1) == '\\') {
        // Allow escaped commas.
        end = strchr(val, ',');
      }
      size_t len = !end ? strlen(val) : end - val;
      char *argument = g_malloc0(len + 1);
      memcpy(argument, val, len);
      return argument;
    }
  }
  return NULL;
}

void file_exists_exit(const char *file) {
  FILE *test = fopen(file, "r");
  if (!test) {
    qemu_plugin_outs("Failed to open binary file: ");
    qemu_plugin_outs(file);
    qemu_plugin_outs("\n");
    exit(1);
  }
  fclose(test);
}

void write_meta(WLOCKED FILE *file, char **plugin_argv, size_t plugin_argc) {
  char *arg_bin_path = get_argv_val(plugin_argv, plugin_argc, "bin_path");
  // Note: Usually we should get the binary path from
  // qemu_plugin_path_to_binary(). But it doesn't seem to work due to dependency
  // issues. See: https://gitlab.com/qemu-project/qemu/-/issues/3014
  const char *bin_path = NULL; // qemu_plugin_path_to_binary();
  if (!bin_path && !arg_bin_path) {
    qemu_plugin_outs("\nFailed to retrieve the binary path\n");
    qemu_plugin_outs("This is required.\n");
    qemu_plugin_outs("You can pass it as plugin argument "
                     "'bin_path=<path>'.\n\n");
    exit(1);
  } else {
    file_exists_exit(arg_bin_path);
  }
  if (bin_path && arg_bin_path) {
    qemu_plugin_outs(
        "'bin_path' argument found, but the binary path is known to the module.\n\
                      Argument 'bin_path' is ignored.\n");
  }

  MetaFrame meta = {0};
  Tracer tracer = {0};
  Target target = {0};
  Fstats fstats = {0};

  meta_frame__init(&meta);
  init_tracer(&tracer, plugin_argv, plugin_argc);
  init_target(&target, bin_path ? bin_path : arg_bin_path, plugin_argv,
              plugin_argc);
  init_fstats(&fstats, bin_path ? bin_path : arg_bin_path);

  meta.tracer = &tracer;
  meta.target = &target;
  meta.fstats = &fstats;
  meta.time = time(NULL);
  char *user = g_strdup(g_get_real_name());
  meta.user = user;

  char *host = g_strdup(g_get_host_name());
  meta.host = host;

  size_t msg_size = meta_frame__get_packed_size(&meta);
  uint8_t *packed_buffer = g_malloc0(msg_size);
  uint64_t packed_size = meta_frame__pack(&meta, packed_buffer);
  g_assert(msg_size == packed_size);
  WRITE(packed_size);

  // I don't know why, but ASAN crashes at this line if the WRITE_BUF macro
  // is used. Although it should be the exact same code.
  if (fwrite((packed_buffer), 1, (packed_size), file) != packed_size) {
   err(1, "fwrite failed");
  }

  g_free(packed_buffer);
  g_free(tracer.name);
  g_free(tracer.version);
  g_free(target.path);
  g_free(target.md5sum.data);

  g_free(user);
  g_free(host);
  g_free(arg_bin_path);
}

/// Copies src to dst. dst will always be in little endian byte order.
void memcpy_le(uint8_t *dst, uint8_t *src, size_t len, bool big_endian) {
  if (!big_endian) {
    memcpy(dst, src, len);
    return;
  }
  for (size_t k = 0; k < len; ++k) {
    dst[k] = src[len - 1 - k];
  }
}

void swap_to_le(uint8_t *buf, size_t len, bool big_endian) {
  if (!big_endian || len == 1) {
    return;
  }
  for (size_t k = 0; k < len / 2; ++k) {
    uint8_t tmp = buf[k];
    buf[k] = buf[len - 1 - k];
    buf[len - 1 - k] = tmp;
  }
}
