/* huffx_core.c - HuffX Huffman + bit I/O + buffer-level codec */

#include "huffx_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================== 小工具：xmalloc ===================== */

void *huffx_xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "HuffX: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

/* ===================== Huffman：建樹 ===================== */

HuffNode *huffx_new_node(int symbol, unsigned long freq,
                         HuffNode *left, HuffNode *right)
{
    HuffNode *node = (HuffNode *)huffx_xmalloc(sizeof(HuffNode));
    node->symbol = symbol;
    node->freq   = freq;
    node->left   = left;
    node->right  = right;
    return node;
}

void huffx_free_tree(HuffNode *root)
{
    if (!root) return;
    huffx_free_tree(root->left);
    huffx_free_tree(root->right);
    free(root);
}

/* 從 freq[] 建 Huffman 樹，使用 O(n^2) 的簡單演算法（n<=256） */
HuffNode *huffx_build_tree(const unsigned long freq[HUFFX_ALPHABET_SIZE])
{
    HuffNode *nodes[HUFFX_ALPHABET_SIZE];
    size_t count = 0;

    /* 先產生所有 leaf 節點 */
    for (int i = 0; i < HUFFX_ALPHABET_SIZE; ++i) {
        if (freq[i] > 0) {
            nodes[count++] = huffx_new_node(i, freq[i], NULL, NULL);
        }
    }

    /* 特例：如果只有一種符號，就再補一個 internal 節點 */
    if (count == 1) {
        nodes[1] = huffx_new_node(-1, nodes[0]->freq, nodes[0], NULL);
        count = 2;
    }

    if (count == 0) {
        return NULL; /* 空輸入 */
    }

    /* 重複合併兩個最小 freq 節點 */
    while (count > 1) {
        size_t min1 = 0, min2 = 1;
        if (nodes[min2]->freq < nodes[min1]->freq) {
            size_t tmp = min1; min1 = min2; min2 = tmp;
        }
        for (size_t i = 2; i < count; ++i) {
            if (nodes[i]->freq < nodes[min1]->freq) {
                min2 = min1;
                min1 = i;
            } else if (nodes[i]->freq < nodes[min2]->freq) {
                min2 = i;
            }
        }

        HuffNode *a = nodes[min1];
        HuffNode *b = nodes[min2];
        HuffNode *parent = huffx_new_node(-1, a->freq + b->freq, a, b);

        /* 用 parent 取代較小 index 的，移除較大 index 的 */
        if (min1 > min2) {
            size_t tmp = min1; min1 = min2; min2 = tmp;
        }
        nodes[min1] = parent;
        for (size_t j = min2 + 1; j < count; ++j) {
            nodes[j - 1] = nodes[j];
        }
        --count;
    }

    return nodes[0];
}

/* ===================== Huffman：產生 code table ===================== */

static void build_code_table_rec(HuffNode *node,
                                 HuffCode codes[HUFFX_ALPHABET_SIZE],
                                 uint8_t *path,
                                 uint8_t depth)
{
    if (!node) return;

    if (!node->left && !node->right) {
        /* leaf */
        if (depth == 0) {
            /* 單一符號特例：至少給一個 bit */
            path[0] = 0;
            depth = 1;
        }
        assert(node->symbol >= 0 && node->symbol < HUFFX_ALPHABET_SIZE);
        HuffCode *c = &codes[node->symbol];
        c->length = depth;
        for (uint8_t i = 0; i < depth; ++i) {
            c->bits[i] = path[i];
        }
        return;
    }

    /* left = 0, right = 1 */
    path[depth] = 0;
    build_code_table_rec(node->left, codes, path, depth + 1);
    path[depth] = 1;
    build_code_table_rec(node->right, codes, path, depth + 1);
}

void huffx_build_code_table(HuffNode *root,
                            HuffCode codes[HUFFX_ALPHABET_SIZE])
{
    for (int i = 0; i < HUFFX_ALPHABET_SIZE; ++i) {
        codes[i].length = 0;
    }
    uint8_t path[HUFFX_MAX_CODE_LEN];
    build_code_table_rec(root, codes, path, 0);
}

/* ===================== BitWriter / BitReader ===================== */

