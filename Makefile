CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3

AR = ar
ARFLAGS = rvs

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
LIBDIR = ./lib
TESTDIR = ./test

INCLUDES = -I $(INCDIR)
LDFLAGS = -L $(LIBDIR)
LDLIBS = -lutil -lqueue

TARGET = $(BINDIR)/generafile 	\
		 $(BINDIR)/farm 		\
		 $(BINDIR)/collector

.PHONY: all clean

# pattern lib .c -> .o
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

# pattern lib.o -> lib.a
$(LIBDIR)/lib%.a: $(SRCDIR)/lib%.o
	$(AR) $(ARFLAGS) $@ $^

# first rule
all: $(TARGET)

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/farm: $(SRCDIR)/masterworker.c $(LIBDIR)/libutil.a $(LIBDIR)/libqueue.a
	$(CC) $< -o $@ $(INCLUDES) $(LDFLAGS) $(LDLIBS) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/collector: $(SRCDIR)/collector.c
	$(CC) $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

clean:
	rm -f $(TARGET)

cleanall: clean
	find . \( -name "*.a" -o -name "*.o" \) -exec rm -f {} \;