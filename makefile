# Makefile for CSC360 Assignment 4
# Builds: diskinfo, disklist, diskget, diskput

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
LDFLAGS  =
SRCS     = fs.c diskinfo.c disklist.c diskget.c diskput.c
OBJS     = $(SRCS:.c=.o)
TARGETS  = diskinfo disklist diskget diskput

.PHONY: all clean

all: $(TARGETS)

diskinfo: diskinfo.o fs.o
	$(CC) $(CFLAGS) -o $@ diskinfo.o fs.o $(LDFLAGS)

disklist: disklist.o fs.o
	$(CC) $(CFLAGS) -o $@ disklist.o fs.o $(LDFLAGS)

diskget: diskget.o fs.o
	$(CC) $(CFLAGS) -o $@ diskget.o fs.o $(LDFLAGS)

diskput: diskput.o fs.o
	$(CC) $(CFLAGS) -o $@ diskput.o fs.o $(LDFLAGS)

%.o: %.c fs.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
