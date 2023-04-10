CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
OBJDIR = ./obj
TESTDIR = ./test

INCLUDES = -I $(INCDIR)

TARGET = $(BINDIR)/farm

.PHONY: test clean

# target rule
$(TARGET): $(SRCDIR)/masterworker.c $(OBJDIR)/queue.o $(OBJDIR)/collector.o
	$(CC) $^ -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/utils.h
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

test: $(TARGET) $(BINDIR)/generafile
	$(TESTDIR)/test.sh

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

clean:
	rm -f $(TARGET) $(BINDIR)/generafile

cleanall: clean
	rm -f $(OBJDIR)/*.o