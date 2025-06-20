#include "tracewrap.h"
#include "trace_consts.h"
#include "exec/cpu_ldst.h"

#include <glib.h>
#include <err.h>
#include "qemu/log.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <limits.h>
#include <stdlib.h>


char tracer_name[] = "qemu";
char tracer_version[] = "2.0.0/tracewrap";

static Frame * g_frame;
static uint64_t frames_per_toc_entry = 64LL;
static uint32_t open_frame = 0;
static FILE *file = NULL;

/* don't use the following data directly!
   use toc_init, toc_update and toc_write functions instead */
static uint64_t *toc = NULL;
static int toc_entries = 0;
static int toc_capacity = 0;
static uint64_t toc_num_frames = 0;

#define MD5LEN 16
static guchar target_md5[MD5LEN];
static char target_path[PATH_MAX] = "unknown";


#define WRITE(x) do {                                   \
        if (!file)                                      \
            err(1, "qemu_trace is not initialized");    \
        if (fwrite(&(x), sizeof(x),1,file) != 1)        \
            err(1, "fwrite failed");                    \
    } while(0)

#define WRITE_BUF(x,n) do {                                 \
        if (!file)                                      \
            err(1, "qemu_trace is not initialized");    \
        if (fwrite((x),1,(n),file) != n)                  \
            err(1, "fwrite failed");                    \
    } while(0)

#define SEEK(off) do {                          \
        if (fseek(file,(off), SEEK_SET) < 0)    \
            err(1, "stream not seekable");      \
    } while(0)


static void toc_init(void) {
    if (toc_entries != 0)
        err(1, "qemu_trace was initialized twice");
    toc = g_new(uint64_t, 1024);
    toc_capacity = 1024;
    toc_entries = 0;
}

static void toc_append(uint64_t entry) {
    if (toc_capacity <= toc_entries) {
        toc = g_renew(uint64_t, toc, toc_capacity * 2);
        toc_capacity *= 2;
    }
    toc[toc_entries++] = entry;
}

static void toc_write(void) {
    int64_t toc_offset = ftell(file);
    if (toc_offset > 0) {
        int i = 0;
        WRITE(frames_per_toc_entry);
        for (i = 0; i < toc_entries; i++)
            WRITE(toc[i]);
        SEEK(num_trace_frames_offset);
        WRITE(toc_num_frames);
        SEEK(toc_offset_offset);
        WRITE(toc_offset);
    }
}

static void toc_update(void) {
    toc_num_frames++;
    if (toc_num_frames % frames_per_toc_entry == 0) {
        int64_t off = ftell(file);
        if (off >= 0) toc_append(off);
    }
}

static void write_header(void) {
    uint64_t toc_off = 0L;
    WRITE(magic_number);
    WRITE(out_trace_version);
    WRITE(frame_arch);
    WRITE(frame_mach);
    WRITE(toc_num_frames);
    WRITE(toc_off);
}

static int list_length(char **list) {
    int n=0;
    if (list) {
        char **p = list;
        for (;*p;p++,n++);
    }
    return n;
}

static void compute_target_md5(void) {
    const GChecksumType md5 = G_CHECKSUM_MD5;
    GChecksum *cs = g_checksum_new(md5);
    FILE *target = fopen(target_path, "r");
    guchar buf[BUFSIZ];
    gsize expected_length = MD5LEN;

    if (!cs) err(1, "failed to create a checksum");
    if (!target) err(1, "failed to open target binary");
    if (g_checksum_type_get_length(md5) != expected_length) abort();

    while (!feof(target)) {
        size_t len = fread(buf,1,BUFSIZ,target);
        if (ferror(target))
            err(1, "failed to read target binary");
        g_checksum_update(cs, buf, len);
    }

    g_checksum_get_digest(cs, target_md5, &expected_length);
    fclose(target);
}

static void store_to_trace(ProtobufCBuffer *self, size_t len, const uint8_t *data) {
    WRITE_BUF(data,len);
}

static void init_tracer(Tracer *tracer, char **argv, char **envp) {
    tracer__init(tracer);
    tracer->name = tracer_name;
    tracer->n_args = list_length(argv);
    tracer->args = argv;
    tracer->n_envp = list_length(envp);
    tracer->envp = envp;
    tracer->version = tracer_version;
}

static void init_target(Target *target, char **argv, char **envp) {
    compute_target_md5();

    target__init(target);
    target->path = target_path;
    target->n_args = list_length(argv);
    target->args = argv;
    target->n_envp = list_length(envp);
    target->envp = envp;
    target->md5sum.len = MD5LEN;
    target->md5sum.data = target_md5;
}

#ifdef G_OS_UNIX
static void unix_fill_fstats(Fstats *fstats, char *path) {
    struct stat stats;
    if (stat(path, &stats) < 0)
        err(1, "failed to obtain file stats");

    fstats->size  = stats.st_size;
    fstats->atime = stats.st_atime;
    fstats->mtime = stats.st_mtime;
    fstats->ctime = stats.st_ctime;
}
#endif


static void init_fstats(Fstats *fstats) {
    fstats__init(fstats);
#ifdef G_OS_UNIX
    unix_fill_fstats(fstats, target_path);
#endif
}


