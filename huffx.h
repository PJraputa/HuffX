/* huffx.h */

#ifndef HUFFX_H
#define HUFFX_H

#include <stddef.h> /* size_t */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 加密演算法種類（目前只有 NONE / XOR，保留擴充空間） */
typedef enum {
    HUFFX_CRYPT_NONE = 0,
    HUFFX_CRYPT_XOR  = 1,
} HuffxCryptAlgo;

/* 壓縮選項 */
typedef struct {
    HuffxCryptAlgo algo; /* 要使用的加密演算法 */
    uint8_t xor_key;     /* 若 algo=HUFFX_CRYPT_XOR，使用的 XOR key */
} HuffxOptions;

/* === 記憶體版 API：輸入/輸出皆為 buffer ===================== */
/*
 * 將 input_data[0..input_size-1] 壓縮成 HuffX 格式。
 * out_data 會由函式以 malloc() 配置，呼叫端負責 free()。
 * 回傳 0 代表成功，其餘為錯誤。
 */
int huffx_compress_buffer(const uint8_t *input_data,
                          size_t input_size,
                          uint8_t **out_data,
                          size_t *out_size,
                          const HuffxOptions *opt);

/*
 * 將一個 HuffX 格式的壓縮 buffer 解壓。
 * out_data 會由函式以 malloc() 配置，呼叫端負責 free()。
 * 回傳 0 代表成功，其餘為錯誤。
 */
int huffx_decompress_buffer(const uint8_t *input_data,
                            size_t input_size,
                            uint8_t **out_data,
                            size_t *out_size);

/* === 檔案版 API：直接操作檔案 =============================== */

/* 以檔案路徑為介面進行壓縮 */
int huffx_compress_file(const char *input_path,
                        const char *output_path,
                        const HuffxOptions *opt);

/* 以檔案路徑為介面進行解壓 */
int huffx_decompress_file(const char *input_path,
                          const char *output_path);

/* 舊版相容介面：直接用 xor_enable/xor_key 呼叫檔案版壓縮 */
int huffx_compress(const char *input_path,
                   const char *output_path,
                   int xor_enable,
                   uint8_t xor_key);

int huffx_decompress(const char *input_path,
                     const char *output_path);

#ifdef __cplusplus
}
#endif

#endif /* HUFFX_H */
