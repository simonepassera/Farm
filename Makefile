CC = gcc -std=gnu99
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

$(BINDIR)/farm: $(SRCDIR)/masterworker.c $(OBJDIR)/queue.o $(OBJDIR)/concurrentqueue.o $(OBJDIR)/threadpool.o $(OBJDIR)/utils.o
	$(CC) $^ -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/collector: $(SRCDIR)/collector.c $(OBJDIR)/utils.o
	$(CC) $^ -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/threadpool.o: $(SRCDIR)/threadpool.c $(INCDIR)/threadpool.h $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/queue.o: $(SRCDIR)/queue.c $(INCDIR)/queue.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/concurrentqueue.o: $(SRCDIR)/concurrentqueue.c $(INCDIR)/concurrentqueue.h $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/utils.o: $(SRCDIR)/utils.c $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

test: all $(BINDIR)/generafile
	$(TESTDIR)/test.sh

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

clean:
	rm -f $(TARGETS) $(BINDIR)/generafile

cleanall: clean
	rm -f $(OBJDIR)/*.o
