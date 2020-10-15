CFLAGS = -g

CFLAGS += -MMD -MT $@ -MF $@.d

OBJ = $(patsubst %.c,%.o,$(filter-out %.app.c,$(wildcard *.c))) $(patsubst %.S,%.o,$(wildcard *.S))

APPS = $(patsubst %.app.c,%.app,$(wildcard *.app.c))

all : main $(APPS)

main : $(OBJ)

$(APPS) : %.app : %.app.c
	$(CC) -nostdlib -fpic -e main -static -Wl,-N -x c $< -o $@ -x none strlen.o

clean :
	rm -f *.o *.d main $(APPS)

-include $(patsubst %,%.d,$(OBJ))
