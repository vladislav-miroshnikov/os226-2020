#define _GNU_SOURCE

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "vm.h"
#include "sched.h"

static int g_memfd = -1;
static unsigned g_memsize;
static off_t offset;
static struct app_range range_buf[16];
static int id;
static struct app_range* curr_range;
static int vm_aligne_pages(void* addr);

int vmbrk(void *addr) {
	if (MAP_FAILED == mmap(USERSPACE_START,
			addr - USERSPACE_START,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_FIXED | MAP_SHARED,
			g_memfd, offset)) {
		perror("mmap g_memfd");
		return -1;
	}

	struct app_range* range = &range_buf[id++];
	range->start = offset;
	range->end = offset + addr - USERSPACE_START -1;
	curr_range = range;
	offset += (vm_aligne_pages(addr) * VM_PAGESIZE); 
	sched_reg_set(curr_range);
	return 0;
}

int vmprotect(void *start, unsigned len, int prot) {
	// int osprot = (prot & VM_EXEC  ? PROT_EXEC  : 0) |
	// 	     (prot & VM_WRITE ? PROT_WRITE : 0) |
	// 	     (prot & VM_READ  ? PROT_READ  : 0);
	// if (mprotect(start, len, osprot)) {
	// 	perror("mprotect");
	// 	return -1;
	// }
	return 0;
}

static int vm_aligne_pages(void* addr)
{
	return ((addr - USERSPACE_START) % VM_PAGESIZE == 0)? 
                (addr - USERSPACE_START) / VM_PAGESIZE :
                (addr - USERSPACE_START) / VM_PAGESIZE + 1;
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

	g_memfd = fd;
	g_memsize = size;
	return 0;
}

int get_g_memfd()
{
	return g_memfd;
}