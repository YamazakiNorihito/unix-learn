# 2.9 Exercises

1. Add a system call to xv6 that returns the amount of free memory available.

主な変更はkernel/kalloc.c。それ以外は、terminalで実行するために必要な修正

## kernel/kalloc.c

変更箇所:

```diff
diff --git a/kernel/kalloc.c b/kernel/kalloc.c
index 0699e7e..6b5a912 100644
--- a/kernel/kalloc.c
+++ b/kernel/kalloc.c
@@ -80,3 +80,19 @@ kalloc(void)
     memset((char*)r, 5, PGSIZE); // fill with junk
   return (void*)r;
 }
+
+uint64
+getfreemem(void)
+{
+  struct run *r;
+  int count = 0;
+
+  acquire(&kmem.lock);
+  r = kmem.freelist;
+  while (r) {
+      count++;
+      r = r->next;
+  }
+  release(&kmem.lock);
+  return count * PGSIZE;
+}

```

## Makefile

変更箇所:

```diff
diff --git a/Makefile b/Makefile
index dde08b1..adf4912 100644
--- a/Makefile
+++ b/Makefile
@@ -143,6 +143,8 @@ UPROGS=\
        $U/_logstress\
        $U/_forphan\
        $U/_dorphan\
+       $U/_freemem\
+       $U/_memtest\

 fs.img: mkfs/mkfs README $(UPROGS)
        mkfs/mkfs fs.img README $(UPROGS)

```

理解するために:

```make
_%: %.o $(ULIB) $U/user.ld
 $(LD) $(LDFLAGS) -T $U/user.ld -o $@ $< $(ULIB)
 $(OBJDUMP) -S $@ > $*.asm
 $(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym
```

`_`で始まる全てのファイルは'.o'＋ライブラリ群をまとめて静的リンクしている
具体例:`_freemem`を作るために`freemem.o`＋`ライブラリ群`をまとめて静的リンクしている

```make
UPROGS=\
 $U/_freemem\
 $U/_memtest\
```

## user/usys.pl

変更箇所:

```diff
diff --git a/user/usys.pl b/user/usys.pl
index c5d4c3a..fd7b439 100755
--- a/user/usys.pl
+++ b/user/usys.pl
@@ -42,3 +42,4 @@ entry("getpid");
 entry("sbrk");
 entry("pause");
 entry("uptime");
+entry("getfreemem");
```

理解するために:

ユーザープログラムが`fork()`などを呼んだとき、実際にカーネルのシステムコールを呼び出すための**ラッパー関数（スタブ）**を自動生成しています。
`user/usys.S`のファイルを生成するために追加。下記を生成している。

```assembly
.global getfreemem
getfreemem:
 li a7, SYS_getfreemem
 ecall
 ret
```

`Makefile` の106行目で
`print`で標準出力に書き出すで、`Makefile`側で`> usys.S`とリダイレクトしてファイルに保存

```makefile
$U/usys.S : $U/usys.pl
 perl $U/usys.pl > $U/usys.S
```

## kernel/defs.h

変更箇所:

```diff
diff --git a/kernel/defs.h b/kernel/defs.h
index 122d9ca..9b44ae2 100644
--- a/kernel/defs.h
+++ b/kernel/defs.h
@@ -59,6 +59,7 @@ void            ireclaim(int);
 void*           kalloc(void);
 void            kfree(void *);
 void            kinit(void);
+uint64          getfreemem(void);

 // log.c
 void            initlog(int, struct superblock*);
```

理解するために:

**defs.h = カーネル空間に公開するインターフェース**

- 各.cファイルの公開関数プロトタイプを集約
- `#include "defs.h"` で他モジュールの関数を呼び出し可能に

例: `kalloc.c`の`getfreemem()`を`sysproc.c`から呼ぶために宣言が必要

## kernel/syscall.c

変更箇所:

```diff
diff --git a/kernel/syscall.c b/kernel/syscall.c
index 076d965..a8c5f0a 100644
--- a/kernel/syscall.c
+++ b/kernel/syscall.c
@@ -101,6 +101,7 @@ extern uint64 sys_unlink(void);
 extern uint64 sys_link(void);
 extern uint64 sys_mkdir(void);
 extern uint64 sys_close(void);
+extern uint64 sys_getfreemem(void);

 // An array mapping syscall numbers from syscall.h
 // to the function that handles the system call.
@@ -126,6 +127,7 @@ static uint64 (*syscalls[])(void) = {
 [SYS_link]    sys_link,
 [SYS_mkdir]   sys_mkdir,
 [SYS_close]   sys_close,
+[SYS_getfreemem] sys_getfreemem,
 };

 void
```

理解するために:

- なぜ`syscall.c`で宣言するのか:
  - `syscalls[]`配列に関数ポインタとして登録するため
  - この配列がシステムコール番号→関数の振り分け係になっている
