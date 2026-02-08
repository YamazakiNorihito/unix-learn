# sleeplock.c

## acquiresleep

```c
void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

```

3つのプロセス (A, B, C) が同じファイルの sleeplock を取ろうとする場合：

```
時刻    CPU 0 (プロセスA)      CPU 1 (プロセスB)      CPU 2 (プロセスC)      sleeplock状態
────    ─────────────────      ─────────────────      ─────────────────      ─────────────
                                                                              locked=0
                                                                              spinlock=0
T1      acquire(&lk->lk)
        └→ spinlock取得 ✓
        while(lk->locked)                                                     spinlock=1
        └→ 0なのでスキップ
        lk->locked = 1                                                        locked=1
        release(&lk->lk)                                                      spinlock=0

        ── ファイル処理中 ──    acquire(&lk->lk)       acquire(&lk->lk)
                               └→ spinlock取得 ✓      └→ spinlock待ち 🔄     spinlock=1
                                                         (whileで回る)
                                                         CPUを譲らない！

T2      ── ファイル処理中 ──    while(lk->locked)
                               └→ 1なのでsleep()
                               └→ spinlock解放        └→ spinlock取得 ✓      spinlock=1
                               └→ 状態=SLEEPING
                               └→ CPU譲る 💤
                                                      while(lk->locked)
                                                      └→ 1なのでsleep()
                                                      └→ spinlock解放        spinlock=0
                                                      └→ 状態=SLEEPING
                                                      └→ CPU譲る 💤

        ── ファイル処理中 ──    💤 寝てる              💤 寝てる              locked=1
                               (CPUは他の仕事)        (CPUは他の仕事)

T3      releasesleep()
        acquire(&lk->lk)                                                      spinlock=1
        lk->locked = 0                                                        locked=0
        wakeup(lk)
        └→ B,C両方起こす 📢
        release(&lk->lk)                                                      spinlock=0

T4      ── 完了 ──             起きた！               起きた！
                               acquire(&lk->lk)       acquire(&lk->lk)
                               └→ spinlock取得 ✓      └→ spinlock待ち 🔄     spinlock=1

T5                             while(lk->locked)
                               └→ 0なのでスキップ！
                               lk->locked = 1                                 locked=1
                               release(&lk->lk)       └→ spinlock取得 ✓      spinlock=1

T6                             ── ファイル処理中 ──   while(lk->locked)
                                                      └→ 1なのでsleep()
                                                      └→ また寝る 💤

T7                             releasesleep()
                               wakeup(lk) 📢                                  locked=0

T8                             ── 完了 ──             起きた！
                                                      while(lk->locked)
                                                      └→ 0なのでスキップ！
                                                      lk->locked = 1          locked=1
                                                      ── ファイル処理中 ──

T9                                                    releasesleep()
                                                      ── 完了 ──              locked=0
```

```
┌───────────────────────┬─────────────────┬────────────────────────┐
│         状況          │     待ち方      │          CPU           │
├───────────────────────┼─────────────────┼────────────────────────┤
│ acquire(&lk->lk) 待ち │ 🔄 while で回る │ 占有（譲らない）       │
├───────────────────────┼─────────────────┼────────────────────────┤
│ sleep() 待ち          │ 💤 寝る         │ 譲る（他の仕事できる） │
└───────────────────────┴─────────────────┴────────────────────────┘
```

spinlock 待ちは一瞬なので問題ないですが、sleeplock 待ちは長いので sleep で CPU を譲ります。
