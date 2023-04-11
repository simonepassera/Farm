CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
OBJDIR = ./obj
TESTDIR = ./test

INCLUDES = -I $(INCDIR)

TARGETS = $(BINDIR)/farm \
		  $(BINDIR)/collector

.PHONY: all test clean

# target rule
all: $(TARGETS)

$(BINDIR)/farm: $(SRCDIR)/masterworker.c $(OBJDIR)/queue.o
	$(CC) $^ -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/collector: $(SRCDIR)/collector.c $(INCDIR)/utils.h
	$(CC) $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

test: all $(BINDIR)/generafile
	$(TESTDIR)/test.sh

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

clean:
	rm -f $(TARGETS) $(BINDIR)/generafile

cleanall: clean
	rm -f $(OBJDIR)/*.o