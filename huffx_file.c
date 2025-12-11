/* huffx_file.c - HuffX 檔案 I/O 包裝 */

#include "huffx_internal.h"

#include <stdio.h>
#include <stdlib.h>

/* 簡單的整檔讀取工具：成功回傳 0，失敗回傳 -1 */
static int read_entire_file(const char *path, uint8_t **out_buf, size_t *out_size)
{
    *out_buf = NULL;
    *out_size = 0;

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return -1;
    }
    long len = ftell(f);
    if (len < 0) {
        perror("ftell");
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return -1;
    }

    uint8_t *buf = (uint8_t *)huffx_xmalloc((size_t)len);
    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (nread != (size_t)len) {
        fprintf(stderr, "HuffX: read_entire_file short read\n");
        free(buf);
        return -1;
    }

    *out_buf = buf;
    *out_size = (size_t)len;
    return 0;
}

/* 將 buffer 完整寫入檔案 */
static int write_entire_file(const char *path, const uint8_t *buf, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    size_t nwritten = fwrite(buf, 1, size, f);
    if (nwritten != size) {
        fprintf(stderr, "HuffX: write_entire_file short write\n");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/* ===================== 檔案版 API ===================== */

int huffx_compress_file(const char *input_path,
                        const char *output_path,
                        const HuffxOptions *opt)
{
    uint8_t *input_buf = NULL;
    size_t   input_size = 0;
    if (read_entire_file(input_path, &input_buf, &input_size) != 0) {
        return -1;
    }

    uint8_t *out_buf = NULL;
    size_t   out_size = 0;
    int rc = huffx_compress_buffer(input_buf, input_size,
                                   &out_buf, &out_size,
                                   opt);
    free(input_buf);

    if (rc != 0) {
        return rc;
    }

    rc = write_entire_file(output_path, out_buf, out_size);
    free(out_buf);
    return rc;
}

int huffx_decompress_file(const char *input_path,
                          const char *output_path)
{
    uint8_t *input_buf = NULL;
    size_t   input_size = 0;
    if (read_entire_file(input_path, &input_buf, &input_size) != 0) {
        return -1;
    }

    uint8_t *out_buf = NULL;
    size_t   out_size = 0;
    int rc = huffx_decompress_buffer(input_buf, input_size,
                                     &out_buf, &out_size);
    free(input_buf);

    if (rc != 0) {
        return rc;
    }

    rc = write_entire_file(output_path, out_buf, out_size);
    free(out_buf);
    return rc;
}

/* 舊版 API 相容（使用 XOR or not）*/

int huffx_compress(const char *input_path,
                   const char *output_path,
                   int xor_enable,
                   uint8_t xor_key)
{
    HuffxOptions opt;
    if (xor_enable) {
        opt.algo    = HUFFX_CRYPT_XOR;
        opt.xor_key = xor_key;
    } else {
        opt.algo    = HUFFX_CRYPT_NONE;
        opt.xor_key = 0;
    }
    return huffx_compress_file(input_path, output_path, &opt);
}

int huffx_decompress(const char *input_path,
                     const char *output_path)
{
    return huffx_decompress_file(input_path, output_path);
}

