CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
TESTDIR = ./test

INCLUDES = -I $(INCDIR)

TARGET = $(BINDIR)/generafile 	\
		 $(BINDIR)/farm 		\
		 $(BINDIR)/collector

.PHONY: all clean

# first rule
all: $(TARGET)

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/farm: $(SRCDIR)/masterworker.c
	$(CC) $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/collector: $(SRCDIR)/collector.c
	$(CC) $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

clean:
	rm -f $(TARGET)
