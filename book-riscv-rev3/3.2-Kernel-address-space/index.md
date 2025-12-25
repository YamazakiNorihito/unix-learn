## trampoline

> a physical page (holding the trampoline code) is
> mapped twice in the virtual address space of the kernel: once at top of the virtual address
> space and once with a direct mapping.

カーネル空間における話。

trampolineに関して物理メモリは 1箇所だけ、仮想アドレス空間は二つ同じ値を管理する。
  物理メモリ                      仮想アドレス空間
  ┌─────────────┐                ┌─────────────┐ MAXVA
  │             │                │ TRAMPOLINE  │───┐
  │             │                ├─────────────┤   │
  │             │                │     ...     │   │
  │             │                │             │   │  同じ物理ページを
  │ trampoline  │◄───────────────┼─────────────┤   │  指している
  │ (1ページ)    │◄───────────────│ direct map  │───┘
  │             │                │             │
  └─────────────┘                └─────────────┘

direct map を使うことも可能だが、kernel stack を high-memory mapping に配置している理由は、guard page を利用して、各プロセス専用の kernel stack のメモリ領域を互いに独立させるためである。

さらに言うと、stack が割り当てられた領域を超えて成長した場合に、stack overflow として確実にエラーを発生させたいという目的がある。guard page は物理メモリを割り当てず、対応するページテーブルエントリを無効にするだけで実現できるため、guard page を設けること自体が追加の物理メモリ消費につながることはない。

- PTE_X (Execute bit) は、そのページから命令フェッチを許可するかどうかを示すビットです。
  - PTE_X = 1: そのページから命令を読み込んで実行できる
  - PTE_X = 0: そのページから命令フェッチするとInstruction Page Faultが発生する
