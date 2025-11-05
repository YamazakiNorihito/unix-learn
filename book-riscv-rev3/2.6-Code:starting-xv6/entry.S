        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        // RISC-Vでは "hart" (hardware thread) という用語でCPUコアを指す

        // start.c では各hartごとに4096バイトのスタック領域を持つ配列が宣言されている
        // __attribute__ ((aligned (16))) char stack0[4096 * NCPU];
        // 例: NCPU=8なら 4096*8 = 32KBの配列で、各hartが4KBずつ使う

        // 各hartのスタック配置イメージ（スタックは下向きに成長）:
        // hart0: stack0[0]    ... stack0[4095]   ← sp初期値: stack0 + 4096
        // hart1: stack0[4096] ... stack0[8191]   ← sp初期値: stack0 + 8192
        // hart2: stack0[8192] ... stack0[12287]  ← sp初期値: stack0 + 12288

        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + ((hartid + 1) * 4096)
        la sp, stack0

        // a0 = 4096 (各hartのスタックサイズ)
        li a0, 1024*4

        // a1 = mhartid (現在実行中のhart ID: 0, 1, 2, ...)
        csrr a1, mhartid

        // a1 = hartid + 1
        // hartid+1を使う理由: 各hartのスタック領域の「最上位アドレス」を算出するため
        // スタックは下向きに成長するので、領域の終端（上端）をspに設定する
        addi a1, a1, 1
        // a0 = 4096 * (hartid + 1) = 自分のスタック領域の終端オフセット
        mul a0, a0, a1
        // sp = stack0 + 4096*(hartid+1) = 自分のスタック領域の最上位アドレス
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:
        j spin
