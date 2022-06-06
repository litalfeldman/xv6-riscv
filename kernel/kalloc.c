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

int references[PHYSICAL_ADDRESS_TO_INDEX(PHYSTOP)];
struct spinlock r_lock;

extern uint cas(volatile void *addr, int expected, int newval);

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&r_lock, "refereces");
  memset(references, 0, sizeof(int)*PHYSICAL_ADDRESS_TO_INDEX(PHYSTOP));
  freerange(end, (void*)PHYSTOP);
}

int
reference_remove(uint64 pa)
{
  int ref;
  do 
  {
    ref = references[PHYSICAL_ADDRESS_TO_INDEX(pa)];
  }
  while (cas(&(references[PHYSICAL_ADDRESS_TO_INDEX(pa)]), ref, ref-1));
  return ref-1;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int index = PHYSICAL_ADDRESS_TO_INDEX((uint64)pa);

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Page count is larger than 0, so there is no need to remove the page
  if (reference_remove((uint64)pa) >= 1)
    return;

  references[index] = 0;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  int index = PHYSICAL_ADDRESS_TO_INDEX((uint64)r);
  int newPage = 1;

  if(r) {
    references[index] = newPage;
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int
reference_add(uint64 pa)
{
  int ref;
  do 
  {
    ref = references[PHYSICAL_ADDRESS_TO_INDEX(pa)];
  }
  while (cas(&(references[PHYSICAL_ADDRESS_TO_INDEX(pa)]), ref, ref+1));
  return ref+1;
}