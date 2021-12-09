// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct refCnt
{
  int ref[(PHYSTOP-KERNBASE) / PGSIZE];
  struct spinlock lock;
} globalRefCnt;

void
acquire_globalRef() {
  acquire(&globalRefCnt.lock);
}

void
release_globalRef() {
  release(&globalRefCnt.lock);
}



// 这样并发写有问题 外部场景下的并发不能考 incr的原子操作完成

void
incrWithoutLock(uint64 pa) {
  // acquire(&globalRefCnt.lock);
  globalRefCnt.ref[(pa - KERNBASE)/PGSIZE] += 1;
  // release(&globalRefCnt.lock);
}

void
decrWithoutLock(uint64 pa) {
  // acquire(&globalRefCnt.lock);
  globalRefCnt.ref[(pa - (uint64)KERNBASE)/PGSIZE] -= 1;
  // release(&globalRefCnt.lock);
}

int
getRefCntWithoutLock(uint64 pa) {
  int res = 0;
  // acquire(&globalRefCnt.lock);
  res = globalRefCnt.ref[(pa - (uint64)KERNBASE)/PGSIZE];
  // release(&globalRefCnt.lock);
  return res;
}

void
setRefCntWithoutLock(uint64 pa, int val) {
  // acquire(&globalRefCnt.lock);
  globalRefCnt.ref[(pa - (uint64)KERNBASE)/PGSIZE] = val;
  // release(&globalRefCnt.lock);
}

void initRefCnt() {
  // 应该不用加锁
  for (int i = 0; i < (PHYSTOP-KERNBASE) / PGSIZE; i++) {
    globalRefCnt.ref[i] = 0;
  }
}

void
kinit()
{
  initlock(&globalRefCnt.lock, "gref");
  initRefCnt();
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  // 不确定效果
  // initRefCnt();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  acquire_globalRef();
  if (getRefCntWithoutLock((uint64)pa) > 1) {
    decrWithoutLock((uint64)pa);
    release_globalRef();
    return ;
  }

  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  // 1 -> 0
  // decr((uint64)pa);
  setRefCntWithoutLock((uint64)pa, 0);
  release_globalRef();

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

  // release_globalRef();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire_globalRef();
    incrWithoutLock((uint64)r);
    release_globalRef();
  }
  return (void*)r;
}


void *
kalloc_nolock(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // acquire_globalRef();
    incrWithoutLock((uint64)r);
    // release_globalRef();
  }
  return (void*)r;
}