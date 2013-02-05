CC=gcc
CXX=g++
RM=rm
WARNINGS := -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align \
			-Wwrite-strings -Wmissing-declarations -Wredundant-decls \
			-Winline -Wno-long-long -Wuninitialized -Wconversion -Werror
CWARNINGS := $(WARNINGS) -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes
CFLAGS := -g -fPIC -std=c99 $(CWARNINGS)

SRCOBJS := $(patsubst %.c,%.o,$(wildcard *.c))

all: interpreter

interpreter: $(SRCOBJS)
	gcc -o interpreter $(SRCOBJS)

# Blindly depend on all headers
%.o: %.c Makefile  $(wildcard *.h)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

clean:
	-$(RM) -f $(SRCOBJS) interpreter

.PHONY: all clean
