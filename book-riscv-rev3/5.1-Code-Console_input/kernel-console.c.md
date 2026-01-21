# kernel/console.c

## consoleinit()

```c
void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();
  
  // 関数のアドレスを `devsw` 配列のメモリに格納している。
  /*
  devsw[CONSOLE] のメモリ内容:
    ┌─────────┬─────────────────────────┐
    │ .read   │ 0x80001234 (例)         │ ← consoleread関数のアドレス
    ├─────────┼─────────────────────────┤
    │ .write  │ 0x80001456 (例)         │ ← consolewrite関数のアドレス
    └─────────┴─────────────────────────┘
  */
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}

// kernel/file.h
#define CONSOLE 1

struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};
```

## なぜこうするのか？

`int (*read)(int, uint64, int)` と `int (*write)(int, uint64, int)` は **インターフェース** のようなもの。

カーネルは `devsw[デバイス番号].read()` を呼ぶだけで、実際にどの関数が呼ばれるかは登録次第。

### 多態性（ポリモーフィズム）の実現

```
ユーザー: read(fd, buf, n)
              ↓
カーネル: devsw[major].read(...)
              ↓
     ┌────────┴────────┐
     ↓                 ↓
consoleread()     将来の別デバイスread()
(CONSOLE=1)       (例: ネットワーク、ディスク等)
```

同じインターフェース（`.read`, `.write`）で異なる実装を呼び分けられる。これがCでの多態性の実現方法。

## consoleread

```c
int
consoleread(int user_dst, // コピー先がユーザー空間かどうかのフラグ
            uint64 dst, //  コピー先のアドレス
            int n // 読み込みたいバイト数
            )
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    // リングバッファから1文字取得。                                                               
    // cons.r をインクリメントし、% 128 で循環（127の次は0に戻る）。 
    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file
      if(n < target){ // すでにデータを読んでいたら
        // Ctrl+D をバッファに戻す（読み取り位置を1つ戻す）
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      // ループを抜ける
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      // 改行文字を検出したらループを抜ける
      break;
    }
  }
  release(&cons.lock);
  // 要求数 - 残り = 読んだバイト数
  return target - n;
}

struct {
  struct spinlock lock;
  
  // input circular buffer
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE]; // char = 1バイト × 128 = 128バイト
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;
```

### リングバッファの仕組み

`cons.r == cons.w` の条件でバッファが空かどうかを判定する。

```
1. 初期状態（空）: r == w

   ┌───┬───┬───┬───┬───┬───┬───┬───┐
   │   │   │   │   │   │   │   │   │
   └───┴───┴───┴───┴───┴───┴───┴───┘
     ↑
    r,w (両方同じ位置 = 空)


2. データ書き込み後: r != w

   ┌───┬───┬───┬───┬───┬───┬───┬───┐
   │ H │ e │ l │ l │ o │   │   │   │
   └───┴───┴───┴───┴───┴───┴───┴───┘
     ↑               ↑
     r               w

   → 読み取り可能なデータがある


3. 読み取り後: r が進む

   ┌───┬───┬───┬───┬───┬───┬───┬───┐
   │ H │ e │ l │ l │ o │   │   │   │
   └───┴───┴───┴───┴───┴───┴───┴───┘
             ↑       ↑
             r       w

   → "He" を読み取り済み、"llo" が残っている


4. 全部読んだ: r == w（再び空）

   ┌───┬───┬───┬───┬───┬───┬───┬───┐
   │ H │ e │ l │ l │ o │   │   │   │
   └───┴───┴───┴───┴───┴───┴───┴───┘
                       ↑
                      r,w

   → バッファは空（データは残っているが読み取り済み）
```

| ポインタ | 役割 |
|---------|------|
| `r` (Read) | 次に読み取る位置 |
| `w` (Write) | 次に書き込む位置 |
| `e` (Edit) | 編集中の位置（バックスペース等で使用） |

### Ctrl+D（EOF）の処理

`C('D')` は Ctrl+D を表すマクロ。ターミナルで「入力終了」を伝える。

```
$ cat
hello      ← 入力
hello      ← catが出力
world      ← 入力
world      ← catが出力
^D         ← Ctrl+D を押す → EOF、catが終了
$
```

| キー | 意味 | 動作 |
|------|------|------|
| Ctrl+D | EOF (End Of File) | 「入力終了」を通知、プログラムは正常終了 |
| Ctrl+C | SIGINT (Interrupt) | プロセスを強制終了（シグナル送信） |
