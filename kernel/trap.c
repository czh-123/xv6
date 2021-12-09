#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

extern void acquire_globalRef();

extern void release_globalRef();

extern void* kalloc_nolock();

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
// 识别page fault 并处理
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if (r_scause() == 15 || r_scause() == 13) {
    // 确认15 怎么来的  查risc-v表拿到
    // uint64 va=r_stval();
    uint64 va = r_stval();
    if(cowcopy(va) == -1 ) {
      p->killed = 1;
    }
  } else if((which_dev = devintr()) != 0){
    // ok
    // 怎么识别 page fault
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}



int
cowcopy(uint64 va) {
  // 不要>
  if(va>=MAXVA) {
      return -1;
  }
  va = PGROUNDDOWN(va);
  pagetable_t p = myproc()->pagetable;
  pte_t* pte = walk(p, va, 0);
  uint64 pa = PTE2PA(*pte);
  uint flags = PTE_FLAGS(*pte);


  if(!(flags & PTE_C)) {
    // printf("not cow\n");
    return -2; // not cow page
  }
  

  acquire_globalRef();
  
  int ref = getRefCntWithoutLock(pa);
  if (ref > 1) {
    // ref > 1, alloc a new page
    // !!!!!
    char* mem = kalloc_nolock();
    if(mem == 0)
      goto bad;
    memmove(mem, (char*)pa, PGSIZE);
    if (mappages(p, va, PGSIZE, (uint64)mem, (flags & (~PTE_C)) | PTE_W) != 0) {
      // 这里会导致死锁 所以需要先release 再 acquire
      release_globalRef();
      kfree(mem);
      acquire_globalRef();
      goto bad;
    }
    // refcnt_setter(pa, ref - 1);
    decrWithoutLock((uint64)mem);
  } else {
    // ref = 1, use this page directly
    *pte = ((*pte) & (~PTE_C)) | PTE_W;
  }
  release_globalRef();
  return 0;

  bad:
  release_globalRef();
  return -1;
}

/*
int deal_cow_page(pagetable_t pagetable,uint64 va ,int usertrap) {

    if(va>MAXVA) {
      return -1;
    }
    pte_t* pte;
    
    if ((pte = walk(pagetable,va,0))==0) {
      return -1;
    }
    
    if (*pte & PTE_C) {
	    uint64 old_pa = PTE2PA(*pte);
	    uint64 flag = PTE_FLAGS(*pte)&(~PTE_C);//清空PTE_C
	    //encounter a COW page(write to a page with PTE_W clear
      char *new_pa = kalloc();//new_pa的ref cnt=1
	    if (new_pa == 0) {
	      return -1;
	    }
      memmove(new_pa,(char *)old_pa, PGSIZE); 
	    uvmunmap(pagetable,va,1,1);//uvmunmap的alloc参数设置为1 会对old_pa进行kfree old_pa的ref cnt--
        
	    if (mappages(pagetable, va, PGSIZE, (uint64)new_pa, flag|PTE_W) < 0) {
	      uvmunmap(pagetable,va,1,1); 
	      return -1;
	    }
    } else if(usertrap) {
	    //如果是usertrap中遇到 pagefault 同时不是cow page 那么需要按照普通Page fault处理
      return -1;
    }

    return 0;
}
*/


//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

