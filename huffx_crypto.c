/* huffx_crypto.c - HuffX 加密/解密模組（目前只有 XOR） */

#include "huffx_internal.h"

#include <string.h>

static inline uint32_t rotl32(uint32_t x, int n) {
return (x << n) | (x >> (32 - n));
}

/* 一個微型的 ARX PRNG 狀態機 */
static uint8_t arx_next_byte(uint32_t *v0, uint32_t *v1) {
    /* 這裡使用類似 SipHash 或 ChaCha 的簡單 ARX 結構 */
    /* Add */
    *v0 += *v1;
    /* Rotate */
    *v1 = rotl32(*v1, 13);
    /* Xor */
    *v1 ^= *v0;

    /* 第二輪混合，增加非線性 */
    *v0 = rotl32(*v0, 16);
    *v1 += *v0;
    *v0 = rotl32(*v0, 7);

    /* 回傳 v1 的最低位 byte 作為 keystream */
    return (*v1 & 0xFF);
}

int huffx_crypto_apply(HuffxCryptAlgo algo,
                       const uint8_t crypt_param[7],
                       uint8_t *buf,
                       size_t size)
{
    if (algo == HUFFX_CRYPT_NONE) {
        return 0; /* 不做任何事 */
    } else if (algo == HUFFX_CRYPT_XOR) {
        uint8_t key = crypt_param[0];
        if (key == 0) {
            /* XOR key 為 0 等於沒加密，但仍然視為成功 */
        }
        for (size_t i = 0; i < size; ++i) {
            buf[i] ^= key;
        }
        return 0;
    } else if (algo == HUFFX_CRYPT_ARX_56) { // 假設你在 enum 加了這個
        /* 1. Key Schedule: 將 7 bytes 擴展為兩個 32-bit 狀態 */
        uint32_t v0 = 0;
        uint32_t v1 = 0;

        /* v0 吃前 4 個 bytes */
        v0 |= (uint32_t)crypt_param[0];
        v0 |= (uint32_t)crypt_param[1] << 8;
        v0 |= (uint32_t)crypt_param[2] << 16;
        v0 |= (uint32_t)crypt_param[3] << 24;

        /* v1 吃後 3 個 bytes，最後 1 byte 補一個 Magic Constant (例如 0x5A) */
        v1 |= (uint32_t)crypt_param[4];
        v1 |= (uint32_t)crypt_param[5] << 8;
        v1 |= (uint32_t)crypt_param[6] << 16;
        v1 |= (uint32_t)0x5A << 24; // Padding to fully utilize 64-bit state

        /* 2. 預熱 (Warm-up): 先跑幾輪讓狀態混合均勻，避免第一字節洩漏 */
        for (int i = 0; i < 8; ++i) {
            arx_next_byte(&v0, &v1);
        }

        /* 3. 加密迴圈 */
        for (size_t i = 0; i < size; ++i) {
            /* 生成密鑰流並與內容 XOR */
            uint8_t key_byte = arx_next_byte(&v0, &v1);
            buf[i] ^= key_byte;
        }
        return 0;
    }
    /* 未支援的演算法 */
    return -1;
}