void huffx_bw_init(BitWriter *bw, uint8_t *buf, size_t capacity)
{
    bw->buf      = buf;
    bw->capacity = capacity;
    bw->size     = 0;
    bw->bit_pos  = 0;
}

/* 寫入單一 bit（0 或 1） */
void huffx_bw_write_bit(BitWriter *bw, int bit)
{
    if (bw->size >= bw->capacity) {
        fprintf(stderr, "HuffX BitWriter overflow\n");
        exit(EXIT_FAILURE);
    }

    if (bw->bit_pos == 0) {
        bw->buf[bw->size] = 0;
    }

    if (bit) {
        bw->buf[bw->size] |= (uint8_t)(1u << (7 - bw->bit_pos));
    }

    bw->bit_pos++;
    if (bw->bit_pos == 8) {
        bw->bit_pos = 0;
        bw->size++;
    }
}

/* 連續寫入 nbits 個 bit（bits 陣列每個元素為 0 或 1） */
void huffx_bw_write_bits(BitWriter *bw, const uint8_t *bits, size_t nbits)
{
    for (size_t i = 0; i < nbits; ++i) {
        huffx_bw_write_bit(bw, bits[i]);
    }
}

/* flush：如最後一個 byte 未滿 8 bits，補 0 並回傳 padding_bits 數 */
uint8_t huffx_bw_flush(BitWriter *bw)
{
    uint8_t padding = 0;
    if (bw->bit_pos != 0) {
        padding = (uint8_t)(8 - bw->bit_pos);
        /* 未使用的 bit 預設已是 0，直接視為 padding */
        bw->size++;
        bw->bit_pos = 0;
    }
    return padding;
}

/* BitReader 初始化 */
void huffx_br_init(BitReader *br,
                   const uint8_t *buf,
                   size_t buf_size,
                   size_t bit_count)
{
    br->buf       = buf;
    br->size      = buf_size;
    br->bit_count = bit_count;
    br->bit_index = 0;
}

/* 讀一個 bit，回傳 0/1；若超出 bit_count 回傳 -1 */
int huffx_br_read_bit(BitReader *br)
{
    if (br->bit_index >= br->bit_count) {
        return -1;
    }
    size_t byte_index = br->bit_index / 8;
    size_t bit_pos    = br->bit_index % 8;
    int bit = (br->buf[byte_index] >> (7 - bit_pos)) & 1;
    br->bit_index++;
    return bit;
}

/* 讀一個完整 byte（8 bits），若不足 8 bits 或讀取失敗則回傳 -1 */
int huffx_br_read_byte(BitReader *br)
{
    int value = 0;
    for (int i = 0; i < 8; ++i) {
        int bit = huffx_br_read_bit(br);
        if (bit < 0) return -1;
        value = (value << 1) | bit;
    }
    return value;
}

/* ===================== Huffman 樹的序列化 ===================== */

/*
 * 使用 pre-order 序列化：
 * - internal node：寫 0
 * - leaf node：寫 1，再寫 8 個 bits 表示 symbol
 */
void huffx_serialize_tree(const HuffNode *root, BitWriter *bw)
{
    if (!root) return;

    if (!root->left && !root->right) {
        /* leaf */
        huffx_bw_write_bit(bw, 1);
        /* 寫入 symbol 的 8 bits（高位先） */
        uint8_t sym = (uint8_t)root->symbol;
        for (int i = 7; i >= 0; --i) {
            int bit = (sym >> i) & 1;
            huffx_bw_write_bit(bw, bit);
        }
    } else {
        /* internal */
        huffx_bw_write_bit(bw, 0);
        huffx_serialize_tree(root->left, bw);
        huffx_serialize_tree(root->right, bw);
    }
}

/* 反序列化：必須與 serialize 格式對應 */
HuffNode *huffx_deserialize_tree(BitReader *br)
{
    int flag = huffx_br_read_bit(br);
    if (flag < 0) {
        return NULL;
    }

    if (flag == 1) {
        /* leaf：讀取 8 bits 當 symbol */
        int sym = huffx_br_read_byte(br);
        if (sym < 0) return NULL;
        return huffx_new_node(sym, 0, NULL, NULL);
    }

    /* internal：遞迴讀取 left/right */
    HuffNode *left  = huffx_deserialize_tree(br);
    HuffNode *right = huffx_deserialize_tree(br);
    return huffx_new_node(-1, 0, left, right);
}

