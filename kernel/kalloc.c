#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void kfree_cpu(void *pa, int cpu_id); // 声明 kfree_cpu 函数

extern char end[]; // first address after kernel. defined by kernel.ld.

struct run {
  struct run *next;
};

// 定义 struct kmem
struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  // char lockname[10]; // 修复变量声明

  for (int i = 0; i < NCPU; i++) {
    // snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&kmems[i].lock, "kmem");
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  uint64 total_mem = (uint64)pa_end - (uint64)pa_start;
  uint64 total_pages = total_mem / PGSIZE;
  uint64 pages_per_cpu = total_pages / NCPU;
  uint64 remaining_pages = total_pages % NCPU;

  p = (char*)PGROUNDUP((uint64)pa_start);

  for (int i = 0; i < NCPU; i++) {
    uint64 pages_for_this_cpu = pages_per_cpu + (i < remaining_pages ? 1 : 0);
    char *cpu_start = p;
    char *cpu_end = cpu_start + pages_for_this_cpu * PGSIZE;

    for (; p + PGSIZE <= cpu_end; p += PGSIZE)
      kfree_cpu(p, i);
  }
}

void
kfree_cpu(void *pa, int cpu_id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree_cpu");

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

void *
kalloc(void)
{
  struct run *r;
  push_off();
  int id = cpuid();
  pop_off();

  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r)
    kmems[id].freelist = r->next;
  release(&kmems[id].lock);

  if(!r) {
    for(int i = 0; i < NCPU; i++) {
      if(i == id) continue;

      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if(r) {
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}

void
kfree(void *pa)
{
  push_off();
  int id = cpuid();
  pop_off();
  kfree_cpu(pa, id);
}
