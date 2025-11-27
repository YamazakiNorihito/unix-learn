# kalloc.c(kernel/kalloc.c) について

kalloc.cのコードを理解することから始める。

## データ構造

```c
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```

空きページそのものをリストノードとして使っています。つまり、使われていないページの先頭8バイトに「次の空きページのアドレス」を格納している。

```
kmem.freelist
    |
    v
空きページ1 (4KB)              空きページ2 (4KB)              空きページ3 (4KB)
┌─────────────────┐            ┌─────────────────┐            ┌─────────────────┐
│ next: 0x5000 ───┼───────────>│ next: 0x6000 ───┼───────────>│ next: NULL      │
├─────────────────┤            ├─────────────────┤            ├─────────────────┤
│                 │            │                 │            │                 │
│  (未使用領域)    │            │  (未使用領域)    │            │  (未使用領域)    │
│                 │            │                 │            │                 │
│     4088 bytes  │            │     4088 bytes  │            │     4088 bytes  │
│                 │            │                 │            │                 │
└─────────────────┘            └─────────────────┘            └─────────────────┘
```

## kinit

`KERNBASE`から`PHYSTOP`までの範囲を、freeマーカーを付けて1ページ（4,096バイト）ごとにkmem.runのfree pageリストに追加している。

```c
// 「未使用メモリの開始地点を示すシンボルのアドレス」を取得するための宣言であり、実際に配列として機能するわけではない
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// memlayout.h
#define KERNBASE 0x80000000L // 2,147,483,648 (10進数)
#define PHYSTOP (KERNBASE + 128*1024*1024) // 2,288,357,376 (10進数)
```

```text
// kernel/kernel.ld
SECTIONS
{
  /*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80000000;

  PROVIDE(end = .);
}
```

## freerange(void *pa_start, void*pa_end)

指定したアドレス範囲の中で、4KB（4096バイト）ごとにページの先頭アドレスを計算し、その各ページを「未使用（free）」としてkmem.runのfree pageリストに順番に追加する関数

```c
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start); // ※1
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// riscv.h
#define PGSIZE 4096 // bytes per page
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

// memlayout.h
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)
```

※1  
PGROUNDUP は引数を 4096 (=PGSIZE) の倍数に切り上げる。`sz + (PGSIZE-1)` で 4096 未満の端数を一度足し込み、その値に `~(PGSIZE-1)` を AND して下位 12 ビットを 0 に戻す。
`4095`(10進数)=`00000000 00000000 00001111 11111111`(2進数)=`FFF`(16進数)

例えば `sz = 5000`(`1001110001000`/`1388`) なら、

1. `5000 + 4095 = 9095`(`10001110000111`/`2387`)
2. `9095 & ~4095 = 8192`（4096 で割った余りを捨てるのと同じ）
3. `8192`をreturnする

## kfree(void *pa)

1ページ（4KB）のメモリ領域を値1で埋めてから、kmem.runのfree pageリストの先頭に追加する関数です。

```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // 1ページ（4KB）のメモリ領域を値1で埋め
  // 「解放したメモリを再利用する際に、以前使われていたデータ（機密情報や古い値など）が新しい利用者に見えてしまうのを防ぐため」
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

## kalloc(void)

freelistから1ページ分(4KB)のベースアドレスを返します。
(戻り値はページ先頭へのポインタなので、呼び出し側は 4KB をまるごと使える前提で利用する。)

```c
void *
kalloc(void)
{
  struct run *r;
  
  // ロック獲得
  acquire(&kmem.lock);
  // freelistの先頭のアドレスを取得
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next; // 先頭のアドレスを割り当てたので、次のアドレスをfreelistのアドレスとして移動する
  
  // ロック解除
  release(&kmem.lock);
  
  // 割り当てたページ全体（4096バイト）を値 5 で埋める。「新しく割り当てたメモリ」を識別しやすくしているっぽい
  // 1バイト = 0000 0101 (2進数) = 0x05 (16進数) = 5 (10進数)
  // 1バイト = 8ビット
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

```

デバッグする時のパターンの違いで状態を識別ができる。
0x01010101... → 解放済みメモリ
0x05050505... → 割り当てられたが未初期化のメモリ
それ以外 → 実際のデータ
