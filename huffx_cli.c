/* huffx_cli.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffx.h"

void print_help(const char *p) {
    fprintf(stderr, "Usage: %s -c/-d [options] <in> <out>\n", p);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -m <algo>   Method: 'xor' or 'arx'\n");
    fprintf(stderr, "  -k <key>    Key string (max 7 chars)\n");
}

int main(int argc, char **argv) {
    if (argc < 4) { print_help(argv[0]); return 1; }

    int mode_c = 0, mode_d = 0;
    const char *in_path = NULL, *out_path = NULL;
    HuffxOptions opt;
    opt.algo = HUFFX_CRYPT_NONE;
    memset(opt.crypt_param, 0, 7);

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-c")) mode_c = 1;
        else if (!strcmp(argv[i], "-d")) mode_d = 1;
        else if (!strcmp(argv[i], "-m") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "xor")) opt.algo = HUFFX_CRYPT_XOR;
            else if (!strcmp(m, "arx")) opt.algo = HUFFX_CRYPT_ARX_56;
        }
        else if (!strcmp(argv[i], "-k") && i+1 < argc) {
            const char *k = argv[++i];
            size_t len = strlen(k);
            if (len > 7) len = 7;
            memset(opt.crypt_param, 0, 7);
            memcpy(opt.crypt_param, k, len);
            /* 如果沒指定演算法但給了 key，預設 ARX (比較強) 或 XOR */
            if (opt.algo == HUFFX_CRYPT_NONE) opt.algo = HUFFX_CRYPT_ARX_56;
        }
        else if (argv[i][0] != '-') {
            if (!in_path) in_path = argv[i];
            else if (!out_path) out_path = argv[i];
        }
    }

    if ((mode_c ^ mode_d) == 0 || !in_path || !out_path) {
        print_help(argv[0]); return 1;
    }

    int rc = 0;
    if (mode_c) {
        rc = huffx_compress_file(in_path, out_path, &opt);
    } else {
        /* 解壓縮現在也把 opt 傳進去，裡面含有 key */
        rc = huffx_decompress_file(in_path, out_path, &opt);
    }

    if (rc != 0) {
        fprintf(stderr, "Operation failed.\n");
        return 1;
    }
    return 0;
}
