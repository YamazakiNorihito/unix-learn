
## binit

```c
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;


void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // head <-> head. 自身への循環を作る。
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // 最終的に: head <-> buf[NBUF-1] <-> ... <-> buf[1] <-> buf[0] <-> head
    // head.nextの値をb[n].nextに引き継ぎ、b[n].prevはheadを指す。
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // head.nextはこの時点でb[n-1]を指しているので、b[n-1].prevをbに書き換える
    bcache.head.next->prev = b;
    // head.nextをb[n]に更新する。
    bcache.head.next = b;
  }
}
```

## brelse

```c

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // head <-> buf[NBUF-1] <-> ... <-> buf[1] <-> buf[0] <-> head と並んでいるとする。
    // 例えば、buf[1]を解放する場合、
    // 最終的には
    // head <-> buf[1] <-> buf[NBUF-1] <-> ... <-> buf[2] <-> buf[0] <-> head
    // b = buf[1]
    // buf[1].next.prev(buf[0].prev) = buf[1].prev = buf[2]
    b->next->prev = b->prev;
    // buf[1].prev.next(buf[2].next) = buf[1].next = buf[0]
    b->prev->next = b->next;
    // buf[1].next = head.next = buf[NBUF-1]
    b->next = bcache.head.next;
    // buf[1].prev = head
    b->prev = &bcache.head;
    // head.next.prev(buf[NBUF-1].prev) = buf[1]
    bcache.head.next->prev = b;
    // head.next = buf[1]
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

```
