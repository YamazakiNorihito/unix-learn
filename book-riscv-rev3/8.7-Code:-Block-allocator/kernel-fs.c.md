
## balloc

```c
// The block allocator provides two functions: balloc allocates a new disk block
// Blocks.

// Allocate a zeroed disk block.
// returns 0 if out of disk space.
// dev:デバイス番号
static uint
balloc(uint dev) // 空きブロックを1つ見つけて返す関数
{
  int b, bi, m;
  struct buf *bp;

/*
  b  = 外側ループ: ビットマップブロックの先頭ブロック番号（0, 8192, 16384...）                                                                                                                      
  bi = 内側ループ: そのビットマップブロック内の何番目か（0〜8191）

  b + bi = 実際のブロック番号

  例:
  b=0,    bi=50  → ブロック50
  b=8192, bi=3   → ブロック8195
*/
  bp = 0;
  for(b = 0; b < sb.size; b += BPB){ //  本当に複数デバイス対応するなら、デバイスごとに sb を持つ必要がありますが、xv6 はそこまでやっていない。
    // BBLOCK はビットマップブロックのセクタ番号を算出するマクロ。bread がそのセクタを読み込む。devは飾りなので無視して、固定のDeviceだと思えば、良い。
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      // bp->data は uchar(1バイト) × 1024個の配列 (kernel/buf.h:10)
      // bi/8 → bp->data の何バイト目か（0〜1023）
      // bi%8 → そのバイト(8ビット)の何ビット目か（0〜7）
      // https://github.com/mit-pdos/xv6-riscv/blob/deaff5d8a689e6aa7b64b38619cf667b963256da/kernel/buf.h#L10
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// Free a disk block.
// uint b : block number
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;
/*
  bp = bread(dev, BBLOCK(b, sb));  // ビットマップブロックを読む                                                                                                                                
  bi = b % BPB;                    // ビットマップブロック内のビット位置（0〜8191）
  bi/8                             // bp->data の何バイト目か（0〜1023）                                                                                                                            
  m = 1 << (bi % 8);              // そのバイトの何ビット目か（0〜7）

  具体例（b=42）:
  bp = bread(...)         → ビットマップブロックを丸ごと読む（1024バイト）
  bi = 42 % 8192 = 42    → 42ビット目
  bi/8 = 5               → bp->data[5]（6バイト目）
  bi%8 = 2, m=00000100   → そのバイトの2ビット目

  あなたの理解の流れは合っています。順番だけ整理すると:

  1. BBLOCK → どのビットマップブロックか
  2. bi = b % BPB → そのブロック内の何ビット目か
  3. bi/8 → 何バイト目か（data の index）
  4. m = 1 << (bi%8) → そのバイトの何ビット目か
*/
  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

```
