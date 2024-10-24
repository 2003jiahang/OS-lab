#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NHASH 13
#define hash(dev, blockno) (((dev) + (blockno)) % NHASH)

struct hbuf {
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct hbuf hash[NHASH];
  struct buf buf[NBUF];
} bcache;

extern uint ticks; // Timestamps from kernel/trap.c

void
binit(void)
{
  struct buf *b;
  int i;

  // Initialize hash buckets
  for(i = 0; i < NHASH; i++) {
    initlock(&bcache.hash[i].lock, "bcache hash");
    bcache.hash[i].head = 0;
  }

  // Initialize buffers and add them to hash buckets
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->dev = ~0;
    b->blockno = ~0;
    b->valid = 0;
    b->refcnt = 0;
    b->timestamp = 0;

    // Assign buffer to a hash bucket based on its index to distribute them evenly
    int h = (b - bcache.buf) % NHASH;

    // Insert buffer into the hash bucket's linked list
    acquire(&bcache.hash[h].lock);
    b->next = bcache.hash[h].head;
    if(bcache.hash[h].head)
      bcache.hash[h].head->prev = b;
    bcache.hash[h].head = b;
    b->prev = 0;
    release(&bcache.hash[h].lock);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int h = hash(dev, blockno);

  acquire(&bcache.hash[h].lock);

  // Is the block already cached?
  for(b = bcache.hash[h].head; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache.hash[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Try to find an unused buffer in the hash bucket.
  for(b = bcache.hash[h].head; b != 0; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->timestamp = ticks;
      release(&bcache.hash[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hash[h].lock);

  // No free buffer in this bucket; search all buckets for the least recently used buffer.
  struct buf *victim = 0;
  uint minticks = 0xFFFFFFFF;
  int i;

  for(i = 0; i < NHASH; i++) {
    acquire(&bcache.hash[i].lock);
    for(b = bcache.hash[i].head; b != 0; b = b->next){
      if(b->refcnt == 0 && b->timestamp <= minticks){
        minticks = b->timestamp;
        victim = b;
      }
    }
    release(&bcache.hash[i].lock);
  }

  if(victim == 0)
    panic("bget: no buffers");

  // Lock both old and new hash buckets in a fixed order to prevent deadlocks
  int hv = hash(victim->dev, victim->blockno);
  int h1 = hv < h ? hv : h;
  int h2 = hv < h ? h : hv;

  if(h1 != h2) {
    acquire(&bcache.hash[h1].lock);
    acquire(&bcache.hash[h2].lock);
  } else {
    acquire(&bcache.hash[h1].lock);
  }

  // Remove victim from old hash bucket
  if(victim->prev)
    victim->prev->next = victim->next;
  else
    bcache.hash[hv].head = victim->next;
  if(victim->next)
    victim->next->prev = victim->prev;

  // Add victim to new hash bucket
  victim->next = bcache.hash[h].head;
  if(bcache.hash[h].head)
    bcache.hash[h].head->prev = victim;
  bcache.hash[h].head = victim;
  victim->prev = 0;

  // Update buffer
  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;
  victim->timestamp = ticks;

  if(h1 != h2) {
    release(&bcache.hash[h2].lock);
    release(&bcache.hash[h1].lock);
  } else {
    release(&bcache.hash[h1].lock);
  }

  acquiresleep(&victim->lock);
  return victim;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

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
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int h = hash(b->dev, b->blockno);
  acquire(&bcache.hash[h].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // Update timestamp to record when the buffer became free
    b->timestamp = ticks;
  }
  release(&bcache.hash[h].lock);
}

void
bpin(struct buf *b)
{
  int h = hash(b->dev, b->blockno);
  acquire(&bcache.hash[h].lock);
  b->refcnt++;
  release(&bcache.hash[h].lock);
}

void
bunpin(struct buf *b)
{
  int h = hash(b->dev, b->blockno);
  acquire(&bcache.hash[h].lock);
  b->refcnt--;
  release(&bcache.hash[h].lock);
}
