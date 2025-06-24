// SPDX-FileCopyrightText: 2025 Rot127 <unisono@quyllur.org>
// SPDX-License-Identifier: GPL-2.0-only

#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>
#include <sys/stat.h>

#include "frame.piqi.pb-c-patched.h"
#include "trace_meta.h"

#define MD5LEN 16

static void compute_target_md5(const char *binary_path) {
  const GChecksumType md5 = G_CHECKSUM_MD5;
  guchar target_md5[MD5LEN];

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

static void meta_write_header(FILE *file) {
  // uint64_t toc_off = 0L;
  // WRITE(magic_number);
  // WRITE(out_trace_version);
  // WRITE(frame_arch);
  // WRITE(frame_mach);
  // WRITE(toc_num_frames);
  // WRITE(toc_off);
}

static void init_tracer(Tracer *tracer, char **argv, char **envp) {
  // tracer__init(tracer);
  // tracer->name = tracer_name;
  // tracer->n_args = list_length(argv);
  // tracer->args = argv;
  // tracer->n_envp = list_length(envp);
  // tracer->envp = envp;
  // tracer->version = tracer_version;
}

static void init_target(Target *target, char **argv, char **envp) {
  // compute_target_md5();

  // target__init(target);
  // target->path = target_path;
  // target->n_args = list_length(argv);
  // target->args = argv;
  // target->n_envp = list_length(envp);
  // target->envp = envp;
  // target->md5sum.len = MD5LEN;
  // target->md5sum.data = target_md5;
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

static void write_meta(WLOCKED FILE *file, char **tracer_argv,
                       char **tracer_envp, char **target_argv,
                       char **target_envp) {
  MetaFrame meta;
  Tracer tracer;
  Target target;
  Fstats fstats;

  meta_frame__init(&meta);
  init_tracer(&tracer, tracer_argv, tracer_envp);
  init_target(&target, target_argv, target_envp);
  init_fstats(&fstats, "target-path");

  meta.tracer = &tracer;
  meta.target = &target;
  meta.fstats = &fstats;
  meta.time = time(NULL);
  char *user = g_strdup(g_get_real_name());
  meta.user = user;

  char *host = g_strdup(g_get_host_name());
  meta.host = host;

  size_t msg_size = meta_frame__get_packed_size(&meta);
  uint8_t *packed_buffer = g_alloca(msg_size);
  uint64_t packed_size = meta_frame__pack(&meta, packed_buffer);
  WRITE(packed_size);
  WRITE_BUF(&meta, packed_size);

  free(user);
  free(host);
}
