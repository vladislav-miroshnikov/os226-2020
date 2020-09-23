
#include "syscall.h"

#include <unistd.h>

int sys_print(char *str, int len) {
	return write(1, str, len);
}
