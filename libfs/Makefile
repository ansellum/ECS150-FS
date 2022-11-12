# Compiler
CC = gcc

# General gcc options
CFLAGS	:= -Wall -Wextra -Werror

# C files to compile
src=$(wildcard *.c)
objs=$(patsubst %.c,%.o,$(src))

# Target library
lib := libfs.a

all: $(lib)

$(lib): $(objs)
	ar rcs $(lib) $(objs)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.a *.o 