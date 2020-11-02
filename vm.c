#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sched.h"
#include "vm.h"

static int memfd = -1;
static unsigned memsize;
static unsigned memfdoff;

static unsigned *pageinfo;
static unsigned npageinfo;
static unsigned freehead;
static unsigned freestart;

static unsigned allocpage(void) {
	if (freehead) {
		unsigned page = freehead;
		freehead = pageinfo[page];
		return page;
	}
	if (freestart < npageinfo) {
		return 1 + freestart++;
	}
	return 0;
}

static void freepage(unsigned page) {
	if (page == 0) {
		abort();
	}
	pageinfo[page - 1] = freehead;
	freehead = page;
}

static off_t page2off(struct vmctx *vm, unsigned page) {
	return (vm->map[page] - 1) * VM_PAGESIZE + memfdoff;
}

static void applyrange(struct vmctx *vm, unsigned from, unsigned to) {
	for (unsigned i = from; i < to; ++i) {
		if (MAP_FAILED == mmap(USERSPACE_START + i * VM_PAGESIZE,
				VM_PAGESIZE,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_SHARED,
				memfd, page2off(vm, i))) {
			perror("mmap");
			abort();
		}
	}
}

void vmapplymap(struct vmctx *vm) {
	munmap(USERSPACE_START, KERNEL_START - USERSPACE_START);
	applyrange(vm, 0, vm->brk);
	applyrange(vm, vm->stack, MAX_USER_MEM / VM_PAGESIZE);
}

int vmbrk(struct vmctx *vm, void *addr) {
	if (MAX_USER_MEM <= (addr - USERSPACE_START)) {
		printf("Out-of-mem\n");
		return -1;
	}

	unsigned newbrk = (addr - USERSPACE_START + VM_PAGESIZE - 1) / VM_PAGESIZE;
	for (unsigned i = vm->brk; i < newbrk; ++i) {
		vm->map[i] = allocpage();
	}
	for (unsigned i = newbrk; i < vm->brk; ++i) {
		freepage(vm->map[i]);
	}
	vm->brk = newbrk;

	return 0;
}

void vmmakestack(struct vmctx *vm) {
	for (unsigned i = vm->stack; i < MAX_USER_MEM / VM_PAGESIZE - 1; ++i) {
		freepage(vm->map[i]);
	}

	vm->stack = MAX_USER_MEM / VM_PAGESIZE - 4;
	for (unsigned i = vm->stack; i < MAX_USER_MEM / VM_PAGESIZE - 1; ++i) {
		vm->map[i] = allocpage();
	}
}

static void copyrange(struct vmctx *vm, unsigned from, unsigned to) {
	for (unsigned i = from; i < to; ++i) {
		vm->map[i] = allocpage();
		if (-1 == pwrite(memfd,
				USERSPACE_START + i * VM_PAGESIZE,
				VM_PAGESIZE, page2off(vm, i))) {
			perror("pwrite");
			abort();
		}
	}
}


void vmctx_copy(struct vmctx *dst, struct vmctx *src) {
	dst->brk = src->brk;
	dst->stack = src->stack;
	copyrange(dst, 0, src->brk);
	copyrange(dst, src->stack, MAX_USER_MEM / VM_PAGESIZE - 1);
}

int vmprotect(void *start, unsigned len, int prot) {
#if 0
	int osprot = (prot & VM_EXEC  ? PROT_EXEC  : 0) |
		     (prot & VM_WRITE ? PROT_WRITE : 0) |
		     (prot & VM_READ  ? PROT_READ  : 0);
	if (mprotect(start, len, osprot)) {
		perror("mprotect");
		return -1;
	}
#endif
	return 0;
}

int vminit(unsigned size) {
	int fd = memfd_create("mem", 0);
	if (fd < 0) {
		perror("memfd_create");
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		perror("ftrucate");
		return -1;
	}

	memfd = fd;
	memsize = size;

	npageinfo = size / VM_PAGESIZE;
	memfdoff = (npageinfo * sizeof(unsigned) + VM_PAGESIZE - 1) & ~(VM_PAGESIZE - 1);
	npageinfo -= memfdoff / VM_PAGESIZE;

	pageinfo = mmap(NULL, npageinfo * sizeof(unsigned),
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			memfd, 0);
	if (pageinfo == MAP_FAILED) {
		perror("mmap failed");
		return -1;
	}

	return 0;
}

