# [kernel/vm.c](https://github.com/mit-pdos/xv6-riscv/blob/9374395cd3dc31c0e6993b3d257e2379a8dec89f/kernel/vm.c)

## walk

```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

```

## mappages

```c

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.

/*
va:virtual_address
pa:physical_address
perm:permission
*/
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  // 最後のページの先頭アドレス
  last = va + size - PGSIZE;
  for(;;){
    // Level 0 の PTE を取得(途中のページテーブルがなければ作成)
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
     // Level 0 の PTE に物理ページアドレス、権限、Valid フラグを設定。
     // これにより仮想アドレス a が物理アドレス pa にマップされる。
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}
```

`last = va + size - PGSIZE;`
va = 0x1000, size = 0x3000 (3ページ分)

マップしたいページ:
  [0x1000 - 0x1FFF]  ← 1ページ目
  [0x2000 - 0x2FFF]  ← 2ページ目
  [0x3000 - 0x3FFF]  ← 3ページ目 (最後)

last = va + size - PGSIZE
      = 0x1000 + 0x3000 - 0x1000
      = 0x3000  ← 最後のページの先頭アドレス

## walkaddr

```c
// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    // そのページにユーザーモード（U-mode）からアクセスできるかどうかを制御
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```

仮想アドレスに対応するページの先頭物理アドレスを返す。

## vmfault

```c
// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.

// The parameter 'read' is currently unused.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    return 0;
  }
  mem = (uint64) kalloc();
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}
```

`if (va >= p->sz)`の処理について
user land において、アドレス空間は 0 から始まります。p->sz にはプロセスが現在使用しているメモリサイズが格納されています。
開始アドレスが 0 であるため、「終端アドレス = 開始アドレス + サイズ = 0 + sz = sz」が成り立ちます。
つまり p->sz は「メモリサイズ」であると同時に「使用アドレスの終端」としても機能します。
このため、if (va >= p->sz) という比較で、仮想アドレスがプロセスの有効なメモリ範囲内にあるかどうかをチェックできます。
一見するとサイズとアドレスを比較しているように見えますが、開始アドレスが 0 という前提を利用したテクニックです。

## copyout

```c
// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.

/*
  pagetable: コピー先プロセスのページテーブル
  dstva:destination virtual address。ユーザー空間のコピー先仮想アドレス
  src:カーネル空間のコピー元アドレス（カーネルは物理アドレスを直接扱える）
  len:コピーするバイト数
*/
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
    
    // va0に対するPhysical addressがあるか確認
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      // va0に対するPhysical addressが割り当てられていなければ割り当てる。
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    // va0のPTEでmemoryの書き込みpermissionの確認をする
    pte = walk(pagetable, va0, 0);
    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;

    // このページ内でコピーできるバイト数を計算
    // dstva からページ末尾までの残りサイズ
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

```

**なぜ char * なのか**

copyout は任意のデータをバイト単位でコピーする汎用関数。char は 1バイトなので、1バイトずつ正確にアドレスを進めることができる。

C言語では ptr(pointer) + n は「n × 型のサイズ」バイト進みます:

| 型       | ptr + 1  | ptr + 5   |
|----------|----------|-----------|
| char *   | +1バイト | +5バイト  |
| uint64 * | +8バイト | +40バイト |

なお、ポインタ自体のサイズは指す先の型に関係なく、64bit システムでは常に 8バイトです。

C言語では ptr + n は 「n × 型のサイズ」バイト進む:
[c playgroundサイト](https://www.onlineide.pro/playground/c)で動かせる

```c
  #include <stdio.h>
  #include <stdint.h>

  int main() {
      // ポインタ演算の確認
      char *cp = (char *)0x1000;
      uint64_t *up = (uint64_t *)0x1000;

      printf("=== ポインタ演算 ===\n");
      printf("char *cp = 0x1000\n");
      printf("cp + 1 = %p\n", (void *)(cp + 1));   // 0x1001 (+1バイト)
      printf("cp + 5 = %p\n", (void *)(cp + 5));   // 0x1005 (+5バイト)

      printf("\nuint64 *up = 0x1000\n");
      printf("up + 1 = %p\n", (void *)(up + 1));   // 0x1008 (+8バイト)
      printf("up + 5 = %p\n", (void *)(up + 5));   // 0x1028 (+40バイト)

      // ポインタ自体のサイズ確認
      printf("\n=== ポインタのサイズ ===\n");
      printf("sizeof(char)      = %zu バイト\n", sizeof(char));       // 1
      printf("sizeof(uint64_t)  = %zu バイト\n", sizeof(uint64_t));   // 8
      printf("sizeof(char *)    = %zu バイト\n", sizeof(char *));     // 8 (64bit)
      printf("sizeof(uint64_t *)= %zu バイト\n", sizeof(uint64_t *)); // 8 (64bit)

      return 0;
  }
```

**copyin**に関してはcopyoutの逆をしている。

## kvmmake

```c
// Make a direct-map page table for the kernel.
// カーネル用のページテーブルを作成する。
// カーネル空間では仮想アドレス = 物理アドレス (direct-map) となる。
// (xv6 book Figure 3.3 参照)
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  // first allocates a page of pyhical memory tohold the root page-table page.
  kpgtbl = (pagetable_t) kalloc();

  // kpgtbl が指すアドレスから PGSIZE (4KB) 分を 0 で埋める
  // Level-2 (root) ページテーブルの 4KB (512 エントリ) を 0 で初期化
  // 各エントリが 0 なので、この時点では全て無効 (PTE_V = 0)
  memset(kpgtbl, 0, PGSIZE);
  //     ↑      ↑  ↑
  //     │      │  └─ 4096 バイト (1ページ分)
  //     │      └──── 埋める値 (0)
  //     └─────────── 開始アドレス

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// kernel/proc.c
// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}
```

`Figure 3.3:`で解説している`kernel address space.`と`RISC-V physical address space`のmapをしている。

`ptrdiff_t d = p - proc` 2つのポインタの減算結果は「[要素数の差](https://chaste.web.fc2.com/Reference.files/C_Standard.files/ptrdiff_t.html)」を返す。

## kvminithart

```c
// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  // sfence_vmaはTLB（Translation Lookaside Buffer）をフラッシュする命令
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// kernel/riscv.h
// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}
// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}
```

## kvmmap

```c
// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}
```
