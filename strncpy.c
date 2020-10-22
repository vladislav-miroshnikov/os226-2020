#include <string.h>

char *strncpy(char *dest, const char *src, size_t n) {
	char *d = dest;
	while (0 < n && (*(d++) = *(src++))) {
		--n;
	}
	return dest;
}
