/* huffx_core.c */

#include "huffx_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================== Crypto Implementation ===================== */

static inline uint32_t rotl32(uint32_t x, int n) {
return (x << n) | (x >> (32 - n));
}

static uint8_t arx_next_byte(uint32_t *v0, uint32_t *v1) {
    *v0 += *v1;
    *v1 = rotl32(*v1, 13);
    *v1 ^= *v0;
    *v0 = rotl32(*v0, 16);
    *v1 += *v0;
    *v0 = rotl32(*v0, 7);
    return (*v1 & 0xFF);
}

int huffx_crypto_apply(HuffxCryptAlgo algo,
                       const uint8_t crypt_param[7],
                       uint8_t *buf,
                       size_t size)
{
    if (algo == HUFFX_CRYPT_NONE) {
        return 0;
    } else if (algo == HUFFX_CRYPT_XOR) {
        uint8_t key = crypt_param[0];
        if (key == 0) return 0;
        for (size_t i = 0; i < size; ++i) {
            buf[i] ^= key;
        }
        return 0;
    } else if (algo == HUFFX_CRYPT_ARX_56) {
        uint32_t v0 = 0, v1 = 0;
        /* Key Schedule */
        v0 |= (uint32_t)crypt_param[0];
        v0 |= (uint32_t)crypt_param[1] << 8;
        v0 |= (uint32_t)crypt_param[2] << 16;
        v0 |= (uint32_t)crypt_param[3] << 24;
        v1 |= (uint32_t)crypt_param[4];
        v1 |= (uint32_t)crypt_param[5] << 8;
        v1 |= (uint32_t)crypt_param[6] << 16;
        v1 |= (uint32_t)0x5A << 24;

        /* Warm-up */
        for (int i = 0; i < 8; ++i) arx_next_byte(&v0, &v1);

        /* Stream Cipher */
        for (size_t i = 0; i < size; ++i) {
            buf[i] ^= arx_next_byte(&v0, &v1);
        }
        return 0;
    }
    return -1;
}

/* ===================== Memory Tools ===================== */

void *huffx_xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "HuffX: out of memory\n"); exit(EXIT_FAILURE); }
    return p;
}

/* ===================== Huffman Logic (Tree/Code) ===================== */

HuffNode *huffx_new_node(int symbol, unsigned long freq, HuffNode *left, HuffNode *right) {
    HuffNode *node = (HuffNode *)huffx_xmalloc(sizeof(HuffNode));
    node->symbol = symbol; node->freq = freq; node->left = left; node->right = right;
    return node;
}

void huffx_free_tree(HuffNode *root) {
    if (!root) return;
    huffx_free_tree(root->left);
    huffx_free_tree(root->right);
    free(root);
}

HuffNode *huffx_build_tree(const unsigned long freq[HUFFX_ALPHABET_SIZE]) {
    HuffNode *nodes[HUFFX_ALPHABET_SIZE];
    size_t count = 0;
    for (int i = 0; i < HUFFX_ALPHABET_SIZE; ++i) {
        if (freq[i] > 0) nodes[count++] = huffx_new_node(i, freq[i], NULL, NULL);
    }
    if (count == 1) { nodes[1] = huffx_new_node(-1, nodes[0]->freq, nodes[0], NULL); count = 2; }
    if (count == 0) return NULL;

    while (count > 1) {
        size_t min1 = 0, min2 = 1;
        if (nodes[min2]->freq < nodes[min1]->freq) { size_t t = min1; min1 = min2; min2 = t; }
        for (size_t i = 2; i < count; ++i) {
            if (nodes[i]->freq < nodes[min1]->freq) { min2 = min1; min1 = i; }
            else if (nodes[i]->freq < nodes[min2]->freq) { min2 = i; }
        }
        HuffNode *parent = huffx_new_node(-1, nodes[min1]->freq + nodes[min2]->freq, nodes[min1], nodes[min2]);
        if (min1 > min2) { size_t t = min1; min1 = min2; min2 = t; }
        nodes[min1] = parent;
        for (size_t j = min2 + 1; j < count; ++j) nodes[j - 1] = nodes[j];
        --count;
    }
    return nodes[0];
}

static void build_code_table_rec(HuffNode *node, HuffCode codes[], uint8_t *path, uint8_t depth) {
    if (!node) return;
    if (!node->left && !node->right) {
        if (depth == 0) { path[0] = 0; depth = 1; }
        codes[node->symbol].length = depth;
        memcpy(codes[node->symbol].bits, path, depth);
        return;
    }
    path[depth] = 0; build_code_table_rec(node->left, codes, path, depth + 1);
    path[depth] = 1; build_code_table_rec(node->right, codes, path, depth + 1);
}

