/* huffx_cli.c - 命令列介面 (CLI) for HuffX library
 *
 * 用法：
 *   壓縮（不加密）：
 *     huffx -c input.bin output.hxf
 *
 *   壓縮（XOR 加密，key = 42）：
 *     huffx -c -k 42 input.bin output.hxf
 *     huffx --compress --xor-key=42 input.bin output.hxf
 *
 *   解壓：
 *     huffx -d input.hxf output.bin
 *     huffx --decompress input.hxf output.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "huffx.h"

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "HuffX - Huffman-based compressor with optional XOR encryption\n\n"
            "Usage:\n"
            "  Compress:\n"
            "    %s -c [options] <input> <output>\n"
            "    %s --compress [options] <input> <output>\n"
            "\n"
            "    Options for compress:\n"
            "      -k <0-255>          Enable XOR encryption with given key (decimal)\n"
            "      --xor-key=<0-255>   Same as -k\n"
            "\n"
            "  Decompress:\n"
            "    %s -d <input> <output>\n"
            "    %s --decompress <input> <output>\n"
            "\n"
            "  Help:\n"
            "    %s -h | --help\n",
            prog, prog, prog, prog, prog
    );
}

/* 將字串解析成 0~255 的 unsigned 整數；成功回傳 1，失敗回傳 0 */
static int parse_u8(const char *s, uint8_t *out)
{
    char *end = NULL;
    long val = strtol(s, &end, 0); /* 支援 10 進位 / 0x.. / 0.. */
    if (end == s || *end != '\0') {
        return 0;
    }
    if (val < 0 || val > 255) {
        return 0;
    }
    *out = (uint8_t)val;
    return 1;
}

int main(int argc, char **argv)
{
    const char *prog = (argc > 0) ? argv[0] : "huffx";

    if (argc < 2) {
        print_usage(prog);
        return 1;
    }

    int mode_compress   = 0;
    int mode_decompress = 0;

    HuffxOptions opt;
    opt.algo    = HUFFX_CRYPT_NONE;
    opt.xor_key = 0;

    const char *input_path  = NULL;
    const char *output_path = NULL;

    /* 解析 argv，簡單手寫，不用 getopt */
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        /* 選項：壓縮 / 解壓 / help */
        if (strcmp(arg, "-c") == 0 || strcmp(arg, "--compress") == 0) {
            mode_compress = 1;
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--decompress") == 0) {
            mode_decompress = 1;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(prog);
            return 0;
        }
            /* 選項：-k <key> */
        else if (strcmp(arg, "-k") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -k requires a numeric key (0-255)\n");
                return 1;
            }
            uint8_t key = 0;
            if (!parse_u8(argv[i + 1], &key)) {
                fprintf(stderr, "Error: invalid XOR key '%s', expected 0-255\n",
                        argv[i + 1]);
                return 1;
            }
            opt.algo    = HUFFX_CRYPT_XOR;
            opt.xor_key = key;
            ++i; /* consume key argument */
        }
            /* 選項：--xor-key=<key> */
        else if (strncmp(arg, "--xor-key=", 10) == 0) {
            const char *val = arg + 10;
            uint8_t key = 0;
            if (!parse_u8(val, &key)) {
                fprintf(stderr, "Error: invalid XOR key '%s', expected 0-255\n",
                        val);
                return 1;
            }
            opt.algo    = HUFFX_CRYPT_XOR;
            opt.xor_key = key;
        }
            /* 非 - 開頭的，視為 input/output 檔名 */
        else if (arg[0] != '-') {
            if (!input_path) {
                input_path = arg;
            } else if (!output_path) {
                output_path = arg;
            } else {
                fprintf(stderr, "Error: too many positional arguments: '%s'\n", arg);
                return 1;
            }
        }
            /* 不認得的參數 */
        else {
            fprintf(stderr, "Error: unknown option '%s'\n", arg);
            print_usage(prog);
            return 1;
        }
    }

    /* 檢查模式是否正確 */
    if (mode_compress && mode_decompress) {
        fprintf(stderr, "Error: cannot specify both -c and -d\n");
        return 1;
    }
    if (!mode_compress && !mode_decompress) {
        fprintf(stderr, "Error: must specify either -c (compress) or -d (decompress)\n");
        print_usage(prog);
        return 1;
    }

    if (!input_path || !output_path) {
        fprintf(stderr, "Error: missing input/output path\n");
        print_usage(prog);
        return 1;
    }

    int rc = 0;

    if (mode_compress) {
        /* 壓縮 */
        rc = huffx_compress_file(input_path, output_path, &opt);
        if (rc != 0) {
            fprintf(stderr, "HuffX: compression failed (code=%d)\n", rc);
            return 1;
        }
    } else { /* mode_decompress */
        /* 解壓 */
        rc = huffx_decompress_file(input_path, output_path);
        if (rc != 0) {
            fprintf(stderr, "HuffX: decompression failed (code=%d)\n", rc);
            return 1;
        }
    }

    return 0;
}
