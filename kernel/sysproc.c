#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"


uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void) {
  uint64 mk = 0;
  argaddr(0, &mk);
  myproc()->mask = (int)mk;
  return 0;
}

uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 addr;
  // 获取用户态传入的sysinfo结构体
  if (argaddr(0, &addr) < 0) 
    return -1;
  //获取当前进程
  struct proc* p = myproc();
  //计算剩余内存
  info.freemem = kgetFree();
  //计算进程数量
  info.nproc = getProcNum();
  // 计算load average 省事一点 打印出来
  /*
  printf("cal load average \n");
  float num = get_load_average();
  printf("when call sysinfo, load average %f \n", num);
  */
  int proc = getRunnableNum();
  /*
  int cpu_num = 8;
  // float会直接panic 但double可以? TODO
  double load_average = 0;
  if (proc < cpu_num) {
    // printf("asdasd %d  %d \n", proc, load_average);
    load_average = 0;
  } else {
    load_average = ((double)(proc - cpu_num)) / cpu_num;
  }
  */
  // printf 不支持%f
  printf("load average %d \n", proc);
 // 拷贝info到用户空间
  if (copyout(p->pagetable, addr, (char*)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}