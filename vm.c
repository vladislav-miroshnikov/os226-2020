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
		unsigned page = freehead - 1;
		freehead = pageinfo[page];
		return page;
	}
	if (freestart < npageinfo) {
		return freestart++;
	}
	return 0;
}

static void freepage(unsigned page) {
	pageinfo[page] = freehead;
	freehead = page + 1;
}

void vmapplymap(void) {
	munmap(USERSPACE_START, KERNEL_START - USERSPACE_START);

	struct task *t = sched_current();
	for (unsigned i = 0; i < t->vmctx.brk; ++i) {
		if (MAP_FAILED == mmap(USERSPACE_START + i * VM_PAGESIZE,
				VM_PAGESIZE,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_SHARED,
				memfd,
				t->vmctx.map[i] * VM_PAGESIZE + memfdoff)) {
			perror("mmap");
			abort();
		}
	}
}

int vmbrk(void *addr) {
	struct task *t = sched_current();

	if (MAX_USER_MEM <= (addr - USERSPACE_START)) {
		printf("Out-of-mem\n");
		return -1;
	}

	unsigned newbrk = (addr - USERSPACE_START + VM_PAGESIZE - 1) / VM_PAGESIZE;
	for (unsigned i = t->vmctx.brk; i < newbrk; ++i) {
		t->vmctx.map[i] = allocpage();
	}
	for (unsigned i = newbrk; i < t->vmctx.brk; ++i) {
		freepage(t->vmctx.map[i]);
		t->vmctx.map[i] = 0;
	}
	t->vmctx.brk = newbrk;

	vmapplymap();
	return 0;
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