/* ===================== buffer-level compress ===================== */

int huffx_compress_buffer(const uint8_t *input_data,
                          size_t input_size,
                          uint8_t **out_data,
                          size_t *out_size,
                          const HuffxOptions *opt)
{
    if (!out_data || !out_size) {
        return -1;
    }
    *out_data = NULL;
    *out_size = 0;

    if (!input_data || input_size == 0) {
        return -1;
    }

    HuffxOptions local_opt;
    if (opt) {
        local_opt = *opt;
    } else {
        local_opt.algo    = HUFFX_CRYPT_NONE;
        local_opt.xor_key = 0;
    }

    /* 1. 統計頻率 */
    unsigned long freq[HUFFX_ALPHABET_SIZE] = {0};
    for (size_t i = 0; i < input_size; ++i) {
        freq[input_data[i]]++;
    }

    /* 2. 建樹 */
    HuffNode *root = huffx_build_tree(freq);
    if (!root) {
        return -1; /* 理論上 input_size>0 不會走到這裡 */
    }

    /* 3. 建 code table */
    HuffCode codes[HUFFX_ALPHABET_SIZE];
    huffx_build_code_table(root, codes);

    /* 4. 預估 bitstream 最大長度（悲觀估算：每 byte 最多 HUFFX_MAX_CODE_LEN bits） */
    size_t max_bits = input_size * HUFFX_MAX_CODE_LEN;
    size_t max_bytes = (max_bits + 7) / 8;
    uint8_t *payload = (uint8_t *)huffx_xmalloc(max_bytes);

    BitWriter bw;
    huffx_bw_init(&bw, payload, max_bytes);

    /* 4-1. 先序列化 Huffman 樹 */
    huffx_serialize_tree(root, &bw);

    /* 4-2. 再寫入實際資料的 Huffman code */
    for (size_t i = 0; i < input_size; ++i) {
        uint8_t ch = input_data[i];
        HuffCode *c = &codes[ch];
        if (c->length == 0) {
            /* 不應該發生：代表該 symbol 未在樹中 */
            huffx_free_tree(root);
            free(payload);
            return -1;
        }
        huffx_bw_write_bits(&bw, c->bits, c->length);
    }

    /* 4-3. flush 並取得 padding bits 數 */
    uint8_t padding_bits = huffx_bw_flush(&bw);
    size_t payload_size = bw.size;

    /* 5. 根據選項決定是否加密 payload */
    HuffxCryptAlgo algo = local_opt.algo;
    uint8_t crypt_param[7] = {0};
    if (algo == HUFFX_CRYPT_XOR) {
        crypt_param[0] = local_opt.xor_key;
    } else {
        algo = HUFFX_CRYPT_NONE;
    }

    if (algo != HUFFX_CRYPT_NONE) {
        if (huffx_crypto_apply(algo, crypt_param, payload, payload_size) != 0) {
            huffx_free_tree(root);
            free(payload);
            return -1;
        }
    }

    /* 6. 組 header + payload 成為最終輸出 buffer */
    HuffxHeader hdr;
    hdr.magic[0]    = HUFFX_MAGIC0;
    hdr.magic[1]    = HUFFX_MAGIC1;
    hdr.magic[2]    = HUFFX_MAGIC2;
    hdr.magic[3]    = HUFFX_MAGIC3;
    hdr.version     = (uint8_t)HUFFX_VERSION;
    hdr.compression = (uint8_t)HUFFX_COMPRESSION_HUFFMAN;
    hdr.crypt_algo  = (uint8_t)algo;
    hdr.reserved    = 0;
    hdr.original_size = (uint32_t)input_size;
    hdr.payload_size  = (uint32_t)payload_size;
    hdr.padding_bits  = padding_bits;
    memset(hdr.crypt_param, 0, sizeof(hdr.crypt_param));
    if (algo == HUFFX_CRYPT_XOR) {
        hdr.crypt_param[0] = crypt_param[0];
    }

    size_t total_size = sizeof(HuffxHeader) + payload_size;
    uint8_t *out_buf = (uint8_t *)huffx_xmalloc(total_size);

    memcpy(out_buf, &hdr, sizeof(HuffxHeader));
    memcpy(out_buf + sizeof(HuffxHeader), payload, payload_size);

    huffx_free_tree(root);
    free(payload);

    *out_data = out_buf;
    *out_size = total_size;
    return 0;
}

