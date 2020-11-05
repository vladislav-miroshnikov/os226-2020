#pragma once

#include "syscall.h"

struct hctx {
	unsigned long rax;
	unsigned long rbx;
	unsigned long rcx;
	unsigned long rdx;
	unsigned long rsi;
	unsigned long rdi;
	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;
	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;
	unsigned long rbp;
	unsigned long eflags;
	unsigned long rip;
};

void syscall_bottom(struct hctx *hctx);

#define SC_DECLARE0(ret, name) \
	ret sys_ ## name(struct hctx*);
#define SC_DECLARE1(ret, name, type1, name1) \
	ret sys_ ## name(struct hctx*, type1);
#define SC_DECLARE2(ret, name, type1, name1, type2, name2) \
	ret sys_ ## name(struct hctx*, type1, type2);
#define SC_DECLARE3(ret, name, type1, name1, type2, name2, type3, name3) \
	ret sys_ ## name(struct hctx*, type1, type2, type3);
#define SC_DECLARE4(ret, name, type1, name1, type2, name2, type3, name3, type4, name4) \
	ret sys_ ## name(struct hctx*, type1, type2, type3, type4);
#define SC_DECLARE5(ret, name, type1, name1, type2, name2, type3, name3, type4, name4, type5, name5) \
	ret sys_ ## name(struct hctx*, type1, type2, type3, type4, void*);
#define SC_DECLARE(name, ret, n, ...) \
	SC_DECLARE ## n (ret, name, ## __VA_ARGS__)
SYSCALL_X(SC_DECLARE)
#undef SC_DECLARE0
#undef SC_DECLARE1
#undef SC_DECLARE2
#undef SC_DECLARE3
#undef SC_DECLARE4
#undef SC_DECLARE5
#undef SC_DECLARE

extern void irq_disable(void);

extern void irq_enable(void);
