/* huffx_internal.h */

#ifndef HUFFX_INTERNAL_H
#define HUFFX_INTERNAL_H

#include "huffx.h"
#include <stddef.h>
#include <stdint.h>

/* Magic Number & Version */
#define HUFFX_MAGIC0 'H'
#define HUFFX_MAGIC1 'X'
#define HUFFX_MAGIC2 'F'
#define HUFFX_MAGIC3 '1'

#define HUFFX_VERSION              1u
#define HUFFX_COMPRESSION_HUFFMAN  1u
#define HUFFX_ALPHABET_SIZE        256
#define HUFFX_MAX_CODE_LEN         256

/* Huffman 節點 */
typedef struct HuffNode {
    int symbol;
    unsigned long freq;
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;

/* Huffman 編碼表 */
typedef struct HuffCode {
    uint8_t bits[HUFFX_MAX_CODE_LEN];
    uint8_t length;
} HuffCode;

/* Bit I/O */
typedef struct BitWriter {
    uint8_t *buf;
    size_t   capacity;
    size_t   size;
    int      bit_pos;
} BitWriter;

typedef struct BitReader {
    const uint8_t *buf;
    size_t         size;
    size_t         bit_count;
    size_t         bit_index;
} BitReader;

/* 檔案 Header (Packed) */
#pragma pack(push,1)
typedef struct {
    uint8_t  magic[4];
    uint8_t  version;
    uint8_t  compression;
    uint8_t  crypt_algo;      /* HuffxCryptAlgo */
    uint8_t  reserved;
    uint32_t original_size;
    uint32_t payload_size;
    uint8_t  padding_bits;
    uint8_t  crypt_param[7];  /* Header 內儲存的參數 (XOR用，ARX可能為空) */
} HuffxHeader;
#pragma pack(pop)

/* 工具函式 */
void *huffx_xmalloc(size_t n);

/* 加密核心 (整合在 core 中) */
int huffx_crypto_apply(HuffxCryptAlgo algo,
                       const uint8_t crypt_param[7],
                       uint8_t *buf,
                       size_t size);

#endif /* HUFFX_INTERNAL_H */