- `extern uint64 sys_getfreemem(void);`は宣言のみ。実装は`kernel/sysproc.c`にある
- extern = 「この関数は別のファイルで定義されている」という意味
- コンパイル時: 実体をチェックしない（信じるだけ）
- リンク時: 全 `.o` から関数名で探して結びつける
- ファイル名は関係ない。どこにあってもOK

## kernel/syscall.h

変更箇所:

```diff
diff --git a/kernel/syscall.h b/kernel/syscall.h
index 3dd926d..9141f53 100644
--- a/kernel/syscall.h
+++ b/kernel/syscall.h
@@ -20,3 +20,4 @@
 #define SYS_link   19
 #define SYS_mkdir  20
 #define SYS_close  21
+#define SYS_getfreemem 22
```

理解するために:

システムコール番号の一覧表
ユーザーとカーネルの「共通言語」となる番号の定義ファイル

## kernel/sysproc.c

変更箇所:

```diff
diff --git a/kernel/sysproc.c b/kernel/sysproc.c
index 419e727..3be0720 100644
--- a/kernel/sysproc.c
+++ b/kernel/sysproc.c
@@ -107,3 +107,9 @@ sys_uptime(void)
   release(&tickslock);
   return xticks;
 }
+
+uint64
+sys_getfreemem(void)
+{
+  return getfreemem();
+}
```

理解するために:

1. ユーザ側でgetfreemem()を呼ぶ
     - [usys.pl](https://github.com/mit-pdos/xv6-riscv/blob/f5dea58cc1057f2b076cdb90b446c2c21d91171e/user/usys.pl#L20)が生成するusys.Sのラッパが動き、RISC-Vのecall命令を発行してシステムコール番号（例: SYS_getfreemem）をレジスタa7に載せる。
2. ecallでトラップしてカーネルに入る
     1. CPUがSモードに切り替わり、stvecレジスタ(「トラップ（ecall/割り込み/例外）が来たら、最初にここに飛べ」というアドレス)が指す[trampoline.S:uservec](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trampoline.S#L22)にジャンプ。
     2. uservecがtrapframe->kernel_trapに保存された[usertrap()](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L38)を呼ぶ。
     3. この設定は[usertrapret()](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L110-L117)で事前に行われている。
3. usertrap()がシステムコールか判定し、syscall()へ
     - scauseを見てecall(値が8)なら[syscall()](https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L68)を呼ぶ。
4. syscall()が番号に対応する関数をディスパッチ
     - [syscalls[]テーブル](https://github.com/mit-pdos/xv6-riscv/blob/f5dea58cc1057f2b076cdb90b446c2c21d91171e/kernel/syscall.c#L108)（kernel/syscall.c）でSYS_getfreemem → sys_getfreememの関数ポインタを引く。
5. sys_getfreemem()（kernel/sysproc.c）が実処理を実行し、戻り値をa0に載せる
     - ここではカーネル内のメモリ残量を計算するなどして返す。

## user/user.h

変更箇所:

```diff
diff --git a/user/user.h b/user/user.h
index ac84de9..511a773 100644
--- a/user/user.h
+++ b/user/user.h
@@ -24,6 +24,7 @@ int getpid(void);
 char* sys_sbrk(int,int);
 int pause(int);
 int uptime(void);
+int getfreemem(void);

 // ulib.c
 int stat(const char*, struct stat*);
```

理解するために:

- user.h は defs.h のユーザーランド版（ユーザープログラム用の関数宣言集）
- int getfreemem(void); は宣言のみ。実装は usys.S（usys.pl から自動生成）にある
- コンパイラに「この関数は存在する」と教えるための宣言
- 実際の結びつけはリンカ（ld）が名前（シンボル）で行う
- usys.S の .global getfreemem で公開されたシンボルとリンク時にマッチする

## user/memtest.c

変更箇所:

```diff
diff --git a/user/memtest.c b/user/memtest.c
new file mode 100644
index 0000000..8caa09a
--- /dev/null
+++ b/user/memtest.c
@@ -0,0 +1,40 @@
+#include "kernel/types.h"
+#include "kernel/stat.h"
+#include "user/user.h"
+
+int
+main(int argc, char *argv[])
+{
+  int before, after, diff;
+  char *p;
+
+  before = getfreemem();
+  printf("Free memory before malloc: %d bytes\n", before);
+
+  // 1MBのメモリを割り当て
+  printf("Allocating 1MB...\n");
+  p = malloc(1000000);
+
+  if (p == 0) {
+    printf("malloc failed\n");
+    exit(1);
+  }
+
+  // メモリに書き込んでページが実際に割り当てられるようにする
+  for (int i = 0; i < 1000000; i++) {
+    p[i] = 'x';
+  }
+
+  after = getfreemem();
+  printf("Free memory after malloc: %d bytes\n", after);
+
+  diff = before - after;
+  printf("Memory used: %d bytes\n", diff);
+
+  free(p);
+
+  int final = getfreemem();
+  printf("Free memory after free: %d bytes\n", final);
+
+  exit(0);
+}
```

理解するために:

メモリが消費されていることを確認する遊びプログラム。
