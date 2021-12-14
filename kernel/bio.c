// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


#define BUCKET_NUM 13
struct {
  struct spinlock lock;
  struct buf buf[NBUF / BUCKET_NUM + 1];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache[BUCKET_NUM];


// 怎么设计hashtable 设计固定size 然后开链 + lru?

void
binit(void)
{
  struct buf *b;
  // initlock(&bcache[0].lock, "bcache");
  for (int i = 0; i < BUCKET_NUM; ++i) {
  initlock(&bcache[i].lock, "bcache");

  // Create linked list of buffers
  bcache[i].head.prev = &bcache[i].head;
  bcache[i].head.next = &bcache[i].head;
  for(b = bcache[i].buf; b < bcache[i].buf+NBUF/BUCKET_NUM + 1; b++){
    b->next = bcache[i].head.next;
    b->prev = &bcache[i].head;
    initsleeplock(&b->lock, "buffer");
    bcache[i].head.next->prev = b;
    bcache[i].head.next = b;
  }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket = blockno % BUCKET_NUM;

  acquire(&bcache[bucket].lock);

  // Is the block already cached?
  for (b = bcache[bucket].head.next; b != &bcache[bucket].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache[bucket].head.prev; b != &bcache[bucket].head; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  int rec_buc = 0;

  // steal from others
  // 当前bucket对应的buf不够呢 ? 溢出?
  for (int i = 0; i < BUCKET_NUM; ++i) {
    if (i == bucket) {
      continue;
    } else {
      acquire(&bcache[i].lock);
      for (b = bcache[i].head.prev; b != &bcache[i].head; b = b->prev) {
        if (b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          rec_buc = i;

          goto find;
          release(&bcache[i].lock);
          // pay attention
          release(&bcache[bucket].lock);
          acquiresleep(&b->lock);
          return b;
        }
      }
      for (b = bcache[i].head.next; b != &bcache[i].head; b = b->next) {
        if (b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          rec_buc = i;
          goto find;
          release(&bcache[i].lock);
          // pay attention
          release(&bcache[bucket].lock);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache[i].lock);
    }
  }



  release(&bcache[bucket].lock);
  panic("bget: no buffers");

  find:
  b->prev->next = b->next;
  b->next->prev = b->prev;
  
  
  // 原来担心这里会溢出 其实不会 因为访问时其实是按照head来访问

  // set to bucket 当release 放回原bucket
  b->next = bcache[bucket].head.next;
  b->prev = &bcache[bucket].head;
  bcache[bucket].head.next->prev = b;
  bcache[bucket].head.next = b;


  release(&bcache[rec_buc].lock);
    // pay attention
  release(&bcache[bucket].lock);
  acquiresleep(&b->lock);
  return b;

}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  // int bucket = blockno % BUCKET_NUM;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  // 都release在一个bucket ？ hash算法的问题

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket = b->blockno % BUCKET_NUM;

  acquire(&bcache[b->blockno % BUCKET_NUM].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache[bucket].head.next;
    b->prev = &bcache[bucket].head;
    bcache[bucket].head.next->prev = b;
    bcache[bucket].head.next = b;
  }

  release(&bcache[bucket].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache[b->blockno % BUCKET_NUM].lock);
  b->refcnt++;
  release(&bcache[b->blockno % BUCKET_NUM].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache[b->blockno % BUCKET_NUM].lock);
  b->refcnt--;
  release(&bcache[b->blockno % BUCKET_NUM].lock);
}


