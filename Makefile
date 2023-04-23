CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3
PTHREAD = -pthread

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
OBJDIR = ./obj
TESTDIR = ./test

INCLUDES = -I $(INCDIR)

TARGET = $(BINDIR)/farm

.PHONY: all test directories clean

# target rule
all: directories $(TARGET)

test: all $(BINDIR)/generafile
	@$(TESTDIR)/test.sh

directories:
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)

clean:
	rm -f $(TARGET) $(BINDIR)/generafile

cleanall: clean
	rm -f $(OBJDIR)/*.o

$(BINDIR)/farm: $(SRCDIR)/masterworker.c $(OBJDIR)/collector.o $(OBJDIR)/queue.o $(OBJDIR)/concurrentqueue.o $(OBJDIR)/threadpool.o $(OBJDIR)/utils.o
	$(CC) $^ -o $@ $(PTHREAD) $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/collector.o: $(SRCDIR)/collector.c $(INCDIR)/collector.h $(INCDIR)/utils.h
	$(CC) $< -c -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/threadpool.o: $(SRCDIR)/threadpool.c $(INCDIR)/threadpool.h $(INCDIR)/concurrentqueue.h $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(PTHREAD) $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/queue.o: $(SRCDIR)/queue.c $(INCDIR)/queue.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/concurrentqueue.o: $(SRCDIR)/concurrentqueue.c $(INCDIR)/concurrentqueue.h $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/utils.o: $(SRCDIR)/utils.c $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)