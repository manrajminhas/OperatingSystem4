# Makefile for CSC360 a4
# Builds the following: diskinfo, disklist, diskget, diskput

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
LDFLAGS  =
SRCS     = diskinfo.c disklist.c diskget.c diskput.c fs.c
OBJS     = $(SRCS:.c=.o)
TARGETS  = diskinfo disklist diskget diskput

.PHONY: all clean

all: $(TARGETS)

# Link each executable
diskinfo: diskinfo.o fs.o
	$(CC) $(CFLAGS) -o $@ diskinfo.o fs.o $(LDFLAGS)

disklist: disklist.o fs.o
	$(CC) $(CFLAGS) -o $@ disklist.o fs.o $(LDFLAGS)

diskget: diskget.o fs.o
	$(CC) $(CFLAGS) -o $@ diskget.o fs.o $(LDFLAGS)

diskput: diskput.o fs.o
	$(CC) $(CFLAGS) -o $@ diskput.o fs.o $(LDFLAGS)

# Compile .c files to .o
%.o: %.c fs.h
	$(CC) $(CFLAGS) -c $<

# Clean up binaries and object files
clean:
	rm -f $(TARGETS) *.o
