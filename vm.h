#pragma once

#define VM_READ  (1 << 0)
#define VM_WRITE (1 << 1)
#define VM_EXEC (1 << 2)

#define USERSPACE_START ((void*)IUSERSPACE_START)
#define KERNEL_START ((void*)IKERNEL_START)
#define VM_PAGESIZE 4096

struct app_range
{
    off_t start;
    off_t end;
};

int vmbrk(void *addr);

int vmprotect(void *start, unsigned len, int prot);

int vminit(unsigned size);

int get_g_memfd();