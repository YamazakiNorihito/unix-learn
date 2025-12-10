# [kernel/memlayout.h](https://github.com/mit-pdos/xv6-riscv/blob/19f111947d2452957c14995f1edd7831f08258aa/kernel/memlayout.h)

Figure 3.3　Virtual Addressesはkernelのvirtual addresses

## UART

UART（Universal Asynchronous Receiver/Transmitter）[読み：ユーアート]は、
コンピュータの中で文字を送ったり受け取ったりする特別な装置。

**1. レジスタとは**

レジスタ = ハードウェアの中にある小さなメモリ

デバイス（UARTチップ）の中に、数バイトの記憶領域があります。ここに値を書いたり読んだりすることで、デバイスを制御します。

**2. UARTは何をするもの？**

```
キーボード → [UART] → CPU    （入力）
CPU → [UART] → 画面           （出力）
```

シリアル通信のチップです。xv6では：

- `printf`で文字を出す → UARTが送信
- キーボードを打つ → UARTが受信

**3. どうやって制御する？**

普通のメモリアクセスと同じ方法で制御します。

```c
// アドレス 0x10000000 に 'A' を書き込む
*(char*)0x10000000 = 'A';
```

これだけで画面に'A'が表示される。

なぜ？ → アドレス`0x10000000`はRAMではなく、UARTチップに繋がっているから。

**4. 図で理解する**

```
CPU が アドレス 0x10000000 に書き込む
           │
           ▼
    ┌──────────────┐
    │   メモリバス   │
    └──────────────┘
           │
     ┌─────┴─────┐
     ▼           ▼
  ┌─────┐    ┌──────┐
  │ RAM │    │ UART │  ← 0x10000000はここに届く
  └─────┘    └──────┘
                 │
                 ▼
              画面に出力
```

### UART0_IRQ

IRQ = Interrupt Request（割り込み要求）番号（割り込み番号）

UARTが「データが届いたよ」「送信完了したよ」とCPUに通知する時の識別番号です。

大まかな流れ

```text
1. キーボードを押す
        │
        ▼
2. UART が文字を受信
        │
        ▼
3. UART → CPU に「割り込み番号10」を通知
        │
        ▼
4. CPU が割り込みハンドラを実行
        │
        ▼
5. 「番号10か、UARTだな」→ uartintr() を呼ぶ
```

[uartintr()](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L196)

## VIRTIO0

VIRTIO0  = virtio ディスク 0番の MMIO ベースアドレス

VIRTIO0_IRQは、UART0_IRQと同じ役割。（割り込み番号）
[virtio_disk_intr()](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L199)

## PLIC

PLIC（Platform-Level Interrupt Controller）は、複数のデバイスからの割り込みを一元管理して、複数のCPUに振り分けるコントローラ。

```c
// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)
```

各マクロの役割

- PLIC_PRIORITY (+0x0): 4バイトごとに1つの割り込み元（デバイス）が割り当てられており、各アドレスにその割り込みの優先度を設定する
- PLIC_PENDING (+0x1000): 1ビットごとに1つの割り込み元が割り当てられており、1なら「待っている」、0なら「待っていない」を表す
- PLIC_SENABLE(hart) (+0x2080): 各CPU（hart）が、どのデバイスからの割り込みを受け付けるかを設定
- PLIC_SPRIORITY(hart) (+0x201000): 各CPUが受け付ける割り込みの優先度閾値を設定。これより高い優先度の割り込みだけを受け付ける
- PLIC_SCLAIM(hart) (+0x201004): 処理すべき最も優先度が高い割り込み番号が入っている。処理完了後は同じ番号を書き込んで完了を通知する

全体の構成

```text
  ┌────────────────────────────────────────────┐
  │                    PC                       │
  │                                            │
  │   ┌──────┐                                 │
  │   │ UART │────┐                            │
  │   └──────┘    │      ┌───────┐             │
  │               ├─────→│ PLIC  │             │
  │   ┌──────┐    │      │（1個）│             │
  │   │ Disk │────┘      └───┬───┘             │
  │   └──────┘               │                 │
  │                          ↓                 │
  │              ┌───────────┴───────────┐     │
  │              ↓           ↓           ↓     │
  │           ┌─────┐    ┌─────┐    ┌─────┐   │
  │           │CPU 0│    │CPU 1│    │CPU 2│   │
  │           └─────┘    └─────┘    └─────┘   │
  └────────────────────────────────────────────┘
```

UARTやDiskなどのデバイスがPLICに割り込み信号を送り、PLICがそれを適切なCPUに振り分ける。

