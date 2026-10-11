#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "bundle.h"
#include "latc-bundle-format.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(char *error, size_t size, const char *what)
{
    if (error && size) snprintf(error, size, "%s: %s", what, strerror(errno));
    return -1;
}

static int copy_stream(FILE *out, FILE *in, GChecksum *sum, uint64_t *count)
{
    unsigned char buf[64 * 1024];
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n) {
            if (fwrite(buf, 1, n, out) != n) return -1;
            if (sum) g_checksum_update(sum, buf, n);
            if (count) *count += n;
        }
        if (n != sizeof(buf)) return ferror(in) ? -1 : 0;
    }
}

static int write_cfg(FILE *out, const CfgProgram *p, uint64_t *size_out)
{
    uint64_t string_size = 0;
    for (size_t i = 0; i < p->function_count; i++)
        string_size += strlen(p->functions[i].name) + 1;
    LatcDiskCfgHeader h = {
        .magic = {'L','A','T','C','C','F','G','1'},
        .version = LATC_CFG_VERSION,
        .record_size = sizeof(h),
        .function_count = p->function_count,
        .tb_count = p->tb_count,
        .edge_count = p->edge_count,
        .exec_range_count = p->exec_range_count,
        .string_size = string_size,
    };
    if (fwrite(&h, sizeof(h), 1, out) != 1) return -1;
    uint64_t name_offset = 0;
    for (size_t i = 0; i < p->function_count; i++) {
        const CfgProgramFunction *fn = &p->functions[i];
        LatcDiskFunction d = {fn->start, fn->size, fn->first_tb, fn->tb_count,
                          name_offset, fn->status, 0};
        if (fwrite(&d, sizeof(d), 1, out) != 1) return -1;
        name_offset += strlen(fn->name) + 1;
    }
    for (size_t i = 0; i < p->exec_range_count; i++) {
        LatcDiskExecRange range = {
            .start = p->exec_ranges[i].start,
            .size = p->exec_ranges[i].size,
        };
        if (fwrite(&range, sizeof(range), 1, out) != 1) return -1;
    }
    for (size_t i = 0; i < p->tb_count; i++) {
        const CfgTb *tb = &p->tbs[i];
        uint32_t semantic_flags = tb->semantic_flags ?
            tb->semantic_flags : CFG_TB_CODE64;
        LatcDiskTb d = {tb->start, tb->end, tb->terminator_pc, tb->first_edge,
                    tb->edge_count, tb->selected, tb->terminator,
                    semantic_flags};
        if (fwrite(&d, sizeof(d), 1, out) != 1) return -1;
    }
    for (size_t i = 0; i < p->edge_count; i++) {
        const CfgProgramEdge *edge = &p->edges[i];
        LatcDiskEdge d = {edge->from, edge->to, edge->kind, edge->resolution};
        if (fwrite(&d, sizeof(d), 1, out) != 1) return -1;
    }
    for (size_t i = 0; i < p->function_count; i++) {
        size_t n = strlen(p->functions[i].name) + 1;
        if (fwrite(p->functions[i].name, 1, n, out) != n) return -1;
    }
    *size_out = sizeof(h) + p->function_count * sizeof(LatcDiskFunction) +
                p->exec_range_count * sizeof(LatcDiskExecRange) +
                p->tb_count * sizeof(LatcDiskTb) + p->edge_count * sizeof(LatcDiskEdge) +
                string_size;
    return 0;
}

int latc_bundle_write(const char *runner_path, const char *guest_path,
                      const char *output_path, const char *aot_path,
                      const CfgProgram *program, const uint8_t guest_digest[32],
                      char *error, size_t error_size)
{
    FILE *runner = fopen(runner_path, "rb");
    if (!runner) return fail(error, error_size, runner_path);
    FILE *guest = fopen(guest_path, "rb");
    if (!guest) { fclose(runner); return fail(error, error_size, guest_path); }
    FILE *aot = aot_path ? fopen(aot_path, "rb") : NULL;
    if (aot_path && !aot) {
        fclose(runner); fclose(guest); return fail(error, error_size, aot_path);
    }
    char *tmp = g_strdup_printf("%s.tmp.%ld", output_path, (long)getpid());
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(runner); fclose(guest); if (aot) fclose(aot); g_free(tmp); return fail(error, error_size, output_path); }

    uint64_t runner_size = 0, guest_size = 0, cfg_size = 0, aot_size = 0;
    GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
    GChecksum *aot_sum = g_checksum_new(G_CHECKSUM_SHA256);
    int rc = -1;
    if (copy_stream(out, runner, NULL, &runner_size) ||
        copy_stream(out, guest, guest_digest ? NULL : sum, &guest_size)) goto out;
    uint64_t cfg_offset = runner_size + guest_size;
    if (write_cfg(out, program, &cfg_size)) goto out;
    const char *digest = g_checksum_get_string(sum);
    char digest_hex[65];
    if (guest_digest) {
        for (size_t i = 0; i < 32; i++) {
            snprintf(digest_hex + i * 2, 3, "%02x", guest_digest[i]);
        }
        digest = digest_hex;
    }
    uint64_t aot_offset = cfg_offset + cfg_size;
    if (aot && copy_stream(out, aot, aot_sum, &aot_size)) goto out;
    const char *aot_digest = g_checksum_get_string(aot_sum);
    uint64_t selected_tb_count = 0;
    for (size_t i = 0; i < program->tb_count; i++)
        selected_tb_count += program->tbs[i].selected;
    LatcDiskFooter footer = {
        .magic = {'L','A','T','C','B','N','D','1'},
        .version = LATC_BUNDLE_VERSION,
        .footer_size = sizeof(footer),
        .runner_size = runner_size,
        .guest_offset = runner_size,
        .guest_size = guest_size,
        .cfg_offset = cfg_offset,
        .cfg_size = cfg_size,
        .aot_offset = aot_offset,
        .aot_size = aot_size,
        .function_count = program->function_count,
        .tb_count = program->tb_count,
        .edge_count = program->edge_count,
        .selected_tb_count = selected_tb_count,
    };
    memcpy(footer.guest_sha256, digest, 64);
    if (aot_size) {
        const char *name = g_path_get_basename(aot_path);
        if (strlen(name) >= sizeof(footer.aot_name)) {
            g_free((void *)name); errno = ENAMETOOLONG; goto out;
        }
        memcpy(footer.aot_sha256, aot_digest, 64);
        strcpy(footer.aot_name, name);
        g_free((void *)name);
    }
    if (fwrite(&footer, sizeof(footer), 1, out) != 1 || fflush(out) ||
        fsync(fileno(out))) goto out;
    if (fclose(out)) { out = NULL; goto out; }
    out = NULL;
    struct stat st;
    mode_t mode = stat(runner_path, &st) == 0 ? st.st_mode & 0777 : 0755;
    if (chmod(tmp, mode) || rename(tmp, output_path)) goto out;
    rc = 0;