/* ===================== buffer-level decompress ===================== */

int huffx_decompress_buffer(const uint8_t *input_data,
                            size_t input_size,
                            uint8_t **out_data,
                            size_t *out_size)
{
    if (!out_data || !out_size) {
        return -1;
    }
    *out_data = NULL;
    *out_size = 0;

    if (!input_data || input_size < sizeof(HuffxHeader)) {
        return -1;
    }

    /* 1. 讀 header */
    HuffxHeader hdr;
    memcpy(&hdr, input_data, sizeof(HuffxHeader));

    if (hdr.magic[0] != HUFFX_MAGIC0 ||
        hdr.magic[1] != HUFFX_MAGIC1 ||
        hdr.magic[2] != HUFFX_MAGIC2 ||
        hdr.magic[3] != HUFFX_MAGIC3) {
        fprintf(stderr, "HuffX: bad magic\n");
        return -1;
    }

    if (hdr.version != HUFFX_VERSION) {
        fprintf(stderr, "HuffX: unsupported version %u\n", (unsigned)hdr.version);
        return -1;
    }

    if (hdr.compression != HUFFX_COMPRESSION_HUFFMAN) {
        fprintf(stderr, "HuffX: unsupported compression method %u\n",
                (unsigned)hdr.compression);
        return -1;
    }

    size_t payload_size = hdr.payload_size;
    if (sizeof(HuffxHeader) + payload_size > input_size) {
        fprintf(stderr, "HuffX: corrupted payload size\n");
        return -1;
    }

    const uint8_t *payload = input_data + sizeof(HuffxHeader);

    /* 2. 如果有加密，先在暫存 buffer 解密（不修改原 input_data） */
    uint8_t *payload_copy = (uint8_t *)huffx_xmalloc(payload_size);
    memcpy(payload_copy, payload, payload_size);

    HuffxCryptAlgo algo = (HuffxCryptAlgo)hdr.crypt_algo;
    if (algo != HUFFX_CRYPT_NONE) {
        if (huffx_crypto_apply(algo, hdr.crypt_param, payload_copy, payload_size) != 0) {
            free(payload_copy);
            return -1;
        }
    }

    /* 3. 用 BitReader 解出 Huffman 樹 + 資料 */
    size_t bit_count = (size_t)payload_size * 8;
    if (hdr.padding_bits > 7) {
        free(payload_copy);
        return -1;
    }
    if (hdr.padding_bits > 0) {
        if (bit_count < hdr.padding_bits) {
            free(payload_copy);
            return -1;
        }
        bit_count -= hdr.padding_bits;
    }

    BitReader br;
    huffx_br_init(&br, payload_copy, payload_size, bit_count);

    /* 3-1. 先還原 Huffman 樹 */
    HuffNode *root = huffx_deserialize_tree(&br);
    if (!root) {
        free(payload_copy);
        return -1;
    }

    /* 3-2. 解碼資料：用 original_size 控制輸出長度 */
    uint32_t original_size = hdr.original_size;
    uint8_t *out_buf = (uint8_t *)huffx_xmalloc(original_size);

    for (uint32_t i = 0; i < original_size; ++i) {
        HuffNode *cur = root;
        while (cur->left || cur->right) {
            int bit = huffx_br_read_bit(&br);
            if (bit < 0) {
                fprintf(stderr, "HuffX: unexpected end of bitstream\n");
                huffx_free_tree(root);
                free(payload_copy);
                free(out_buf);
                return -1;
            }
            cur = (bit == 0) ? cur->left : cur->right;
            if (!cur) {
                fprintf(stderr, "HuffX: invalid bitstream (NULL node)\n");
                huffx_free_tree(root);
                free(payload_copy);
                free(out_buf);
                return -1;
            }
        }
        out_buf[i] = (uint8_t)cur->symbol;
    }

    huffx_free_tree(root);
    free(payload_copy);

    *out_data = out_buf;
    *out_size = original_size;
    return 0;
}
