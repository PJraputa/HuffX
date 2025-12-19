/* huffx_file.c */

#include "huffx_internal.h"
#include <stdio.h>
#include <stdlib.h>

static int read_whole_file(const char *path, uint8_t **buf, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(len);
    fread(*buf, 1, len, f);
    fclose(f);
    *sz = len;
    return 0;
}

static int write_whole_file(const char *path, const uint8_t *buf, size_t sz) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(buf, 1, sz, f);
    fclose(f);
    return 0;
}

int huffx_compress_file(const char *input_path, const char *output_path, const HuffxOptions *opt) {
    uint8_t *in, *out;
    size_t insz, outsz;
    if (read_whole_file(input_path, &in, &insz) != 0) return -1;

    int rc = huffx_compress_buffer(in, insz, &out, &outsz, opt);
    free(in);
    if (rc == 0) {
        if (write_whole_file(output_path, out, outsz) != 0) rc = -1;
        free(out);
    }
    return rc;
}

/* 修正：接受 opt 以支援解密參數 */
int huffx_decompress_file(const char *input_path, const char *output_path, const HuffxOptions *opt) {
    uint8_t *in, *out;
    size_t insz, outsz;
    if (read_whole_file(input_path, &in, &insz) != 0) return -1;

    int rc = huffx_decompress_buffer(in, insz, &out, &outsz, opt);
    free(in);
    if (rc == 0) {
        if (write_whole_file(output_path, out, outsz) != 0) rc = -1;
        free(out);
    }
    return rc;
}