/* ===================== Bit I/O ===================== */

void huffx_bw_init(BitWriter *bw, uint8_t *buf, size_t capacity) {
    bw->buf = buf; bw->capacity = capacity; bw->size = 0; bw->bit_pos = 0;
}
void huffx_bw_write_bit(BitWriter *bw, int bit) {
    if (bw->size >= bw->capacity) exit(EXIT_FAILURE);
    if (bw->bit_pos == 0) bw->buf[bw->size] = 0;
    if (bit) bw->buf[bw->size] |= (1u << (7 - bw->bit_pos));
    if (++bw->bit_pos == 8) { bw->bit_pos = 0; bw->size++; }
}
void huffx_bw_write_bits(BitWriter *bw, const uint8_t *bits, size_t nbits) {
    for (size_t i = 0; i < nbits; ++i) huffx_bw_write_bit(bw, bits[i]);
}
uint8_t huffx_bw_flush(BitWriter *bw) {
    uint8_t pad = 0;
    if (bw->bit_pos != 0) { pad = 8 - bw->bit_pos; bw->size++; bw->bit_pos = 0; }
    return pad;
}

void huffx_br_init(BitReader *br, const uint8_t *buf, size_t size, size_t bit_count) {
    br->buf = buf; br->size = size; br->bit_count = bit_count; br->bit_index = 0;
}
int huffx_br_read_bit(BitReader *br) {
    if (br->bit_index >= br->bit_count) return -1;
    int bit = (br->buf[br->bit_index / 8] >> (7 - (br->bit_index % 8))) & 1;
    br->bit_index++;
    return bit;
}
int huffx_br_read_byte(BitReader *br) {
    int val = 0;
    for (int i = 0; i < 8; ++i) {
        int b = huffx_br_read_bit(br);
        if (b < 0) return -1;
        val = (val << 1) | b;
    }
    return val;
}

/* ===================== Serialization ===================== */

void huffx_serialize_tree(const HuffNode *root, BitWriter *bw) {
    if (!root) return;
    if (!root->left && !root->right) {
        huffx_bw_write_bit(bw, 1);
        uint8_t s = (uint8_t)root->symbol;
        for (int i = 7; i >= 0; --i) huffx_bw_write_bit(bw, (s >> i) & 1);
    } else {
        huffx_bw_write_bit(bw, 0);
        huffx_serialize_tree(root->left, bw);
        huffx_serialize_tree(root->right, bw);
    }
}

HuffNode *huffx_deserialize_tree(BitReader *br) {
    int flag = huffx_br_read_bit(br);
    if (flag < 0) return NULL;
    if (flag == 1) {
        int sym = huffx_br_read_byte(br);
        return (sym < 0) ? NULL : huffx_new_node(sym, 0, NULL, NULL);
    }
    HuffNode *l = huffx_deserialize_tree(br);
    HuffNode *r = huffx_deserialize_tree(br);
    return huffx_new_node(-1, 0, l, r);
}

/* ===================== Compress Buffer ===================== */

