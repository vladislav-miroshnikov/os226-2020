#include <string.h>
#include "syscall.h"

int main(int argc, char *argv[]) {
	for (int i = 1; i < argc - 1; ++i) {
		os_print(argv[i], strlen(argv[i]));
		os_print(" ", 1);
	}
	if (1 < argc) {
		os_print(argv[argc - 1], strlen(argv[argc - 1]));
		os_print("\n", 1);
	}
	return argc - 1;
}


