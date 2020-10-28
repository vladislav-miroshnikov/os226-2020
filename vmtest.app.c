#include <string.h>
#include "syscall.h"

char msg[16];

int main(int argc, char *argv[]) {
	strncpy(msg, argv[1], sizeof(msg));
	msg[15] = '\0';
	int msglen = strlen(msg);

	while (1) {
		os_print(msg, msglen);
		os_print("\n", 1);
		for (volatile int i = 100000; 0 < i; --i) {
		}
	}

	return 0;
}