int huffx_compress_buffer(const uint8_t *input_data, size_t input_size,
                          uint8_t **out_data, size_t *out_size,
                          const HuffxOptions *opt)
{
    if (!out_data || !out_size || !input_data) return -1;

    /* 1. 處理參數 */
    HuffxOptions local_opt;
    if (opt) local_opt = *opt;
    else { local_opt.algo = HUFFX_CRYPT_NONE; memset(local_opt.crypt_param, 0, 7); }

    /* 2. 統計頻率與建樹 */
    unsigned long freq[HUFFX_ALPHABET_SIZE] = {0};
    for (size_t i = 0; i < input_size; ++i) freq[input_data[i]]++;
    HuffNode *root = huffx_build_tree(freq);
    if (!root) return -1;

    HuffCode codes[HUFFX_ALPHABET_SIZE];
    uint8_t path[HUFFX_MAX_CODE_LEN];
    for (int i=0; i<256; ++i) codes[i].length = 0;
    build_code_table_rec(root, codes, path, 0);

    /* 3. 產生 Payload */
    size_t max_bytes = input_size * 2 + 4096; /* 安全緩衝 */
    uint8_t *payload = (uint8_t *)huffx_xmalloc(max_bytes);
    BitWriter bw;
    huffx_bw_init(&bw, payload, max_bytes);

    huffx_serialize_tree(root, &bw);
    for (size_t i = 0; i < input_size; ++i) {
        HuffCode *c = &codes[input_data[i]];
        huffx_bw_write_bits(&bw, c->bits, c->length);
    }
    uint8_t padding = huffx_bw_flush(&bw);
    size_t payload_size = bw.size;

    /* 4. 加密 (In-place) */
    if (local_opt.algo != HUFFX_CRYPT_NONE) {
        huffx_crypto_apply(local_opt.algo, local_opt.crypt_param, payload, payload_size);
    }

    /* 5. 組合 Header 與輸出 */
    HuffxHeader hdr = {0};
    hdr.magic[0] = HUFFX_MAGIC0; hdr.magic[1] = HUFFX_MAGIC1;
    hdr.magic[2] = HUFFX_MAGIC2; hdr.magic[3] = HUFFX_MAGIC3;
    hdr.version = HUFFX_VERSION;
    hdr.compression = HUFFX_COMPRESSION_HUFFMAN;
    hdr.crypt_algo = (uint8_t)local_opt.algo;
    hdr.original_size = (uint32_t)input_size;
    hdr.payload_size = (uint32_t)payload_size;
    hdr.padding_bits = padding;

    /* 策略：若是 XOR，我們把 key 存進 header (相容舊版)。若是 ARX，我們不在 Header 存 Key (保持為 0) */
    if (local_opt.algo == HUFFX_CRYPT_XOR) {
        memcpy(hdr.crypt_param, local_opt.crypt_param, 7);
    }
    /* ARX 則保留 hdr.crypt_param 為 0 */

    size_t total = sizeof(HuffxHeader) + payload_size;
    *out_data = (uint8_t *)huffx_xmalloc(total);
    memcpy(*out_data, &hdr, sizeof(HuffxHeader));
    memcpy(*out_data + sizeof(HuffxHeader), payload, payload_size);

    free(payload);
    huffx_free_tree(root);
    *out_size = total;
    return 0;
}

/* ===================== Decompress Buffer (Modified) ===================== */

int huffx_decompress_buffer(const uint8_t *input_data, size_t input_size,
                            uint8_t **out_data, size_t *out_size,
                            const HuffxOptions *opt)
{
    if (!out_data || !out_size || !input_data) return -1;
    if (input_size < sizeof(HuffxHeader)) return -1;

    HuffxHeader hdr;
    memcpy(&hdr, input_data, sizeof(HuffxHeader));

    if (hdr.magic[0] != HUFFX_MAGIC0 || hdr.magic[3] != HUFFX_MAGIC3) return -1;
    if (hdr.payload_size + sizeof(HuffxHeader) > input_size) return -1;

    uint8_t *payload_copy = (uint8_t *)huffx_xmalloc(hdr.payload_size);
    memcpy(payload_copy, input_data + sizeof(HuffxHeader), hdr.payload_size);

    /* --- 解密邏輯開始 --- */
    HuffxCryptAlgo algo = (HuffxCryptAlgo)hdr.crypt_algo;
    uint8_t final_param[7];
    memset(final_param, 0, 7);

    if (algo != HUFFX_CRYPT_NONE) {
        /* 預設使用 Header 裡的參數 (適用於 XOR) */
        memcpy(final_param, hdr.crypt_param, 7);

        /* 【關鍵修正】如果 User 透過 opt 傳入了 Key，則覆蓋 Header 的參數 */
        /* 這對於 ARX 這種 Header 裡不存 Key 的算法是必須的 */
        if (opt && opt->algo == algo) {
            /* 檢查 opt 裡是否有非零內容 (假設 key 不全為 0) */
            /* 或者是無條件信任使用者輸入 */
            memcpy(final_param, opt->crypt_param, 7);
        }

        /* 執行解密 (對稱加密，再次執行 XOR/ARX 即可還原) */
        huffx_crypto_apply(algo, final_param, payload_copy, hdr.payload_size);
    }
    /* --- 解密邏輯結束 --- */

    BitReader br;
    size_t valid_bits = hdr.payload_size * 8 - hdr.padding_bits;
    huffx_br_init(&br, payload_copy, hdr.payload_size, valid_bits);

    HuffNode *root = huffx_deserialize_tree(&br);
    if (!root) { free(payload_copy); return -1; }

    *out_data = (uint8_t *)huffx_xmalloc(hdr.original_size);
    for (uint32_t i = 0; i < hdr.original_size; ++i) {
        HuffNode *cur = root;
        while (cur->left || cur->right) {
            int b = huffx_br_read_bit(&br);
            if (b < 0) { free(*out_data); free(payload_copy); huffx_free_tree(root); return -1; }
            cur = (b == 0) ? cur->left : cur->right;
        }
        (*out_data)[i] = (uint8_t)cur->symbol;
    }

    *out_size = hdr.original_size;
    free(payload_copy);
    huffx_free_tree(root);
    return 0;
}
