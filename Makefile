CFLAGS = -g

CFLAGS += -MMD -MT $@ -MF $@.d

OBJ = $(patsubst %.c,%.o,$(filter-out %.app.c,$(wildcard *.c))) $(patsubst %.S,%.o,$(wildcard *.S))

APPS = $(patsubst %.app.c,%.app,$(wildcard *.app.c))

KERNEL_START := 0xf00000000
USERSPACE_START := 0x400000

CFLAGS += -DIKERNEL_START=$(KERNEL_START) -DIUSERSPACE_START=$(USERSPACE_START)

all : main $(APPS)

main : $(OBJ)
	$(CC) -Wl,-Ttext-segment=$(KERNEL_START) $^ -o $@

$(APPS) : %.app : %.app.c
	$(CC) -fno-pic -Wl,-Ttext-segment=$(USERSPACE_START) -nostdlib -e main -static $< -o $@ strlen.o strncpy.c

clean :
	rm -f *.o *.d main $(APPS)

-include $(patsubst %,%.d,$(OBJ))
