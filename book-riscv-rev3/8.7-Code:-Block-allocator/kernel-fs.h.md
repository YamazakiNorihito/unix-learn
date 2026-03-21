```c

// 1ブロックのサイズ（バイト数）です。 
#define BSIZE 1024  // block size

// 1つのビットマップブロックで管理できるブロック数
// 1バイト   = 8ビット なので　BSIZE*8　とする
// Bitmap bits per block
#define BPB           (BSIZE*8) // BPB = 1024 × 8 = 8192ブロック分の情報を1つのビットマップブロックに入れられる

// kernel/param.h
#define FSSIZE       2000  // size of file system in blocks

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)
```
