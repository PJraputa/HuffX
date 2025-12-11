/* huffx_crypto.c - HuffX 加密/解密模組（目前只有 XOR） */

#include "huffx_internal.h"

#include <string.h>

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
    }

    /* 未支援的演算法 */
    return -1;
}
