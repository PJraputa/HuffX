/* huffx_file.c */

#include "huffx_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "huffx.h"
#include "huffx_file.h"
#define PATH_MAX 4096

static long long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return (long long)st.st_size;
    return 0;
}

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

int is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

void ensure_dir_exists(const char *dir_path) {
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Failed to create directory: %s\n", dir_path);
            exit(1);
        }
    }
}


void queue_init(PathQueue *q) {
    q->front = q->rear = NULL;
}

void queue_push(PathQueue *q, const char *path) {
    PathNode *new_node = (PathNode *)malloc(sizeof(PathNode));
    if (!new_node) {
        fprintf(stderr, "Out of memory for queue node\n");
        exit(1);
    }
    strncpy(new_node->path, path, PATH_MAX - 1);
    new_node->path[PATH_MAX - 1] = '\0';
    new_node->next = NULL;
    if (q->rear) {
        q->rear->next = new_node;
    } else {
        q->front = new_node;
    }
    q->rear = new_node;
}

char *queue_pop(PathQueue *q) {
    if (!q->front) return NULL;
    PathNode *temp = q->front;
    char *path = (char *)malloc(PATH_MAX);
    if (!path) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    strncpy(path, temp->path, PATH_MAX);
    q->front = q->front->next;
    if (!q->front) q->rear = NULL;
    free(temp);
    return path;
}

void queue_free(PathQueue *q) {
    while (q->front) {
        char *path = queue_pop(q);
        free(path);
    }
}
/* Process a single file */
int process_single_file(const char *in_path, const char *out_path, const HuffxOptions *opt, int decompress_mode, HuffxStats *stats) {
    char final_out[PATH_MAX];
    if (decompress_mode) {
        size_t len = strlen(out_path);
        if (len > 4 && strcmp(out_path + len - 4, ".hxf") == 0) {
            snprintf(final_out, PATH_MAX, "%.*s", (int)(len - 4), out_path);
        } else {
            strncpy(final_out, out_path, PATH_MAX);
        }
    } else {
        snprintf(final_out, PATH_MAX, "%s.hxf", out_path);
    }

    printf("%s: %s → %s\n",
           decompress_mode ? "Decompressing" : "Compressing",
           in_path, final_out);

    int rc = decompress_mode ?
        huffx_decompress_file(in_path, final_out, opt) :
        huffx_compress_file(in_path, final_out, opt);

    if (rc != 0) {
        fprintf(stderr, "Failed on: %s\n", in_path);
        return -1;
    }

    if (stats) {
        stats->file_count++;
        stats->total_in_bytes += get_file_size(in_path);
        stats->total_out_bytes += get_file_size(final_out);
    }

    return 0;
}



/* Process folder using linked list queue for BFS (non-recursive) */
void process_folder(const char *in_dir, const char *out_dir, const HuffxOptions *opt, int decompress_mode, HuffxStats *stats) {
    ensure_dir_exists(out_dir);

    PathQueue q;
    queue_init(&q);
    queue_push(&q, in_dir);

    while (1) {
        char *current_in = queue_pop(&q);
        if (!current_in) break;

        DIR *dir = opendir(current_in);
        if (!dir) {
            fprintf(stderr, "Cannot open: %s\n", current_in);
            free(current_in);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_in[PATH_MAX];
            snprintf(full_in, PATH_MAX, "%s/%s", current_in, entry->d_name);

            /* Compute relative path from original in_dir */
            const char *rel_path = full_in + strlen(in_dir) + 1;  // Skip in_dir/
            char full_out[PATH_MAX];
            snprintf(full_out, PATH_MAX, "%s/%s", out_dir, rel_path);

            if (is_directory(full_in)) {
                ensure_dir_exists(full_out);
                queue_push(&q, full_in);
            } else {
                /* For decompress, skip non-.hxf files */
                if (decompress_mode) {
                    size_t len = strlen(entry->d_name);
                    if (len < 4 || strcmp(entry->d_name + len - 4, ".hxf") != 0)
                        continue;
                }
                process_single_file(full_in, full_out, opt, decompress_mode, stats);
            }
        }
        closedir(dir);
        free(current_in);
    }
    queue_free(&q);
}