時系列の流れ

1. UARTが「文字来た」とPLICに信号を送る
2. PLICが優先度を確認し、CPU0とCPU1の割り込みピンを立てる
3. CPU0が割り込みを検知し、割り込みハンドラにジャンプ
4. カーネルがPLIC_SCLAIMを読んで、何の割り込みか確認
5. PLICが「UART（10番）」と返す。この瞬間、他のCPUがclaimしても0が返る
6. カーネルがUARTの処理を実行
7. 処理完了後、PLIC_SCLAIMに10を書き込んで完了を通知
8. PLICが次の割り込み待ちに戻る

各CPUには物理的な割り込み入力線（ピン）があり、CPUの「外部割り込み入力」という端子にPLICからの線がつながっている。

PRIORITYとPENDINGのメモリレイアウト

```text
割り込み番号    PRIORITY（優先度）     PENDING（待ち状態）
─────────────────────────────────────────────────────
    0         PLIC+0x00 → 4バイト    PLIC+0x1000 のビット0
    1(Disk)   PLIC+0x04 → 4バイト    PLIC+0x1000 のビット1
    2         PLIC+0x08 → 4バイト    PLIC+0x1000 のビット2
   ...             ...                    ...
   10(UART)   PLIC+0x28 → 4バイト    PLIC+0x1000 のビット10
   ...             ...                    ...
   31         PLIC+0x7C → 4バイト    PLIC+0x1000 のビット31
   32         PLIC+0x80 → 4バイト    PLIC+0x1004 のビット0  ← 次の4バイトへ
```

PRIORITYは「割り込み番号 × 4バイト」で配列のように並ぶ。PENDINGは割り込み番号がそのままビット位置に対応する。どちらも割り込み番号で対応関係がある。

例: Disk（割り込み番号1）はPRIORITYで優先度を設定し、PENDINGのビット1で待ち状態を確認する。UART（割り込み番号10）はPRIORITYで優先度を設定し、PENDINGのビット10で待ち状態を確認する。

UARTやDiskの優先度は[kernel/plic.c](https://github.com/mit-pdos/xv6-riscv/blob/dd5a720044c41a88e0a09f174fb602289b93fe28/kernel/plic.c#L12)で設定している。

## KERNBASE & PHYSTOP

```c
// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)
```

カーネルとユーザが128MBの物理メモリを分け合って使う。

0x80000000 は 2^31

## TRAMPOLINE

```c
#define PGSIZE 4096                      // 4KB（1ページ）
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))  // 最大仮想アドレス　= 1L << 38 = 0x40_0000_0000 = 256GB


// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)
```

仮想アドレス空間の一番上に配置された4KBのページ。開始アドレスは MAXVA - PGSIZE。
trampolineのsizeはPGSIZE。

## KSTACK

```c
// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)
```

プロセス番号 p によって、カーネルスタックの仮想アドレスが静的に決まります。
カーネルスタックとは、カーネルランドでのみ使われるスタック。

```text
  具体例（PGSIZE = 4096 = 0x1000 の場合）

  p=0  → KSTACK(0) = TRAMPOLINE - 2*PGSIZE   = TRAMPOLINE - 0x2000
  p=1  → KSTACK(1) = TRAMPOLINE - 4*PGSIZE   = TRAMPOLINE - 0x4000
  p=2  → KSTACK(2) = TRAMPOLINE - 6*PGSIZE   = TRAMPOLINE - 0x6000
  ...
  p=63 → KSTACK(63) = TRAMPOLINE - 128*PGSIZE = TRAMPOLINE - 0x80000
```

## TRAPFRAME

```c
// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
```

 ユーザー空間の話
 TRAPFRAMEは、システムコール/割り込み時にレジスタを保存する場所

```text
  高アドレス (MAXVA)
  ┌─────────────────────────┐
  │ TRAMPOLINE              │ ← カーネルと同じページを共有
  ├─────────────────────────┤
  │ TRAPFRAME               │ ← レジスタ保存用（p->trapframe）
  ├─────────────────────────┤
  │ ...                     │ ← 未使用領域
  ├─────────────────────────┤
  │ expandable heap         │ ← sbrk()で拡張可能、↓方向に伸びる
  ├─────────────────────────┤
  │ fixed-size stack        │ ← ユーザースタック（固定サイズ）
  ├─────────────────────────┤
  │ original data and bss   │ ← グローバル変数、初期化データ
  ├─────────────────────────┤
  │ text                    │ ← プログラムのコード
  └─────────────────────────┘
  低アドレス (0)
```
