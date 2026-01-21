# uart.c

## レジスタ定義

```c
// kernel/memlayout.h
#define UART0 0x10000000L

// kernel/uart.c
// the UART control registers.
// some have different meanings for read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define FCR 2                 // FIFO control register
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LSR 5                 // line status register
```

| オフセット | 読み取り                       | 書き込み                        | 説明               |
|------------|--------------------------------|---------------------------------|--------------------|
| 0          | RHR (Receive Holding Register) | THR (Transmit Holding Register) | 受信/送信データ    |
| 1          | IER                            | IER (Interrupt Enable Register) | 割り込み有効化     |
| 2          | ISR (Interrupt Status Register)| FCR (FIFO Control Register)     | 割り込み状態/FIFO制御 |
| 3          | LCR                            | LCR (Line Control Register)     | ライン制御         |
| 5          | LSR (Line Status Register)     | -                               | ライン状態         |

UART0 = 0x10000000 なので：

| レジスタ | オフセット | 実際のアドレス |
|----------|------------|----------------|
| RHR/THR  | 0          | 0x10000000     |
| IER      | 1          | 0x10000001     |
| FCR/ISR  | 2          | 0x10000002     |
| LCR      | 3          | 0x10000003     |
| LSR      | 5          | 0x10000005     |

---

## ビットフラグ（ビットマスク）

```c
/*
 * IER (Interrupt Enable Register) - オフセット1
 * ┌──────┬──────┬──────┬──────┬──────┬──────┬────────┬────────┐
 * │ bit7 │ bit6 │ bit5 │ bit4 │ bit3 │ bit2 │  bit1  │  bit0  │
 * ├──────┼──────┼──────┼──────┼──────┼──────┼────────┼────────┤
 * │  -   │  -   │  -   │  -   │  -   │  -   │ TX割込 │ RX割込 │
 * └──────┴──────┴──────┴──────┴──────┴──────┴────────┴────────┘
 *   - bit0=1: 受信データがあると割り込み発生
 *   - bit1=1: 送信バッファが空になると割り込み発生
 */
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
```

```c
/*
 * FCR (FIFO Control Register) - オフセット2
 * ┌──────┬──────┬──────┬──────┬──────┬──────────┬──────────┬──────────┐
 * │ bit7 │ bit6 │ bit5 │ bit4 │ bit3 │   bit2   │   bit1   │   bit0   │
 * ├──────┼──────┼──────┼──────┼──────┼──────────┼──────────┼──────────┤
 * │  -   │  -   │  -   │  -   │  -   │ TXクリア │ RXクリア │ FIFO有効 │
 * └──────┴──────┴──────┴──────┴──────┴──────────┴──────────┴──────────┘
 *   - bit0=1: FIFOを有効化
 *   - bit1=1: 受信FIFOをクリア
 *   - bit2=1: 送信FIFOをクリア
 *   - (3<<1) = 0b110 → bit1とbit2両方セット（両方クリア）
 */
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
```

```c
/*
 * LCR (Line Control Register) - オフセット3
 * ┌──────┬──────┬──────┬──────┬──────┬──────┬─────────────┬─────────────┐
 * │ bit7 │ bit6 │ bit5 │ bit4 │ bit3 │ bit2 │    bit1     │    bit0     │
 * ├──────┼──────┼──────┼──────┼──────┼──────┼─────────────┼─────────────┤
 * │ DLAB │  -   │  -   │  -   │  -   │  -   │ データ長[1] │ データ長[0] │
 * └──────┴──────┴──────┴──────┴──────┴──────┴─────────────┴─────────────┘
 *   - bit0-1: データ長（0b11=8ビット、0b10=7ビット、0b01=6ビット、0b00=5ビット）
 *   - bit7=1 (DLAB): オフセット0,1がボーレート設定レジスタに切り替わる
 */
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
```

```c
/*
 * LSR (Line Status Register) - オフセット5
 * ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬────────────┐
 * │ bit7 │ bit6 │ bit5 │ bit4 │ bit3 │ bit2 │ bit1 │    bit0    │
 * ├──────┼──────┼──────┼──────┼──────┼──────┼──────┼────────────┤
 * │  -   │  -   │ TX空 │  -   │  -   │  -   │  -   │ RX準備完了 │
 * └──────┴──────┴──────┴──────┴──────┴──────┴──────┴────────────┘
 *   - bit0=1: RHRに読み取り可能なデータあり
 *   - bit5=1: THRが空（次の文字を送信可能）
 */
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send
```

---

## レジスタアクセスマクロ

```c
/*
 * Reg(0) の場合:
 *
 *   UART0 + 0
 * = 0x10000000 + 0
 * = 0x10000000
 *
 *   (volatile unsigned char *)(0x10000000)
 * = 「アドレス0x10000000を指す、1バイトのポインタ」
 *
 * 結果: アドレスを返す（まだ値は読んでいない）
 */
#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))

/*
 * ReadReg(0) の場合:
 *
 *   *(Reg(0))
 * = *(0x10000000へのポインタ)
 * = 0x10000000の中身を読む  ← ここで「*」が参照外し
 *
 * *（デリファレンス）がポイント！
 *
 *   ポインタ  → アドレスを持つ
 *   *ポインタ → そのアドレスの中身
 */
#define ReadReg(reg) (*(Reg(reg)))

/*
 * WriteReg(0, 'A') の場合:
 *
 *   *(Reg(0)) = 'A'
 * = *(0x10000000へのポインタ) = 'A'
 * = 0x10000000の中身に'A'を書き込む
 */
#define WriteReg(reg, v) (*(Reg(reg)) = (v))
```

---

## ビットシフト演算

```c
1 << 0  →  1を左に0ビットシフト = 1 (0b00000001)
1 << 1  →  1を左に1ビットシフト = 2 (0b00000010)
1 << 2  →  1を左に2ビットシフト = 4 (0b00000100)
```
