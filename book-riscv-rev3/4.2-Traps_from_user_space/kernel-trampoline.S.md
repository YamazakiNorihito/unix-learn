
# Kernel Trampoline

```assembly
trampoline:
.align 4
.globl uservec
uservec:    
        #
        # trap.c sets stvec to point here, so
        # traps from user space start here,
        # in supervisor mode, but with a
        # user page table.
        #

        # save user a0 in sscratch so
        # a0 can be used to get at TRAPFRAME.
        csrw sscratch, a0

        # each process has a separate p->trapframe memory area,
        # but it's mapped to the same virtual address
        # (TRAPFRAME) in every process's user page table.
        li a0, TRAPFRAME
        
        # save the user registers in TRAPFRAME
        # kernel/proc.h
        # https://github.com/mit-pdos/xv6-riscv/blob/cf0c095721b22b8ce011484a2509cd27858cbe77/kernel/proc.h#L43
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

        # save the user a0 in p->trapframe->a0
        csrr t0, sscratch
        sd t0, 112(a0)

        # initialize kernel stack pointer, from p->trapframe->kernel_sp
        ld sp, 8(a0)

        # make tp hold the current hartid, from p->trapframe->kernel_hartid
        ld tp, 32(a0)

        # load the address of usertrap(), from p->trapframe->kernel_trap
        ld t0, 16(a0)

        # fetch the kernel page table address, from p->trapframe->kernel_satp.
        # trapframe[0..7] には、トラップ発生前に C 側でkernel page table (kernel_satp) が設定されている
        # https://github.com/mit-pdos/xv6-riscv/blob/7d7adbb1b0acbd67c9766a20d0f9900fef2789fa/kernel/trap.c#L115
        ld t1, 0(a0)

        # wait for any previous memory operations to complete, so that
        # they use the user page table.
        sfence.vma zero, zero

        # install the kernel page table.
        csrw satp, t1

        # flush now-stale user entries from the TLB.
        sfence.vma zero, zero

        # call usertrap()
        jalr t0

```

## TRAPFRAMEに保存されるレジスタ一覧

| オフセット | レジスタ | 個数 | 説明 |
|-----------|---------|------|------|
| 0(a0) | kernel_satp | 1 | カーネルページテーブル（読み込み用） |
| 8(a0) | kernel_sp | 1 | カーネルスタックポインタ（読み込み用） |
| 16(a0) | kernel_trap | 1 | usertrap()のアドレス（読み込み用） |
| 32(a0) | kernel_hartid | 1 | ハートID（読み込み用） |
| 40(a0) | ra | 1 | リターンアドレス |
| 48(a0) | sp | 1 | スタックポインタ |
| 56(a0) | gp | 1 | グローバルポインタ |
| 64(a0) | tp | 1 | スレッドポインタ |
| 72-88(a0) | t0-t2 | 3 | テンポラリレジスタ |
| 96(a0) | s0 | 1 | 保存レジスタ |
| 104(a0) | s1 | 1 | 保存レジスタ |
| 112(a0) | a0 | 1 | 引数レジスタ（sscratch経由で保存） |
| 120-168(a0) | a1-a7 | 7 | 引数レジスタ |
| 176-248(a0) | s2-s11 | 10 | 保存レジスタ |
| 256-280(a0) | t3-t6 | 4 | テンポラリレジスタ |

TRAPFRAME はプロセスごとに1ページ（4KB）確保され、トラップ発生時のユーザ CPU レジスタ状態と、カーネルへ遷移・復帰するための情報を8バイト単位で保存する領域である。
a0 は TRAPFRAME のアドレスを指すために再利用されるため、元のユーザ a0 の値は事前に sscratch に退避される。

このアセンブリは、ユーザプロセス実行中にトラップが発生した際、ユーザCPUレジスタ状態を trapframe が指す 4KB の保存領域に退避し、その後 usertrap を安全に実行するためにページテーブルやスタックをカーネル用に切り替えてからusertrap を呼び出す処理である。
