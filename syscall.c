
#include "syscall.h"

#include <unistd.h>

static int sys_print(char *str, int len) {
	return write(1, str, len);
}
