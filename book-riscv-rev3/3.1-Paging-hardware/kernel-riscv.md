# [kernel/riscv.h](https://github.com/mit-pdos/xv6-riscv/blob/e90b2575ae6efd40927fedb2425a1fc54ffa23df/kernel/riscv.h#L1)

## r_mhartid

```c

// which hart (core) is this?
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

```

1. asm volatile
   1. 「これはCコンパイラにとってアセンブリ命令です」と宣言している。
2. %0 は GCC/Clang の placeholder（代用記号）
     1. → コンパイラが「x をどのレジスタで保持するか」を決め、そのレジスタ名に置き換える。
3. : "=r"(x) は
      1. "=r" → 出力専用で 汎用レジスタを使え
      2. (x) → そのレジスタと C の変数 x を 関連付ける（割り当てる）
         1. 変数xようのメモリーを確保せず使える
4. コンパイル時にコンパイラはこうする：
      1. 「x を保持するのは x10 にしよう」と決める
      2. すると "csrr %0, mhartid" → "csrr x10, mhartid" に置き換わる
5. 実行時に CPU が mhartid → x10 へ読み込み、その x10 の値が C の変数 x として見える。

## r/w_mstatus と w_mepc

```c
// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11) // Machine mode → … 0011 0000 0000 0000（12–11ビットが 11）
#define MSTATUS_MPP_S (1L << 11) // Supervisor mode → … 0001 0000 0000 0000（12–11ビットが 01）
#define MSTATUS_MPP_U (0L << 11) // User mode → … 0000 0000 0000 0000（12–11ビットが 00）


static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}
```

### mstatus

- `<< 11`は左に11ビットシフト。
- 12–11ビットに 11, 01, 00 の状態を作成。

mstatusレジスタのビット12–11（2ビット）がMPPフィールドです。
MPPは「例外（システムコール等）から戻るときのモード（M/S/U）」を保存します。

### mepc

M-mode（マシンモード）で動作中に、mret命令で移行する先のアドレスを設定するレジスタ。xv6では起動時にM-modeからS-mode（カーネル）へ移行す
る際、移行先であるmain()のアドレスを書き込むために使用される。
ブート直後でしか使われてない。

### xv6での典型的な使われ方（mstatus + mepc）

※2.6 Code: starting xv6, the first process and system call より

> The function start performs some configuration that is only allowed in machine mode, and then switches to supervisor mode. To enter supervisor mode, RISC-V provides the instruction mret.

1. ブート直後はMモードで動作
2. OS本体はSモードで動かしたいので、mstatusのMPPに「Sモードで戻る」と書く（`MSTATUS_MPP_S`）
3. mepcに「Sモードで実行を始めたいアドレス」を書く
4. `mret`を実行すると、CPUはSモードでmepcのアドレスから再開

## SSTATUSとw/r_sstatus

```c

// Supervisor Status Register, sstatus

// 戻り先モードのBit
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User

// 割り込み許可の退避先
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable

// 今割り込み許可フラグ
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable


static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

```

- SPP
  - トラップ前のモード（SかUか）を保存するビット。
  - sret 実行時、この値に応じて S か U に戻る。
- SPIE / SIE
  - SIE が「今の Sモードでの割り込み許可フラグ」。
  - トラップ発生時に SPIE ← SIE, SIE ← 0（割り込み禁止）。
  - sret 時に SIE ← SPIE で元に戻す。
- UPIE / UIE
  - Uモードでの割り込み許可の「退避先／現在値」という役割で、構造は S のものと対応している。
- r/w_sstatus
  - sstatus レジスタ 64ビットの読み書き

## r/w_sip

```c
// Supervisor Interrupt Pending
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}
```

「各種類の割り込みが保留中かどうか」 を管理
どこにも使われてなさそう

## SIEとr/w_sie

```c
// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}
```

外部またはタイマー割り込みの許可フラグを管理するRegisterへの読み書き。
SSTATUS_SIE二段構えになっている。SSTATUS_SIEでONかつSIE_SEIEまたはSIE_STIEがONじゃないとexternalまたはtimerの割り込みが処理されない。

sieは個別の許可設定
SSTATUS_SIEは全体の許可設定
と覚えればよさそう

## MIE_STIEとw/r_mie

