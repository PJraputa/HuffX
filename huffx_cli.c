/* huffx_cli.c - Supports single file or folder batch mode with input/output paths */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "huffx.h"
#include "huffx_file.h"

#define PATH_MAX 4096


void print_help(const char *p) {
    fprintf(stderr, "Usage: %s -c|-d [options] <input> <output>\n", p);
    fprintf(stderr, "Processes file or folder:\n");
    fprintf(stderr, "  - If <input> is file, <output> is file.\n");
    fprintf(stderr, "  - If <input> is folder, <output> is folder (mirrors structure).\n");
    fprintf(stderr, "Compressed files get .hxf extension.\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -m <algo>   'xor' or 'arx'\n");
    fprintf(stderr, "  -k <key>    Key (up to 7 chars). Defaults to arx if given without -m.\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s -c -k secret123 input output\n", p);
    fprintf(stderr, "  %s -c singlefile.txt compressed.hxf\n", p);
}



int main(int argc, char **argv) {
    if (argc < 4) {  // Need at least -c/-d, input, output
        print_help(argv[0]);
        return 1;
    }

    int mode_c = 0, mode_d = 0;
    const char *in_path = NULL, *out_path = NULL;
    HuffxOptions opt = {0};
    opt.algo = HUFFX_CRYPT_NONE;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0) mode_c = 1;
        else if (strcmp(argv[i], "-d") == 0) mode_d = 1;
        else if (strcmp(argv[i], "-m") == 0 && i+1 < argc) {
            const char *m = argv[++i];
            if (strcmp(m, "xor") == 0) opt.algo = HUFFX_CRYPT_XOR;
            else if (strcmp(m, "arx") == 0) opt.algo = HUFFX_CRYPT_ARX_56;
            else { fprintf(stderr, "Bad method\n"); return 1; }
        }
        else if (strcmp(argv[i], "-k") == 0 && i+1 < argc) {
            const char *k = argv[++i];
            size_t len = strlen(k);
            if (len > 7) len = 7;
            memcpy(opt.crypt_param, k, len);
            if (opt.algo == HUFFX_CRYPT_NONE) opt.algo = HUFFX_CRYPT_ARX_56;
        }
        else if (argv[i][0] != '-') {
            if (!in_path) in_path = argv[i];
            else if (!out_path) out_path = argv[i];
            else {
                fprintf(stderr, "Too many arguments\n");
                print_help(argv[0]);
                return 1;
            }
        }
    }

    if ((mode_c && mode_d) || (!mode_c && !mode_d) || !in_path || !out_path) {
        print_help(argv[0]);
        return 1;
    }

    /* 初始化統計數據 */
    HuffxStats stats = {0};
    stats.total_in_bytes = 0;
    stats.total_out_bytes = 0;
    stats.file_count = 0;

    printf("--- Operation Start ---\n");
    clock_t start_time = clock(); /* 按下碼表 */

    int is_dir = is_directory(in_path);
    if (is_dir != is_directory(out_path)) {
        fprintf(stderr, "Input and output must both be files or both folders\n");
        return 1;
    }

    if (is_dir) {
        process_folder(in_path, out_path, &opt, mode_d, &stats);
    } else {
        process_single_file(in_path, out_path, &opt, mode_d, &stats);
    }

    clock_t end_time = clock(); /* 停止碼表 */
    printf("--- Operation Finished ---\n");

    /* 計算與顯示報告 */
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double ratio = 0.0;
    if (stats.total_in_bytes > 0) {
        ratio = (double)stats.total_out_bytes / (double)stats.total_in_bytes * 100.0;
    }

    printf("\n[Summary]\n");
    printf("Time Taken    : %.4f seconds\n", time_spent);
    printf("Files Processed: %d\n", stats.file_count);
    printf("Total Input   : %lld bytes\n", stats.total_in_bytes);
    printf("Total Output  : %lld bytes\n", stats.total_out_bytes);
    
    if (mode_c) {
        printf("Compression Ratio: %.2f%% (Size: %.2f%% of original)\n", 
               100.0 - ratio, ratio);
    } else {
        // 解壓縮通常看膨脹率，或者是還原了多少
        printf("Decompression: Expanded to %.2f%% of input size\n", ratio);
    }

    printf("All done.\n");
    return 0;
}
