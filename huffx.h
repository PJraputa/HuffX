/* huffx.h */

#ifndef HUFFX_H
#define HUFFX_H

#include <stddef.h> /* size_t */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 加密演算法種類 */
typedef enum {
    HUFFX_CRYPT_NONE   = 0,
    HUFFX_CRYPT_XOR    = 1,
    HUFFX_CRYPT_ARX_56 = 2
} HuffxCryptAlgo;

/* 選項結構：包含演算法與 7-byte 參數 */
typedef struct {
    HuffxCryptAlgo algo;
    uint8_t crypt_param[7];
} HuffxOptions;

/* === 記憶體版 API ===================== */

/* * 壓縮
 * opt: 指定加密演算法與密鑰
 */
int huffx_compress_buffer(const uint8_t *input_data,
                          size_t input_size,
                          uint8_t **out_data,
                          size_t *out_size,
                          const HuffxOptions *opt);

/* * 解壓
 * opt: 用於傳入解密密鑰 (特別是 ARX 這類不存密鑰的算法)
 * 如果 opt 為 NULL，則僅依賴檔案 Header 資訊 (適用於 XOR)
 */
int huffx_decompress_buffer(const uint8_t *input_data,
                            size_t input_size,
                            uint8_t **out_data,
                            size_t *out_size,
                            const HuffxOptions *opt);

/* === 檔案版 API =============================== */

int huffx_compress_file(const char *input_path,
                        const char *output_path,
                        const HuffxOptions *opt);

int huffx_decompress_file(const char *input_path,
                          const char *output_path,
                          const HuffxOptions *opt);

#ifdef __cplusplus
}
#endif

#endif /* HUFFX_H */
