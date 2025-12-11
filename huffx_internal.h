/* huffx_internal.h */

#ifndef HUFFX_INTERNAL_H
#define HUFFX_INTERNAL_H

#include "huffx.h"

#include <stddef.h>
#include <stdint.h>

/* 內部共用常數與結構，僅供 library 各 .c 檔 include 使用 */

#define HUFFX_MAGIC_STR "HXF1"
#define HUFFX_MAGIC0 'H'
#define HUFFX_MAGIC1 'X'
#define HUFFX_MAGIC2 'F'
#define HUFFX_MAGIC3 '1'

#define HUFFX_VERSION              1u
#define HUFFX_COMPRESSION_HUFFMAN  1u

#define HUFFX_ALPHABET_SIZE 256
#define HUFFX_MAX_CODE_LEN  256 /* 理論上用不到這麼長，但安全 */

/* Huffman 節點 */
typedef struct HuffNode {
    int symbol;                 /* 0..255 for leaf, -1 for internal */
    unsigned long freq;
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;

/* Huffman 編碼（每個 symbol 一條 bit 路徑） */
typedef struct HuffCode {
    uint8_t bits[HUFFX_MAX_CODE_LEN]; /* 每個元素為 0 或 1 */
    uint8_t length;                   /* 實際使用的 bits 數 */
} HuffCode;

/* Bit writer：把單一 bit/bit 序列寫成 byte buffer */
typedef struct BitWriter {
    uint8_t *buf;
    size_t   capacity;
    size_t   size;       /* 目前已使用的 bytes 數 */
    int      bit_pos;    /* 下一個要寫入的 bit 位於當前 byte 的哪個 bit (0..7) */
} BitWriter;

/* Bit reader：從 byte buffer 中一個一個讀 bit */
typedef struct BitReader {
    const uint8_t *buf;
    size_t         size;       /* buffer bytes 數 */
    size_t         bit_count;  /* buffer 中實際有效的 bit 數 */
    size_t         bit_index;  /* 下一個要讀取的 bit index [0..bit_count) */
} BitReader;

/* 檔案 header：metadata（借用常見格式概念） */
#pragma pack(push,1)
typedef struct {
    uint8_t  magic[4];       /* "HXF1" */
    uint8_t  version;        /* 格式版本，目前為 1 */
    uint8_t  compression;    /* 1 = Huffman */
    uint8_t  crypt_algo;     /* HuffxCryptAlgo */
    uint8_t  reserved;       /* 保留 */

    uint32_t original_size;  /* 原始資料長度（bytes） */
    uint32_t payload_size;   /* header 後面 payload 的 bytes 數 */
    uint8_t  padding_bits;   /* 最後一個 byte 無效 bits 數（0..7） */
    uint8_t  crypt_param[7]; /* 加密演算法參數（目前 XOR 只用 [0]） */
} HuffxHeader;
#pragma pack(pop)

/* 內部共用的小工具：malloc 包裝 */
void *huffx_xmalloc(size_t n);

/* Huffman 相關函式 */
HuffNode *huffx_new_node(int symbol, unsigned long freq,
                         HuffNode *left, HuffNode *right);
void      huffx_free_tree(HuffNode *root);

/* Huffman 樹建構與 code table */
HuffNode *huffx_build_tree(const unsigned long freq[HUFFX_ALPHABET_SIZE]);
void      huffx_build_code_table(HuffNode *root,
                                 HuffCode codes[HUFFX_ALPHABET_SIZE]);

/* Huffman 樹序列化與反序列化 */
void      huffx_serialize_tree(const HuffNode *root, BitWriter *bw);
HuffNode *huffx_deserialize_tree(BitReader *br);

/* Bit writer/reader 介面 */
void    huffx_bw_init(BitWriter *bw, uint8_t *buf, size_t capacity);
void    huffx_bw_write_bit(BitWriter *bw, int bit);
void    huffx_bw_write_bits(BitWriter *bw, const uint8_t *bits, size_t nbits);
uint8_t huffx_bw_flush(BitWriter *bw); /* 回傳 padding bits 數 */

void  huffx_br_init(BitReader *br,
                    const uint8_t *buf,
                    size_t buf_size,
                    size_t bit_count);
int   huffx_br_read_bit(BitReader *br);
int   huffx_br_read_byte(BitReader *br);

/* crypto 介面（由 huffx_crypto.c 實作） */
int huffx_crypto_apply(HuffxCryptAlgo algo,
                       const uint8_t crypt_param[7],
                       uint8_t *buf,
                       size_t size);

#endif /* HUFFX_INTERNAL_H */
