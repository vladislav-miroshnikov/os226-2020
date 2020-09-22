CFLAGS = -g

CFLAGS += -MMD -MT $@ -MF $@.d

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)

all : main

main : $(OBJ)

clean :
	rm -f *.o *.d main

-include $(patsubst %,%.d,$(OBJ))