static void write_meta(
    char **tracer_argv,
    char **tracer_envp,
    char **target_argv,
    char **target_envp)
{
    MetaFrame meta;
    Tracer tracer;
    Target target;
    Fstats fstats;
    ProtobufCBuffer buffer;

    buffer.append = store_to_trace;


    meta_frame__init(&meta);
    init_tracer(&tracer, tracer_argv, tracer_envp);
    init_target(&target, target_argv, target_envp);
    init_fstats(&fstats);

    meta.tracer = &tracer;
    meta.target = &target;
    meta.fstats = &fstats;
    meta.time = time(NULL);
    char *user = g_strdup(g_get_real_name());
    meta.user = user;

    char *host = g_strdup(g_get_host_name());
    meta.host = host;

    uint64_t size = meta_frame__get_packed_size(&meta);
    WRITE(size);

    meta_frame__pack_to_buffer(&meta, &buffer);

    free(user);
    free(host);
}


void qemu_trace_init(const char *filename,
                     const char *targetname,
                     char **argv, char **envp,
                     char **target_argv,
                     char **target_envp) {
    qemu_log("Initializing tracer\n");
    if (realpath(targetname,target_path) == NULL)
        err(1, "can't get target path");


    char *name = filename
        ? g_strdup(filename)
        : g_strdup_printf("%s.frames", basename(target_path));
    file = fopen(name, "wb");
    if (file == NULL)
        err(1, "tracewrap: can't open trace file %s", name);
    write_header();
    write_meta(argv, envp, target_argv, target_envp);
    toc_init();
    g_free(name);
}


void qemu_trace_newframe(target_ulong addr, int __unused/*thread_id*/ ) {
    int thread_id = 1;
    if (open_frame) {
        qemu_log("frame is still open");
        qemu_trace_endframe(NULL, 0, 0);
    }

    open_frame = 1;
    g_frame = g_new(Frame,1);
    frame__init(g_frame);

    StdFrame *sframe = g_new(StdFrame, 1);
    std_frame__init(sframe);
    g_frame->std_frame = sframe;

    sframe->address = addr;
    sframe->thread_id = thread_id;

    OperandValueList *ol_in = g_new(OperandValueList,1);
    operand_value_list__init(ol_in);
    ol_in->n_elem = 0;
    sframe->operand_pre_list = ol_in;

    OperandValueList *ol_out = g_new(OperandValueList,1);
    operand_value_list__init(ol_out);
    ol_out->n_elem = 0;
    sframe->operand_post_list = ol_out;
}

static inline void free_operand(OperandInfo *oi) {
    OperandInfoSpecific *ois = oi->operand_info_specific;

    //Free reg-operand
    RegOperand *ro = ois->reg_operand;
    if (ro && ro->name)
        g_free(ro->name);
    g_free(ro);

    //Free mem-operand
    MemOperand *mo = ois->mem_operand;
    g_free(mo);
    g_free(oi->value.data);
    g_free(oi->taint_info);
    g_free(ois);
    g_free(oi->operand_usage);
    g_free(oi);
}

void qemu_trace_add_operand(OperandInfo *oi, int inout) {
    if (!open_frame) {
        if (oi)
            free_operand(oi);
        return;
    }
    OperandValueList *ol;
    if (inout & 0x1) {
        ol = g_frame->std_frame->operand_pre_list;
    } else {
        ol = g_frame->std_frame->operand_post_list;
    }

    oi->taint_info = g_new(TaintInfo, 1);
    taint_info__init(oi->taint_info);
    oi->taint_info->no_taint = 1;
    oi->taint_info->has_no_taint = 1;

    ol->n_elem += 1;
    ol->elem = g_renew(OperandInfo *, ol->elem, ol->n_elem);
    ol->elem[ol->n_elem - 1] = oi;
}

void qemu_trace_endframe(CPUArchState *env, target_ulong pc, target_ulong size) {
    int i = 0;
    StdFrame *sframe = g_frame->std_frame;

    if (!open_frame) return;

    sframe->rawbytes.len = size;
    sframe->rawbytes.data = g_malloc(size);
    for (i = 0; i < size; i++) {
        sframe->rawbytes.data[i] = cpu_ldub_code(env, pc+i);
    }

    size_t msg_size = frame__get_packed_size(g_frame);
    uint8_t *packed_buffer = g_alloca(msg_size);
    uint64_t packed_size = frame__pack(g_frame, packed_buffer);
    WRITE(packed_size);
    WRITE_BUF(packed_buffer, packed_size);
    toc_update();

    //counting num_frames in newframe does not work by far ...
    //how comes? disas_arm_insn might not always return at the end?
    for (i = 0; i < sframe->operand_pre_list->n_elem; i++)
        free_operand(sframe->operand_pre_list->elem[i]);
    g_free(sframe->operand_pre_list->elem);
    g_free(sframe->operand_pre_list);

    for (i = 0; i < sframe->operand_post_list->n_elem; i++)
        free_operand(sframe->operand_post_list->elem[i]);
    g_free(sframe->operand_post_list->elem);
    g_free(sframe->operand_post_list);

    g_free(sframe->rawbytes.data);
    g_free(sframe);
    g_free(g_frame);
    open_frame = 0;
}

void qemu_trace_finish(uint32_t exit_code) {
    toc_write();
    if (fclose(file) != 0)
        err(1,"failed to write trace file, the file maybe corrupted");
}
