#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
    uint64 addr;
    int flags;

    // 获取两个参数：addr 和 flags
    if (argaddr(0, &addr) < 0 || argint(1, &flags) < 0) {
        return -1;
    }

    // 调用 wait 函数时传递两个参数
    return wait(addr, flags);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

int sys_yield(void) {
    struct proc *p = myproc();  // 当前执行的进程
    struct proc *next = 0;      // 用于存储下一个 RUNNABLE 的进程
    int found = 0;              // 标记是否找到 RUNNABLE 的进程

    acquire(&p->lock);  // 获取当前进程的锁

    // 打印当前进程上下文保存的地址区间
    printf("Save the context of the process to the memory region from address %p to %p\n", 
           &(p->context.ra), 
           &(p->context.s11)+1);

    // 打印当前进程的用户态PC值
    printf("Current running process pid is %d and user pc is %p\n", 
           p->pid, 
           (void*)p->trapframe->epc);

    // 环形遍历，寻找下一个 RUNNABLE 的进程
    for (int i = 0; i < NPROC; i++) {
        next = &proc[(p - proc + i + 1) % NPROC];  // 从当前进程开始的下一个

        acquire(&next->lock);  // 获取下一个进程的锁

        if (next->state == RUNNABLE) {
            // 找到下一个 RUNNABLE 进程，打印信息
            printf("Next runnable process pid is %d and user pc is %p\n", 
                   next->pid, 
                   (void*)next->trapframe->epc);
            found = 1;  // 标记已找到
            release(&next->lock);
            break;
        }

        release(&next->lock);  // 如果进程不是 RUNNABLE，释放锁
    }

    release(&p->lock);  // 释放当前进程的锁

    if (found) {
        // 当前进程设置为 RUNNABLE
        p->state = RUNNABLE;
        yield();
    }
    return 0;
}