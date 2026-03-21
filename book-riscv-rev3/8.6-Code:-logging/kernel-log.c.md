
## log_write

```c
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGBLOCKS)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  /*
    log.lh.n:「要素数」と「次の空き位置」の両方を兼ねている
      - 要素は block[0] ～ block[n-1] に入っている
      - だから block[n] が次の空き 
  */
  for (i = 0; i < log.lh.n; i++) {
    //  allocates that block the same slot in the log
    if (log.lh.block[i] == b->blockno)   // log absorption
      break;
  }
  // i はちょうど n まで進むので、block[n]に書き込めば末尾追加になり、n++ すれば要素数が正しく更新される。
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // Add new block to log?
    // bpin は refcnt をインクリメントするだけ
    bpin(b);
    // 新規ブロックを追加したので、ログの有効エントリ数を1増やす
    log.lh.n++;
  }
  release(&log.lock);
}
```
