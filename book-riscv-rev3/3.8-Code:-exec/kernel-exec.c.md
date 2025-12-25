
## kexec

```c

//
// the implementation of the exec() system call
//
int
kexec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  /*
    exec.c では ELF ファイルを読むためにファイルシステムにアクセスするので
    クラッシュしてもファイルシステムが壊れないように、操作をログに記録する仕組みの「開始宣言」
  */
  begin_op();

  // Open the executable file.
  // ip:inode pointer
  // opens the named binary path using namei, which is explained in Chapter 8
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Read the ELF header.
  // widely-used ELF format for executables
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  // Is this really an ELF file?
  // The first step is a quick check that the file probably contains an ELF binary. An ELF binary starts with the four-byte “magic number” 0x7F, ‘E’, ‘L’, ‘F’, or ELF_MAGIC
  if(elf.magic != ELF_MAGIC)
    goto bad;

  // exec allocates a new page table with no user mappings with proc_pagetable,
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  // Each progvhdr describes a section of the application that must be loaded into memory
  //  xv6 programs have two program section headers: one for instructions and one for data.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    // readi to read from the file.
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    // allocates memory for each ELF segment with uvmalloc , and loads each segment
    // into memory with loadseg 
    // loadseg uses walkaddr to find the physical address of the allocated memory at which to write each page of the ELF segment.
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
/*
  高アドレス
  ┌─────────────────┐ ← sz (= sp) スタックの開始点
  │  USERSTACK ページ │ ← 実際に使えるスタック領域
  │   (使用可能)     │
  ├─────────────────┤ ← stackbase
  │  1ページ (guard) │ ← アクセス不可（ガードページ）
  ├─────────────────┤ ← PGROUNDUP(sz) 元の sz
  │  data/text      │ ← プログラム本体
  └─────────────────┘
*/
  sz = PGROUNDUP(sz);
  uint64 sz1;
  // スタック + ガードページ1つ分を確保
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  // 初の1ページをアクセス不可にする
  uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
  // スタックは上から下へ伸びるので、スタックポインタを一番上に設定
  sp = sz;
  // スタックがここより下に行ったらオーバーフロー
  stackbase = sp - USERSTACK*PGSIZE;

  // Copy argument strings into new stack, remember their
  // addresses in ustack[].
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;

    // 引数文字列分のスペースを確保し、sp を16バイト境界に揃える
    // 引数文字列分だけ sp を下げる
    sp -= strlen(argv[argc]) + 1;
    // sp を16の倍数に切り下げてアラインメントを取る
    sp -= sp % 16; // riscv sp must be 16-byte aligned

    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  // It places a null pointer at the end of what will be the argv list passed to main. 
  //  C言語では NULL = 0 と定義されていて、「有効なアドレスがない（どこも指していない）」ことを表します。
  //  0番地は通常アクセス不可な領域なので、ポインタが 0 なら「無効」と判断できます。
  ustack[argc] = 0;

  // push a copy of ustack[], the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    // exec places an inaccessible page just below the stack page, so that programs that try to use more than one page will fault.
    goto bad;
  // spのpointer位置に、ustack配列の内容をコピー
  // sp → argv[0] → argv[1] → ... → argv[argc]=0
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // a0 and a1 contain arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = ulib.c:start()
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// kernel/elf.h
// File header
// 64ビットELF（ELF64）ヘッダー: 合計64バイト
struct elfhdr {
  uint magic;       // 4バイト - must equal ELF_MAGIC
  uchar elf[12];    // 12バイト
  ushort type;      // 2バイト
  ushort machine;   // 2バイト
  uint version;     // 4バイト
  uint64 entry;     // 8バイト
  uint64 phoff;     // 8バイト
  uint64 shoff;     // 8バイト
  uint flags;       // 4バイト
  ushort ehsize;    // 2バイト
  ushort phentsize; // 2バイト
  ushort phnum;     // 2バイト
  ushort shentsize; // 2バイト
  ushort shnum;     // 2バイト
  ushort shstrndx;  // 2バイト
};

// Program section header
struct proghdr {
  uint32 type;
  uint32 flags;
  uint64 off;
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz;
  uint64 align;
};
```
