
#include <stdarg.h>
#include <stdio.h>

#include "syscall.h"
#include "libc.h"

int os_printf(const char *fmt, ...) {
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return os_print(buf, ret);
}
