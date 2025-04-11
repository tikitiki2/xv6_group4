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



/*-----------PAGE FAULT HANDLING----------*/

int handle_page_fault(struct proc *p, uint64 fault_addr) {


    printf("handle pagefault\n");
    struct lazyseg *segs = p->lazysegs;
    printf("Handling page fault at address 0x%lx\n", fault_addr);

    for (int i = 0; i < p->num_lazysegs; i++) {
        struct lazyseg *seg = &segs[i];
        if (fault_addr >= seg->va_start && fault_addr < seg->va_end) {
            // Found a segment covering the faulting address

            
            char *mem = kalloc();
            if (!mem) {
                printf("Failed to allocate memory for page\n");
                return -1;
            }
            

            // Compute how much to read — don't go past segment end
            uint64 page_start = PGROUNDDOWN(fault_addr);
            uint64 offset_in_seg = page_start - seg->va_start;
            uint file_offset = seg->file_offset + offset_in_seg;


            uint bytes_to_read = PGSIZE;
            if (page_start + PGSIZE > seg->va_end)
                bytes_to_read = seg->va_end - page_start;

            ilock(seg->ip);
            int read = readi(seg->ip, 0, (uint64)mem, file_offset, bytes_to_read);
            iunlock(seg->ip);
            
       
            if (read < 0) {
                kfree(mem);
                return -1;
            }
            
            // Map page with appropriate permissions 
           
            int perm = PTE_U | PTE_V;
            if (seg->flags & 0x1) perm |= PTE_X;  // Executable
            if (seg->flags & 0x2) perm |= PTE_W;  // Writable
            if (seg->flags & 0x4) perm |= PTE_R;  // Readable


  
            
            printf("Mapping VA 0x%lx (file offset: %d, size: %d)\n", page_start, file_offset, bytes_to_read);
            pte_t *pte = walk(p->pagetable, page_start, 0);
            
            if (pte && (*pte & PTE_V)) {
                printf("Page already mapped at 0x%lx — assuming OK, but skipping load.\n", page_start);
                kfree(mem);
                return 0;
            }

            if (mappages(p->pagetable, page_start, PGSIZE, (uint64)mem, perm) < 0) {
                kfree(mem);
                printf("FAIL -1\n");
                return -1;
            }
            pte_t *pte_check = walk(p->pagetable, PGROUNDDOWN(fault_addr), 0);
            if (!pte_check || !(*pte_check & PTE_V)) {
                printf("❌ ERROR: Faulting address 0x%lx is NOT mapped after mappages()\n", fault_addr);
            } else {
                printf("✅ Mapped faulting address 0x%lx → PTE flags: 0x%lx\n", fault_addr, *pte_check);
            }
            printf("SUCCESS\n");
            return 0; // success
        }
    }

    printf("FAIL - NO MATCHING SEGMENT\n");
    return 1; // No matching segment
}

/*-----------PAGE FAULT HANDLING----------*/



//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
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

/*--------PAGE FAULT CATCHING---------*/

//printf("Current process pid: %d\n", p->pid);

    // Check for page fault -> if the cause is from instruction, load or store access fault
if (r_scause() == 0xc || r_scause() == 0xd || r_scause() == 0xf) { 
        uint64 fault_addr = r_stval(); // faulting address


        printf("Faulting address: 0x%lx\n", r_stval());

        int ret_val = handle_page_fault(p, fault_addr);
        if (ret_val == 0) {
            printf("Page catching success for pid=%d, va=0x%lx\n", p->pid, fault_addr);
            usertrapret(); // successful, just return
       } else if (ret_val == -2) {
          return;  // this means the fault address is outside of stack region (which is checked in handle_page_fault) so we just return back to the process
        } else {
            p->killed = 1; // marking it to kill
            printf("Page fault handling failed for pid=%d, va=0x%lx\n", p->pid, fault_addr);
        }
    }


 /*--------PAGE FAULT CATCHING---------*/
  
  // save user program counter.
  p->trapframe->epc = r_sepc();

  // print the value of sepc
  //printf("HELLO sepc value: 0x%lx\n", p->trapframe->epc);

  
  if(r_scause() == 8 || r_scause() == 9){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok */
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();
  //printf("returning to usermode\n");
  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
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

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
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
    // interrupt or trap from an unknown source
    printf("Unknown interrupt or trap\n");
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  } 

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  w_stimecmp(r_time() + 1000000);
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

  if(scause == 0x8000000000000009L){
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
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else if(scause == 0xc) { // HANDLING PAGE FAULTTTTT
    usertrap();
    printf("Page fault or access violation occurred\n");
    return 0;  
  } else {
    return 0;
  }
}

