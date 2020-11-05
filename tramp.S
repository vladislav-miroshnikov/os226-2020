
.text
.global exittramp
exittramp:
	mov %rbx, %rsp

	pop %rax
	pop %rbx
	pop %rcx
	pop %rdx
	pop %rsi
	pop %rdi
	pop %r8
	pop %r9
	pop %r10
	pop %r11
	pop %r12
	pop %r13
	pop %r14
	pop %r15
	pop %rbp
	popfq

	add $128/*red_zone*/ + 8/*rip push*/,%rsp
	jmp *-(128+8)(%rsp)
