#pragma once

#define VM_READ  (1 << 0)
#define VM_WRITE (1 << 1)
#define VM_EXEC (1 << 2)

#define USERSPACE_START ((void*)IUSERSPACE_START)
#define KERNEL_START ((void*)IKERNEL_START)
#define VM_PAGESIZE 4096

#define MAX_USER_MEM (1024 * 1024)

struct task;

struct vmctx {
	unsigned map[MAX_USER_MEM / VM_PAGESIZE];
	unsigned brk;
	unsigned stack;
};

static inline void vmctx_make(struct vmctx *vm) {
	vm->brk = 0;
	vm->stack = MAX_USER_MEM / VM_PAGESIZE;
}

void vmctx_copy(struct vmctx *dst, struct vmctx *src);

void vmmakestack(struct vmctx *vm);

int vmbrk(struct vmctx *vm, void *addr);

int vmprotect(void *start, unsigned len, int prot);

int vminit(unsigned size);

void vmapplymap(struct vmctx *vm);
