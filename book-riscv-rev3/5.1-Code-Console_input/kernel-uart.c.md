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

## uartintr

```c

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from devintr().
void
uartintr(void)
{
  ReadReg(ISR); // acknowledge the interrupt

  acquire(&tx_lock);
  if(ReadReg(LSR) & LSR_TX_IDLE){
    // UART finished transmitting; wake up sending thread.
    tx_busy = 0;
    wakeup(&tx_chan);
  }
  release(&tx_lock);

  // read and process incoming characters, if any.
  /*
    uartgetc() でUARTハードウェアから待機中の文字を全て読み取り、1文字ずつ consoleintr()
    に渡します。文字を待たない（ブロックしない）のは、次の入力があれば新しい割り込みが発生するからです。
  */
  while(1){
    int c = uartgetc(); // UARTから1文字読む（なければ-1）
    if(c == -1)
      break;
    consoleintr(c); // 各文字をconsoleintrに渡す
  }
}
```

## uartputc & uartstart

```bash
git checkout 27057bc -- kernel/uart.c
```

```c
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
int uart_tx_w; // write next to uart_tx_buf[uart_tx_w++]

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
//
// usually called from the top-half -- by a process
// calling write(). can also be called from a uart
// interrupt to echo a received character, or by printf
// or panic from anywhere in the kernel.
//
// the block argument controls what happens if the
// buffer is full. for write(), block is 1, and the
// process waits. for kernel printf's and echoed
// characters, block is 0, and the character is
// discarded; this is necessary since sleep() is
// not possible in interrupts.
/*

int block : 
  1 : バッファ満杯なら sleep で待つ 
  0 : バッファ満杯なら文字を捨てて戻る
*/
void
uartputc(int c, int block)
{
  acquire(&uart_tx_lock);
  while(1){
    if(((uart_tx_w + 1) % UART_TX_BUF_SIZE) == uart_tx_r){
      // 書き込み(w) が 読み取り(r) に「追いついた」= 満杯
      // buffer is full.
      if(block){
        // wait for uartstart() to open up space in the buffer.
        sleep(&uart_tx_r, &uart_tx_lock);
      } else {
        // caller does not want us to wait.
        release(&uart_tx_lock);
        return;
      }
    } else {
      uart_tx_buf[uart_tx_w] = c;
      uart_tx_w = (uart_tx_w + 1) % UART_TX_BUF_SIZE;
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}

// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void
uartstart()
{
  while(1){
    if(uart_tx_w == uart_tx_r){
      // transmit buffer is empty.
      return;
    }
    
    if((ReadReg(LSR) & (1 << 5)) == 0){
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r];
    uart_tx_r = (uart_tx_r + 1) % UART_TX_BUF_SIZE;
    
    // maybe uartputc() is waiting for space in the buffer.
    wakeup(&uart_tx_r);
    
    // UARTに1文字送信
    WriteReg(THR, c);
  }
}

// read one input character from the UART.
// return -1 if none is waiting.
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
// === 疑問と回答 ===
//
// Q1: uartintr() は現在のプロセスを sleep させていないのに、
//     なぜ続きの処理（次の文字の送信）ができるの？
//
// A1: 割り込みはプロセスとは無関係に発生する。
//     UART が1文字送信完了するたびに割り込みが発生し、
//     uartintr() → uartstart() で次の文字を送信する。
//     プロセスが sleep する必要はない。
//
//     1文字送信完了 → 割り込み → uartstart() → 次の1文字送信
//     これを繰り返してバッファの全文字を送る。
//
// Q2: 複数のユーザープロセスが同時に write() したら、
//     文字が混ざってしまわない？
//
// A2: 混ざる可能性がある。これは仕様。
//     uart_tx_lock はバッファのデータ構造を保護するが、
//     複数プロセスの出力が混ざらないことは保証しない。
//     Unix 系 OS では複数プロセスが同時に書くと混ざりうる。
//
// Q3: wakeup(&uart_tx_r) は毎回呼ばれるけど、
//     誰も sleep していない時はどうなる？
//
// A3: 空振りする（何も起きない）。
//     sleep している人がいる時だけ、その人を RUNNABLE にする。
//     バッファが満杯で uartputc() が sleep している時に意味がある。
void
uartintr(void)
{
  // read and process incoming characters.
  // 受信文字の処理
  // キーボード入力があれば consoleintr() に渡す
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // 送信バッファの処理
  // UART 送信完了割り込みで呼ばれた場合、
  // バッファに残っている次の文字を送信する
  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}

```

### `((uart_tx_w + 1) % UART_TX_BUF_SIZE) == uart_tx_r` の説明

【リングバッファ: w が r に追いつく様子】

```text
① 初期状態（空）
┌───┬───┬───┬───┬───┬───┬───┬───┐
│   │   │   │   │   │   │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑
 w,r   w == r → 空

② 3文字書き込み
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │   │   │   │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑           ↑
  r           w

③ さらに書き込み（w が進む）
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │ E │ F │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑                       ↑
  r                       w

④ もう1文字書き込み（満杯）
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │ E │ F │ G │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑                           ↑
  r                           w
  ↑
  w+1  (w+1) % 8 == r → 追いついた！満杯！

⑤ これ以上書くと...
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │ E │ F │ G │ ? │ ← r の位置を上書き = NG
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑                           ↑
 w,r                         (w が r に追いついて衝突)
```