out:
    if (out) fclose(out);
    fclose(runner); fclose(guest); if (aot) fclose(aot);
    g_checksum_free(sum); g_checksum_free(aot_sum);
    if (rc) { unlink(tmp); fail(error, error_size, "write bundle"); }
    g_free(tmp);
    return rc;
}

int latc_bundle_inspect(const char *path, LatcBundleInfo *info,
                        int verify_guest, char *error, size_t error_size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return fail(error, error_size, path);
    int rc = -1;
    if (fseeko(fp, 0, SEEK_END)) goto out;
    off_t total = ftello(fp);
    if (total < (off_t)sizeof(LatcDiskFooter) ||
        fseeko(fp, total - (off_t)sizeof(LatcDiskFooter), SEEK_SET)) goto invalid;
    LatcDiskFooter f;
    if (fread(&f, sizeof(f), 1, fp) != 1) goto out;
    if (memcmp(f.magic, "LATCBND1", 8) || f.version != LATC_BUNDLE_VERSION ||
        f.footer_size != sizeof(f) || f.guest_offset != f.runner_size ||
        f.cfg_offset != f.guest_offset + f.guest_size ||
        f.aot_offset != f.cfg_offset + f.cfg_size ||
        f.aot_offset + f.aot_size + sizeof(f) != (uint64_t)total ||
        (f.aot_size && (!f.aot_name[0] ||
                        f.aot_name[sizeof(f.aot_name) - 1] ||
                        strchr(f.aot_name, '/')))) goto invalid;
    if (verify_guest) {
        if (fseeko(fp, (off_t)f.guest_offset, SEEK_SET)) goto out;
        GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
        unsigned char buf[64 * 1024];
        uint64_t left = f.guest_size;
        while (left) {
            size_t want = left < sizeof(buf) ? (size_t)left : sizeof(buf);
            size_t n = fread(buf, 1, want, fp);
            if (n != want) { g_checksum_free(sum); goto out; }
            g_checksum_update(sum, buf, n); left -= n;
        }
        int match = memcmp(g_checksum_get_string(sum), f.guest_sha256, 64) == 0;
        g_checksum_free(sum);
        if (!match) goto invalid;
        if (f.aot_size) {
            if (fseeko(fp, (off_t)f.aot_offset, SEEK_SET)) goto out;
            sum = g_checksum_new(G_CHECKSUM_SHA256);
            left = f.aot_size;
            while (left) {
                size_t want = left < sizeof(buf) ? (size_t)left : sizeof(buf);
                size_t n = fread(buf, 1, want, fp);
                if (n != want) { g_checksum_free(sum); goto out; }
                g_checksum_update(sum, buf, n); left -= n;
            }
            match = memcmp(g_checksum_get_string(sum), f.aot_sha256, 64) == 0;
            g_checksum_free(sum);
            if (!match) goto invalid;
        }
    }
    memset(info, 0, sizeof(*info));
    info->runner_size = f.runner_size; info->guest_offset = f.guest_offset;
    info->guest_size = f.guest_size; info->cfg_offset = f.cfg_offset;
    info->cfg_size = f.cfg_size; info->aot_offset = f.aot_offset;
    info->aot_size = f.aot_size; info->function_count = f.function_count;
    info->tb_count = f.tb_count; info->edge_count = f.edge_count;
    info->selected_tb_count = f.selected_tb_count;
    memcpy(info->guest_sha256, f.guest_sha256, 64);
    memcpy(info->aot_sha256, f.aot_sha256, 64);
    memcpy(info->aot_name, f.aot_name, sizeof(f.aot_name));
    rc = 0; goto out;
invalid:
    errno = EINVAL;
out:
    fclose(fp);
    if (rc) fail(error, error_size, "invalid latc bundle");
    return rc;
}
