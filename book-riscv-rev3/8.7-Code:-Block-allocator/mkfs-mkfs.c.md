## mkfs の balloc — ビットマップの初期設定

> The program mkfs sets the bits corresponding to the boot sector, superblock,
> log blocks, inode blocks, and bitmap blocks.

### ディスクレイアウト (mkfs/mkfs.c:20)

```
[ boot block | sb block | log | inode blocks | free bit map | data blocks ]
  ブロック0    ブロック1   2~    ...            ...            ここから空き
  ←────────────── メタブロック(nmeta個) ──────────→
```

### 1. メタブロック数の算出 (mkfs.c:93)

```c
nmeta = 2 + nlog + ninodeblocks + nbitmap;
//      ^    ^      ^              ^
//      |    |      |              bitmap用ブロック数
//      |    |      inodeテーブル用ブロック数
//      |    logヘッダ+データブロック数
//      boot + superblock
```

### 2. ビットマップのブロック数 (mkfs.c:23)

```c
int nbitmap = FSSIZE/BPB + 1;  // 2000/8192 + 1 = 1
```

- FSSIZE = 2000（ディスク全体のブロック総数）
- BPB = BSIZE*8 = 8192（ビットマップ1ブロックで管理できるブロック数）
- FSSIZE/BPB + 1 で、ビットマップに必要なブロック数を算出（+1 は切り上げ）

### 3. freeblock カウンター (mkfs.c:108, 272)

```c
freeblock = nmeta;  // 最初の空きブロック番号 = メタブロックの直後
```

`freeblock` は「次に使える空きブロックの番号」を示すカウンター。
ファイルをディスクに書く `iappend` 内で、データブロックを割り当てるたびにインクリメントされる:

```c
din.addrs[fbn] = xint(freeblock++);  // mkfs.c:272
// freeblock: 38 → 39 → 40 → 41 ...
```

### 4. balloc — ビットマップへの書き込み (mkfs.c:237-250)

main の最後に `balloc(freeblock)` を呼び出す (mkfs.c:172)。
この時点で freeblock = メタブロック数 + データで使ったブロック数。

```c
void
balloc(int used)  // used = freeblock = 使用済みブロックの総数
{
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BPB);   // ビットマップ1ブロック(8192ビット)に収まるか確認
  bzero(buf, BSIZE);    // 全ビットを0（空き）で初期化
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));  // ← "sets the bits" がこれ
  }
  printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);  // ビットマップブロックとしてディスクに書き込む
}
```

#### ビット操作の仕組み

1バイト（8ビット）で8ブロック分の使用状態を管理する:

- `i/8` → 何バイト目か
- `i%8` → そのバイトの何ビット目か
- `0x1 << (i%8)` → 対象ビットだけが1のマスクを作る
- `|`（OR） → 他のビットを壊さず、対象ビットだけ1にする

例（i=0〜18 が使用済みの場合）:

```
buf[0] = 11111111  → ブロック 0〜7  使用中
buf[1] = 11111111  → ブロック 8〜15 使用中
buf[2] = 00000111  → ブロック16〜18 使用中, 19〜23 空き
buf[3] = 00000000  → ブロック24〜31 空き
...
```
