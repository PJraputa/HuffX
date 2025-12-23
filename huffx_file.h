#ifndef HUFFX_FILE_H
#define HUFFX_FILE_H

#include <stddef.h>
#include "huffx.h"

#define PATH_MAX 4096


/* Linked list node for file/directory paths (used as a queue for BFS traversal) */
typedef struct PathNode {
    char path[PATH_MAX];
    struct PathNode *next;
} PathNode;

/* Queue structure for linked list-based traversal */
typedef struct {
    PathNode *front;
    PathNode *rear;
} PathQueue;

void queue_init(PathQueue *q);
void queue_push(PathQueue *q, const char *path);
char *queue_pop(PathQueue *q);
void queue_free(PathQueue *q);
int process_single_file(const char *in_path, const char *out_path, const HuffxOptions *opt, int decompress_mode) ;
void process_folder(const char *in_dir, const char *out_dir, const HuffxOptions *opt, int decompress_mode) ;
int is_directory(const char *path);
void ensure_dir_exists(const char *dir_path) ;
#endif // HUFFX_FILE_H