```c
// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5)  // supervisor timer
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}
```

Machine-mode から見た、Supervisor向けタイマ割り込みを流すかどうかの許可

```text
  ┌────────────────────────────────────────────────────┐
| 現在 M-mode で実行中                                │
|                                                    │
|   タイマー割り込み発生！                            │
|         │                                          │
|         ▼                                          │
|   MIE.STIE == 1 ?                                  │
|      │                                             │
|   Yes │    No                                      │
|      ▼     ▼                                       │
|   受け付ける  無視                                  │
  └────────────────────────────────────────────────────┘

```

## r/w_sepc

```c
// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}
```

S-mode（スーパーバイザーモード）で動作中に、sret命令で戻る先のアドレスを保持するレジスタ。xv6ではシステムコールやトラップ処理後にS-mode
（カーネル）からU-mode（ユーザープログラム）へ戻る際、戻り先のアドレスを設定するために使用される。

## r/w_medeleg

```c
// Machine Exception Delegation
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}
```

[呼び出し方](https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/kernel/start.c#L31)

medeleg は、各ビットが対応する例外ごとに「M で処理するか / S に任せるか」を 0/1 で指定する設定レジスタです。

<details>

<summary>各ビットと対応する例外の一覧</summary>

| Bit位置 | 例外名（Exception）                 | 説明                               |
|--------|--------------------------------------|------------------------------------|
| 0      | Instruction address misaligned       | 命令アドレスの不正なアラインメント |
| 1      | Instruction access fault             | 命令アクセス違反                   |
| 2      | Illegal instruction                  | 不正な命令                         |
| 3      | Breakpoint                           | ブレークポイント                   |
| 4      | Load address misaligned              | ロードアドレスの不正なアラインメント |
| 5      | Load access fault                    | ロードアクセス違反                 |
| 6      | Store/AMO address misaligned         | ストア/AMOアドレスの不正なアラインメント |
| 7      | Store/AMO access fault               | ストア/AMOアクセス違反             |
| 8      | Environment call from U-mode         | Uモードからのecall                |
| 9      | Environment call from S-mode         | Sモードからのecall                |
| 10     | Reserved                             | 予約                               |
| 11     | Environment call from M-mode         | Mモードからのecall                |
| 12     | Instruction page fault               | 命令ページフォールト               |
| 13     | Load page fault                      | ロードページフォールト             |
| 14     | Reserved                             | 予約                               |
| 15     | Store/AMO page fault                 | ストア/AMOページフォールト         |

</details>

## w/r_mideleg

```c
// Machine Interrupt Delegation
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}
```

[呼び出し方](https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/kernel/start.c#L31)

mideleg は、各ビットが対応する割り込みごとに「M で処理するか / S に任せるか」を 0/1 で指定する設定レジスタです。(割り込みを誰が担当するか（M or S）)

<details>

<summary>各ビットと対応する割り込みの一覧</summary>

| Bit位置 | 略称 | 割り込み名（Interrupt）              | 説明                               |
|--------|------|--------------------------------------|------------------------------------|
| 0      | -    | Reserved                             | 予約                               |
| 1      | SSIE | Supervisor Software Interrupt        | Sモードソフトウェア割り込み         |
| 2      | -    | Reserved                             | 予約                               |
| 3      | MSIE | Machine Software Interrupt           | Mモードソフトウェア割り込み         |
| 4      | -    | Reserved                             | 予約                               |
| 5      | STIE | Supervisor Timer Interrupt           | Sモードタイマー割り込み             |
| 6      | -    | Reserved                             | 予約                               |
| 7      | MTIE | Machine Timer Interrupt              | Mモードタイマー割り込み             |
| 8      | -    | Reserved                             | 予約                               |
| 9      | SEIE | Supervisor External Interrupt        | Sモード外部割り込み                 |
| 10     | -    | Reserved                             | 予約                               |
| 11     | MEIE | Machine External Interrupt           | Mモード外部割り込み                 |

</details>

## r/w_stvec

```c
// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}
```

S-modeでトラップ（例外/割り込み）が発生したとき、ジャンプ先となるハンドラのメモリアドレスを格納するレジスタ。
(S-modeが担当する場合、どこにジャンプするか)

## w/r_stimecmp

```c
// Supervisor Timer Comparison Register
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r" (x) );
  return x;
}

static inline void 
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r" (x));
}
```

- stimecmp（CSR 0x14d）は
  - 「この時刻になったらタイマー割り込みを出せ」という目覚まし時計の時刻を入れるレジスタ。
  - r_stimecmp() で読む、w_stimecmp() で書く。
- 割り込みの起き方
  - time という現在時刻カウンタがカチカチ増え続ける。
    - [time](https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/kernel/riscv.h#L278)はハードウェアのグローバルカウンタ
  - time >= stimecmp になった瞬間に1回だけタイマー割り込みが発生する。
    - CPUハードウェアが time >= stimecmp を検知
  - 繰り返し割り込みさせたいときは、割り込みハンドラの中で毎回 w_stimecmp(now + interval) し直す。
- クロック数と「秒」
  - 1000000 みたいな値はあくまで「カウントの数」。
  - 何秒かは クロック数 ÷ 周波数(Hz) で決まるので、クロック周波数が変われば秒数も変わる。
  - xv6 は「この環境では time はだいたい○Hz」という前提で、適当な interval を決め打ちしている。
    - [code](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L177)
- タイムスライスと割り込み
  - タイマー割り込みは「A用」「B用」ではなく、CPU全体に一定周期で飛んでくるだけ。
  - 割り込みが来た瞬間に「そのとき動いていたプロセスの文脈（レジスタ等）」を保存し、OS が「time slice 終了なら別のプロセスに切り替える」という判断をする。
  - だから A実行中でもB実行中でも、関係なく割り込みは発生する。
- xv6でのタイマーSleep処理の流れ
  1. ユーザーランドでSleepを呼ぶ（例：3秒）
  2. カーネルが「目覚まし時刻」を計算  
     - 現在のtick（時刻カウンタ）＋3秒分のtick数を計算
     - プロセスの状態をSLEEPにし、chan（待ち理由）をtimer/ticksにセット
  3. カーネルがw_stimecmpで次の割り込み時刻をセット  
     - 一番早く起きるべきプロセスの時刻をstimecmpにセット
  4. CPUが自動でr_time() >= stimecmpを検知し、タイマー割り込みを発生
  5. カーネルはtimer待ちでsleep中のプロセスを全てwakeup
  6. プロセスが起きる  
     - CPUを割り当てられた時、カーネルが「本当に目覚まし時刻に達したか」を確認
       - 達していなければ再度sleep
       - 達していればユーザーランドに復帰

## w/r_menvcfg

```c
// Machine Environment Configuration Register
static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r" (x) );
  return x;
}

static inline void 
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r" (x));
}

```

- Machineモードが下位モード（S/U）に何を許可するかを設定するレジスタ
- bit 63 (STCE): Supervisorモードが stimecmp に書き込めるかどうかの設定
- xv6ではboot時に[w_menvcfg(r_menvcfg() | (1L << 63));](<https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/ke>
  rnel/start.c#L59)でSTCE=1を設定し、カーネルがタイマー割り込みを制御できるようにしている
- 実際の権限チェックはCPU（ハードウェア）が行う

## w_pmpcfg0とw_pmpaddr0

```c
// Physical Memory Protection
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}
```

- 実際にお呼び出し[箇所](https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/kernel/start.c#L38)
- w_pmpaddr0
  - どのアドレス範囲かを指定
- w_pmpcfg0
  - その範囲に何を許可するかを指定（R/W/X）
- この2つの関数はセットで使う
- xv6は
  - xv6の設定は「全部OK」という意味。それだけです。
    - w_pmpaddr0(0x3fffffffffffffull);  // 全メモリ 0x3FFFFFFFFFFFFF (2^54-1)
    - w_pmpcfg0(0xf);                    // 全許可

```text
// ullなしだと
0x3fffffffffffff   // コンパイラが32ビットと解釈するかも

// ullありだと
0x3fffffffffffffull  // 確実に64ビット符号なし整数
```

## def SATP_SV39 & MAKE_SATP

```c
// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))
```

SATPレジスタとは：CPUに「ページテーブルがどこにあるか」を教えるレジスタです。
SATP(Supervisor Address Translation and Protection)レジスタの構造（RV64）：

- bits 63-60: MODE (4ビット)
  - 0: Bare (ページング無効)
  - 8: Sv39
  - 9: Sv48
  - 10: Sv57
- bits 59-44: ASID (16ビット) - アドレス空間ID
- bits 43-0: PPN (44ビット) - ページテーブルのベースアドレスの物理ページ番号

SATP_SV39 = (8L << 60) は、MODEフィールドに8を設定して、Sv39モードを有効にしています。

`(uint64)pagetable) >> 12)`: pagetableを,12bit右にシフトしている。
ページテーブルのアドレスは必ず4KB (= 4096 = 2^12) の倍数のため、下位12ビットは常に0なので、捨てても情報は失われない。

## r/w_satp

```c
// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}
```

satp registerの読み書き

## r_scause

```c
// Supervisor Trap Cause
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}
```

トラップ（例外・割り込み）が発生した原因を示すレジスタ
[使い方](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L54)

## r_stval

```c
// Supervisor Trap Value
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}
```

r_stvalはr_scauseの詳細。

r_scauseは原因コードが入るのに対して、r_stvalは詳細な情報が入る。

例えば
r_scause=15(書き込みでのページフォルト)であれば、r_stvalは書き込みしようとした仮想アドレスが入ります。

使い方のコードは[ここ](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L72)

**stvalに入る値は例外の種類によって違う**

| 例外の種類              | stvalに入る値          |
|------------------------|----------------------|
| ページフォルト (12,13,15) | 問題の仮想アドレス       |
| アクセス違反 (5,7)       | 問題の仮想アドレス       |
| 不正命令 (2)            | その命令のビットパターン  |
| アドレスミスアライン (0,4,6) | 問題の仮想アドレス     |
| ecall (8)              | 0（特に情報なし）       |
| 割り込み                | 0（特に情報なし）       |

## r/w_mcounteren

```c
// Machine-mode Counter-Enable
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counter
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}
```

**mcounteren（Machine Counter-Enable）レジスタ**

S-mode（カーネル）やU-mode（ユーザー）が特定のハードウェアカウンタにアクセスできるかを制御するレジスタ。CPU（コア）ごとに存在する。

**RISC-Vのハードウェアカウンタ**

| ビット | 名前 | カウンタ | 何を数える | 用途 |
|-------|------|---------|-----------|------|
| bit0 | CY | cycle | CPUクロック数 | 処理時間の精密測定 |
| bit1 | TM | time | 実時間（固定周波数） | タイマー、時刻取得 |
| bit2 | IR | instret | 実行した命令数 | 性能分析、IPC計算 |

> **xv6での使用**: bit1（TM）を有効化して、timeカウンタへのアクセスを[許可](https://github.com/mit-pdos/xv6-riscv/blob/29ba4ec5f09f3d45d3cdbbf44e9174c2662cfce2/kernel/start.c#L62)している。

## 汎用Register

```c
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}
```

**csrr と mv の違い**

読み書きするレジスタの種類が違う。

| 命令          | 対象                  | 用途             | 例                   |
|---------------|----------------------|------------------|----------------------|
| mv            | 汎用レジスタ (x0-x31) | 通常の計算・データ保存 | sp, tp, ra, a0       |
| csrr / csrw   | CSR (制御/状態レジスタ) | システム制御       | time, sstatus, satp  |

```asm
mv   %0, sp        # 汎用レジスタ sp を読む
csrr %0, time      # CSR time を読む
csrw sstatus, %0   # CSR sstatus に書く
```

CSRはシステム管理用の重要な値（時間、割り込み、ページテーブル等）を持つため、専用命令でのみアクセス可能にして保護している。

| レジスタ | 正式名                 | 役割              |
|------|---------------------|-----------------|
| sp   | Stack Pointer (x2)  | スタックの先頭アドレス     |
| tp   | Thread Pointer (x4) | スレッド固有データへのポインタ |
| ra   | Return Address (x1) | 関数の戻り先アドレス      |

**対応表**

| 本名  | 別名  | 意味             |
|-----|-----|----------------|
| x1  | ra  | Return Address |
| x2  | sp  | Stack Pointer  |
| x4  | tp  | Thread Pointer |

**RISC-V 汎用レジスタ一覧 (x0〜x31)**

| 本名      | 別名     | 用途              |
|---------|--------|-----------------|
| x0      | zero   | 常に0             |
| x1      | ra     | 戻りアドレス          |
| x2      | sp     | スタックポインタ        |
| x3      | gp     | グローバルポインタ       |
| x4      | tp     | スレッドポインタ        |
| x5-x7   | t0-t2  | 一時レジスタ          |
| x8      | s0/fp  | 保存レジスタ/フレームポインタ |
| x9      | s1     | 保存レジスタ          |
| x10-x17 | a0-a7  | 引数/戻り値          |
| x18-x27 | s2-s11 | 保存レジスタ          |
| x28-x31 | t3-t6  | 一時レジスタ          |

## sfence_vma

```c
// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
//                          │     │
//                          │     └─ ASID (アドレス空間ID): zero = 全ASID
//                          └─ 仮想アドレス: zero = 全アドレス
// sfence.vma → 命令
}
```

TLB (Translation Lookaside Buffer) をフラッシュ（クリア）する命令
ページテーブルの変換結果をキャッシュする場所

TLBの中身のイメージ

**TLB (キャッシュ)**
┌──────────────┬──────────────┬─────────────┐
| 仮想アドレス   │ 物理アドレス   │ どのプロセス  │
├──────────────┼──────────────┼─────────────┤
| 0x1000       │ 0x8000       │ プロセスA    │
| 0x2000       │ 0x9000       │ プロセスA    │
| 0x1000       │ 0x5000       │ プロセスB    │
| 0x3000       │ 0x7000       │ プロセスB    │
└──────────────┴──────────────┴─────────────┘
          ↑                           ↑
      第1引数で指定              第2引数(ASID)で指定

**引数の意味**

| 引数          | 指定すると         | zero だと  |
|-------------|---------------|----------|
| 第1引数        | その仮想アドレスだけクリア | 全アドレスクリア |
| 第2引数 (ASID) | そのプロセスだけクリア   | 全プロセスクリア |

具体例

```assembly
sfence.vma zero, zero   # 全部クリア（一番シンプル）
sfence.vma a0, zero     # 仮想アドレス a0 のエントリだけクリア
sfence.vma zero, a1     # プロセス a1 のエントリだけクリア
sfence.vma a0, a1       # 特定アドレス＆特定プロセスだけクリア
```

## pte_t & pagetable_t

```c
// ページテーブルエントリ (1つのエントリ)
typedef uint64 pte_t;

// ページテーブル (512個のpte_tの配列へのポインタ)
typedef uint64 *pagetable_t; // 512 PTEs

```

## PGROUNDUP & PGROUNDDOWN

```c
#define PGSIZE 4096 // bytes per page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
```

4096 = 2¹² なので、4096の倍数は下位12ビットが常に0になる：

```text
4096 × 1 = 0001 000000000000
4096 × 2 = 0010 000000000000
4096 × 3 = 0011 000000000000
4096 × 5 = 0101 000000000000
```

マスク `~(PGSIZE - 1)` の導出：

```text
PGSIZE     = 4096 = 1000000000000   (1の後ろに0が12個)
PGSIZE - 1 = 4095 = 0111111111111   (1が12個)
~(PGSIZE - 1)     = ...11111000000000000  (下位12ビットだけ0)
```

- **PGROUNDDOWN(a)**: `a`と`~(PGSIZE-1)`のANDを取り、下位12ビットを0にする。`a`以下で最大の4096の倍数を返す。
- **PGROUNDUP(sz)**: `sz`に`PGSIZE-1`を加えてから下位12ビットを0にする。`sz`以上で最小の4096の倍数を返す。

## PTE

```c
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)
```

`3.1 Paging hardware`で説明されているPTEのところ

## extract

```c
#define PGSHIFT 12  // bits of offset within a page

// extract the three 9-bit page table indices from a virtual address.
// レベルのVPNのビットの値を抽出するためのMASK
#define PXMASK          0x1FF // 9 bits

// 各レベルのVPNのビット開始位置を求めています。
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))

// 仮想アドレスから指定レベルのVPN値を取得
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
// xv6 の「有効な仮想アドレスの上限」を表す定数です。
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
```

```text
// 16 to 2 進数
F = 1111
1FF = 111111111
```
