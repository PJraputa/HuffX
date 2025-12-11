# HuffX – A Small Huffman-Based Compression Format

HuffX 是一個以 Huffman 編碼為核心的壓縮格式，支援：

- 任意二進位資料（不限文字）
- 自訂容器格式（Header + Payload）
- 可選擇啟用的 XOR 加密（未來可擴充成其他演算法）
- 清楚的分層設計（核心演算法 / 加密 / 檔案 I/O / CLI）

本專案目標同時兼顧「教學 / 研究」與「工程實務的模組化」。

---

## 專案結構

```
huffx.h              公開 API（給外部程式 / CLI 使用）
huffx_internal.h     Library 內部共用定義（Huffman 節點、Bit I/O、Header 等）

huffx_core.c         Huffman + bit I/O + buffer-level compress/decompress
huffx_crypto.c       加解密模組（目前只實作 XOR）
huffx_file.c         檔案 I/O，包裝成 file-level API 和舊版接口

huffx_cli.c          命令列工具 (main)，呼叫 huffx_compress_file / _decompress_file
```

---

## 編譯方式

### 1. 使用 gcc 編譯 CLI

```
gcc -std=c11 -Wall -Wextra     huffx_core.c huffx_crypto.c huffx_file.c huffx_cli.c     -o huffx
```

### 2. 編譯成靜態 Library（選擇性）

```
gcc -std=c11 -Wall -Wextra -c huffx_core.c huffx_crypto.c huffx_file.c
ar rcs libhuffx.a huffx_core.o huffx_crypto.o huffx_file.o
```

---

## 使用方式（命令列）

### 壓縮（不加密）

```
./huffx -c input.bin output.hxf
```

### 壓縮 + XOR 加密

```
./huffx -c -k 42 input.bin output.hxf
```

### 解壓

```
./huffx -d input.hxf output.bin
```

---

## HuffX 檔案格式概要

### Header 結構

```
magic[4]      = "HXF1"
version       = 1
compression   = 1 (Huffman)
crypt_algo    = 0 or 1 (XOR)
original_size = uint32
payload_size  = uint32
padding_bits  = 0-7
crypt_param   = 7 bytes（目前 XOR 只用 [0]）
```

### Payload

1. Huffman 樹（pre-order）
2. Huffman bitstream
3. padding bits（補到 byte 對齊）

---

